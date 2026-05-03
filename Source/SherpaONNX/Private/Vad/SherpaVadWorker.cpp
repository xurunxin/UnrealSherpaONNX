#include "SherpaVadWorker.h"

#if WITH_SHERPA_ONNX
#include "SherpaKws/SherpaKwsNative.h"
#include "sherpa-onnx/c-api/c-api.h"
#include "Misc/Paths.h"
#include "Async/Async.h"

DEFINE_LOG_CATEGORY_STATIC(LogSherpaVadWorker, Log, All);

FSherpaVadWorker::FSherpaVadWorker(const FSherpaVadConfig& InConfig)
	: Config(InConfig)
{
}

FSherpaVadWorker::~FSherpaVadWorker()
{
	RequestStopAndWait();
}

bool FSherpaVadWorker::InitVad()
{
	if (Config.ModelPath.IsEmpty())
	{
		EmitError(TEXT("VAD model path is empty."));
		return false;
	}

	if (!FPaths::FileExists(Config.ModelPath))
	{
		EmitError(FString::Printf(TEXT("VAD model not found: %s"), *Config.ModelPath));
		return false;
	}

	SherpaOnnxVadModelConfig VadConfig;
	FMemory::Memzero(&VadConfig, sizeof(VadConfig));

	FTCHARToUTF8 FullPath(*FPaths::ConvertRelativePathToFull(Config.ModelPath));
	ModelPathUtf8 = FullPath.Get();
	FTCHARToUTF8 Prov(*Config.Provider);
	ProviderUtf8  = Prov.Get();

	if (Config.ModelType == ESherpaVadModelType::Silero)
	{
		VadConfig.silero_vad.model              = ModelPathUtf8.c_str();
		VadConfig.silero_vad.threshold          = Config.Threshold;
		VadConfig.silero_vad.min_silence_duration = Config.MinSilenceDuration;
		VadConfig.silero_vad.min_speech_duration  = Config.MinSpeechDuration;
		VadConfig.silero_vad.max_speech_duration  = Config.MaxSpeechDuration;
		VadConfig.silero_vad.window_size          = Config.WindowSize;
	}
	else
	{
		EmitError(TEXT("TEN VAD is not implemented in this version."));
		return false;
	}

	VadConfig.sample_rate = Config.SampleRate;
	VadConfig.num_threads = Config.NumThreads;
	VadConfig.provider    = ProviderUtf8.c_str();
	VadConfig.debug       = Config.bDebug ? 1 : 0;

	auto& API = SherpaKws_GetAPI();
	if (!API.IsVadLoaded())
	{
		EmitError(TEXT("VAD API not loaded (sherpa-onnx DLL missing VAD symbols?)"));
		return false;
	}

	Vad = API.CreateVoiceActivityDetector(&VadConfig, Config.BufferSizeInSeconds);
	if (!Vad)
	{
		EmitError(TEXT("Failed to create SherpaOnnxVoiceActivityDetector."));
		return false;
	}

	return true;
}

void FSherpaVadWorker::DestroyVad()
{
	if (Vad)
	{
		auto& API = SherpaKws_GetAPI();
		if (API.DestroyVoiceActivityDetector)
		{
			API.DestroyVoiceActivityDetector(Vad);
		}
		Vad = nullptr;
	}
}

uint32 FSherpaVadWorker::Run()
{
	bRunning = true;

	if (!InitVad())
	{
		bRunning = false;
		return 1;
	}

	while (!bStopRequested)
	{
		TArray<float> Block;
		if (AudioQueue.Dequeue(Block))
		{
			ProcessAudioBlock(Block);
		}
		else
		{
			FPlatformProcess::Sleep(0.005f);
		}
	}

	if (Vad)
	{
		auto& API = SherpaKws_GetAPI();
		if (API.VadFlush)
		{
			API.VadFlush(Vad);
		}
		if (Config.bEmitSpeechSegment)
		{
			PollSegments();
		}
	}

	DestroyVad();
	bRunning = false;
	return 0;
}

void FSherpaVadWorker::ProcessAudioBlock(const TArray<float>& Samples)
{
	if (!Vad || Samples.Num() <= 0) return;

	auto& API = SherpaKws_GetAPI();
	if (!API.VadAcceptWaveform) return;

	API.VadAcceptWaveform(Vad, Samples.GetData(), Samples.Num());

	const bool bDetectedNow = API.VadDetected && API.VadDetected(Vad) != 0;
	SetSpeechState(bDetectedNow);

	if (Config.bEmitSpeechSegment)
	{
		PollSegments();
	}
}

void FSherpaVadWorker::SetSpeechState(bool bNewDetected)
{
	const bool bOldDetected = bSpeechDetected;
	if (bOldDetected == bNewDetected) return;

	bSpeechDetected = bNewDetected;

	if (bNewDetected)
	{
		if (OnSpeechStart)
		{
			TFunction<void()> Callback = OnSpeechStart;
			AsyncTask(ENamedThreads::GameThread, [Callback]() { Callback(); });
		}
	}
	else
	{
		if (OnSpeechEnd)
		{
			TFunction<void()> Callback = OnSpeechEnd;
			AsyncTask(ENamedThreads::GameThread, [Callback]() { Callback(); });
		}
	}
}

void FSherpaVadWorker::PollSegments()
{
	if (!Vad) return;

	auto& API = SherpaKws_GetAPI();
	if (!API.VadEmpty || !API.VadFront || !API.VadPop) return;

	while (!API.VadEmpty(Vad))
	{
		const SherpaOnnxSpeechSegment* Segment = API.VadFront(Vad);
		if (Segment)
		{
			FSherpaVadSegment OutSegment;
			OutSegment.StartSample = Segment->start;
			OutSegment.NumSamples   = Segment->n;
			OutSegment.SampleRate   = Config.SampleRate;

			if (Segment->samples && Segment->n > 0)
			{
				OutSegment.Samples.SetNumUninitialized(Segment->n);
				FMemory::Memcpy(OutSegment.Samples.GetData(), Segment->samples,
					sizeof(float) * Segment->n);
			}

			if (API.DestroySpeechSegment)
			{
				API.DestroySpeechSegment(Segment);
			}

			if (OnSegmentReady)
			{
				TFunction<void(const FSherpaVadSegment&)> Callback = OnSegmentReady;
				AsyncTask(ENamedThreads::GameThread,
					[Callback, OutSegment]() { Callback(OutSegment); });
			}
		}
		API.VadPop(Vad);
	}
}

bool FSherpaVadWorker::Start()
{
	if (Thread) return true;
	bStopRequested = false;
	Thread = FRunnableThread::Create(this, TEXT("SherpaVadWorker"), 0, TPri_Normal);
	return Thread != nullptr;
}

void FSherpaVadWorker::RequestStopAndWait()
{
	bStopRequested = true;
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FSherpaVadWorker::PushAudio(const TArray<float>& InSamples)
{
	if (InSamples.Num() > 0)
	{
		AudioQueue.Enqueue(InSamples);
	}
}

void FSherpaVadWorker::Reset()
{
	if (Vad)
	{
		auto& API = SherpaKws_GetAPI();
		if (API.VadReset)
		{
			API.VadReset(Vad);
		}
	}
}

bool FSherpaVadWorker::IsRunning() const
{
	return bRunning;
}

bool FSherpaVadWorker::IsSpeechDetected() const
{
	return bSpeechDetected;
}

void FSherpaVadWorker::EmitError(const FString& Error)
{
	UE_LOG(LogSherpaVadWorker, Error, TEXT("%s"), *Error);
	if (OnError)
	{
		TFunction<void(const FString&)> Callback = OnError;
		AsyncTask(ENamedThreads::GameThread, [Callback, Error]() { Callback(Error); });
	}
}

#endif // WITH_SHERPA_ONNX
