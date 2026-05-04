#pragma once

#include "CoreMinimal.h"
#include "SherpaAsrTypes.generated.h"

UENUM(BlueprintType)
enum class ESherpaAsrPreset : uint8
{
	Bilingual_ZhEn_Fp32_2023  UMETA(DisplayName = "中英双语 | fp32 (342MB)"),
	Bilingual_ZhEn_Int8_2023  UMETA(DisplayName = "中英双语 | int8 (190MB)"),
	Chinese_Zh_Int8_2025      UMETA(DisplayName = "纯中文 | int8 | 2025新版 (122MB)"),
	Chinese_Zh_Fp32_2025      UMETA(DisplayName = "纯中文 | fp32 | 2025新版 (567MB)"),
	English_En_Fp32_2023      UMETA(DisplayName = "纯英文 | fp32 | 2023"),
	Custom                    UMETA(DisplayName = "自定义路径"),
};

USTRUCT(BlueprintType)
struct SHERPAONNX_API FSherpaAsrConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Model")
	ESherpaAsrPreset Preset = ESherpaAsrPreset::Bilingual_ZhEn_Fp32_2023;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Model")
	FString EncoderPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Model")
	FString DecoderPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Model")
	FString JoinerPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Model")
	FString TokensPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Decoder")
	FString DecodingMethod = TEXT("greedy_search");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Decoder")
	int32 MaxActivePaths = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Endpoint")
	bool bEnableEndpoint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Endpoint")
	float Rule1MinTrailingSilence = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Endpoint")
	float Rule2MinTrailingSilence = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Endpoint")
	float Rule3MinUtteranceLength = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Hotwords")
	bool bEnableHotwords = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Hotwords")
	FString ModelingUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Hotwords")
	FString BpeVocab;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Hotwords")
	FString HotwordsString;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Hotwords")
	FString HotwordsFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Hotwords")
	float HotwordsScore = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Runtime")
	int32 NumThreads = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sherpa ASR|Runtime")
	FString Provider = TEXT("cpu");
};

USTRUCT(BlueprintType)
struct SHERPAONNX_API FSherpaAsrResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sherpa ASR")
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category = "Sherpa ASR")
	TArray<float> Timestamps;

	UPROPERTY(BlueprintReadOnly, Category = "Sherpa ASR")
	bool bIsPartial = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sherpa ASR")
	int32 SegmentId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sherpa ASR")
	FString Json;
};
