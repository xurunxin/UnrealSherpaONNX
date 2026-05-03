#include "SherpaONNXModule.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

#if WITH_SHERPA_ONNX
#include "Kws/KwsWorker.h"
#include "SherpaKws/SherpaKwsNative.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSherpaONNX, Log, All);

void FSherpaONNXModule::StartupModule()
{
#if WITH_SHERPA_ONNX
	FCoreDelegates::OnHandleSystemError.AddRaw(this, &FSherpaONNXModule::ShutdownKwsWorkers);
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FSherpaONNXModule::ShutdownKwsWorkers);
	FCoreDelegates::OnPreExit.AddRaw(this, &FSherpaONNXModule::ShutdownKwsWorkers);

	if (!SherpaKws_LoadLibrary())
	{
		UE_LOG(LogSherpaONNX, Error,
			TEXT("Failed to load sherpa-onnx native library. "
				 "Place libsherpa-onnx-c-api + onnxruntime in the plugin ThirdParty/lib/<platform>/ directory."));
	}
	else
	{
		UE_LOG(LogSherpaONNX, Log, TEXT("sherpa-onnx KWS native library loaded successfully"));
	}
#endif
}

void FSherpaONNXModule::ShutdownModule()
{
#if WITH_SHERPA_ONNX
	FCoreDelegates::OnPreExit.RemoveAll(this);
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
	FCoreDelegates::OnHandleSystemError.RemoveAll(this);
	ShutdownKwsWorkers();
	SherpaKws_UnloadLibrary();
#endif
}

void FSherpaONNXModule::ShutdownKwsWorkers()
{
#if WITH_SHERPA_ONNX
	UE_LOG(LogSherpaONNX, Log, TEXT("Stopping all SherpaONNX KWS workers"));
	FKwsWorker::ShutdownAllWorkers();
#endif
}

bool FSherpaONNXModule::IsAvailable()
{
#if WITH_SHERPA_ONNX
	return SherpaKws_GetAPI().IsLoaded();
#else
	return false;
#endif
}

IMPLEMENT_MODULE(FSherpaONNXModule, SherpaONNX)
