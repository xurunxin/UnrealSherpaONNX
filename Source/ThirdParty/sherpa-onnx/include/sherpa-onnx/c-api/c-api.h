// sherpa-onnx/c-api/c-api.h — KWS subset
// Exact struct layout match with sherpa-onnx v1.13 C ABI (commit c6691594)
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #if defined(SHERPA_ONNX_BUILD_SHARED_LIBS)
    #define SHERPA_ONNX_API __declspec(dllexport)
  #elif defined(SHERPA_ONNX_LINK_STATIC)
    #define SHERPA_ONNX_API
  #else
    #define SHERPA_ONNX_API __declspec(dllimport)
  #endif
#else
  #define SHERPA_ONNX_API __attribute__((visibility("default")))
#endif

// ============================================================
// Model config sub-structs
// ============================================================
typedef struct SherpaOnnxOnlineTransducerModelConfig {
  const char *encoder;
  const char *decoder;
  const char *joiner;
} SherpaOnnxOnlineTransducerModelConfig;

typedef struct SherpaOnnxOnlineParaformerModelConfig {
  const char *encoder;
  const char *decoder;
} SherpaOnnxOnlineParaformerModelConfig;

typedef struct SherpaOnnxOnlineZipformer2CtcModelConfig {
  const char *model;
} SherpaOnnxOnlineZipformer2CtcModelConfig;

typedef struct SherpaOnnxOnlineNemoCtcModelConfig {
  const char *model;
} SherpaOnnxOnlineNemoCtcModelConfig;

typedef struct SherpaOnnxOnlineToneCtcModelConfig {
  const char *model;
} SherpaOnnxOnlineToneCtcModelConfig;

// ============================================================
// Model config
// ============================================================
typedef struct SherpaOnnxOnlineModelConfig {
  SherpaOnnxOnlineTransducerModelConfig transducer;
  SherpaOnnxOnlineParaformerModelConfig paraformer;
  SherpaOnnxOnlineZipformer2CtcModelConfig zipformer2_ctc;
  const char *tokens;
  int32_t num_threads;
  const char *provider;
  int32_t debug;
  const char *model_type;
  const char *modeling_unit;
  const char *bpe_vocab;
  const char *tokens_buf;
  int32_t tokens_buf_size;
  SherpaOnnxOnlineNemoCtcModelConfig nemo_ctc;
  SherpaOnnxOnlineToneCtcModelConfig t_one_ctc;
} SherpaOnnxOnlineModelConfig;

// ============================================================
// Feature config
// ============================================================
typedef struct SherpaOnnxFeatureConfig {
  int32_t sample_rate;
  int32_t feature_dim;
} SherpaOnnxFeatureConfig;

// ============================================================
// Keyword spotter config
// ============================================================
typedef struct SherpaOnnxKeywordSpotterConfig {
  SherpaOnnxFeatureConfig        feat_config;
  SherpaOnnxOnlineModelConfig    model_config;
  int32_t                        max_active_paths;
  int32_t                        num_trailing_blanks;
  float                          keywords_score;
  float                          keywords_threshold;
  const char                    *keywords_file;
  const char                    *keywords_buf;
  int32_t                        keywords_buf_size;
} SherpaOnnxKeywordSpotterConfig;

// ============================================================
// Keyword result
// ============================================================
typedef struct SherpaOnnxKeywordResult {
  const char  *keyword;
  const char  *tokens;
  const char *const *tokens_arr;
  int32_t      count;
  float       *timestamps;
  float        start_time;
  const char  *json;
} SherpaOnnxKeywordResult;

// ============================================================
// Opaque handles
// ============================================================
typedef struct SherpaOnnxKeywordSpotter SherpaOnnxKeywordSpotter;
typedef struct SherpaOnnxOnlineStream    SherpaOnnxOnlineStream;

// ============================================================
// Functions
// ============================================================
SHERPA_ONNX_API const SherpaOnnxKeywordSpotter*
SherpaOnnxCreateKeywordSpotter(const SherpaOnnxKeywordSpotterConfig *config);

SHERPA_ONNX_API void
SherpaOnnxDestroyKeywordSpotter(const SherpaOnnxKeywordSpotter *spotter);

SHERPA_ONNX_API const SherpaOnnxOnlineStream*
SherpaOnnxCreateKeywordStream(const SherpaOnnxKeywordSpotter *spotter);

SHERPA_ONNX_API const SherpaOnnxOnlineStream*
SherpaOnnxCreateKeywordStreamWithKeywords(
    const SherpaOnnxKeywordSpotter *spotter, const char *keywords);

SHERPA_ONNX_API void
SherpaOnnxOnlineStreamAcceptWaveform(
    const SherpaOnnxOnlineStream *stream,
    int32_t sample_rate,
    const float *samples,
    int32_t n);

SHERPA_ONNX_API void
SherpaOnnxOnlineStreamInputFinished(const SherpaOnnxOnlineStream *stream);

SHERPA_ONNX_API int32_t
SherpaOnnxIsKeywordStreamReady(
    const SherpaOnnxKeywordSpotter *spotter,
    const SherpaOnnxOnlineStream *stream);

SHERPA_ONNX_API void
SherpaOnnxDecodeKeywordStream(
    const SherpaOnnxKeywordSpotter *spotter,
    const SherpaOnnxOnlineStream *stream);

SHERPA_ONNX_API void
SherpaOnnxResetKeywordStream(
    const SherpaOnnxKeywordSpotter *spotter,
    const SherpaOnnxOnlineStream *stream);

SHERPA_ONNX_API const SherpaOnnxKeywordResult*
SherpaOnnxGetKeywordResult(
    const SherpaOnnxKeywordSpotter *spotter,
    const SherpaOnnxOnlineStream *stream);

SHERPA_ONNX_API void
SherpaOnnxDestroyKeywordResult(const SherpaOnnxKeywordResult *r);

SHERPA_ONNX_API const char*
SherpaOnnxGetKeywordResultAsJson(
    const SherpaOnnxKeywordSpotter *spotter,
    const SherpaOnnxOnlineStream *stream);

SHERPA_ONNX_API void
SherpaOnnxFreeKeywordResultJson(const char *s);

// ============================================================
// VAD
// ============================================================

typedef struct SherpaOnnxSileroVadModelConfig {
  const char *model;
  float threshold;
  float min_silence_duration;
  float min_speech_duration;
  int32_t window_size;
  float max_speech_duration;
} SherpaOnnxSileroVadModelConfig;

typedef struct SherpaOnnxTenVadModelConfig {
  const char *model;
  float threshold;
  float min_silence_duration;
  float min_speech_duration;
  int32_t window_size;
  float max_speech_duration;
} SherpaOnnxTenVadModelConfig;

typedef struct SherpaOnnxVadModelConfig {
  SherpaOnnxSileroVadModelConfig silero_vad;
  int32_t sample_rate;
  int32_t num_threads;
  const char *provider;
  int32_t debug;
  SherpaOnnxTenVadModelConfig ten_vad;
} SherpaOnnxVadModelConfig;

typedef struct SherpaOnnxSpeechSegment {
  int32_t start;
  float *samples;
  int32_t n;
} SherpaOnnxSpeechSegment;

typedef struct SherpaOnnxVoiceActivityDetector SherpaOnnxVoiceActivityDetector;

SHERPA_ONNX_API const SherpaOnnxVoiceActivityDetector*
SherpaOnnxCreateVoiceActivityDetector(const SherpaOnnxVadModelConfig *config,
                                      float buffer_size_in_seconds);

SHERPA_ONNX_API void
SherpaOnnxDestroyVoiceActivityDetector(const SherpaOnnxVoiceActivityDetector *p);

SHERPA_ONNX_API void
SherpaOnnxVoiceActivityDetectorAcceptWaveform(
    const SherpaOnnxVoiceActivityDetector *p, const float *samples, int32_t n);

SHERPA_ONNX_API int32_t
SherpaOnnxVoiceActivityDetectorEmpty(const SherpaOnnxVoiceActivityDetector *p);

SHERPA_ONNX_API int32_t
SherpaOnnxVoiceActivityDetectorDetected(const SherpaOnnxVoiceActivityDetector *p);

SHERPA_ONNX_API void
SherpaOnnxVoiceActivityDetectorPop(const SherpaOnnxVoiceActivityDetector *p);

SHERPA_ONNX_API void
SherpaOnnxVoiceActivityDetectorClear(const SherpaOnnxVoiceActivityDetector *p);

SHERPA_ONNX_API const SherpaOnnxSpeechSegment*
SherpaOnnxVoiceActivityDetectorFront(const SherpaOnnxVoiceActivityDetector *p);

SHERPA_ONNX_API void
SherpaOnnxDestroySpeechSegment(const SherpaOnnxSpeechSegment *p);

SHERPA_ONNX_API void
SherpaOnnxVoiceActivityDetectorReset(const SherpaOnnxVoiceActivityDetector *p);

SHERPA_ONNX_API void
SherpaOnnxVoiceActivityDetectorFlush(const SherpaOnnxVoiceActivityDetector *p);

#ifdef __cplusplus
}
#endif
