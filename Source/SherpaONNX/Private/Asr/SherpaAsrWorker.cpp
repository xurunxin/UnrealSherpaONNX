#include "SherpaAsrWorker.h"

#if WITH_SHERPA_ONNX
#include "SherpaKws/SherpaKwsNative.h"
#include "sherpa-onnx/c-api/c-api.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogSherpaAsrWorker, Log, All);

FSherpaAsrWorker::FSherpaAsrWorker(const FSherpaAsrConfig& InConfig)
	: Config(InConfig)
{
}

FSherpaAsrWorker::~FSherpaAsrWorker()
{
	RequestStopAndWait();
}

bool FSherpaAsrWorker::InitAsr()
{
	if (Config.EncoderPath.IsEmpty() || Config.DecoderPath.IsEmpty() ||
		Config.JoinerPath.IsEmpty()  || Config.TokensPath.IsEmpty())
	{
		EmitError(TEXT("ASR model paths are incomplete."));
		return false;
	}

	TArray<FString> MissingModelPaths;
	auto CheckModelFile = [&MissingModelPaths](const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return;
		}
		FString AbsolutePath = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(AbsolutePath);
		if (!FPaths::FileExists(AbsolutePath))
		{
			MissingModelPaths.Add(AbsolutePath);
		}
	};
	CheckModelFile(Config.EncoderPath);
	CheckModelFile(Config.DecoderPath);
	CheckModelFile(Config.JoinerPath);
	CheckModelFile(Config.TokensPath);
	CheckModelFile(Config.BpeVocab);
	if (!MissingModelPaths.IsEmpty())
	{
		FString Error = TEXT("ASR model files not found:");
		for (const FString& MissingPath : MissingModelPaths)
		{
			Error += FString::Printf(TEXT("\n  - %s"), *MissingPath);
		}
		UE_LOG(LogSherpaAsrWorker, Error, TEXT("%s"), *Error);
		EmitError(Error);
		return false;
	}

	auto& API = SherpaKws_GetAPI();
	if (!API.IsAsrLoaded())
	{
		EmitError(TEXT("ASR API not loaded (sherpa-onnx DLL missing ASR symbols?)"));
		return false;
	}

	FTCHARToUTF8 Enc(*FPaths::ConvertRelativePathToFull(Config.EncoderPath));
	FTCHARToUTF8 Dec(*FPaths::ConvertRelativePathToFull(Config.DecoderPath));
	FTCHARToUTF8 Joi(*FPaths::ConvertRelativePathToFull(Config.JoinerPath));
	FTCHARToUTF8 Tok(*FPaths::ConvertRelativePathToFull(Config.TokensPath));
	FTCHARToUTF8 Prov(*Config.Provider);
	FTCHARToUTF8 Meth(Config.bEnableHotwords ? TEXT("modified_beam_search") : *Config.DecodingMethod);
	FTCHARToUTF8 ModelUnit(*Config.ModelingUnit);
	FTCHARToUTF8 BpeVoc(*Config.BpeVocab);

	EncoderPathUtf8    = Enc.Get();
	DecoderPathUtf8    = Dec.Get();
	JoinerPathUtf8     = Joi.Get();
	TokensPathUtf8     = Tok.Get();
	ProviderUtf8       = Prov.Get();
	DecodingMethodUtf8 = Meth.Get();
	ModelingUnitUtf8   = ModelUnit.Get();
	BpeVocabUtf8       = BpeVoc.Get();

	// Config-level hotwords string
	if (Config.bEnableHotwords && !Config.HotwordsString.IsEmpty())
	{
		FTCHARToUTF8 Hw(*Config.HotwordsString);
		HotwordsBuf = std::string(Hw.Get(), Hw.Length());
	}

	SherpaOnnxOnlineRecognizerConfig Cfg;
	FMemory::Memzero(&Cfg, sizeof(Cfg));

	Cfg.feat_config.sample_rate = 16000;
	Cfg.feat_config.feature_dim = 80;

	Cfg.model_config.transducer.encoder = EncoderPathUtf8.c_str();
	Cfg.model_config.transducer.decoder = DecoderPathUtf8.c_str();
	Cfg.model_config.transducer.joiner  = JoinerPathUtf8.c_str();
	Cfg.model_config.tokens   = TokensPathUtf8.c_str();
	Cfg.model_config.num_threads = Config.NumThreads;
	Cfg.model_config.provider = ProviderUtf8.c_str();
	Cfg.model_config.modeling_unit = ModelingUnitUtf8.c_str();
	if (!BpeVocabUtf8.empty()) Cfg.model_config.bpe_vocab = BpeVocabUtf8.c_str();

	Cfg.decoding_method = DecodingMethodUtf8.c_str();
	Cfg.max_active_paths = Config.MaxActivePaths;
	Cfg.enable_endpoint  = Config.bEnableEndpoint ? 1 : 0;
	Cfg.rule1_min_trailing_silence = Config.Rule1MinTrailingSilence;
	Cfg.rule2_min_trailing_silence = Config.Rule2MinTrailingSilence;
	Cfg.rule3_min_utterance_length = Config.Rule3MinUtteranceLength;
	Cfg.hotwords_score  = Config.HotwordsScore;
	if (!HotwordsBuf.empty())
	{
		Cfg.hotwords_buf      = HotwordsBuf.c_str();
		Cfg.hotwords_buf_size = (int32_t)HotwordsBuf.size();
	}

	Recognizer = API.CreateOnlineRecognizer(&Cfg);
	if (!Recognizer)
	{
		EmitError(TEXT("Failed to create ASR online recognizer."));
		return false;
	}

	Stream = API.CreateOnlineStream(Recognizer);
	if (!Stream)
	{
		API.DestroyOnlineRecognizer(Recognizer);
		Recognizer = nullptr;
		EmitError(TEXT("Failed to create ASR online stream."));
		return false;
	}

	return true;
}

void FSherpaAsrWorker::DestroyAsr()
{
	auto& API = SherpaKws_GetAPI();
	if (API.IsAsrLoaded())
	{
		if (Stream && API.DestroyOnlineStream)
		{
			API.DestroyOnlineStream(Stream);
			Stream = nullptr;
		}
		if (Recognizer && API.DestroyOnlineRecognizer)
		{
			API.DestroyOnlineRecognizer(Recognizer);
			Recognizer = nullptr;
		}
	}
}

uint32 FSherpaAsrWorker::Run()
{
	bRunning = true;

	if (!InitAsr())
	{
		bRunning = false;
		return 1;
	}

	while (!bStopRequested)
	{
		TArray<float> Block;
		if (AudioQueue.Dequeue(Block))
		{
			auto& API = SherpaKws_GetAPI();
			if (API.AcceptWaveform && Block.Num() > 0)
			{
				API.AcceptWaveform(Stream, 16000, Block.GetData(), Block.Num());
			}
			ProcessResults();
		}
		else
		{
			FPlatformProcess::Sleep(0.005f);
		}
	}

	// Flush remaining audio
	if (Stream)
	{
		auto& API = SherpaKws_GetAPI();
		if (API.InputFinished) API.InputFinished(Stream);
		ProcessResults();
	}

	DestroyAsr();
	bRunning = false;
	return 0;
}

void FSherpaAsrWorker::ProcessResults()
{
	auto& API = SherpaKws_GetAPI();
	if (!API.IsOnlineStreamReady || !API.DecodeOnlineStream || !API.GetOnlineStreamResult) return;
	if (!Recognizer || !Stream) return;

	while (API.IsOnlineStreamReady(Recognizer, Stream))
	{
		API.DecodeOnlineStream(Recognizer, Stream);

		const char* Json = API.GetOnlineStreamResultAsJson ?
			API.GetOnlineStreamResultAsJson(Recognizer, Stream) : nullptr;

		if (Json && *Json)
		{
			FSherpaAsrResult Result = BuildResult(Json, nullptr, true, SegmentIndex);
			if (!Result.Text.IsEmpty())
			{
				auto Callback = OnPartialResult;
				AsyncTask(ENamedThreads::GameThread,
					[Callback, Result]() { Callback(Result); });
			}

			if (API.DestroyOnlineStreamResultJson)
			{
				API.DestroyOnlineStreamResultJson(Json);
			}
		}

		bool bIsEndpoint = API.OnlineStreamIsEndpoint &&
			API.OnlineStreamIsEndpoint(Recognizer, Stream) != 0;

		// Get final text after endpoint
		if (bIsEndpoint)
		{
			const SherpaOnnxOnlineRecognizerResult* R =
				API.GetOnlineStreamResult(Recognizer, Stream);
			if (R && R->text)
			{
				FString FinalText(UTF8_TO_TCHAR(R->text));
				if (!FinalText.TrimStartAndEnd().IsEmpty())
				{
					FSherpaAsrResult FinalResult;
					FinalResult.Text = FinalText;
					FinalResult.bIsPartial = false;
					FinalResult.SegmentId = SegmentIndex++;

					auto Callback = OnFinalResult;
					AsyncTask(ENamedThreads::GameThread,
						[Callback, FinalResult]() { Callback(FinalResult); });
				}
			}
			if (R && API.DestroyOnlineRecognizerResult)
			{
				API.DestroyOnlineRecognizerResult(R);
			}
			if (API.OnlineStreamReset)
			{
				API.OnlineStreamReset(Recognizer, Stream);
			}
		}
	}
}

FSherpaAsrResult FSherpaAsrWorker::BuildResult(const char* Json, const char* Text, bool bPartial, int32 SegmentId)
{
	FSherpaAsrResult Result;
	Result.bIsPartial = bPartial;
	Result.SegmentId  = SegmentId;

	FString JsonStr;
	if (Json) JsonStr = UTF8_TO_TCHAR(Json);
	if (Text) Result.Text = UTF8_TO_TCHAR(Text);
	Result.Json = JsonStr;

	// Try to extract text from JSON
	if (Result.Text.IsEmpty() && !JsonStr.IsEmpty())
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			JsonObj->TryGetStringField(TEXT("text"), Result.Text);

			const TArray<TSharedPtr<FJsonValue>>* TimestampsArr;
			if (JsonObj->TryGetArrayField(TEXT("timestamps"), TimestampsArr))
			{
				Result.Timestamps.Reserve(TimestampsArr->Num());
				for (const auto& Val : *TimestampsArr)
				{
					Result.Timestamps.Add((float)Val->AsNumber());
				}
			}
		}
	}

	return Result;
}

bool FSherpaAsrWorker::Start()
{
	if (Thread) return true;
	bStopRequested = false;
	Thread = FRunnableThread::Create(this, TEXT("SherpaAsrWorker"), 0, TPri_Normal);
	return Thread != nullptr;
}

void FSherpaAsrWorker::RequestStopAndWait()
{
	bStopRequested = true;
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FSherpaAsrWorker::PushAudio(const TArray<float>& InSamples)
{
	if (InSamples.Num() > 0)
	{
		AudioQueue.Enqueue(InSamples);
	}
}

void FSherpaAsrWorker::SetHotwords(const FString& Hotwords)
{
	auto& API = SherpaKws_GetAPI();
	if (!API.IsAsrLoaded() || !Recognizer || !API.CreateOnlineStreamWithHotwords) return;

	FTCHARToUTF8 Hw(*Hotwords);
	const SherpaOnnxOnlineStream* NewStream = API.CreateOnlineStreamWithHotwords(Recognizer, Hw.Get());
	if (!NewStream) return;

	if (Stream && API.DestroyOnlineStream)
	{
		API.DestroyOnlineStream(Stream);
	}
	Stream = NewStream;
	UE_LOG(LogSherpaAsrWorker, Log, TEXT("Hotwords updated: %s"), *Hotwords);
}

void FSherpaAsrWorker::EmitError(const FString& Error)
{
	UE_LOG(LogSherpaAsrWorker, Error, TEXT("%s"), *Error);
	if (OnError)
	{
		auto Callback = OnError;
		AsyncTask(ENamedThreads::GameThread, [Callback, Error]() { Callback(Error); });
	}
}

#endif // WITH_SHERPA_ONNX
