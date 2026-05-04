#include "Asr/SherpaAsrComponent.h"

#if WITH_SHERPA_ONNX
#include "SherpaAsrWorker.h"
#include "SherpaAudioCapture.h"
#endif

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSherpaAsrComponent, Log, All);

static void ResolvePresetPaths(ESherpaAsrPreset Preset, FSherpaAsrConfig& OutConfig)
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SherpaONNX"));
	if (!Plugin.IsValid()) return;

	const FString Root = Plugin->GetContentDir() / TEXT("Models/Asr/");

	switch (Preset)
	{
	case ESherpaAsrPreset::Bilingual_ZhEn_Fp32_2023:
		OutConfig.EncoderPath = Root / TEXT("sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/encoder-epoch-99-avg-1.onnx");
		OutConfig.DecoderPath = Root / TEXT("sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/decoder-epoch-99-avg-1.onnx");
		OutConfig.JoinerPath  = Root / TEXT("sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/joiner-epoch-99-avg-1.onnx");
		OutConfig.TokensPath  = Root / TEXT("sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/tokens.txt");
		OutConfig.ModelingUnit = TEXT("cjkchar+bpe");
		if (OutConfig.BpeVocab.IsEmpty()) OutConfig.BpeVocab = Root / TEXT("sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/bpe.model");
		break;
	case ESherpaAsrPreset::Bilingual_ZhEn_Int8_2023:
		OutConfig.EncoderPath = Root / TEXT("sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/encoder-epoch-99-avg-1.int8.onnx");
		OutConfig.DecoderPath = Root / TEXT("sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/decoder-epoch-99-avg-1.int8.onnx");
		OutConfig.JoinerPath  = Root / TEXT("sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/joiner-epoch-99-avg-1.int8.onnx");
		OutConfig.TokensPath  = Root / TEXT("sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/tokens.txt");
		OutConfig.ModelingUnit = TEXT("cjkchar+bpe");
		if (OutConfig.BpeVocab.IsEmpty()) OutConfig.BpeVocab = Root / TEXT("sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/bpe.model");
		break;
	case ESherpaAsrPreset::Chinese_Zh_Int8_2025:
		OutConfig.EncoderPath = Root / TEXT("zh-int8/sherpa-onnx-streaming-zipformer-zh-int8-2025-06-30/encoder.int8.onnx");
		OutConfig.DecoderPath = Root / TEXT("zh-int8/sherpa-onnx-streaming-zipformer-zh-int8-2025-06-30/decoder.onnx");
		OutConfig.JoinerPath  = Root / TEXT("zh-int8/sherpa-onnx-streaming-zipformer-zh-int8-2025-06-30/joiner.int8.onnx");
		OutConfig.TokensPath  = Root / TEXT("zh-int8/sherpa-onnx-streaming-zipformer-zh-int8-2025-06-30/tokens.txt");
		OutConfig.ModelingUnit = TEXT("cjkchar");
		break;
	case ESherpaAsrPreset::Chinese_Zh_Fp32_2025:
		OutConfig.EncoderPath = Root / TEXT("zh-fp32/sherpa-onnx-streaming-zipformer-zh-2025-06-30/encoder.onnx");
		OutConfig.DecoderPath = Root / TEXT("zh-fp32/sherpa-onnx-streaming-zipformer-zh-2025-06-30/decoder.onnx");
		OutConfig.JoinerPath  = Root / TEXT("zh-fp32/sherpa-onnx-streaming-zipformer-zh-2025-06-30/joiner.onnx");
		OutConfig.TokensPath  = Root / TEXT("zh-fp32/sherpa-onnx-streaming-zipformer-zh-2025-06-30/tokens.txt");
		OutConfig.ModelingUnit = TEXT("cjkchar");
		break;
	case ESherpaAsrPreset::English_En_Fp32_2023:
		OutConfig.EncoderPath = Root / TEXT("en-fp32/sherpa-onnx-streaming-zipformer-en-2023-06-26/encoder-epoch-99-avg-1-chunk-16-left-128.onnx");
		OutConfig.DecoderPath = Root / TEXT("en-fp32/sherpa-onnx-streaming-zipformer-en-2023-06-26/decoder-epoch-99-avg-1-chunk-16-left-128.onnx");
		OutConfig.JoinerPath  = Root / TEXT("en-fp32/sherpa-onnx-streaming-zipformer-en-2023-06-26/joiner-epoch-99-avg-1-chunk-16-left-128.onnx");
		OutConfig.TokensPath  = Root / TEXT("en-fp32/sherpa-onnx-streaming-zipformer-en-2023-06-26/tokens.txt");
		OutConfig.ModelingUnit = TEXT("bpe");
		if (OutConfig.BpeVocab.IsEmpty()) OutConfig.BpeVocab = Root / TEXT("en-fp32/sherpa-onnx-streaming-zipformer-en-2023-06-26/bpe.model");
		break;
	default: break;
	}
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
	// 从 Details 面板的 Config.Preset 解析路径
	if (Config.Preset != ESherpaAsrPreset::Custom)
	{
		ResolvePresetPaths(Config.Preset, Config);
	}
	return StartASRInternal(Config);
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
		ResolvePresetPaths(Preset, ResolvedConfig);
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
