#pragma once

#include "CoreMinimal.h"
#include "KwsTypes.generated.h"

UENUM(BlueprintType)
enum class ESherpaKwsPreset : uint8
{
	Bilingual_Fp32_320ms  UMETA(DisplayName = "中英双语 | fp32 | 320ms 延迟 (精度最高)"),
	Bilingual_Int8_320ms  UMETA(DisplayName = "中英双语 | int8 | 320ms 延迟 (体积最小)"),
	Bilingual_Fp32_160ms  UMETA(DisplayName = "中英双语 | fp32 | 160ms 延迟 (低延迟)"),
	Bilingual_Int8_160ms  UMETA(DisplayName = "中英双语 | int8 | 160ms 延迟 (低延迟+小体积)"),
	Chinese_Fp32_320ms    UMETA(DisplayName = "纯中文 | fp32 | 320ms 延迟"),
};

USTRUCT(BlueprintType)
struct SHERPAONNX_API FSherpaKwsModelConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Model")
	FString EncoderPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Model")
	FString DecoderPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Model")
	FString JoinerPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Model")
	FString TokensPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Model")
	FString KeywordsFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Model")
	FString KeywordsString;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Model")
	int32 NumThreads = 1;  // ORT thread pool may conflict with UE5

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Model")
	FString Provider = TEXT("cpu");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Decoder")
	int32 MaxActivePaths = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Decoder")
	int32 NumTrailingBlanks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Decoder")
	float KeywordsScore = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KWS|Decoder")
	float KeywordsThreshold = 0.25f;
};

USTRUCT(BlueprintType)
struct SHERPAONNX_API FSherpaKwsKeywordResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "KWS|Result")
	FString Keyword;

	UPROPERTY(BlueprintReadOnly, Category = "KWS|Result")
	TArray<float> Timestamps;

	UPROPERTY(BlueprintReadOnly, Category = "KWS|Result")
	float StartTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "KWS|Result")
	FString Json;
};
