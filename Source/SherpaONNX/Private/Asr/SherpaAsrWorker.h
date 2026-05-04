#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include "Asr/SherpaAsrTypes.h"

#include <string>

struct SherpaOnnxOnlineRecognizer;
struct SherpaOnnxOnlineStream;

class FSherpaAsrWorker : public FRunnable
{
public:
	explicit FSherpaAsrWorker(const FSherpaAsrConfig& InConfig);
	virtual ~FSherpaAsrWorker() override;

	bool Start();
	void RequestStopAndWait();

	void PushAudio(const TArray<float>& InSamples);
	void SetHotwords(const FString& Hotwords);

	bool IsRunning() const { return bRunning; }

	TFunction<void(const FSherpaAsrResult&)> OnPartialResult;
	TFunction<void(const FSherpaAsrResult&)> OnFinalResult;
	TFunction<void(const FString&)> OnError;

	virtual uint32 Run() override;
	virtual void Stop() override { bStopRequested = true; }

private:
	bool InitAsr();
	void DestroyAsr();
	void ProcessResults();
	void EmitError(const FString& Error);
	static FSherpaAsrResult BuildResult(const char* Json, const char* Text, bool bPartial, int32 SegmentId);

	FSherpaAsrConfig Config;

	FRunnableThread* Thread = nullptr;
	std::atomic<bool> bStopRequested{false};
	std::atomic<bool> bRunning{false};

	TQueue<TArray<float>, EQueueMode::Mpsc> AudioQueue;

	const SherpaOnnxOnlineRecognizer* Recognizer = nullptr;
	const SherpaOnnxOnlineStream*    Stream = nullptr;

	int32 SegmentIndex = 0;
	std::string EncoderPathUtf8;
	std::string DecoderPathUtf8;
	std::string JoinerPathUtf8;
	std::string TokensPathUtf8;
	std::string ProviderUtf8;
	std::string DecodingMethodUtf8;
	std::string ModelingUnitUtf8;
	std::string BpeVocabUtf8;
	std::string HotwordsBuf;  // config-level hotwords (owned string)
};
