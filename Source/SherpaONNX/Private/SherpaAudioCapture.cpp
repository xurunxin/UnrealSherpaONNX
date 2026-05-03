#include "SherpaAudioCapture.h"

#include "AudioResampler.h"

DEFINE_LOG_CATEGORY_STATIC(LogSherpaAudioCapture, Log, All);

namespace
{
constexpr int32 TargetSampleRate = 16000;
} // namespace

USherpaAudioCapture::USherpaAudioCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USherpaAudioCapture::Init(int32& SampleRate)
{
	const bool bInitialized = Super::Init(SampleRate);
	CaptureSampleRate = SampleRate;

	UE_LOG(LogSherpaAudioCapture, Log,
		TEXT("SherpaAudioCapture init: device_rate=%d channels=%d target=%d"),
		CaptureSampleRate, NumChannels, TargetSampleRate);

	return bInitialized;
}

int32 USherpaAudioCapture::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	NumSamples = Super::OnGenerateAudio(OutAudio, NumSamples);

	if (!bCapturing || NumSamples <= 0) return NumSamples;

	const int32 InputChannels = FMath::Max(1, NumChannels);
	const int32 InputFrames   = NumSamples / InputChannels;
	if (InputFrames <= 0) return NumSamples;

	// 多声道 → 单声道
	Audio::VectorOps::FAlignedFloatBuffer MonoBuffer;
	MonoBuffer.AddUninitialized(InputFrames);

	if (InputChannels == 1)
	{
		FMemory::Memcpy(MonoBuffer.GetData(), OutAudio, InputFrames * sizeof(float));
	}
	else
	{
		for (int32 Frame = 0; Frame < InputFrames; ++Frame)
		{
			float Sum = 0.0f;
			for (int32 Ch = 0; Ch < InputChannels; ++Ch)
				Sum += OutAudio[Frame * InputChannels + Ch];
			MonoBuffer[Frame] = Sum / static_cast<float>(InputChannels);
		}
	}

	// 重采样 → 16kHz
	TArray<float> OutputBuffer;
	if (CaptureSampleRate == TargetSampleRate)
	{
		OutputBuffer = TArray<float>(MonoBuffer.GetData(), MonoBuffer.Num());
	}
	else
	{
		Audio::FResamplingParameters Params = {
			Audio::EResamplingMethod::Linear,
			1,
			static_cast<float>(CaptureSampleRate),
			static_cast<float>(TargetSampleRate),
			MonoBuffer
		};
		Audio::VectorOps::FAlignedFloatBuffer ResampledBuffer;
		ResampledBuffer.AddUninitialized(Audio::GetOutputBufferSize(Params));
		Audio::FResamplerResults Results;
		Results.OutBuffer = &ResampledBuffer;

		if (Audio::Resample(Params, Results) && Results.OutputFramesGenerated > 0)
		{
			OutputBuffer = TArray<float>(ResampledBuffer.GetData(), Results.OutputFramesGenerated);
		}
		else
		{
			return NumSamples;
		}
	}

	// 分发给所有消费者（16kHz mono float32）
	if (OutputBuffer.Num() > 0)
	{
		FScopeLock Lock(&ConsumersLock);
		for (const auto& Consumer : Consumers)
		{
			Consumer(OutputBuffer);
		}
	}

	return NumSamples;
}

void USherpaAudioCapture::AddConsumer(FAudioConsumer InConsumer)
{
	FScopeLock Lock(&ConsumersLock);
	Consumers.Add(MoveTemp(InConsumer));
}

void USherpaAudioCapture::ClearConsumers()
{
	FScopeLock Lock(&ConsumersLock);
	Consumers.Empty();
}

void USherpaAudioCapture::StartCapturing()
{
	bCapturing = true;
}

void USherpaAudioCapture::StopCapturing()
{
	bCapturing = false;
}
