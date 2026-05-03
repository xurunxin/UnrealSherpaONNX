# SherpaONNX — UE5.7 离线语音唤醒插件

基于 [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) (12k+ Stars) Zipformer 模型的跨平台关键词检测插件，支持 Windows / Linux / Android。

**核心特性**：离线运行、零训练更换关键词、中英双语、低延迟（160ms/320ms）、极小模型（fp32 13MB / int8 5MB）。

---

## 目录结构

```
Plugins/SherpaONNX/
├── SherpaONNX.uplugin
├── README.md
├── Content/
│   └── Models/                                          # 预训练模型 & 关键词文件
│       ├── sherpa-onnx-kws-zipformer-zh-en-3M-2025-12-20/  # 中英双语 (推荐)
│       └── keywords.txt                                 # 默认关键词
├── Resources/
└── Source/
    ├── SherpaONNX/                                       # 运行时模块
    │   ├── SherpaONNX.Build.cs                           # 跨平台编译规则
    │   ├── SherpaONNX_Android.xml                        # Android 权限声明
    │   ├── Public/
    │   │   ├── SherpaONNXModule.h
    │   │   └── Kws/
    │   │       ├── KwsTypes.h            # 配置结构体 & 预设枚举
    │   │       ├── KwsWakeWordComponent.h  # Blueprint 可调用组件（主入口）
    │   │       ├── KwsAudioCapture.h       # 音频采集组件
    │   │       └── SherpaKwsLibrary.h      # 蓝图函数库（预设配置工厂）
    │   └── Private/
    │       ├── SherpaONNXModule.cpp
    │       └── Kws/
    │           ├── KwsWorker.h/.cpp         # 后台推理线程 (FRunnable)
    │           ├── KwsWakeWordComponent.cpp
    │           ├── KwsAudioCapture.cpp
    │           └── SherpaKwsLibrary.cpp
    └── ThirdParty/
        └── sherpa-onnx/
            ├── include/sherpa-onnx/c-api/c-api.h  # C ABI 声明
            └── lib/                                # 预编译库（按平台）
                ├── Win64/         # sherpa-onnx-c-api.dll + onnxruntime.dll
                ├── Linux/         # libsherpa-onnx-c-api.so + libonnxruntime.so
                └── Android/       # libsherpa-onnx-c-api.a + libonnxruntime.a
                    ├── arm64-v8a/
                    └── x86_64/
```

---

## 架构

```
蓝图层
┌──────────────────────────────────────┐
│  UKwsWakeWordComponent (ActorComp)    │
│  ├─ StartListening(Config)           │
│  ├─ StopListening()                  │
│  ├─ SetKeywords("tokens @keyword")   │
│  └─ OnKeywordDetected (Event)        │
└──────────┬───────────────────────────┘
           │
C++ 层     │
┌──────────┴───────────────────────────┐
│  UKwsAudioCapture ──→ FKwsWorker     │
│  (麦克风 → PCM)      (ONNX 推理线程)  │
│                            │          │
│                     FKwsNativeAPI    │
│                     (C ABI FFI)      │
└──────────┬───────────────────────────┘
           │
Native 层  │
┌──────────┴───────────────────────────┐
│  sherpa-onnx-c-api.dll/.so           │
│  └─ onnxruntime.dll/.so              │
│       └─ encoder/decoder/joiner.onnx │
└──────────────────────────────────────┘
```

---

## 快速开始

### 1. 环境要求

| 平台 | 状态 |
|------|------|
| Windows x64 (Editor/Packaged) | ✅ 已编译 DLL |
| Linux x64 | 需编译 native .so |
| Android arm64-v8a | 需编译 native .a |

### 2. Blueprint 用法

#### 最简方式（使用预设）

```
1. 在任意 Actor 上添加 UKwsWakeWordComponent
2. Event BeginPlay → 调用以下节点:

   Make Preset Config
   ├─ Preset = "中英双语 | fp32 | 320ms 延迟"
   └─ KeywordsString = 粘贴关键词 token 序列

   → StartListening(Config)

3. 绑定 OnKeywordDetected 事件 → 获取 Result.Keyword
```

#### 手动配置

在 Details 面板中直接填写 `FSherpaKwsModelConfig` 结构体，或调用 `Make Custom Config` 节点。

### 3. 关键词格式

每条关键词一行，格式为 `<token序列> @<原始文本>`：

```
x iǎo ài tóng x ué @小爱同学
n ǐ hǎo jī qì rén @你好机器人
dǎ kāi cài dān @打开菜单
```

**生成 token 序列**（新关键词）：

```bash
pip install sherpa-onnx
sherpa-onnx-cli text2token \
  --tokens tokens.txt \
  --tokens-type phone+ppinyin \
  --lexicon en.phone \
  raw_keywords.txt keywords.txt
```

---

## 模型预设

| 预设枚举 | 延迟 | 精度 | 大小 |
|----------|------|------|------|
| `Bilingual_Fp32_320ms` | 320ms | 最高 | 13 MB |
| `Bilingual_Int8_320ms` | 320ms | 中等 | 5 MB |
| `Bilingual_Fp32_160ms` | 160ms | 高 | 13 MB |
| `Bilingual_Int8_160ms` | 160ms | 中等 | 5 MB |
| `Chinese_Fp32_320ms` | 320ms | 高 | ~18 MB |

> 桌面端推荐 `Bilingual_Fp32_320ms`，移动端推荐 `Bilingual_Int8_320ms`。

---

## C++ API

```cpp
// 创建配置
FSherpaKwsModelConfig Config = USherpaKwsLibrary::MakeBilingualPresetConfig(
    ESherpaKwsPreset::Bilingual_Fp32_320ms,
    TEXT("x iǎo ài tóng x ué @小爱同学\nn ǐ hǎo jī qì rén @你好机器人")
);

// 启动
MyComponent->StartListening(Config);
MyComponent->OnKeywordDetected.AddDynamic(this, &AMyActor::OnKwsDetected);

// 运行时切换关键词
MyComponent->SetKeywords(TEXT("dǎ kāi cài dān @打开菜单"));

// 停止
MyComponent->StopListening();
```

---

## 配置字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `EncoderPath` | FString | Zipformer 编码器 ONNX 模型路径 |
| `DecoderPath` | FString | 解码器 ONNX 模型路径 |
| `JoinerPath` | FString | Joiner 网络 ONNX 模型路径 |
| `TokensPath` | FString | token 映射表 (tokens.txt) 路径 |
| `KeywordsFile` | FString | 关键词文件路径（可选） |
| `KeywordsString` | FString | 关键词字符串（优先于文件） |
| `NumThreads` | int32 | ONNX Runtime 推理线程数，默认 2 |
| `Provider` | FString | 推理后端: `"cpu"` / `"dml"` / `"cuda"` |

---

## 跨平台部署

### 编译 Native 库

```bash
# 克隆
git clone https://github.com/k2-fsa/sherpa-onnx.git
cd sherpa-onnx

# Windows DLL (已编译)
mkdir build && cd build
cmake .. -G "Visual Studio 18 2026" -A x64 \
  -DBUILD_SHARED_LIBS=ON -DSHERPA_ONNX_ENABLE_C_API=ON \
  -DSHERPA_ONNX_ENABLE_TTS=OFF -DSHERPA_ONNX_ENABLE_BINARY=OFF \
  -DSHERPA_ONNX_USE_STATIC_CRT=ON
cmake --build . --config Release --target sherpa-onnx-c-api --parallel 8
# → bin/Release/sherpa-onnx-c-api.dll + lib/Release/sherpa-onnx-c-api.lib

# Linux
cmake .. -DBUILD_SHARED_LIBS=ON -DSHERPA_ONNX_ENABLE_C_API=ON \
  -DSHERPA_ONNX_ENABLE_TTS=OFF -DSHERPA_ONNX_LINK_LIBSTDCPP_STATICALLY=ON
make -j sherpa-onnx-c-api
# → lib/libsherpa-onnx-c-api.so

# Android arm64-v8a
export ANDROID_NDK=/path/to/ndk
./build-android-arm64-v8a.sh
# → libsherpa-onnx-c-api.a + libonnxruntime.a
```

### 放置产物

将编译产物放入 `Source/ThirdParty/sherpa-onnx/lib/<Platform>/`。

### 打包注意事项

- 模型文件自动作为 NonUFS 文件打包到 `<PackageDir>/<ProjectName>/Content/Models/`
- 运行时通过 `FPaths::ProjectDir()` 定位
- Android 需要在 APL 中声明 `RECORD_AUDIO` 权限（已配置）

---

## 事件

| 事件 | 类型 | 触发时机 |
|------|------|---------|
| `OnKeywordDetected` | `FSherpaKwsKeywordResult` | 检测到关键词 |
| `OnKwsReady` | 无参数 | 引擎初始化完成 |
| `OnKwsError` | `FString` | 发生错误 |

---

## 许可

- 插件代码: MIT
- sherpa-onnx: Apache 2.0
- ONNX Runtime: MIT
- 预训练模型: 遵循 [sherpa-onnx 模型许可](https://github.com/k2-fsa/sherpa-onnx)
