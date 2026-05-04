#pragma once

#include "CoreMinimal.h"

#if WITH_SHERPA_ONNX
#include "sherpa-onnx/c-api/c-api.h"
#endif

using Fn_KwsCreate   = const SherpaOnnxKeywordSpotter* (*)(const SherpaOnnxKeywordSpotterConfig*);
using Fn_KwsDestroy  = void (*)(const SherpaOnnxKeywordSpotter*);
using Fn_StreamCreate = const SherpaOnnxOnlineStream* (*)(const SherpaOnnxKeywordSpotter*);
using Fn_StreamCreateWithKW = const SherpaOnnxOnlineStream* (*)(const SherpaOnnxKeywordSpotter*, const char*);
using Fn_AcceptWave  = void (*)(const SherpaOnnxOnlineStream*, int32_t, const float*, int32_t);
using Fn_InputDone   = void (*)(const SherpaOnnxOnlineStream*);
using Fn_IsReady     = int32_t (*)(const SherpaOnnxKeywordSpotter*, const SherpaOnnxOnlineStream*);
using Fn_Decode      = void (*)(const SherpaOnnxKeywordSpotter*, const SherpaOnnxOnlineStream*);
using Fn_Reset       = void (*)(const SherpaOnnxKeywordSpotter*, const SherpaOnnxOnlineStream*);
using Fn_GetResult   = const SherpaOnnxKeywordResult* (*)(const SherpaOnnxKeywordSpotter*, const SherpaOnnxOnlineStream*);
using Fn_DestroyResult = void (*)(const SherpaOnnxKeywordResult*);
using Fn_GetResultJson = const char* (*)(const SherpaOnnxKeywordSpotter*, const SherpaOnnxOnlineStream*);
using Fn_FreeResultJson = void (*)(const char*);

// VAD function pointer types
using Fn_VadCreate         = const SherpaOnnxVoiceActivityDetector* (*)(const SherpaOnnxVadModelConfig*, float);
using Fn_VadDestroy        = void (*)(const SherpaOnnxVoiceActivityDetector*);
using Fn_VadAcceptWaveform = void (*)(const SherpaOnnxVoiceActivityDetector*, const float*, int32_t);
using Fn_VadEmpty          = int32_t (*)(const SherpaOnnxVoiceActivityDetector*);
using Fn_VadDetected       = int32_t (*)(const SherpaOnnxVoiceActivityDetector*);
using Fn_VadPop            = void (*)(const SherpaOnnxVoiceActivityDetector*);
using Fn_VadClear          = void (*)(const SherpaOnnxVoiceActivityDetector*);
using Fn_VadFront          = const SherpaOnnxSpeechSegment* (*)(const SherpaOnnxVoiceActivityDetector*);
using Fn_VadDestroySegment = void (*)(const SherpaOnnxSpeechSegment*);
using Fn_VadReset          = void (*)(const SherpaOnnxVoiceActivityDetector*);
using Fn_VadFlush          = void (*)(const SherpaOnnxVoiceActivityDetector*);

// ASR function pointer types
using Fn_AsrCreate          = const SherpaOnnxOnlineRecognizer* (*)(const SherpaOnnxOnlineRecognizerConfig*);
using Fn_AsrDestroy         = void (*)(const SherpaOnnxOnlineRecognizer*);
using Fn_AsrCreateStream    = const SherpaOnnxOnlineStream* (*)(const SherpaOnnxOnlineRecognizer*);
using Fn_AsrCreateStreamHW = const SherpaOnnxOnlineStream* (*)(const SherpaOnnxOnlineRecognizer*, const char*);
using Fn_AsrDestroyStream   = void (*)(const SherpaOnnxOnlineStream*);
using Fn_AsrIsReady         = int32_t (*)(const SherpaOnnxOnlineRecognizer*, const SherpaOnnxOnlineStream*);
using Fn_AsrDecode          = void (*)(const SherpaOnnxOnlineRecognizer*, const SherpaOnnxOnlineStream*);
using Fn_AsrGetResult       = const SherpaOnnxOnlineRecognizerResult* (*)(const SherpaOnnxOnlineRecognizer*, const SherpaOnnxOnlineStream*);
using Fn_AsrDestroyResult   = void (*)(const SherpaOnnxOnlineRecognizerResult*);
using Fn_AsrGetResultJson   = const char* (*)(const SherpaOnnxOnlineRecognizer*, const SherpaOnnxOnlineStream*);
using Fn_AsrDestroyResultJson = void (*)(const char*);
using Fn_AsrReset           = void (*)(const SherpaOnnxOnlineRecognizer*, const SherpaOnnxOnlineStream*);
using Fn_AsrIsEndpoint      = int32_t (*)(const SherpaOnnxOnlineRecognizer*, const SherpaOnnxOnlineStream*);

struct SHERPAONNX_API FKwsNativeAPI
{
	Fn_KwsCreate       CreateKeywordSpotter   = nullptr;
	Fn_KwsDestroy      DestroyKeywordSpotter  = nullptr;
	Fn_StreamCreate    CreateKeywordStream    = nullptr;
	Fn_StreamCreateWithKW CreateKeywordStreamWithKeywords = nullptr;
	Fn_AcceptWave      AcceptWaveform         = nullptr;
	Fn_InputDone       InputFinished          = nullptr;
	Fn_IsReady         IsReady                = nullptr;
	Fn_Decode          Decode                 = nullptr;
	Fn_Reset           Reset                  = nullptr;
	Fn_GetResult       GetResult              = nullptr;
	Fn_DestroyResult   DestroyResult          = nullptr;
	Fn_GetResultJson   GetResultJson          = nullptr;
	Fn_FreeResultJson  FreeResultJson         = nullptr;

	// VAD
	Fn_VadCreate          CreateVoiceActivityDetector  = nullptr;
	Fn_VadDestroy         DestroyVoiceActivityDetector = nullptr;
	Fn_VadAcceptWaveform  VadAcceptWaveform            = nullptr;
	Fn_VadEmpty           VadEmpty                     = nullptr;
	Fn_VadDetected        VadDetected                  = nullptr;
	Fn_VadPop             VadPop                       = nullptr;
	Fn_VadClear           VadClear                     = nullptr;
	Fn_VadFront           VadFront                     = nullptr;
	Fn_VadDestroySegment  DestroySpeechSegment         = nullptr;
	Fn_VadReset           VadReset                     = nullptr;
	Fn_VadFlush           VadFlush                     = nullptr;

	// ASR
	Fn_AsrCreate           CreateOnlineRecognizer       = nullptr;
	Fn_AsrDestroy          DestroyOnlineRecognizer      = nullptr;
	Fn_AsrCreateStream     CreateOnlineStream           = nullptr;
	Fn_AsrCreateStreamHW   CreateOnlineStreamWithHotwords = nullptr;
	Fn_AsrDestroyStream    DestroyOnlineStream          = nullptr;
	Fn_AsrIsReady          IsOnlineStreamReady          = nullptr;
	Fn_AsrDecode           DecodeOnlineStream           = nullptr;
	Fn_AsrGetResult        GetOnlineStreamResult        = nullptr;
	Fn_AsrDestroyResult    DestroyOnlineRecognizerResult = nullptr;
	Fn_AsrGetResultJson    GetOnlineStreamResultAsJson  = nullptr;
	Fn_AsrDestroyResultJson DestroyOnlineStreamResultJson = nullptr;
	Fn_AsrReset            OnlineStreamReset            = nullptr;
	Fn_AsrIsEndpoint       OnlineStreamIsEndpoint       = nullptr;

	bool IsLoaded() const { return CreateKeywordSpotter != nullptr; }
	bool IsVadLoaded() const { return CreateVoiceActivityDetector != nullptr; }
	bool IsAsrLoaded() const { return CreateOnlineRecognizer != nullptr; }
};

bool SherpaKws_LoadLibrary();
void SherpaKws_UnloadLibrary();
FKwsNativeAPI& SherpaKws_GetAPI();
