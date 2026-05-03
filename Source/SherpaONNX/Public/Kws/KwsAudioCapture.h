#pragma once

#include "CoreMinimal.h"
#include "AudioCaptureComponent.h"
#include "KwsAudioCapture.generated.h"

class FKwsWorker;

UCLASS(ClassGroup=(SherpaONNX), meta=(BlueprintSpawnableComponent))
class SHERPAONNX_API UKwsAudioCapture : public UAudioCaptureComponent
{
	GENERATED_BODY()

public:
	UKwsAudioCapture(const FObjectInitializer& ObjectInitializer);

	void SetWorker(FKwsWorker* InWorker) { Worker = InWorker; }

	UFUNCTION(BlueprintCallable, Category = "Sherpa KWS|Capture")
	void StartCapturing();

	UFUNCTION(BlueprintCallable, Category = "Sherpa KWS|Capture")
	void StopCapturing();

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	FKwsWorker* Worker = nullptr;
	std::atomic<bool> bCapturing{false};
	int32 CaptureSampleRate = 16000;
	int64 CapturedFramesSinceLastLog = 0;
};
