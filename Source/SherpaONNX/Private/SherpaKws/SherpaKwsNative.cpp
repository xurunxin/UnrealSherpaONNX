#include "SherpaKws/SherpaKwsNative.h"

#if WITH_SHERPA_ONNX

DEFINE_LOG_CATEGORY_STATIC(LogSherpaKwsNative, Log, All);

static void* G_DllHandle = nullptr;
static void* G_OrtDllHandle = nullptr;
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

	G_API.CreateVoiceActivityDetector    = &SherpaOnnxCreateVoiceActivityDetector;
	G_API.DestroyVoiceActivityDetector   = &SherpaOnnxDestroyVoiceActivityDetector;
	G_API.VadAcceptWaveform              = &SherpaOnnxVoiceActivityDetectorAcceptWaveform;
	G_API.VadEmpty                       = &SherpaOnnxVoiceActivityDetectorEmpty;
	G_API.VadDetected                    = &SherpaOnnxVoiceActivityDetectorDetected;
	G_API.VadPop                         = &SherpaOnnxVoiceActivityDetectorPop;
	G_API.VadClear                       = &SherpaOnnxVoiceActivityDetectorClear;
	G_API.VadFront                       = &SherpaOnnxVoiceActivityDetectorFront;
	G_API.DestroySpeechSegment           = &SherpaOnnxDestroySpeechSegment;
	G_API.VadReset                       = &SherpaOnnxVoiceActivityDetectorReset;
	G_API.VadFlush                       = &SherpaOnnxVoiceActivityDetectorFlush;

	G_API.CreateOnlineRecognizer          = &SherpaOnnxCreateOnlineRecognizer;
	G_API.DestroyOnlineRecognizer         = &SherpaOnnxDestroyOnlineRecognizer;
	G_API.CreateOnlineStream              = &SherpaOnnxCreateOnlineStream;
	G_API.CreateOnlineStreamWithHotwords  = &SherpaOnnxCreateOnlineStreamWithHotwords;
	G_API.DestroyOnlineStream             = &SherpaOnnxDestroyOnlineStream;
	G_API.IsOnlineStreamReady             = &SherpaOnnxIsOnlineStreamReady;
	G_API.DecodeOnlineStream              = &SherpaOnnxDecodeOnlineStream;
	G_API.GetOnlineStreamResult           = &SherpaOnnxGetOnlineStreamResult;
	G_API.DestroyOnlineRecognizerResult   = &SherpaOnnxDestroyOnlineRecognizerResult;
	G_API.GetOnlineStreamResultAsJson     = &SherpaOnnxGetOnlineStreamResultAsJson;
	G_API.DestroyOnlineStreamResultJson   = &SherpaOnnxDestroyOnlineStreamResultJson;
	G_API.OnlineStreamReset               = &SherpaOnnxOnlineStreamReset;
	G_API.OnlineStreamIsEndpoint          = &SherpaOnnxOnlineStreamIsEndpoint;
	return G_API.IsLoaded() && G_API.IsVadLoaded() && G_API.IsAsrLoaded();
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
#include "SherpaKws/SherpaKwsNativeLoader.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr uint32 RequiredOrtApiVersion = 24;

bool G_OwnsDllHandle = false;
bool G_OwnsOrtDllHandle = false;

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

	return ApiBase->GetApi && ApiBase->GetApi(RequiredOrtApiVersion) != nullptr;
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

#if PLATFORM_WINDOWS
bool TryGetLoadedModulePath(void* DllHandle, FString& OutPath)
{
	if (!DllHandle)
	{
		return false;
	}

	TArray<TCHAR> Buffer;
	Buffer.SetNumUninitialized(512);
	for (;;)
	{
		const DWORD Length = ::GetModuleFileNameW(
			static_cast<HMODULE>(DllHandle),
			Buffer.GetData(),
			static_cast<DWORD>(Buffer.Num()));
		if (Length == 0)
		{
			return false;
		}
		if (Length < static_cast<DWORD>(Buffer.Num()))
		{
			OutPath = FString(static_cast<int32>(Length), Buffer.GetData());
			return true;
		}
		if (Buffer.Num() >= 32768)
		{
			return false;
		}
		Buffer.SetNumUninitialized(FMath::Min(Buffer.Num() * 2, 32768));
	}
}

void ReleaseModule(void*& DllHandle, bool& bOwnsHandle)
{
	if (DllHandle && bOwnsHandle)
	{
		::FreeLibrary(static_cast<HMODULE>(DllHandle));
	}
	DllHandle = nullptr;
	bOwnsHandle = false;
}

void ResetDynamicState()
{
	G_API = FKwsNativeAPI{};
	ReleaseModule(G_DllHandle, G_OwnsDllHandle);
	ReleaseModule(G_OrtDllHandle, G_OwnsOrtDllHandle);
}

bool AcquireExpectedModule(
	const TCHAR* LibraryName,
	const FString& ExpectedPath,
	bool bRequireOrtCompatibility,
	void*& OutHandle,
	bool& bOutOwnsHandle,
	FString* OutRuntimeVersion = nullptr)
{
	using namespace SherpaKwsNativeLoader;

	OutHandle = nullptr;
	bOutOwnsHandle = false;

	HMODULE ExistingModule = ::GetModuleHandleW(LibraryName);
	if (ExistingModule)
	{
		HMODULE ReferencedModule = nullptr;
		if (!::GetModuleHandleExW(0, LibraryName, &ReferencedModule) || !ReferencedModule)
		{
			UE_LOG(LogSherpaKwsNative, Error,
				TEXT("Found already-loaded %s, but failed to acquire a protected module reference (Windows error %u)."),
				LibraryName,
				static_cast<uint32>(::GetLastError()));
			return false;
		}
		if (ReferencedModule != ExistingModule)
		{
			UE_LOG(LogSherpaKwsNative, Error,
				TEXT("The loaded %s changed while acquiring a protected reference. Refusing to continue."),
				LibraryName);
			::FreeLibrary(ReferencedModule);
			return false;
		}

		FLoadedModuleState State;
		State.bIsLoaded = true;
		State.bPathAvailable = TryGetLoadedModulePath(ReferencedModule, State.Path);

		FString RuntimeVersion = TEXT("unknown");
		if (bRequireOrtCompatibility)
		{
			State.bCompatible = IsOnnxRuntimeCompatible(ReferencedModule, RuntimeVersion);
		}

		const ELoadedModuleDecision Decision = DecideLoadedModule(
			State,
			ExpectedPath,
			bRequireOrtCompatibility);
		if (Decision == ELoadedModuleDecision::AcquireExisting)
		{
			OutHandle = ReferencedModule;
			bOutOwnsHandle = true;
			if (OutRuntimeVersion)
			{
				*OutRuntimeVersion = RuntimeVersion;
			}
			if (bRequireOrtCompatibility)
			{
				if (AreSameWindowsModulePath(State.Path, ExpectedPath))
				{
					UE_LOG(LogSherpaKwsNative, Log,
						TEXT("Acquired a protected reference to already-loaded %s version %s from %s"),
						LibraryName,
						*RuntimeVersion,
						*State.Path);
				}
				else
				{
					UE_LOG(LogSherpaKwsNative, Warning,
						TEXT("Acquired a protected reference to compatible already-loaded %s version %s from %s instead of bundled %s. API %u is available."),
						LibraryName,
						*RuntimeVersion,
						*State.Path,
						*ExpectedPath,
						RequiredOrtApiVersion);
				}
			}
			else
			{
				UE_LOG(LogSherpaKwsNative, Log,
					TEXT("Acquired a protected reference to already-loaded %s from %s"),
					LibraryName,
					*State.Path);
			}
			return true;
		}

		switch (Decision)
		{
		case ELoadedModuleDecision::RejectPathUnavailable:
			UE_LOG(LogSherpaKwsNative, Error,
				TEXT("Refusing to load %s because a same-name module is already loaded but its path cannot be inspected. Expected: %s"),
				LibraryName,
				*ExpectedPath);
			break;

		case ELoadedModuleDecision::RejectPathMismatch:
			UE_LOG(LogSherpaKwsNative, Error,
				TEXT("Refusing to load %s because a conflicting same-name module is already loaded from %s. Expected: %s"),
				LibraryName,
				*State.Path,
				*ExpectedPath);
			break;

		case ELoadedModuleDecision::RejectIncompatible:
			UE_LOG(LogSherpaKwsNative, Error,
				TEXT("sherpa-onnx-c-api requires ONNX Runtime API %u, but already-loaded %s is version %s from %s."),
				RequiredOrtApiVersion,
				LibraryName,
				*RuntimeVersion,
				*State.Path);
			break;

		case ELoadedModuleDecision::AcquireExisting:
		case ELoadedModuleDecision::LoadExpected:
			UE_LOG(LogSherpaKwsNative, Error,
				TEXT("Unexpected module decision for already-loaded %s."),
				LibraryName);
			break;
		}

		::FreeLibrary(ReferencedModule);
		return false;
	}

	HMODULE LoadedModule = ::LoadLibraryW(*ExpectedPath);
	if (!LoadedModule)
	{
		UE_LOG(LogSherpaKwsNative, Error,
			TEXT("Failed to load %s from %s (Windows error %u)"),
			LibraryName,
			*ExpectedPath,
			static_cast<uint32>(::GetLastError()));
		return false;
	}

	OutHandle = LoadedModule;
	bOutOwnsHandle = true;

	FString ActualPath;
	if (!TryGetLoadedModulePath(LoadedModule, ActualPath))
	{
		UE_LOG(LogSherpaKwsNative, Error,
			TEXT("Loaded %s, but its actual path cannot be inspected. Expected: %s"),
			LibraryName,
			*ExpectedPath);
		ReleaseModule(OutHandle, bOutOwnsHandle);
		return false;
	}
	if (!AreSameWindowsModulePath(ActualPath, ExpectedPath))
	{
		UE_LOG(LogSherpaKwsNative, Error,
			TEXT("Windows bound %s to %s instead of the expected %s. Refusing to continue."),
			LibraryName,
			*ActualPath,
			*ExpectedPath);
		ReleaseModule(OutHandle, bOutOwnsHandle);
		return false;
	}

	if (bRequireOrtCompatibility)
	{
		FString RuntimeVersion = TEXT("unknown");
		if (!IsOnnxRuntimeCompatible(LoadedModule, RuntimeVersion))
		{
			UE_LOG(LogSherpaKwsNative, Error,
				TEXT("sherpa-onnx-c-api requires ONNX Runtime API %u, but %s is version %s from %s."),
				RequiredOrtApiVersion,
				LibraryName,
				*RuntimeVersion,
				*ActualPath);
			ReleaseModule(OutHandle, bOutOwnsHandle);
			return false;
		}
		if (OutRuntimeVersion)
		{
			*OutRuntimeVersion = RuntimeVersion;
		}
	}

	return true;
}
#endif
}

bool SherpaKws_LoadLibrary()
{
	const FString OrtPath = GetWin64LibraryPath(TEXT("onnxruntime.dll"));
	const FString SherpaPath = GetWin64LibraryPath(TEXT("sherpa-onnx-c-api.dll"));

#if PLATFORM_WINDOWS
	if (G_DllHandle && G_API.IsLoaded() && G_API.IsVadLoaded() && G_API.IsAsrLoaded())
	{
		return true;
	}
	ResetDynamicState();

	FString OrtRuntimeVersion = TEXT("unknown");
	if (!AcquireExpectedModule(
			TEXT("onnxruntime.dll"),
			OrtPath,
			true,
			G_OrtDllHandle,
			G_OwnsOrtDllHandle,
			&OrtRuntimeVersion))
	{
		ResetDynamicState();
		return false;
	}

	if (!AcquireExpectedModule(
			TEXT("sherpa-onnx-c-api.dll"),
			SherpaPath,
			false,
			G_DllHandle,
			G_OwnsDllHandle))
	{
		ResetDynamicState();
		return false;
	}
#else
	G_OrtDllHandle = FPlatformProcess::GetDllHandle(*OrtPath);
	G_DllHandle = FPlatformProcess::GetDllHandle(*SherpaPath);
	if (!G_OrtDllHandle || !G_DllHandle)
	{
		SherpaKws_UnloadLibrary();
		return false;
	}
#endif

	bool bAllExportsLoaded = true;
	auto Load = [&](const TCHAR* Name) -> void*
	{
		void* Export = FPlatformProcess::GetDllExport(G_DllHandle, Name);
		if (!Export)
		{
			UE_LOG(LogSherpaKwsNative, Error,
				TEXT("Required sherpa-onnx export is missing: %s (%s)"),
				Name,
				*SherpaPath);
			bAllExportsLoaded = false;
		}
		return Export;
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

	G_API.CreateVoiceActivityDetector     = (Fn_VadCreate)         Load(TEXT("SherpaOnnxCreateVoiceActivityDetector"));
	G_API.DestroyVoiceActivityDetector    = (Fn_VadDestroy)        Load(TEXT("SherpaOnnxDestroyVoiceActivityDetector"));
	G_API.VadAcceptWaveform               = (Fn_VadAcceptWaveform) Load(TEXT("SherpaOnnxVoiceActivityDetectorAcceptWaveform"));
	G_API.VadEmpty                        = (Fn_VadEmpty)          Load(TEXT("SherpaOnnxVoiceActivityDetectorEmpty"));
	G_API.VadDetected                     = (Fn_VadDetected)       Load(TEXT("SherpaOnnxVoiceActivityDetectorDetected"));
	G_API.VadPop                          = (Fn_VadPop)            Load(TEXT("SherpaOnnxVoiceActivityDetectorPop"));
	G_API.VadClear                        = (Fn_VadClear)          Load(TEXT("SherpaOnnxVoiceActivityDetectorClear"));
	G_API.VadFront                        = (Fn_VadFront)          Load(TEXT("SherpaOnnxVoiceActivityDetectorFront"));
	G_API.DestroySpeechSegment            = (Fn_VadDestroySegment) Load(TEXT("SherpaOnnxDestroySpeechSegment"));
	G_API.VadReset                        = (Fn_VadReset)           Load(TEXT("SherpaOnnxVoiceActivityDetectorReset"));
	G_API.VadFlush                        = (Fn_VadFlush)           Load(TEXT("SherpaOnnxVoiceActivityDetectorFlush"));

	G_API.CreateOnlineRecognizer          = (Fn_AsrCreate)          Load(TEXT("SherpaOnnxCreateOnlineRecognizer"));
	G_API.DestroyOnlineRecognizer         = (Fn_AsrDestroy)         Load(TEXT("SherpaOnnxDestroyOnlineRecognizer"));
	G_API.CreateOnlineStream              = (Fn_AsrCreateStream)    Load(TEXT("SherpaOnnxCreateOnlineStream"));
	G_API.CreateOnlineStreamWithHotwords  = (Fn_AsrCreateStreamHW)  Load(TEXT("SherpaOnnxCreateOnlineStreamWithHotwords"));
	G_API.DestroyOnlineStream             = (Fn_AsrDestroyStream)   Load(TEXT("SherpaOnnxDestroyOnlineStream"));
	G_API.IsOnlineStreamReady             = (Fn_AsrIsReady)         Load(TEXT("SherpaOnnxIsOnlineStreamReady"));
	G_API.DecodeOnlineStream              = (Fn_AsrDecode)          Load(TEXT("SherpaOnnxDecodeOnlineStream"));
	G_API.GetOnlineStreamResult           = (Fn_AsrGetResult)       Load(TEXT("SherpaOnnxGetOnlineStreamResult"));
	G_API.DestroyOnlineRecognizerResult   = (Fn_AsrDestroyResult)   Load(TEXT("SherpaOnnxDestroyOnlineRecognizerResult"));
	G_API.GetOnlineStreamResultAsJson     = (Fn_AsrGetResultJson)   Load(TEXT("SherpaOnnxGetOnlineStreamResultAsJson"));
	G_API.DestroyOnlineStreamResultJson   = (Fn_AsrDestroyResultJson) Load(TEXT("SherpaOnnxDestroyOnlineStreamResultJson"));
	G_API.OnlineStreamReset               = (Fn_AsrReset)           Load(TEXT("SherpaOnnxOnlineStreamReset"));
	G_API.OnlineStreamIsEndpoint          = (Fn_AsrIsEndpoint)      Load(TEXT("SherpaOnnxOnlineStreamIsEndpoint"));

	if (!bAllExportsLoaded || !G_API.IsLoaded() || !G_API.IsVadLoaded() || !G_API.IsAsrLoaded())
	{
		SherpaKws_UnloadLibrary();
		return false;
	}
	return true;
}

void SherpaKws_UnloadLibrary()
{
#if PLATFORM_WINDOWS
	ResetDynamicState();
#else
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
#endif
}
#endif

FKwsNativeAPI& SherpaKws_GetAPI()
{
	return G_API;
}

#endif // WITH_SHERPA_ONNX
