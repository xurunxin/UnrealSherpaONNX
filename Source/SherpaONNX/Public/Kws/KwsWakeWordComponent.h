#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KwsTypes.h"
#include "KwsWakeWordComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnKeywordDetected,
	const FSherpaKwsKeywordResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnKwsReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKwsError, const FString&, ErrorMessage);

class FKwsWorker;
class USherpaAudioCapture;

UCLASS(ClassGroup=(SherpaONNX), meta=(BlueprintSpawnableComponent))
class SHERPAONNX_API UKwsWakeWordComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKwsWakeWordComponent();

	UFUNCTION(BlueprintCallable, Category = "Sherpa KWS")
	bool StartKWS(const FSherpaKwsModelConfig& Config);

	UFUNCTION(BlueprintCallable, Category = "Sherpa KWS")
	void StopKWS();

	UFUNCTION(BlueprintCallable, Category = "Sherpa KWS")
	bool SetKeywords(const FString& Keywords);

	UFUNCTION(BlueprintPure, Category = "Sherpa KWS")
	bool IsRunning() const;

	// ---- 旧接口（兼容，标记为废弃） ----
	UFUNCTION(BlueprintCallable, Category = "Sherpa KWS", meta = (DeprecatedFunction, DeprecationMessage = "Use StartKWS instead"))
	bool StartListening(const FSherpaKwsModelConfig& Config) { return StartKWS(Config); }

	UFUNCTION(BlueprintCallable, Category = "Sherpa KWS", meta = (DeprecatedFunction, DeprecationMessage = "Use StopKWS instead"))
	void StopListening() { StopKWS(); }

	UFUNCTION(BlueprintPure, Category = "Sherpa KWS", meta = (DeprecatedFunction, DeprecationMessage = "Use IsRunning instead"))
	bool IsListening() const { return IsRunning(); }

	UPROPERTY(BlueprintAssignable, Category = "Sherpa KWS|Events")
	FOnKeywordDetected OnKeywordDetected;

	UPROPERTY(BlueprintAssignable, Category = "Sherpa KWS|Events")
	FOnKwsReady OnKwsReady;

	UPROPERTY(BlueprintAssignable, Category = "Sherpa KWS|Events")
	FOnKwsError OnKwsError;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	FSherpaKwsModelConfig CurrentConfig;

	FKwsWorker* Worker = nullptr;

	UPROPERTY()
	TObjectPtr<USherpaAudioCapture> AudioCapture;

	void HandleKeywordResult(const FString& Json);
	void HandleKwsError();
	void HandleKwsReady();
};
