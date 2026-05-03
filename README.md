# SherpaONNX — UE5.7 离线语音 AI 插件

基于 [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) (12k+ Stars) 的跨平台语音处理插件，支持 Windows / Linux / Android。

**功能模块**：

| 模块 | 模型 | 蓝图入口 | 统一 API |
|------|------|---------|---------|
| **KWS** (关键词唤醒) | Zipformer 3M (中英双语) | `UKwsWakeWordComponent` | `StartKWS` / `StopKWS` |
| **VAD** (语音活动检测) | Silero VAD (208KB int8) | `USherpaVadComponent` | `StartVAD` / `StopVAD` |

**统一音频采集**：`USherpaAudioCapture` 一次麦克风采集 + 重采样 → 16kHz mono → 同时供给 KWS 和 VAD，零冗余。

---

## 目录结构

```
Plugins/SherpaONNX/
├── SherpaONNX.uplugin
├── README.md
├── Content/
│   └── Models/
│       ├── sherpa-onnx-kws-zipformer-zh-en-3M-2025-12-20/  # KWS 中英双语模型
│       ├── Vad/Silero/silero_vad.int8.onnx                 # VAD 模型 (208KB)
│       └── keywords.txt                                    # 默认关键词
├── Resources/
└── Source/
    ├── SherpaONNX/
    │   ├── SherpaONNX.Build.cs
    │   ├── SherpaONNX_Android.xml
    │   ├── Public/
    │   │   ├── SherpaONNXModule.h
    │   │   ├── SherpaAudioCapture.h          # 统一音频采集（16kHz mono → KWS+VAD）
    │   │   ├── SherpaKws/SherpaKwsNative.h   # C ABI 函数指针表 (KWS + VAD)
    │   │   ├── Kws/
    │   │   │   ├── KwsTypes.h                 # KWS 配置结构体 & 预设枚举
    │   │   │   ├── KwsWakeWordComponent.h     # 关键词唤醒组件
    │   │   │   └── SherpaKwsLibrary.h         # 蓝图函数库（预设配置工厂）
    │   │   └── Vad/
    │   │       ├── SherpaVadTypes.h            # VAD 配置 & 结果结构体
    │   │       └── SherpaVadComponent.h        # 语音活动检测组件
    │   └── Private/
    │       ├── SherpaONNXModule.cpp
    │       ├── SherpaAudioCapture.cpp           # 统一采集实现（单声道+重采样+分发）
    │       ├── SherpaKws/SherpaKwsNative.cpp    # 动态/静态库加载
    │       ├── Kws/
    │       │   ├── KwsWorker.h/.cpp              # KWS 后台推理线程
    │       │   ├── KwsWakeWordComponent.cpp
    │       │   └── SherpaKwsLibrary.cpp
    │       └── Vad/
    │           ├── SherpaVadWorker.h/.cpp         # VAD 后台推理线程
    │           └── SherpaVadComponent.cpp
    └── ThirdParty/sherpa-onnx/
        ├── include/sherpa-onnx/c-api/c-api.h     # C ABI 声明 (KWS + VAD)
        └── lib/                                   # 预编译库（按平台）
            ├── Win64/
            ├── Linux/
            └── Android/
                ├── arm64-v8a/
                └── x86_64/
```

---

## 架构

```
蓝图层
┌──────────────────────────────────────────────────────────┐
│  UKwsWakeWordComponent         USherpaVadComponent        │
│  ├─ StartKWS(Config)           ├─ StartVAD()             │
│  ├─ SetKeywords("tk @word")    ├─ IsSpeechDetected()     │
│  └─ OnKeywordDetected          ├─ OnSpeechStart           │
│                                ├─ OnSpeechEnd             │
│                                └─ OnSpeechSegmentReady    │
└──────────┬───────────────────────┬───────────────────────┘
           │                       │
C++ 层     │                       │
┌──────────┴───────────────────────┴───────────────────────┐
│                USherpaAudioCapture                        │
│            (统一采集: mono + 16kHz 重采样)                  │
│                    ↓          ↓                           │
│               FKwsWorker    FSherpaVadWorker              │
│              (ONNX KWS)     (ONNX VAD)                    │
│                    ↓          ↓                           │
│  ┌─────────────────┴──────────┴─────────────┐            │
│  │            FKwsNativeAPI                  │            │
│  │       (C ABI FFI: KWS + VAD)              │            │
│  └─────────────────┬─────────────────────────┘            │
└────────────────────┼──────────────────────────────────────┘
                     │
Native 层            │
┌────────────────────┴──────────────────────┐
│  sherpa-onnx-c-api.dll/.so                │
│  └─ onnxruntime.dll/.so                   │
│       ├─ encoder/decoder/joiner.onnx (KWS)│
│       └─ silero_vad.int8.onnx (VAD)       │
└───────────────────────────────────────────┘
```

---

## 快速开始

### 环境要求

| 平台 | 状态 |
|------|------|
| Windows x64 (Editor/Packaged) | ✅ 已编译 DLL |
| Linux x64 | 需编译 native .so |
| Android arm64-v8a | 需编译 native .a |

---

## 一、关键词唤醒 (KWS)

### Blueprint 用法

```
1. 在任意 Actor 上添加 UKwsWakeWordComponent
2. Event BeginPlay → 调用:

   Make Preset Config
   ├─ Preset = "中英双语 | fp32 | 320ms 延迟"
   └─ KeywordsString = 粘贴关键词 token 序列

   → StartKWS(Config)    ← 自动开麦采集

3. 绑定 OnKeywordDetected → 获取 Result.Keyword
```

### 关键词格式

每条一行，格式为 `<token 序列> @<原始文本>`：

```
x iǎo ài tóng x ué @小爱同学
n ǐ hǎo jī qì rén @你好机器人
dǎ kāi cài dān @打开菜单
```

**生成 token 序列**：

```bash
pip install sherpa-onnx
sherpa-onnx-cli text2token \
  --tokens tokens.txt \
  --tokens-type phone+ppinyin \
  --lexicon en.phone \
  raw_keywords.txt keywords.txt
```

### KWS 模型预设

| 枚举 | 延迟 | 精度 | 大小 |
|------|------|------|------|
| `Bilingual_Fp32_320ms` | 320ms | 最高 | 13 MB |
| `Bilingual_Int8_320ms` | 320ms | 中等 | 5 MB |
| `Bilingual_Fp32_160ms` | 160ms | 高 | 13 MB |
| `Bilingual_Int8_160ms` | 160ms | 中等 | 5 MB |
| `Chinese_Fp32_320ms` | 320ms | 高 | ~18 MB |

### KWS C++ API

```cpp
auto Config = USherpaKwsLibrary::MakeBilingualPresetConfig(
    ESherpaKwsPreset::Bilingual_Fp32_320ms,
    TEXT("x iǎo ài tóng x ué @小爱同学"));

MyComponent->StartKWS(Config);
MyComponent->OnKeywordDetected.AddDynamic(this, &AMyActor::OnKwsDetected);
MyComponent->SetKeywords(TEXT("dǎ kāi cài dān @打开菜单"));  // 运行时换词
MyComponent->StopKWS();
```

### KWS 配置字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `EncoderPath` | FString | Zipformer 编码器 ONNX 模型路径 |
| `DecoderPath` | FString | 解码器 ONNX 模型路径 |
| `JoinerPath` | FString | Joiner 网络 ONNX 模型路径 |
| `TokensPath` | FString | token 映射表路径 |
| `KeywordsFile` | FString | 关键词文件路径（可选） |
| `KeywordsString` | FString | 关键词字符串（优先于文件） |
| `NumThreads` | int32 | 推理线程数，默认 1 |
| `Provider` | FString | 推理后端: `cpu` / `dml` / `cuda` |
| `MaxActivePaths` | int32 | 最大解码路径数，默认 4 |
| `NumTrailingBlanks` | int32 | 尾部空白帧阈值，默认 1 |
| `KeywordsScore` | float | 关键词加权分数，默认 1.0 |
| `KeywordsThreshold` | float | 检测阈值，默认 0.25 |

### KWS 事件

| 事件 | 类型 | 触发时机 |
|------|------|---------|
| `OnKeywordDetected` | `FSherpaKwsKeywordResult` | 检测到关键词 |
| `OnKwsReady` | 无参数 | 引擎初始化完成 |
| `OnKwsError` | `FString` | 发生错误 |

---

## 二、语音活动检测 (VAD)

### Blueprint 用法

```
1. 在任意 Actor 上添加 USherpaVadComponent
2. 在 Details 面板设置 Config.ModelPath = "…/silero_vad.int8.onnx"
3. Event BeginPlay → StartVAD()    ← 自动开麦采集（无需手动 PushAudioFloat）

4. 绑定事件：
   OnSpeechStart  → 开始说话
   OnSpeechEnd    → 停止说话
   OnSpeechSegmentReady → 完整语音片段（含 PCM 数据）

5. （可选）外部音频源可通过 PushAudioFloat 喂入
```

### VAD 配置字段

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `ModelType` | enum | Silero | VAD 模型类型（TEN VAD 预留） |
| `ModelPath` | FString | — | silero_vad.onnx 路径 |
| `SampleRate` | int32 | 16000 | 输入采样率 |
| `WindowSize` | int32 | 512 | 推理窗口大小（采样点数） |
| `Threshold` | float | 0.25 | 语音概率阈值 |
| `MinSilenceDuration` | float | 0.5 | 判定语音结束的最短静音时长（秒） |
| `MinSpeechDuration` | float | 0.25 | 保留语音段的最短持续时长（秒） |
| `MaxSpeechDuration` | float | 10.0 | 语音段最大持续时长（秒） |
| `BufferSizeInSeconds` | float | 30.0 | 内部缓冲大小（秒） |
| `NumThreads` | int32 | 1 | 推理线程数 |
| `Provider` | FString | cpu | 推理后端 |
| `bAutoStart` | bool | false | BeginPlay 自动启动 |
| `bEmitSpeechSegment` | bool | true | 是否输出语音片段数据 |

### VAD C++ API

```cpp
auto* Vad = MyActor->FindComponentByClass<USherpaVadComponent>();

Vad->Config.ModelPath = TEXT("Content/Models/Vad/Silero/silero_vad.int8.onnx");
Vad->StartVAD();                    // 自动开麦

bool bSpeaking = Vad->IsSpeechDetected();

Vad->OnSpeechStart.AddDynamic(this, &AMyActor::OnStart);
Vad->OnSpeechEnd.AddDynamic(this, &AMyActor::OnEnd);

// 外部音频源（替代麦克风）
Vad->PushAudioFloat(Samples, 48000, 2);

Vad->ResetVAD();                    // 重置状态
Vad->StopVAD();                     // 停止
```

### VAD 事件

| 事件 | 类型 | 触发时机 |
|------|------|---------|
| `OnSpeechStart` | 无参数 | 检测到语音开始 |
| `OnSpeechEnd` | 无参数 | 语音结束（静音超阈值） |
| `OnSpeechSegmentReady` | `FSherpaVadSegment` | 完整语音片段（含 PCM 数据） |
| `OnVadError` | `FString` | 发生错误 |

### VAD + KWS 组合

两个组件各自独立，共享同一个 `USherpaAudioCapture` 通道（一次重采样，两份数据）：

```
Event BeginPlay
  ├─ KWS.StartKWS(Config)     → KWS Worker 自动接收 16kHz mono
  └─ VAD.StartVAD()           → VAD Worker 自动接收 16kHz mono

蓝图层可选联动：
  if VAD.IsSpeechDetected()   → 有人在说话时才处理 KWS 事件
```

---

## 调试开关

在控制台输入以下命令控制详细日志输出：

| CVar | 默认 | 控制内容 |
|------|------|---------|
| `SherpaONNX.Debug.Audio` | 0 | KWS 音频处理帧日志 |
| `SherpaONNX.Debug.Decode` | 0 | KWS 解码进度统计 |
| `SherpaONNX.Debug.Verbose` | 0 | KWS 初始化详细日志 |
| `SherpaONNX.Debug.Capture` | 0 | 麦克风采集 RMS/Peak 日志 |

```
SherpaONNX.Debug.Verbose 1   # 开启
SherpaONNX.Debug.Verbose 0   # 关闭
```

---

## 跨平台部署

### 编译 Native 库

```bash
git clone https://github.com/k2-fsa/sherpa-onnx.git
cd sherpa-onnx

# Windows DLL
mkdir build && cd build
cmake .. -G "Visual Studio 18 2026" -A x64 \
  -DBUILD_SHARED_LIBS=ON -DSHERPA_ONNX_ENABLE_C_API=ON \
  -DSHERPA_ONNX_ENABLE_TTS=OFF -DSHERPA_ONNX_ENABLE_BINARY=OFF \
  -DSHERPA_ONNX_USE_STATIC_CRT=ON
cmake --build . --config Release --target sherpa-onnx-c-api --parallel 8

# Linux
cmake .. -DBUILD_SHARED_LIBS=ON -DSHERPA_ONNX_ENABLE_C_API=ON \
  -DSHERPA_ONNX_ENABLE_TTS=OFF -DSHERPA_ONNX_LINK_LIBSTDCPP_STATICALLY=ON
make -j sherpa-onnx-c-api

# Android arm64-v8a
export ANDROID_NDK=/path/to/ndk
./build-android-arm64-v8a.sh
```

### 放置产物

将编译产物放入 `Source/ThirdParty/sherpa-onnx/lib/<Platform>/`。

### 打包

- 模型文件作为 NonUFS 文件打包到 `<PackageDir>/<ProjectName>/Content/Models/`
- 运行时通过 `FPaths::ProjectDir()` 定位
- Android 需声明 `RECORD_AUDIO` 权限（已配置）

---

## 许可

- 插件代码: MIT
- sherpa-onnx: Apache 2.0
- ONNX Runtime: MIT
- Silero VAD: MIT
- 预训练模型: 遵循 [sherpa-onnx 模型许可](https://github.com/k2-fsa/sherpa-onnx)
