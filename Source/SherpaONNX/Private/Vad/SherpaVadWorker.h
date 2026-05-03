#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include "Vad/SherpaVadTypes.h"

#include <string>

struct SherpaOnnxVoiceActivityDetector;

class FSherpaVadWorker : public FRunnable
{
public:
	explicit FSherpaVadWorker(const FSherpaVadConfig& InConfig);
	virtual ~FSherpaVadWorker() override;

	bool Start();
	void RequestStopAndWait();

	void PushAudio(const TArray<float>& InSamples);
	void Reset();

	bool IsRunning() const;
	bool IsSpeechDetected() const;

	TFunction<void()> OnSpeechStart;
	TFunction<void()> OnSpeechEnd;
	TFunction<void(const FSherpaVadSegment&)> OnSegmentReady;
	TFunction<void(const FString&)> OnError;

	virtual uint32 Run() override;
	virtual void Stop() override { bStopRequested = true; }

private:
	bool InitVad();
	void DestroyVad();
	void ProcessAudioBlock(const TArray<float>& Samples);
	void PollSegments();
	void SetSpeechState(bool bNewDetected);
	void EmitError(const FString& Error);

	FSherpaVadConfig Config;

	FRunnableThread* Thread = nullptr;

	FThreadSafeBool bStopRequested = false;
	FThreadSafeBool bRunning = false;
	FThreadSafeBool bSpeechDetected = false;

	TQueue<TArray<float>, EQueueMode::Mpsc> AudioQueue;

	const SherpaOnnxVoiceActivityDetector* Vad = nullptr;

	std::string ModelPathUtf8;
	std::string ProviderUtf8;
};
