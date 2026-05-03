#include "KwsWorker.h"

#if WITH_SHERPA_ONNX
#include "SherpaKws/SherpaKwsNative.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogKwsWorker, Log, All);

namespace
{
constexpr double KwsShutdownWaitSeconds = 2.0;
FCriticalSection GWorkerRegistryLock;
TArray<FKwsWorker*> GWorkerRegistry;

#if PLATFORM_WINDOWS
bool ForceTerminateThreadById(uint32 ThreadId)
{
	HANDLE ThreadHandle = ::OpenThread(THREAD_TERMINATE | SYNCHRONIZE, false, ThreadId);
	if (!ThreadHandle)
	{
		return false;
	}

	const bool bTerminated = ::TerminateThread(ThreadHandle, 1) != 0;
	if (bTerminated)
	{
		::WaitForSingleObject(ThreadHandle, 1000);
	}

	::CloseHandle(ThreadHandle);
	return bTerminated;
}
#endif

void RegisterWorker(FKwsWorker* Worker)
{
	FScopeLock Lock(&GWorkerRegistryLock);
	GWorkerRegistry.AddUnique(Worker);
}

void UnregisterWorker(FKwsWorker* Worker)
{
	FScopeLock Lock(&GWorkerRegistryLock);
	GWorkerRegistry.Remove(Worker);
}

bool ValidateKeywordsAgainstTokens(const FString& TokensPath, const FString& Keywords, FString& OutError)
{
	TArray<FString> TokenLines;
	if (!FFileHelper::LoadFileToStringArray(TokenLines, *TokensPath))
	{
		OutError = FString::Printf(TEXT("Failed to read tokens file: %s"), *TokensPath);
		return false;
	}

	TSet<FString> KnownTokens;
	for (const FString& Line : TokenLines)
	{
		TArray<FString> Parts;
		Line.ParseIntoArrayWS(Parts);
		if (Parts.Num() > 0)
		{
			KnownTokens.Add(Parts[0]);
		}
	}

	TArray<FString> KeywordLines;
	Keywords.ParseIntoArrayLines(KeywordLines, true);
	for (const FString& Line : KeywordLines)
	{
		FString TokenPart;
		FString DisplayPart;
		if (!Line.Split(TEXT("@"), &TokenPart, &DisplayPart))
		{
			OutError = FString::Printf(TEXT("Invalid keyword line, missing '@': %s"), *Line);
			return false;
		}

		TArray<FString> KeywordTokens;
		TokenPart.TrimStartAndEnd().ParseIntoArrayWS(KeywordTokens);
		for (const FString& Token : KeywordTokens)
		{
			if (!KnownTokens.Contains(Token))
			{
				OutError = FString::Printf(TEXT("Keyword token '%s' is not present in tokens file: %s"), *Token, *Line);
				return false;
			}
		}
	}

	return true;
}

bool TryExtractKeywordFromResultJson(const FString& ResultJson, FString& OutKeyword)
{
	TSharedPtr<FJsonObject> JsonObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResultJson);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		return false;
	}

	return JsonObj->TryGetStringField(TEXT("keyword"), OutKeyword);
}
}

FKwsWorker::FKwsWorker(const FSherpaKwsModelConfig& Config)
	: ModelConfig(Config)
{
	RegisterWorker(this);
}

FKwsWorker::~FKwsWorker()
{
	Shutdown();
	DestroySpotter();
	UnregisterWorker(this);
}

bool FKwsWorker::Init()
{
	UE_LOG(LogKwsWorker, Log, TEXT("Worker Init: thread created"));
	return true;
}

uint32 FKwsWorker::Run()
{
	bRunning = true;
	UE_LOG(LogKwsWorker, Log, TEXT("KWS worker thread started"));

	UE_LOG(LogKwsWorker, Log, TEXT("Worker Run: creating spotter..."));
	if (!CreateSpotter())
	{
		bFailed = true;
		bRunning = false;
		UE_LOG(LogKwsWorker, Error, TEXT("KWS worker failed to create spotter"));

		const FOnKwsWorkerError ErrorDelegate = OnError;
		AsyncTask(ENamedThreads::GameThread, [ErrorDelegate]() {
			ErrorDelegate.ExecuteIfBound();
		});
		return 1;
	}

	bInitialized = true;
	{
		const FOnKwsWorkerReady ReadyDelegate = OnReady;
		AsyncTask(ENamedThreads::GameThread, [ReadyDelegate]() {
			ReadyDelegate.ExecuteIfBound();
		});
	}

	while (!bStopRequested)
	{
		TArray<float> Chunk;
		{
			FScopeLock Lock(&QueueLock);
			AudioQueue.Dequeue(Chunk);
		}

		if (Chunk.Num() > 0)
		{
			ProcessAudio(Chunk);
			CheckForKeywords();
		}
		else
		{
			FPlatformProcess::Sleep(0.005f);
		}
	}

	bRunning = false;
	bInitialized = false;
	UE_LOG(LogKwsWorker, Log, TEXT("KWS worker thread stopped"));
	return 0;
}

bool FKwsWorker::Start()
{
	if (bRunning) return true;

	bStopRequested = false;
	bFailed = false;
	bInitialized = false;
	bNativeThreadAborted = false;

	Thread = FRunnableThread::Create(this, TEXT("KwsWorker"), 2 * 1024 * 1024, TPri_Normal);
	if (!Thread)
	{
		UE_LOG(LogKwsWorker, Error, TEXT("Failed to create KWS worker thread"));
		return false;
	}
	return true;
}

void FKwsWorker::Shutdown()
{
	bStopRequested = true;
	if (Thread)
	{
		const double StopDeadline = FPlatformTime::Seconds() + KwsShutdownWaitSeconds;
		while (bRunning && FPlatformTime::Seconds() < StopDeadline)
		{
			FPlatformProcess::Sleep(0.01f);
		}

		if (bRunning)
		{
			UE_LOG(LogKwsWorker, Error,
				TEXT("KWS worker did not stop within %.1f seconds; native sherpa initialization is likely blocked"),
				KwsShutdownWaitSeconds);

#if PLATFORM_WINDOWS
			const uint32 ThreadId = Thread->GetThreadID();
			if (ForceTerminateThreadById(ThreadId))
			{
				bNativeThreadAborted = true;
				bRunning = false;
				bInitialized = false;
				UE_LOG(LogKwsWorker, Error, TEXT("Force-terminated blocked KWS worker thread %u"), ThreadId);
			}
			else
			{
				UE_LOG(LogKwsWorker, Error, TEXT("Failed to force-terminate blocked KWS worker thread %u"), ThreadId);
			}
#endif
		}

		Thread->Kill(false);
		delete Thread;
		Thread = nullptr;
	}
	bRunning = false;
	bInitialized = false;
	DestroySpotter();
}

void FKwsWorker::ShutdownAllWorkers()
{
	TArray<FKwsWorker*> Workers;
	{
		FScopeLock Lock(&GWorkerRegistryLock);
		Workers = GWorkerRegistry;
	}

	for (FKwsWorker* Worker : Workers)
	{
		if (Worker)
		{
			Worker->Shutdown();
		}
	}
}

void FKwsWorker::PushAudio(const TArray<float>& Samples)
{
	if (Samples.Num() <= 0)
	{
		return;
	}

	FScopeLock Lock(&QueueLock);
	AudioQueue.Enqueue(Samples);
}

void FKwsWorker::SetKeywords(const FString& Keywords)
{
	auto& API = SherpaKws_GetAPI();
	if (!API.IsLoaded() || !API.CreateKeywordStreamWithKeywords) return;

	if (Stream)
	{
		API.Reset(Spotter, Stream);
		Stream = nullptr;
	}

	FTCHARToUTF8 KeywordsUTF8(*Keywords);
	Stream = API.CreateKeywordStreamWithKeywords(Spotter, KeywordsUTF8.Get());
	UE_LOG(LogKwsWorker, Log, TEXT("Keywords updated: %s"), *Keywords);
}

bool FKwsWorker::CreateSpotter()
{
	auto& API = SherpaKws_GetAPI();
	if (!API.IsLoaded())
	{
		UE_LOG(LogKwsWorker, Error, TEXT("sherpa-onnx API not loaded"));
		return false;
	}

	// 验证模型文件存在
	auto CheckFile = [](const FString& Path, const TCHAR* Label) -> bool {
		if (!FPaths::FileExists(Path))
		{
			UE_LOG(LogKwsWorker, Error, TEXT("%s not found: %s"), Label, *Path);
			return false;
		}
		UE_LOG(LogKwsWorker, Log, TEXT("%s OK: %s"), Label, *Path);
		return true;
	};

	if (!CheckFile(ModelConfig.EncoderPath, TEXT("Encoder")) ||
		!CheckFile(ModelConfig.DecoderPath, TEXT("Decoder")) ||
		!CheckFile(ModelConfig.JoinerPath,  TEXT("Joiner"))  ||
		!CheckFile(ModelConfig.TokensPath,  TEXT("Tokens")))
	{
		return false;
	}

	const FString EncoderPath = FPaths::ConvertRelativePathToFull(ModelConfig.EncoderPath);
	const FString DecoderPath = FPaths::ConvertRelativePathToFull(ModelConfig.DecoderPath);
	const FString JoinerPath = FPaths::ConvertRelativePathToFull(ModelConfig.JoinerPath);
	const FString TokensPath = FPaths::ConvertRelativePathToFull(ModelConfig.TokensPath);

	FTCHARToUTF8 Encoder(*EncoderPath);
	FTCHARToUTF8 Decoder(*DecoderPath);
	FTCHARToUTF8 Joiner(*JoinerPath);
	FTCHARToUTF8 Tokens(*TokensPath);
	FTCHARToUTF8 Provider(*ModelConfig.Provider);

	TUniquePtr<FTCHARToUTF8> pKeywordsFile;
	if (!ModelConfig.KeywordsFile.IsEmpty())
	{
		const FString KeywordsFilePath = FPaths::ConvertRelativePathToFull(ModelConfig.KeywordsFile);
		pKeywordsFile = MakeUnique<FTCHARToUTF8>(*KeywordsFilePath);
	}

	TUniquePtr<FTCHARToUTF8> pKeywordsBuf;
	if (!ModelConfig.KeywordsString.IsEmpty())
	{
		pKeywordsBuf = MakeUnique<FTCHARToUTF8>(*ModelConfig.KeywordsString);
	}

	SherpaOnnxKeywordSpotterConfig Cfg{};
	Cfg.feat_config.sample_rate = 16000;
	Cfg.feat_config.feature_dim   = 80;

	Cfg.model_config.transducer.encoder = Encoder.Get();
	Cfg.model_config.transducer.decoder = Decoder.Get();
	Cfg.model_config.transducer.joiner  = Joiner.Get();
	Cfg.model_config.tokens    = Tokens.Get();
	Cfg.model_config.num_threads = 1;  // ORT线程池与UE5线程池冲突，单线程规避
	Cfg.model_config.provider  = Provider.Get();
	Cfg.model_config.model_type = "";

	Cfg.max_active_paths = ModelConfig.MaxActivePaths > 0 ? ModelConfig.MaxActivePaths : 4;
	Cfg.num_trailing_blanks = ModelConfig.NumTrailingBlanks > 0 ? ModelConfig.NumTrailingBlanks : 1;
	Cfg.keywords_score = ModelConfig.KeywordsScore > 0.0f ? ModelConfig.KeywordsScore : 1.0f;
	Cfg.keywords_threshold = ModelConfig.KeywordsThreshold > 0.0f ? ModelConfig.KeywordsThreshold : 0.25f;

	UE_LOG(LogKwsWorker, Log,
		TEXT("KWS decode config: max_active_paths=%d, num_trailing_blanks=%d, keywords_score=%.2f, keywords_threshold=%.2f"),
		Cfg.max_active_paths, Cfg.num_trailing_blanks, Cfg.keywords_score, Cfg.keywords_threshold);

	if (pKeywordsBuf.IsValid())
	{
		FString ValidationError;
		if (!ValidateKeywordsAgainstTokens(TokensPath, ModelConfig.KeywordsString, ValidationError))
		{
			UE_LOG(LogKwsWorker, Error, TEXT("%s"), *ValidationError);
			return false;
		}
		Cfg.keywords_buf      = pKeywordsBuf->Get();
		Cfg.keywords_buf_size = pKeywordsBuf->Length();
		UE_LOG(LogKwsWorker, Log, TEXT("Using keywords_buf (%d bytes)"), Cfg.keywords_buf_size);
	}
	else if (pKeywordsFile.IsValid())
	{
		FString KeywordsFromFile;
		FString ValidationError;
		if (!FFileHelper::LoadFileToString(KeywordsFromFile, *ModelConfig.KeywordsFile) ||
			!ValidateKeywordsAgainstTokens(TokensPath, KeywordsFromFile, ValidationError))
		{
			UE_LOG(LogKwsWorker, Error, TEXT("%s"), ValidationError.IsEmpty() ? TEXT("Failed to read keywords file") : *ValidationError);
			return false;
		}
		Cfg.keywords_file = pKeywordsFile->Get();
		UE_LOG(LogKwsWorker, Log, TEXT("Using keywords_file: %s"), *ModelConfig.KeywordsFile);
	}

	UE_LOG(LogKwsWorker, Log, TEXT("Creating KeywordSpotter..."));
	UE_LOG(LogKwsWorker, Log, TEXT("Native encoder path: %s"), *EncoderPath);
	UE_LOG(LogKwsWorker, Log, TEXT("Native decoder path: %s"), *DecoderPath);
	UE_LOG(LogKwsWorker, Log, TEXT("Native joiner path: %s"), *JoinerPath);
	UE_LOG(LogKwsWorker, Log, TEXT("Native tokens path: %s"), *TokensPath);
	Spotter = API.CreateKeywordSpotter(&Cfg);
	if (!Spotter)
	{
		UE_LOG(LogKwsWorker, Error, TEXT("Failed to create KeywordSpotter (returned NULL)"));
		return false;
	}

	UE_LOG(LogKwsWorker, Log, TEXT("Creating KeywordStream..."));
	Stream = API.CreateKeywordStream(Spotter);
	if (!Stream)
	{
		UE_LOG(LogKwsWorker, Error, TEXT("Failed to create KeywordStream (returned NULL)"));
		API.DestroyKeywordSpotter(Spotter);
		Spotter = nullptr;
		return false;
	}

	UE_LOG(LogKwsWorker, Log, TEXT("KeywordSpotter created successfully"));
	return true;
}

void FKwsWorker::DestroySpotter()
{
	if (bNativeThreadAborted)
	{
		Spotter = nullptr;
		Stream = nullptr;
		return;
	}

	auto& API = SherpaKws_GetAPI();
	if (API.IsLoaded())
	{
		if (Spotter) { API.DestroyKeywordSpotter(Spotter); Spotter = nullptr; }
	}
	Stream = nullptr;
}

void FKwsWorker::ProcessAudio(const TArray<float>& Chunk)
{
	auto& API = SherpaKws_GetAPI();
	if (!API.IsLoaded() || !Stream) return;

	API.AcceptWaveform(Stream, 16000, Chunk.GetData(), Chunk.Num());

	ProcessedAudioSamples += Chunk.Num();
	ProcessedAudioChunks += 1;
	if (ProcessedAudioChunks == 1 || ProcessedAudioSamples >= NextAudioLogSample)
	{
		UE_LOG(LogKwsWorker, Log, TEXT("KWS worker accepted audio: chunks=%lld, samples=%lld, last_chunk=%d"),
			ProcessedAudioChunks, ProcessedAudioSamples, Chunk.Num());
		NextAudioLogSample = ProcessedAudioSamples + 16000;
	}
}

void FKwsWorker::CheckForKeywords()
{
	auto& API = SherpaKws_GetAPI();
	if (!API.IsLoaded() || !Spotter || !Stream) return;

	while (API.IsReady(Spotter, Stream))
	{
		API.Decode(Spotter, Stream);
		DecodeCount += 1;
		const char* Json = API.GetResultJson(Spotter, Stream);
		if (Json && *Json)
		{
			FString ResultJson(UTF8_TO_TCHAR(Json));
			if (API.FreeResultJson)
			{
				API.FreeResultJson(Json);
			}

			FString Keyword;
			TryExtractKeywordFromResultJson(ResultJson, Keyword);

			if (!Keyword.TrimStartAndEnd().IsEmpty())
			{
				UE_LOG(LogKwsWorker, Log, TEXT("KWS native keyword result: %s"), *ResultJson);
				const FOnKwsWorkerResult ResultDelegate = OnResult;
				AsyncTask(ENamedThreads::GameThread, [ResultDelegate, ResultJson]() {
					ResultDelegate.ExecuteIfBound(ResultJson);
				});
				API.Reset(Spotter, Stream);
			}
			else
			{
				EmptyResultCount += 1;
			}
		}

		if (DecodeCount >= NextDecodeLogCount)
		{
			UE_LOG(LogKwsWorker, Log, TEXT("KWS decode progress: decodes=%lld, empty_results=%lld"),
				DecodeCount, EmptyResultCount);
			NextDecodeLogCount = DecodeCount + 50;
		}
	}
}

#endif // WITH_SHERPA_ONNX
