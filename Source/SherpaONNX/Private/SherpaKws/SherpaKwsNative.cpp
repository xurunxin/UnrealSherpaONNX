#include "SherpaKws/SherpaKwsNative.h"

#if WITH_SHERPA_ONNX

static void* G_DllHandle = nullptr;
static void* G_OrtDllHandle = nullptr;
static void* G_OrtProvidersDllHandle = nullptr;
static FKwsNativeAPI G_API;

#if SHERPA_ONNX_LINK_STATIC
// Static linking — functions resolved at link time
bool SherpaKws_LoadLibrary()
{
	G_API.CreateKeywordSpotter            = &SherpaOnnxCreateKeywordSpotter;
	G_API.DestroyKeywordSpotter           = &SherpaOnnxDestroyKeywordSpotter;
	G_API.CreateKeywordStream             = &SherpaOnnxCreateKeywordStream;
	G_API.CreateKeywordStreamWithKeywords = &SherpaOnnxCreateKeywordStreamWithKeywords;
	G_API.AcceptWaveform                  = &SherpaOnnxOnlineStreamAcceptWaveform;
	G_API.InputFinished                   = &SherpaOnnxOnlineStreamInputFinished;
	G_API.IsReady                         = &SherpaOnnxIsKeywordStreamReady;
	G_API.Decode                          = &SherpaOnnxDecodeKeywordStream;
	G_API.Reset                           = &SherpaOnnxResetKeywordStream;
	G_API.GetResult                       = &SherpaOnnxGetKeywordResult;
	G_API.DestroyResult                   = &SherpaOnnxDestroyKeywordResult;
	G_API.GetResultJson                   = &SherpaOnnxGetKeywordResultAsJson;
	G_API.FreeResultJson                  = &SherpaOnnxFreeKeywordResultJson;
	return G_API.IsLoaded();
}

void SherpaKws_UnloadLibrary()
{
	G_API = FKwsNativeAPI{};
}
#else
// Dynamic loading — resolve via GetDllExport
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace
{
constexpr uint32 RequiredOrtApiVersion = 24;

struct FOrtApi;

struct FOrtApiBase
{
	const FOrtApi* (*GetApi)(uint32 Version);
	const char* (*GetVersionString)();
};

using Fn_OrtGetApiBase = const FOrtApiBase* (*)();

bool IsOnnxRuntimeCompatible(void* OrtDllHandle, FString& OutRuntimeVersion)
{
	if (!OrtDllHandle)
	{
		return false;
	}

	Fn_OrtGetApiBase GetApiBase = reinterpret_cast<Fn_OrtGetApiBase>(
		FPlatformProcess::GetDllExport(OrtDllHandle, TEXT("OrtGetApiBase")));

	if (!GetApiBase)
	{
		return false;
	}

	const FOrtApiBase* ApiBase = GetApiBase();
	if (!ApiBase)
	{
		return false;
	}

	if (ApiBase->GetVersionString)
	{
		OutRuntimeVersion = UTF8_TO_TCHAR(ApiBase->GetVersionString());
	}

	const bool bCompatible = ApiBase->GetApi && ApiBase->GetApi(RequiredOrtApiVersion) != nullptr;
	return bCompatible;
}

FString GetWin64LibraryPath(const TCHAR* LibraryName)
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SherpaONNX"));
	if (Plugin.IsValid())
	{
		return FPaths::ConvertRelativePathToFull(
			Plugin->GetBaseDir() / TEXT("Source/ThirdParty/sherpa-onnx/lib/Win64") / LibraryName);
	}

	return LibraryName;
}
}

bool SherpaKws_LoadLibrary()
{
	const FString OrtProvidersPath = GetWin64LibraryPath(TEXT("onnxruntime_providers_shared.dll"));
	G_OrtProvidersDllHandle = FPlatformProcess::GetDllHandle(*OrtProvidersPath);
	if (!G_OrtProvidersDllHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load ONNX Runtime provider library: %s"), *OrtProvidersPath);
		return false;
	}

	const FString OrtPath = GetWin64LibraryPath(TEXT("onnxruntime.dll"));
	G_OrtDllHandle = FPlatformProcess::GetDllHandle(*OrtPath);
	if (!G_OrtDllHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load ONNX Runtime library: %s"), *OrtPath);
		FPlatformProcess::FreeDllHandle(G_OrtProvidersDllHandle);
		G_OrtProvidersDllHandle = nullptr;
		return false;
	}

	FString OrtRuntimeVersion = TEXT("unknown");
	if (!IsOnnxRuntimeCompatible(G_OrtDllHandle, OrtRuntimeVersion))
	{
		UE_LOG(LogTemp, Error,
			TEXT("sherpa-onnx-c-api requires ONNX Runtime API %u, but loaded onnxruntime.dll is %s from %s. "
				 "Replace the plugin Win64 onnxruntime.dll with ONNX Runtime 1.24.x, or rebuild sherpa-onnx-c-api against the bundled runtime."),
			RequiredOrtApiVersion,
			*OrtRuntimeVersion,
			*OrtPath);

		FPlatformProcess::FreeDllHandle(G_OrtDllHandle);
		G_OrtDllHandle = nullptr;
		FPlatformProcess::FreeDllHandle(G_OrtProvidersDllHandle);
		G_OrtProvidersDllHandle = nullptr;
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Loaded ONNX Runtime %s from %s"), *OrtRuntimeVersion, *OrtPath);

	const FString SherpaPath = GetWin64LibraryPath(TEXT("sherpa-onnx-c-api.dll"));
	G_DllHandle = FPlatformProcess::GetDllHandle(*SherpaPath);
	if (!G_DllHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load sherpa-onnx native library: %s"), *SherpaPath);
		FPlatformProcess::FreeDllHandle(G_OrtDllHandle);
		G_OrtDllHandle = nullptr;
		FPlatformProcess::FreeDllHandle(G_OrtProvidersDllHandle);
		G_OrtProvidersDllHandle = nullptr;
		return false;
	}

	auto Load = [&](const TCHAR* Name) -> void* {
		return FPlatformProcess::GetDllExport(G_DllHandle, Name);
	};

	G_API.CreateKeywordSpotter            = (Fn_KwsCreate)        Load(TEXT("SherpaOnnxCreateKeywordSpotter"));
	G_API.DestroyKeywordSpotter           = (Fn_KwsDestroy)       Load(TEXT("SherpaOnnxDestroyKeywordSpotter"));
	G_API.CreateKeywordStream             = (Fn_StreamCreate)     Load(TEXT("SherpaOnnxCreateKeywordStream"));
	G_API.CreateKeywordStreamWithKeywords = (Fn_StreamCreateWithKW) Load(TEXT("SherpaOnnxCreateKeywordStreamWithKeywords"));
	G_API.AcceptWaveform                  = (Fn_AcceptWave)       Load(TEXT("SherpaOnnxOnlineStreamAcceptWaveform"));
	G_API.InputFinished                   = (Fn_InputDone)        Load(TEXT("SherpaOnnxOnlineStreamInputFinished"));
	G_API.IsReady                         = (Fn_IsReady)          Load(TEXT("SherpaOnnxIsKeywordStreamReady"));
	G_API.Decode                          = (Fn_Decode)           Load(TEXT("SherpaOnnxDecodeKeywordStream"));
	G_API.Reset                           = (Fn_Reset)            Load(TEXT("SherpaOnnxResetKeywordStream"));
	G_API.GetResult                       = (Fn_GetResult)        Load(TEXT("SherpaOnnxGetKeywordResult"));
	G_API.DestroyResult                   = (Fn_DestroyResult)    Load(TEXT("SherpaOnnxDestroyKeywordResult"));
	G_API.GetResultJson                   = (Fn_GetResultJson)    Load(TEXT("SherpaOnnxGetKeywordResultAsJson"));
	G_API.FreeResultJson                  = (Fn_FreeResultJson)   Load(TEXT("SherpaOnnxFreeKeywordResultJson"));

	if (!G_API.IsLoaded())
	{
		FPlatformProcess::FreeDllHandle(G_DllHandle);
		G_DllHandle = nullptr;
		FPlatformProcess::FreeDllHandle(G_OrtDllHandle);
		G_OrtDllHandle = nullptr;
		FPlatformProcess::FreeDllHandle(G_OrtProvidersDllHandle);
		G_OrtProvidersDllHandle = nullptr;
		return false;
	}
	return true;
}

void SherpaKws_UnloadLibrary()
{
	G_API = FKwsNativeAPI{};
	if (G_DllHandle)
	{
		FPlatformProcess::FreeDllHandle(G_DllHandle);
		G_DllHandle = nullptr;
	}
	if (G_OrtDllHandle)
	{
		FPlatformProcess::FreeDllHandle(G_OrtDllHandle);
		G_OrtDllHandle = nullptr;
	}
	if (G_OrtProvidersDllHandle)
	{
		FPlatformProcess::FreeDllHandle(G_OrtProvidersDllHandle);
		G_OrtProvidersDllHandle = nullptr;
	}
}
#endif

FKwsNativeAPI& SherpaKws_GetAPI()
{
	return G_API;
}

#endif // WITH_SHERPA_ONNX
