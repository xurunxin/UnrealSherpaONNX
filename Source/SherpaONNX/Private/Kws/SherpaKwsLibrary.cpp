#include "Kws/SherpaKwsLibrary.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"

static FString GetModelBasePath()
{
	// 编辑器：用插件 Content 目录（原地读取）
	// 打包运行：NonUFS 文件安装到 ProjectDir/Content/Models/
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SherpaONNX"));
	if (Plugin.IsValid())
	{
		FString EditorPath = Plugin->GetContentDir() / TEXT("Models/");
		if (FPaths::DirectoryExists(EditorPath))
		{
			return EditorPath;
		}
	}
	// 兜底：打包后的 NonUFS 路径
	return FPaths::ProjectDir() / TEXT("Content/Models/");
}

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

	const FString Base = GetModelBasePath() + TEXT("sherpa-onnx-kws-zipformer-zh-en-3M-2025-12-20/");

	switch (Preset)
	{
	case ESherpaKwsPreset::Bilingual_Fp32_320ms:
		Config.EncoderPath = Base + TEXT("encoder-epoch-13-avg-2-chunk-16-left-64.onnx");
		Config.DecoderPath = Base + TEXT("decoder-epoch-13-avg-2-chunk-16-left-64.onnx");
		Config.JoinerPath  = Base + TEXT("joiner-epoch-13-avg-2-chunk-16-left-64.onnx");
		break;
	case ESherpaKwsPreset::Bilingual_Int8_320ms:
		Config.EncoderPath = Base + TEXT("encoder-epoch-13-avg-2-chunk-16-left-64.int8.onnx");
		Config.DecoderPath = Base + TEXT("decoder-epoch-13-avg-2-chunk-16-left-64.onnx");
		Config.JoinerPath  = Base + TEXT("joiner-epoch-13-avg-2-chunk-16-left-64.int8.onnx");
		break;
	case ESherpaKwsPreset::Bilingual_Fp32_160ms:
		Config.EncoderPath = Base + TEXT("encoder-epoch-13-avg-2-chunk-8-left-64.onnx");
		Config.DecoderPath = Base + TEXT("decoder-epoch-13-avg-2-chunk-8-left-64.onnx");
		Config.JoinerPath  = Base + TEXT("joiner-epoch-13-avg-2-chunk-8-left-64.onnx");
		break;
	case ESherpaKwsPreset::Bilingual_Int8_160ms:
		Config.EncoderPath = Base + TEXT("encoder-epoch-13-avg-2-chunk-8-left-64.int8.onnx");
		Config.DecoderPath = Base + TEXT("decoder-epoch-13-avg-2-chunk-8-left-64.onnx");
		Config.JoinerPath  = Base + TEXT("joiner-epoch-13-avg-2-chunk-8-left-64.int8.onnx");
		break;
	case ESherpaKwsPreset::Chinese_Fp32_320ms:
	{
		const FString ZhBase = GetModelBasePath() + TEXT("sherpa-onnx-kws-zipformer-wenetspeech-3.3M-2024-01-01/");
		Config.EncoderPath = ZhBase + TEXT("encoder-epoch-12-avg-2-chunk-16-left-64.onnx");
		Config.DecoderPath = ZhBase + TEXT("decoder-epoch-12-avg-2-chunk-16-left-64.onnx");
		Config.JoinerPath  = ZhBase + TEXT("joiner-epoch-12-avg-2-chunk-16-left-64.onnx");
		break;
	}
	}

	Config.TokensPath      = Base + TEXT("tokens.txt");
	if (KeywordsString.IsEmpty() || LooksLikeMojibake(KeywordsString))
	{
		if (!KeywordsString.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("KWS KeywordsString appears to be mojibake; falling back to plugin keywords.txt"));
		}
		Config.KeywordsFile = GetModelBasePath() + TEXT("keywords.txt");
	}
	else
	{
		Config.KeywordsString = KeywordsString;
	}
	Config.NumThreads      = 1;
	Config.Provider        = TEXT("cpu");
	Config.MaxActivePaths = 4;
	Config.NumTrailingBlanks = 1;
	Config.KeywordsScore = 1.0f;
	Config.KeywordsThreshold = 0.25f;

	UE_LOG(LogTemp, Log, TEXT("KWS preset config: encoder=%s, decoder=%s, joiner=%s, keywords_threshold=%.2f"),
		*FPaths::GetCleanFilename(Config.EncoderPath),
		*FPaths::GetCleanFilename(Config.DecoderPath),
		*FPaths::GetCleanFilename(Config.JoinerPath),
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
	const FString Base = GetModelBasePath() + ModelDir + TEXT("/");
	Config.EncoderPath = Base + EncoderName;
	Config.DecoderPath = Base + DecoderName;
	Config.JoinerPath  = Base + JoinerName;
	Config.TokensPath  = Base + TokensFileName;
	Config.KeywordsFile = KeywordsFile;
	Config.NumThreads   = NumThreads;
	Config.Provider     = Provider;
	return Config;
}
