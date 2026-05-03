#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kws/KwsTypes.h"
#include "SherpaKwsLibrary.generated.h"

UCLASS()
class SHERPAONNX_API USherpaKwsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Sherpa KWS|Config",
		meta = (DisplayName = "Make Preset Config (zh-en 3M Bilingual)"))
	static FSherpaKwsModelConfig MakeBilingualPresetConfig(
		ESherpaKwsPreset Preset,
		const FString& KeywordsString = TEXT(""));

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Sherpa KWS|Config",
		meta = (DisplayName = "Make Custom Config"))
	static FSherpaKwsModelConfig MakeCustomConfig(
		const FString& ModelDir,
		const FString& EncoderName,
		const FString& DecoderName,
		const FString& JoinerName,
		const FString& TokensFileName,
		const FString& KeywordsFile,
		int32 NumThreads = 2,
		const FString& Provider = TEXT("cpu"));
};
