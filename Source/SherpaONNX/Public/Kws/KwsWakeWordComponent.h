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
class UKwsAudioCapture;

UCLASS(ClassGroup=(SherpaONNX), meta=(BlueprintSpawnableComponent))
class SHERPAONNX_API UKwsWakeWordComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKwsWakeWordComponent();

	UFUNCTION(BlueprintCallable, Category = "Sherpa KWS")
	bool StartListening(const FSherpaKwsModelConfig& Config);

	UFUNCTION(BlueprintCallable, Category = "Sherpa KWS")
	void StopListening();

	UFUNCTION(BlueprintCallable, Category = "Sherpa KWS")
	bool SetKeywords(const FString& Keywords);

	UFUNCTION(BlueprintPure, Category = "Sherpa KWS")
	bool IsListening() const;

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
	TObjectPtr<UKwsAudioCapture> AudioCapture;

	void HandleKeywordResult(const FString& Json);
	void HandleKwsError();
	void HandleKwsReady();
};
