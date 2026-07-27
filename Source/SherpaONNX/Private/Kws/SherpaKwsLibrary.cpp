#include "Kws/SherpaKwsLibrary.h"
#include "Model/SherpaModelPathResolver.h"
#include "Misc/Paths.h"

static bool LooksLikeMojibake(const FString& Text)
{
	return Text.Contains(TEXT("Ã")) ||
		Text.Contains(TEXT("Ç")) ||
		Text.Contains(TEXT("Ä")) ||
		Text.Contains(TEXT("Å"));
}

FSherpaKwsModelConfig USherpaKwsLibrary::MakeBilingualPresetConfig(
	ESherpaKwsPreset Preset, const FString& KeywordsString)
{
	FSherpaKwsModelConfig Config;

	FString ModelDirectory = TEXT("Kws/zh-en-3M");
	FString TokensRelativePath = TEXT("Kws/zh-en-3M/tokens.txt");
	FString EncoderName;
	FString DecoderName;
	FString JoinerName;

	switch (Preset)
	{
	case ESherpaKwsPreset::Bilingual_Fp32_320ms:
		EncoderName = TEXT("encoder-epoch-13-avg-2-chunk-16-left-64.onnx");
		DecoderName = TEXT("decoder-epoch-13-avg-2-chunk-16-left-64.onnx");
		JoinerName = TEXT("joiner-epoch-13-avg-2-chunk-16-left-64.onnx");
		break;
	case ESherpaKwsPreset::Bilingual_Int8_320ms:
		EncoderName = TEXT("encoder-epoch-13-avg-2-chunk-16-left-64.int8.onnx");
		DecoderName = TEXT("decoder-epoch-13-avg-2-chunk-16-left-64.onnx");
		JoinerName = TEXT("joiner-epoch-13-avg-2-chunk-16-left-64.int8.onnx");
		break;
	case ESherpaKwsPreset::Bilingual_Fp32_160ms:
		EncoderName = TEXT("encoder-epoch-13-avg-2-chunk-8-left-64.onnx");
		DecoderName = TEXT("decoder-epoch-13-avg-2-chunk-8-left-64.onnx");
		JoinerName = TEXT("joiner-epoch-13-avg-2-chunk-8-left-64.onnx");
		break;
	case ESherpaKwsPreset::Bilingual_Int8_160ms:
		EncoderName = TEXT("encoder-epoch-13-avg-2-chunk-8-left-64.int8.onnx");
		DecoderName = TEXT("decoder-epoch-13-avg-2-chunk-8-left-64.onnx");
		JoinerName = TEXT("joiner-epoch-13-avg-2-chunk-8-left-64.int8.onnx");
		break;
	case ESherpaKwsPreset::Chinese_Fp32_320ms:
		ModelDirectory = TEXT("sherpa-onnx-kws-zipformer-wenetspeech-3.3M-2024-01-01");
		EncoderName = TEXT("encoder-epoch-12-avg-2-chunk-16-left-64.onnx");
		DecoderName = TEXT("decoder-epoch-12-avg-2-chunk-16-left-64.onnx");
		JoinerName = TEXT("joiner-epoch-12-avg-2-chunk-16-left-64.onnx");
		break;
	}

	const bool bUseKeywordsFile = KeywordsString.IsEmpty() || LooksLikeMojibake(KeywordsString);
	TArray<FString> RequiredRelativePaths = {
		FPaths::Combine(ModelDirectory, EncoderName),
		FPaths::Combine(ModelDirectory, DecoderName),
		FPaths::Combine(ModelDirectory, JoinerName),
		TokensRelativePath
	};
	if (bUseKeywordsFile)
	{
		RequiredRelativePaths.Add(TEXT("keywords.txt"));
	}

	const SherpaModelPathResolver::FModelPathResolution Resolution =
		SherpaModelPathResolver::ResolveDefaultModelSet(RequiredRelativePaths);
	if (Resolution.bSuccess)
	{
		Config.EncoderPath = Resolution.Paths[0];
		Config.DecoderPath = Resolution.Paths[1];
		Config.JoinerPath = Resolution.Paths[2];
		Config.TokensPath = Resolution.Paths[3];
		if (bUseKeywordsFile)
		{
			Config.KeywordsFile = Resolution.Paths[4];
		}
		UE_LOG(LogTemp, Log, TEXT("KWS preset model root selected: %s"), *Resolution.Root);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("KWS preset model resolution failed: %s"), *Resolution.ErrorMessage);
	}

	if (bUseKeywordsFile)
	{
		if (!KeywordsString.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("KWS KeywordsString appears to be mojibake; falling back to keywords.txt"));
		}
	}
	else
	{
		Config.KeywordsString = KeywordsString;
	}
	Config.NumThreads = 1;
	Config.Provider = TEXT("cpu");
	Config.MaxActivePaths = 4;
	Config.NumTrailingBlanks = 1;
	Config.KeywordsScore = 1.0f;
	Config.KeywordsThreshold = 0.25f;

	UE_LOG(LogTemp, Log, TEXT("KWS preset config: encoder=%s, decoder=%s, joiner=%s, tokens=%s, keywords_threshold=%.2f"),
		*Config.EncoderPath,
		*Config.DecoderPath,
		*Config.JoinerPath,
		*Config.TokensPath,
		Config.KeywordsThreshold);

	return Config;
}

FSherpaKwsModelConfig USherpaKwsLibrary::MakeCustomConfig(
	const FString& ModelDir, const FString& EncoderName,
	const FString& DecoderName, const FString& JoinerName,
	const FString& TokensFileName, const FString& KeywordsFile,
	int32 NumThreads, const FString& Provider)
{
	FSherpaKwsModelConfig Config;
	Config.KeywordsFile = KeywordsFile;
	Config.NumThreads = NumThreads;
	Config.Provider = Provider;

	if (!FPaths::IsRelative(ModelDir))
	{
		FString AbsoluteModelDir = FPaths::ConvertRelativePathToFull(ModelDir);
		FPaths::NormalizeDirectoryName(AbsoluteModelDir);
		Config.EncoderPath = FPaths::Combine(AbsoluteModelDir, EncoderName);
		Config.DecoderPath = FPaths::Combine(AbsoluteModelDir, DecoderName);
		Config.JoinerPath = FPaths::Combine(AbsoluteModelDir, JoinerName);
		Config.TokensPath = FPaths::Combine(AbsoluteModelDir, TokensFileName);
		UE_LOG(LogTemp, Log, TEXT("KWS custom absolute model directory: %s"), *AbsoluteModelDir);
		return Config;
	}

	const TArray<FString> RequiredRelativePaths = {
		FPaths::Combine(ModelDir, EncoderName),
		FPaths::Combine(ModelDir, DecoderName),
		FPaths::Combine(ModelDir, JoinerName),
		FPaths::Combine(ModelDir, TokensFileName)
	};
	const SherpaModelPathResolver::FModelPathResolution Resolution =
		SherpaModelPathResolver::ResolveDefaultModelSet(RequiredRelativePaths);
	if (!Resolution.bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("KWS custom relative model resolution failed: %s"), *Resolution.ErrorMessage);
		return Config;
	}

	Config.EncoderPath = Resolution.Paths[0];
	Config.DecoderPath = Resolution.Paths[1];
	Config.JoinerPath = Resolution.Paths[2];
	Config.TokensPath = Resolution.Paths[3];
	UE_LOG(LogTemp, Log, TEXT("KWS custom relative model root selected: %s"), *Resolution.Root);
	return Config;
}
