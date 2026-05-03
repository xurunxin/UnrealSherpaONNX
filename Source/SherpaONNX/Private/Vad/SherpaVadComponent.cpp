#include "Vad/SherpaVadComponent.h"

#if WITH_SHERPA_ONNX
#include "SherpaVadWorker.h"
#include "SherpaAudioCapture.h"
#endif

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSherpaVadComponent, Log, All);

USherpaVadComponent::USherpaVadComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USherpaVadComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Config.bAutoStart)
	{
		StartVAD();
	}
}

void USherpaVadComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopVAD();
	Super::EndPlay(EndPlayReason);
}

bool USherpaVadComponent::StartVAD()
{
#if !WITH_SHERPA_ONNX
	return false;
#else
	StopVAD();

	// 自动解析默认模型路径（对齐 KWS 预设模式）
	if (Config.ModelPath.IsEmpty())
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SherpaONNX"));
		if (Plugin.IsValid())
		{
			FString DefaultPath = Plugin->GetContentDir() / TEXT("Models/Vad/Silero/silero_vad.int8.onnx");
			if (FPaths::FileExists(DefaultPath))
			{
				Config.ModelPath = DefaultPath;
			}
		}
	}

	if (Config.ModelPath.IsEmpty())
	{
		OnVadError.Broadcast(TEXT("VAD model path is empty."));
		return false;
	}

	if (!FPaths::FileExists(Config.ModelPath))
	{
		OnVadError.Broadcast(FString::Printf(TEXT("VAD model not found: %s"), *Config.ModelPath));
		return false;
	}

	Worker = new FSherpaVadWorker(Config);
	Worker->OnSpeechStart  = [this]() { OnSpeechStart.Broadcast(); };
	Worker->OnSpeechEnd    = [this]() { OnSpeechEnd.Broadcast(); };
	Worker->OnSegmentReady = [this](const FSherpaVadSegment& Seg) { OnSpeechSegmentReady.Broadcast(Seg); };
	Worker->OnError        = [this](const FString& Err) { OnVadError.Broadcast(Err); };

	if (!Worker->Start())
	{
		delete Worker;
		Worker = nullptr;
		OnVadError.Broadcast(TEXT("Failed to start VAD worker."));
		return false;
	}

	// 统一音频采集（16kHz mono float32）
	AudioCapture = NewObject<USherpaAudioCapture>(this);
	if (!AudioCapture)
	{
		delete Worker;
		Worker = nullptr;
		OnVadError.Broadcast(TEXT("Failed to create audio capture."));
		return false;
	}

	FSherpaVadWorker* VadWorker = Worker;
	AudioCapture->AddConsumer([VadWorker](const TArray<float>& Samples) {
		VadWorker->PushAudio(Samples);
	});

	AudioCapture->RegisterComponent();
	AudioCapture->bAutoActivate = false;
	AudioCapture->Activate(true);
	AudioCapture->StartCapturing();

	UE_LOG(LogSherpaVadComponent, Log, TEXT("VAD started (mic auto-capture)"));
	return true;
#endif
}

void USherpaVadComponent::StopVAD()
{
	if (AudioCapture)
	{
		AudioCapture->StopCapturing();
		AudioCapture->ClearConsumers();
		AudioCapture->Deactivate();
		AudioCapture->DestroyComponent();
		AudioCapture = nullptr;
	}

	if (Worker)
	{
		Worker->RequestStopAndWait();
		delete Worker;
		Worker = nullptr;
	}
}

void USherpaVadComponent::ResetVAD()
{
	if (Worker) { Worker->Reset(); }
}

bool USherpaVadComponent::IsRunning() const
{
	return Worker != nullptr && Worker->IsRunning();
}

bool USherpaVadComponent::IsSpeechDetected() const
{
	return Worker != nullptr && Worker->IsSpeechDetected();
}

void USherpaVadComponent::PushAudioFloat(const TArray<float>& Samples, int32 SampleRate, int32 NumChannels)
{
	if (!Worker || Samples.Num() == 0) return;
	if (SampleRate <= 0 || NumChannels <= 0) return;

	TArray<float> Mono;
	if (NumChannels <= 1)
	{
		Mono = Samples;
	}
	else
	{
		const int32 NumFrames = Samples.Num() / NumChannels;
		Mono.SetNumUninitialized(NumFrames);
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			float Sum = 0.0f;
			for (int32 Ch = 0; Ch < NumChannels; ++Ch)
				Sum += Samples[Frame * NumChannels + Ch];
			Mono[Frame] = Sum / static_cast<float>(NumChannels);
		}
	}

	if (SampleRate == Config.SampleRate)
	{
		Worker->PushAudio(Mono);
	}
	else
	{
		const double Ratio = static_cast<double>(SampleRate) / Config.SampleRate;
		const int32 OutNum = FMath::FloorToInt(Mono.Num() / Ratio);
		if (OutNum <= 0) return;

		TArray<float> Resampled;
		Resampled.SetNumUninitialized(OutNum);
		for (int32 i = 0; i < OutNum; ++i)
		{
			const double SrcIdx = i * Ratio;
			const int32 Idx0 = FMath::Clamp(static_cast<int32>(SrcIdx), 0, Mono.Num() - 1);
			const int32 Idx1 = FMath::Clamp(Idx0 + 1, 0, Mono.Num() - 1);
			Resampled[i] = FMath::Lerp(Mono[Idx0], Mono[Idx1], static_cast<float>(SrcIdx - Idx0));
		}
		Worker->PushAudio(Resampled);
	}
}
