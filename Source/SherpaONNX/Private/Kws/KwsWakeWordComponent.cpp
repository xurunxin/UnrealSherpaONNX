#include "Kws/KwsWakeWordComponent.h"

#if WITH_SHERPA_ONNX
#include "KwsWorker.h"
#include "Kws/KwsAudioCapture.h"
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
	StopListening();
	Super::EndPlay(Reason);
}

bool UKwsWakeWordComponent::StartListening(const FSherpaKwsModelConfig& Config)
{
#if !WITH_SHERPA_ONNX
	UE_LOG(LogKwsComponent, Error, TEXT("WITH_SHERPA_ONNX=0 — plugin built without native support"));
	return false;
#else
	StopListening();

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

	AudioCapture = NewObject<UKwsAudioCapture>(this);
	if (!AudioCapture)
	{
		delete Worker;
		Worker = nullptr;
		OnKwsError.Broadcast(TEXT("Failed to create audio capture"));
		return false;
	}

	AudioCapture->SetWorker(Worker);
	AudioCapture->RegisterComponent();
	AudioCapture->bAutoActivate = false;

	UE_LOG(LogKwsComponent, Log, TEXT("KWS listening starting"));
	return true;
#endif
}

void UKwsWakeWordComponent::StopListening()
{
	if (AudioCapture)
	{
		AudioCapture->StopCapturing();
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

	UE_LOG(LogKwsComponent, Log, TEXT("KWS listening stopped"));
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

bool UKwsWakeWordComponent::IsListening() const
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
	StopListening();
	OnKwsError.Broadcast(TEXT("KWS worker encountered an error"));
}

void UKwsWakeWordComponent::HandleKwsReady()
{
	if (AudioCapture)
	{
		AudioCapture->Activate(true);
		AudioCapture->StartCapturing();
	}

	UE_LOG(LogKwsComponent, Log, TEXT("KWS listening started"));
	OnKwsReady.Broadcast();
}
