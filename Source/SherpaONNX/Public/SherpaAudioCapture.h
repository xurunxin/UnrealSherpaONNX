#pragma once

#include "CoreMinimal.h"
#include "AudioCaptureComponent.h"
#include "SherpaAudioCapture.generated.h"

UCLASS(ClassGroup=(SherpaONNX), meta=(BlueprintSpawnableComponent))
class SHERPAONNX_API USherpaAudioCapture : public UAudioCaptureComponent
{
	GENERATED_BODY()

public:
	USherpaAudioCapture(const FObjectInitializer& ObjectInitializer);

	using FAudioConsumer = TFunction<void(const TArray<float>& /*16kHz mono float32*/)>;

	/** 注册一个音频消费者（KWS Worker、VAD Worker 等） */
	void AddConsumer(FAudioConsumer InConsumer);

	/** 清除所有消费者 */
	void ClearConsumers();

	UFUNCTION(BlueprintCallable, Category = "Sherpa|Capture")
	void StartCapturing();

	UFUNCTION(BlueprintCallable, Category = "Sherpa|Capture")
	void StopCapturing();

	bool IsCapturing() const { return bCapturing; }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	TArray<FAudioConsumer> Consumers;
	FCriticalSection ConsumersLock;
	std::atomic<bool> bCapturing{false};
	int32 CaptureSampleRate = 48000;
};
