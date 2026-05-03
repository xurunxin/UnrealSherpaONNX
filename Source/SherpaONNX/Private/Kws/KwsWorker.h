#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include "Kws/KwsTypes.h"

#if WITH_SHERPA_ONNX
#include "sherpa-onnx/c-api/c-api.h"
#endif

DECLARE_DELEGATE_OneParam(FOnKwsWorkerResult, const FString& /*Json*/);
DECLARE_DELEGATE(FOnKwsWorkerError);
DECLARE_DELEGATE(FOnKwsWorkerReady);

class FKwsWorker : public FRunnable
{
public:
	FKwsWorker(const FSherpaKwsModelConfig& Config);
	virtual ~FKwsWorker() override;

	bool Start();
	void Shutdown();
	static void ShutdownAllWorkers();

	void PushAudio(const TArray<float>& Samples);
	void SetKeywords(const FString& Keywords);

	bool IsRunning() const { return bRunning && bInitialized && !bFailed; }

	FOnKwsWorkerResult OnResult;
	FOnKwsWorkerError  OnError;
	FOnKwsWorkerReady  OnReady;

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override { bStopRequested = true; }

private:
	FSherpaKwsModelConfig ModelConfig;

	const SherpaOnnxKeywordSpotter* Spotter = nullptr;
	const SherpaOnnxOnlineStream*    Stream  = nullptr;
	FCriticalSection SpotterLock;   // guards Spotter + Stream across threads

	TQueue<TArray<float>> AudioQueue;

	FRunnableThread* Thread = nullptr;
	std::atomic<bool> bRunning{false};
	std::atomic<bool> bInitialized{false};
	std::atomic<bool> bFailed{false};
	std::atomic<bool> bStopRequested{false};
	std::atomic<bool> bNativeThreadAborted{false};
	int64 ProcessedAudioChunks = 0;
	int64 ProcessedAudioSamples = 0;
	int64 NextAudioLogSample = 16000;
	int64 DecodeCount = 0;
	int64 EmptyResultCount = 0;
	int64 NextDecodeLogCount = 50;

	bool CreateSpotter();
	void DestroySpotter();
	void ProcessAudio(const TArray<float>& Chunk);
	void CheckForKeywords();
};
