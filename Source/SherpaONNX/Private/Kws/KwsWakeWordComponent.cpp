#include "Kws/KwsWakeWordComponent.h"

#if WITH_SHERPA_ONNX
#include "KwsWorker.h"
#include "SherpaAudioCapture.h"
#endif

#include "JsonObjectConverter.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogKwsComponent, Log, All);

UKwsWakeWordComponent::UKwsWakeWordComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UKwsWakeWordComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UKwsWakeWordComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopKWS();
	Super::EndPlay(Reason);
}

bool UKwsWakeWordComponent::StartKWS(const FSherpaKwsModelConfig& Config)
{
#if !WITH_SHERPA_ONNX
	UE_LOG(LogKwsComponent, Error, TEXT("WITH_SHERPA_ONNX=0 — plugin built without native support"));
	return false;
#else
	StopKWS();

	CurrentConfig = Config;

	if (Config.EncoderPath.IsEmpty() || Config.DecoderPath.IsEmpty() ||
		Config.JoinerPath.IsEmpty()  || Config.TokensPath.IsEmpty())
	{
		UE_LOG(LogKwsComponent, Error, TEXT("Model config paths are incomplete"));
		OnKwsError.Broadcast(TEXT("Model config paths are incomplete"));
		return false;
	}

	Worker = new FKwsWorker(Config);
	Worker->OnResult.BindUObject(this, &UKwsWakeWordComponent::HandleKeywordResult);
	Worker->OnError.BindUObject(this, &UKwsWakeWordComponent::HandleKwsError);
	Worker->OnReady.BindUObject(this, &UKwsWakeWordComponent::HandleKwsReady);

	if (!Worker->Start())
	{
		delete Worker;
		Worker = nullptr;
		OnKwsError.Broadcast(TEXT("Failed to start KWS worker thread"));
		return false;
	}

	// 统一音频采集（16kHz mono float32）
	AudioCapture = NewObject<USherpaAudioCapture>(this);
	if (!AudioCapture)
	{
		delete Worker;
		Worker = nullptr;
		OnKwsError.Broadcast(TEXT("Failed to create audio capture"));
		return false;
	}

	// KWS 作为消费者注册到统一采集通道
	FKwsWorker* KwsWorker = Worker;
	AudioCapture->AddConsumer([KwsWorker](const TArray<float>& Samples) {
		KwsWorker->PushAudio(Samples);
	});

	AudioCapture->RegisterComponent();
	AudioCapture->bAutoActivate = false;
	AudioCapture->Activate(true);
	AudioCapture->StartCapturing();

	UE_LOG(LogKwsComponent, Log, TEXT("KWS started (mic auto-capture)"));
	OnKwsReady.Broadcast();
	return true;
#endif
}

void UKwsWakeWordComponent::StopKWS()
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
		Worker->Shutdown();
		delete Worker;
		Worker = nullptr;
	}

	UE_LOG(LogKwsComponent, Log, TEXT("KWS stopped"));
}

bool UKwsWakeWordComponent::SetKeywords(const FString& Keywords)
{
#if WITH_SHERPA_ONNX
	if (Worker != nullptr && Worker->IsRunning())
	{
		Worker->SetKeywords(Keywords);
		return true;
	}
#endif
	return false;
}

bool UKwsWakeWordComponent::IsRunning() const
{
	return Worker != nullptr && Worker->IsRunning();
}

void UKwsWakeWordComponent::HandleKeywordResult(const FString& Json)
{
	FSherpaKwsKeywordResult Result;
	Result.Json = Json;

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
	{
		JsonObj->TryGetStringField(TEXT("keyword"), Result.Keyword);

		const TArray<TSharedPtr<FJsonValue>>* TimestampsArr;
		if (JsonObj->TryGetArrayField(TEXT("timestamps"), TimestampsArr))
		{
			Result.Timestamps.Reserve(TimestampsArr->Num());
			for (const auto& Val : *TimestampsArr)
			{
				Result.Timestamps.Add((float)Val->AsNumber());
			}
		}

		double StartTime = 0.0;
		JsonObj->TryGetNumberField(TEXT("start_time"), StartTime);
		Result.StartTime = (float)StartTime;
	}

	if (Result.Keyword.TrimStartAndEnd().IsEmpty())
	{
		UE_LOG(LogKwsComponent, Verbose, TEXT("Ignoring empty KWS result: %s"), *Json);
		return;
	}

	UE_LOG(LogKwsComponent, Log, TEXT("Keyword detected: %s"), *Result.Keyword);
	OnKeywordDetected.Broadcast(Result);
}

void UKwsWakeWordComponent::HandleKwsError()
{
	StopKWS();
	OnKwsError.Broadcast(TEXT("KWS worker encountered an error"));
}

void UKwsWakeWordComponent::HandleKwsReady()
{
	// Worker 就绪时无需额外操作，音频采集已在 StartKWS 中启动
}
