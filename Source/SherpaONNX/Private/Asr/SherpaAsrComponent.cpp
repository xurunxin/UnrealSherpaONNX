#include "Asr/SherpaAsrComponent.h"

#if WITH_SHERPA_ONNX
#include "Model/SherpaModelPathResolver.h"
#include "SherpaAsrWorker.h"
#include "SherpaAudioCapture.h"
#endif

#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSherpaAsrComponent, Log, All);

static bool ResolvePresetPaths(ESherpaAsrPreset Preset, FSherpaAsrConfig& OutConfig, FString& OutError)
{
	FString EncoderRelativePath;
	FString DecoderRelativePath;
	FString JoinerRelativePath;
	FString TokensRelativePath;
	FString DefaultBpeRelativePath;
	FString ModelingUnit;

	switch (Preset)
	{
	case ESherpaAsrPreset::Bilingual_ZhEn_Fp32_2023:
		EncoderRelativePath = TEXT("Asr/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/encoder-epoch-99-avg-1.onnx");
		DecoderRelativePath = TEXT("Asr/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/decoder-epoch-99-avg-1.onnx");
		JoinerRelativePath = TEXT("Asr/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/joiner-epoch-99-avg-1.onnx");
		TokensRelativePath = TEXT("Asr/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/tokens.txt");
		DefaultBpeRelativePath = TEXT("Asr/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/bpe.model");
		ModelingUnit = TEXT("cjkchar+bpe");
		break;
	case ESherpaAsrPreset::Bilingual_ZhEn_Int8_2023:
		EncoderRelativePath = TEXT("Asr/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/encoder-epoch-99-avg-1.int8.onnx");
		DecoderRelativePath = TEXT("Asr/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/decoder-epoch-99-avg-1.int8.onnx");
		JoinerRelativePath = TEXT("Asr/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/joiner-epoch-99-avg-1.int8.onnx");
		TokensRelativePath = TEXT("Asr/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/tokens.txt");
		DefaultBpeRelativePath = TEXT("Asr/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/bpe.model");
		ModelingUnit = TEXT("cjkchar+bpe");
		break;
	case ESherpaAsrPreset::Chinese_Zh_Int8_2025:
		EncoderRelativePath = TEXT("Asr/zh-int8/sherpa-onnx-streaming-zipformer-zh-int8-2025-06-30/encoder.int8.onnx");
		DecoderRelativePath = TEXT("Asr/zh-int8/sherpa-onnx-streaming-zipformer-zh-int8-2025-06-30/decoder.onnx");
		JoinerRelativePath = TEXT("Asr/zh-int8/sherpa-onnx-streaming-zipformer-zh-int8-2025-06-30/joiner.int8.onnx");
		TokensRelativePath = TEXT("Asr/zh-int8/sherpa-onnx-streaming-zipformer-zh-int8-2025-06-30/tokens.txt");
		ModelingUnit = TEXT("cjkchar");
		break;
	case ESherpaAsrPreset::Chinese_Zh_Fp32_2025:
		EncoderRelativePath = TEXT("Asr/zh-fp32/sherpa-onnx-streaming-zipformer-zh-2025-06-30/encoder.onnx");
		DecoderRelativePath = TEXT("Asr/zh-fp32/sherpa-onnx-streaming-zipformer-zh-2025-06-30/decoder.onnx");
		JoinerRelativePath = TEXT("Asr/zh-fp32/sherpa-onnx-streaming-zipformer-zh-2025-06-30/joiner.onnx");
		TokensRelativePath = TEXT("Asr/zh-fp32/sherpa-onnx-streaming-zipformer-zh-2025-06-30/tokens.txt");
		ModelingUnit = TEXT("cjkchar");
		break;
	case ESherpaAsrPreset::English_En_Fp32_2023:
		EncoderRelativePath = TEXT("Asr/en-fp32/sherpa-onnx-streaming-zipformer-en-2023-06-26/encoder-epoch-99-avg-1-chunk-16-left-128.onnx");
		DecoderRelativePath = TEXT("Asr/en-fp32/sherpa-onnx-streaming-zipformer-en-2023-06-26/decoder-epoch-99-avg-1-chunk-16-left-128.onnx");
		JoinerRelativePath = TEXT("Asr/en-fp32/sherpa-onnx-streaming-zipformer-en-2023-06-26/joiner-epoch-99-avg-1-chunk-16-left-128.onnx");
		TokensRelativePath = TEXT("Asr/en-fp32/sherpa-onnx-streaming-zipformer-en-2023-06-26/tokens.txt");
		DefaultBpeRelativePath = TEXT("Asr/en-fp32/sherpa-onnx-streaming-zipformer-en-2023-06-26/bpe.model");
		ModelingUnit = TEXT("bpe");
		break;
	default:
		OutError = TEXT("Custom ASR paths must be supplied explicitly.");
		return false;
	}

	TArray<FString> RequiredRelativePaths = {
		EncoderRelativePath,
		DecoderRelativePath,
		JoinerRelativePath,
		TokensRelativePath
	};
	const bool bBpePreset = !DefaultBpeRelativePath.IsEmpty();
	const bool bUseDefaultBpe = bBpePreset && OutConfig.BpeVocab.IsEmpty();
	if (bUseDefaultBpe)
	{
		RequiredRelativePaths.Add(DefaultBpeRelativePath);
	}

	const SherpaModelPathResolver::FModelPathResolution Resolution =
		SherpaModelPathResolver::ResolveDefaultModelSet(RequiredRelativePaths);
	if (!Resolution.bSuccess)
	{
		OutError = Resolution.ErrorMessage;
		return false;
	}

	OutConfig.EncoderPath = Resolution.Paths[0];
	OutConfig.DecoderPath = Resolution.Paths[1];
	OutConfig.JoinerPath = Resolution.Paths[2];
	OutConfig.TokensPath = Resolution.Paths[3];
	OutConfig.ModelingUnit = ModelingUnit;
	if (bUseDefaultBpe)
	{
		OutConfig.BpeVocab = Resolution.Paths[4];
	}
	else if (!bBpePreset)
	{
		OutConfig.BpeVocab.Reset();
	}
	UE_LOG(LogSherpaAsrComponent, Log,
		TEXT("ASR preset model root selected: %s (encoder=%s, decoder=%s, joiner=%s, tokens=%s, bpe=%s)"),
		*Resolution.Root,
		*OutConfig.EncoderPath,
		*OutConfig.DecoderPath,
		*OutConfig.JoinerPath,
		*OutConfig.TokensPath,
		*OutConfig.BpeVocab);
	return true;
}

USherpaAsrComponent::USherpaAsrComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USherpaAsrComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USherpaAsrComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopASR();
	Super::EndPlay(EndPlayReason);
}

bool USherpaAsrComponent::StartASR()
{
#if !WITH_SHERPA_ONNX
	return false;
#else
	FSherpaAsrConfig ResolvedConfig = Config;
	// 从 Details 面板的 Config.Preset 解析本次启动路径，不回写可编辑配置。
	if (ResolvedConfig.Preset != ESherpaAsrPreset::Custom)
	{
		FString ResolutionError;
		if (!ResolvePresetPaths(ResolvedConfig.Preset, ResolvedConfig, ResolutionError))
		{
			UE_LOG(LogSherpaAsrComponent, Error, TEXT("%s"), *ResolutionError);
			OnAsrError.Broadcast(ResolutionError);
			return false;
		}
	}
	return StartASRInternal(ResolvedConfig);
#endif
}

bool USherpaAsrComponent::StartASRWithPreset(ESherpaAsrPreset Preset, bool bEnableHotwords, const FString& HotwordsString)
{
#if !WITH_SHERPA_ONNX
	return false;
#else
	FSherpaAsrConfig ResolvedConfig = Config;
	ResolvedConfig.Preset = Preset;
	ResolvedConfig.bEnableHotwords = bEnableHotwords;
	if (!HotwordsString.IsEmpty())
	{
		ResolvedConfig.HotwordsString = HotwordsString;
	}

	if (Preset != ESherpaAsrPreset::Custom)
	{
		FString ResolutionError;
		if (!ResolvePresetPaths(Preset, ResolvedConfig, ResolutionError))
		{
			UE_LOG(LogSherpaAsrComponent, Error, TEXT("%s"), *ResolutionError);
			OnAsrError.Broadcast(ResolutionError);
			return false;
		}
	}
	return StartASRInternal(ResolvedConfig);
#endif
}

bool USherpaAsrComponent::StartASRInternal(const FSherpaAsrConfig& ResolvedConfig)
{
	StopASR();

	if (ResolvedConfig.EncoderPath.IsEmpty() || ResolvedConfig.DecoderPath.IsEmpty() ||
		ResolvedConfig.JoinerPath.IsEmpty()  || ResolvedConfig.TokensPath.IsEmpty())
	{
		OnAsrError.Broadcast(TEXT("ASR model paths are incomplete."));
		return false;
	}

	TArray<FString> MissingModelPaths;
	auto CheckModelFile = [&MissingModelPaths](const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return;
		}
		FString AbsolutePath = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(AbsolutePath);
		if (!FPaths::FileExists(AbsolutePath))
		{
			MissingModelPaths.Add(AbsolutePath);
		}
	};
	CheckModelFile(ResolvedConfig.EncoderPath);
	CheckModelFile(ResolvedConfig.DecoderPath);
	CheckModelFile(ResolvedConfig.JoinerPath);
	CheckModelFile(ResolvedConfig.TokensPath);
	CheckModelFile(ResolvedConfig.BpeVocab);
	if (!MissingModelPaths.IsEmpty())
	{
		FString Error = TEXT("ASR model files not found:");
		for (const FString& MissingPath : MissingModelPaths)
		{
			Error += FString::Printf(TEXT("\n  - %s"), *MissingPath);
		}
		UE_LOG(LogSherpaAsrComponent, Error, TEXT("%s"), *Error);
		OnAsrError.Broadcast(Error);
		return false;
	}

	auto NormalizeFinalPath = [](const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return FString();
		}
		FString AbsolutePath = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(AbsolutePath);
		return AbsolutePath;
	};
	const FString FinalEncoderPath = NormalizeFinalPath(ResolvedConfig.EncoderPath);
	const FString FinalDecoderPath = NormalizeFinalPath(ResolvedConfig.DecoderPath);
	const FString FinalJoinerPath = NormalizeFinalPath(ResolvedConfig.JoinerPath);
	const FString FinalTokensPath = NormalizeFinalPath(ResolvedConfig.TokensPath);
	const FString FinalBpePath = NormalizeFinalPath(ResolvedConfig.BpeVocab);
	UE_LOG(LogSherpaAsrComponent, Log,
		TEXT("ASR final model paths: encoder=%s, decoder=%s, joiner=%s, tokens=%s, bpe=%s"),
		*FinalEncoderPath,
		*FinalDecoderPath,
		*FinalJoinerPath,
		*FinalTokensPath,
		*FinalBpePath);

	Worker = new FSherpaAsrWorker(ResolvedConfig);
	Worker->OnPartialResult = [this](const FSherpaAsrResult& R) { OnPartialResult.Broadcast(R); };
	Worker->OnFinalResult   = [this](const FSherpaAsrResult& R) { OnFinalResult.Broadcast(R); };
	Worker->OnError         = [this](const FString& E) { OnAsrError.Broadcast(E); };

	if (!Worker->Start())
	{
		delete Worker;
		Worker = nullptr;
		OnAsrError.Broadcast(TEXT("Failed to start ASR worker."));
		return false;
	}

	AudioCapture = NewObject<USherpaAudioCapture>(this);
	if (!AudioCapture)
	{
		delete Worker;
		Worker = nullptr;
		OnAsrError.Broadcast(TEXT("Failed to create audio capture."));
		return false;
	}

	FSherpaAsrWorker* AsrWorker = Worker;
	AudioCapture->AddConsumer([AsrWorker](const TArray<float>& Samples) {
		AsrWorker->PushAudio(Samples);
	});

	AudioCapture->RegisterComponent();
	AudioCapture->bAutoActivate = false;
	AudioCapture->Activate(true);
	AudioCapture->StartCapturing();

	UE_LOG(LogSherpaAsrComponent, Log, TEXT("ASR started (mic auto-capture)"));
	return true;
}

void USherpaAsrComponent::StopASR()
{
	if (AudioCapture)
	{
		AudioCapture->StopCapturing();
		AudioCapture->ClearConsumers();
		AudioCapture->Deactivate();
		AudioCapture->DestroyComponent();
		AudioCapture = nullptr;
	}

	if (Worker)
	{
		Worker->RequestStopAndWait();
		delete Worker;
		Worker = nullptr;
	}
}

bool USherpaAsrComponent::IsRunning() const
{
	return Worker != nullptr && Worker->IsRunning();
}

void USherpaAsrComponent::SetHotwords(const FString& Hotwords)
{
	if (Worker) { Worker->SetHotwords(Hotwords); }
}
