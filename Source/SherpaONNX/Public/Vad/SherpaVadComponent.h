#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vad/SherpaVadTypes.h"
#include "SherpaVadComponent.generated.h"

class FSherpaVadWorker;
class USherpaAudioCapture;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSherpaVadSpeechStart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSherpaVadSpeechEnd);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSherpaVadSegmentReady,
	const FSherpaVadSegment&,
	Segment);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSherpaVadError,
	const FString&,
	ErrorMessage);

UCLASS(ClassGroup=(SherpaONNX), meta=(BlueprintSpawnableComponent))
class SHERPAONNX_API USherpaVadComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USherpaVadComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	FSherpaVadConfig Config;

	UFUNCTION(BlueprintCallable, Category = "Sherpa VAD")
	bool StartVAD();

	UFUNCTION(BlueprintCallable, Category = "Sherpa VAD")
	void StopVAD();

	UFUNCTION(BlueprintCallable, Category = "Sherpa VAD")
	void ResetVAD();

	UFUNCTION(BlueprintCallable, Category = "Sherpa VAD")
	bool IsRunning() const;

	UFUNCTION(BlueprintCallable, Category = "Sherpa VAD")
	bool IsSpeechDetected() const;

	/** 外部音频源输入（自动转 mono + 重采样 → 16kHz） */
	UFUNCTION(BlueprintCallable, Category = "Sherpa VAD")
	void PushAudioFloat(const TArray<float>& Samples, int32 SampleRate, int32 NumChannels);

	UPROPERTY(BlueprintAssignable, Category = "Sherpa VAD")
	FOnSherpaVadSpeechStart OnSpeechStart;

	UPROPERTY(BlueprintAssignable, Category = "Sherpa VAD")
	FOnSherpaVadSpeechEnd OnSpeechEnd;

	UPROPERTY(BlueprintAssignable, Category = "Sherpa VAD")
	FOnSherpaVadSegmentReady OnSpeechSegmentReady;

	UPROPERTY(BlueprintAssignable, Category = "Sherpa VAD")
	FOnSherpaVadError OnVadError;

private:
	FSherpaVadWorker* Worker = nullptr;

	UPROPERTY()
	TObjectPtr<USherpaAudioCapture> AudioCapture;
};
