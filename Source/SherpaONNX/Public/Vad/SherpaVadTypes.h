#pragma once

#include "CoreMinimal.h"
#include "SherpaVadTypes.generated.h"

UENUM(BlueprintType)
enum class ESherpaVadModelType : uint8
{
	Silero  UMETA(DisplayName = "Silero VAD"),
	TenVad  UMETA(DisplayName = "TEN VAD")
};

USTRUCT(BlueprintType)
struct SHERPAONNX_API FSherpaVadConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	ESherpaVadModelType ModelType = ESherpaVadModelType::Silero;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	FString ModelPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	int32 SampleRate = 16000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	int32 WindowSize = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	float Threshold = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	float MinSilenceDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	float MinSpeechDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	float MaxSpeechDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	float BufferSizeInSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	int32 NumThreads = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	FString Provider = TEXT("cpu");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	bool bDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	bool bAutoStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa VAD")
	bool bEmitSpeechSegment = true;
};

USTRUCT(BlueprintType)
struct SHERPAONNX_API FSherpaVadSegment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sherpa VAD")
	int32 StartSample = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sherpa VAD")
	int32 NumSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sherpa VAD")
	int32 SampleRate = 16000;

	UPROPERTY(BlueprintReadOnly, Category = "Sherpa VAD")
	TArray<float> Samples;
};
