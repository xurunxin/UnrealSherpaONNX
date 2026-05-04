#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Asr/SherpaAsrTypes.h"
#include "SherpaAsrComponent.generated.h"

class FSherpaAsrWorker;
class USherpaAudioCapture;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSherpaAsrPartialResult,
	const FSherpaAsrResult&,
	Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSherpaAsrFinalResult,
	const FSherpaAsrResult&,
	Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSherpaAsrError,
	const FString&,
	ErrorMessage);

UCLASS(ClassGroup=(SherpaONNX), meta=(BlueprintSpawnableComponent))
class SHERPAONNX_API USherpaAsrComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USherpaAsrComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR")
	FSherpaAsrConfig Config;

	UFUNCTION(BlueprintCallable, Category = "Sherpa ASR")
	bool StartASR();

	/** 使用预设模型启动识别，可选择开启热词偏置 */
	UFUNCTION(BlueprintCallable, Category = "Sherpa ASR", meta = (DisplayName = "Start ASR (Preset)", AutoCreateRefTerm = "HotwordsString"))
	bool StartASRWithPreset(ESherpaAsrPreset Preset, bool bEnableHotwords = false, const FString& HotwordsString = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Sherpa ASR")
	void StopASR();

	UFUNCTION(BlueprintCallable, Category = "Sherpa ASR")
	bool IsRunning() const;

	UFUNCTION(BlueprintCallable, Category = "Sherpa ASR")
	void SetHotwords(const FString& Hotwords);

	UPROPERTY(BlueprintAssignable, Category = "Sherpa ASR")
	FOnSherpaAsrPartialResult OnPartialResult;

	UPROPERTY(BlueprintAssignable, Category = "Sherpa ASR")
	FOnSherpaAsrFinalResult OnFinalResult;

	UPROPERTY(BlueprintAssignable, Category = "Sherpa ASR")
	FOnSherpaAsrError OnAsrError;

private:
	bool StartASRInternal(const FSherpaAsrConfig& ResolvedConfig);

	FSherpaAsrWorker* Worker = nullptr;

	UPROPERTY()
	TObjectPtr<USherpaAudioCapture> AudioCapture;
};
