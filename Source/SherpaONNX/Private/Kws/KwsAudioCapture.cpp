#include "Kws/KwsAudioCapture.h"

#include "AudioResampler.h"

#if WITH_SHERPA_ONNX
#include "KwsWorker.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogKwsAudioCapture, Log, All);

namespace
{
constexpr int32 KwsTargetSampleRate = 16000;
constexpr int32 KwsAudioLogIntervalFrames = KwsTargetSampleRate;

void LogAudioLevelIfNeeded(
	int64& FramesSinceLastLog,
	const float* Samples,
	int32 NumSamples,
	int32 SourceSampleRate,
	int32 SourceChannels)
{
	FramesSinceLastLog += NumSamples;
	if (FramesSinceLastLog < KwsAudioLogIntervalFrames)
	{
		return;
	}

	double SumSquares = 0.0;
	float Peak = 0.0f;
	for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
	{
		const float Sample = Samples[SampleIndex];
		const float AbsSample = FMath::Abs(Sample);
		Peak = FMath::Max(Peak, AbsSample);
		SumSquares += static_cast<double>(Sample) * Sample;
	}

	const float Rms = FMath::Sqrt(static_cast<float>(SumSquares / FMath::Max(1, NumSamples)));
	UE_LOG(LogKwsAudioCapture, Log, TEXT("KWS audio chunk: source_rate=%d, source_channels=%d, output_samples=%d, rms=%.6f, peak=%.6f"),
		SourceSampleRate, SourceChannels, NumSamples, Rms, Peak);
	FramesSinceLastLog = 0;
}
}

UKwsAudioCapture::UKwsAudioCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UKwsAudioCapture::Init(int32& SampleRate)
{
	const bool bInitialized = Super::Init(SampleRate);
	CaptureSampleRate = SampleRate;

	UE_LOG(LogKwsAudioCapture, Log, TEXT("KWS audio capture initialized: sample rate=%d, channels=%d, target sample rate=%d"),
		CaptureSampleRate, NumChannels, KwsTargetSampleRate);

	return bInitialized;
}

int32 UKwsAudioCapture::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	NumSamples = Super::OnGenerateAudio(OutAudio, NumSamples);

#if WITH_SHERPA_ONNX
	if (bCapturing && Worker && NumSamples > 0)
	{
		const int32 InputChannels = FMath::Max(1, NumChannels);
		const int32 InputFrames = NumSamples / InputChannels;
		if (InputFrames <= 0)
		{
			return NumSamples;
		}

		Audio::VectorOps::FAlignedFloatBuffer MonoBuffer;
		MonoBuffer.AddUninitialized(InputFrames);

		if (InputChannels == 1)
		{
			FMemory::Memcpy(MonoBuffer.GetData(), OutAudio, InputFrames * sizeof(float));
		}
		else
		{
			for (int32 FrameIndex = 0; FrameIndex < InputFrames; ++FrameIndex)
			{
				float MixedSample = 0.0f;
				for (int32 ChannelIndex = 0; ChannelIndex < InputChannels; ++ChannelIndex)
				{
					MixedSample += OutAudio[FrameIndex * InputChannels + ChannelIndex];
				}
				MonoBuffer[FrameIndex] = MixedSample / static_cast<float>(InputChannels);
			}
		}

		if (CaptureSampleRate == KwsTargetSampleRate)
		{
			TArray<float> Buffer(MonoBuffer.GetData(), MonoBuffer.Num());
			Worker->PushAudio(Buffer);
			LogAudioLevelIfNeeded(CapturedFramesSinceLastLog, MonoBuffer.GetData(), MonoBuffer.Num(), CaptureSampleRate, InputChannels);
		}
		else
		{
			Audio::FResamplingParameters ResamplerParams = {
				Audio::EResamplingMethod::Linear,
				1,
				static_cast<float>(CaptureSampleRate),
				static_cast<float>(KwsTargetSampleRate),
				MonoBuffer
			};

			Audio::VectorOps::FAlignedFloatBuffer ResampledBuffer;
			ResampledBuffer.AddUninitialized(Audio::GetOutputBufferSize(ResamplerParams));

			Audio::FResamplerResults ResamplerResults;
			ResamplerResults.OutBuffer = &ResampledBuffer;

			if (Audio::Resample(ResamplerParams, ResamplerResults))
			{
				const int32 OutputSamples = ResamplerResults.OutputFramesGenerated;
				if (OutputSamples > 0)
				{
					TArray<float> Buffer(ResampledBuffer.GetData(), OutputSamples);
					Worker->PushAudio(Buffer);
					LogAudioLevelIfNeeded(CapturedFramesSinceLastLog, ResampledBuffer.GetData(), OutputSamples, CaptureSampleRate, InputChannels);
				}
			}
			else
			{
				UE_LOG(LogKwsAudioCapture, Warning, TEXT("KWS audio resample failed: source sample rate=%d, target sample rate=%d"),
					CaptureSampleRate, KwsTargetSampleRate);
			}
		}
	}
#endif

	return NumSamples;
}

void UKwsAudioCapture::StartCapturing()
{
	CapturedFramesSinceLastLog = 0;
	bCapturing = true;
	UE_LOG(LogKwsAudioCapture, Log, TEXT("KWS audio capture forwarding started"));
}

void UKwsAudioCapture::StopCapturing()
{
	bCapturing = false;
	UE_LOG(LogKwsAudioCapture, Log, TEXT("KWS audio capture forwarding stopped"));
}
