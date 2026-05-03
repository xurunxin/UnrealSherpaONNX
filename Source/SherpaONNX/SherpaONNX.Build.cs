using System.IO;
using UnrealBuildTool;

public class SherpaONNX : ModuleRules
{
	public SherpaONNX(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Projects",
			"AudioCaptureCore",
			"AudioCapture"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AudioMixer",
			"AudioPlatformConfiguration",
			"SignalProcessing",
			"Json",
			"JsonUtilities"
		});

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PrivateDependencyModuleNames.Add("AudioCaptureRtAudio");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.Add("AudioCaptureRtAudio");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AudioCaptureAndroid",
				"AndroidPermission"
			});

			string BuildPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin",
				Path.Combine(BuildPath, "..", "SherpaONNX_Android.xml"));
		}

		PublicDefinitions.Add("WITH_SHERPA_ONNX=1");

		// ---- ThirdParty: sherpa-onnx native libs ----
		string ThirdPartyPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", "sherpa-onnx");
		string IncludePath    = Path.Combine(ThirdPartyPath, "include");
		string LibPath        = Path.Combine(ThirdPartyPath, "lib");

		PublicIncludePaths.Add(IncludePath);

		bool bHaveNativeLibs = false;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string PlatformLib = Path.Combine(LibPath, "Win64");

			bHaveNativeLibs = File.Exists(Path.Combine(PlatformLib, "sherpa-onnx-c-api.lib"));

			if (bHaveNativeLibs)
			{
				PublicAdditionalLibraries.Add(Path.Combine(PlatformLib, "sherpa-onnx-c-api.lib"));

				RuntimeDependencies.Add(
					Path.Combine("$(BinaryOutputDir)", "sherpa-onnx-c-api.dll"),
					Path.Combine(PlatformLib, "sherpa-onnx-c-api.dll"));
				RuntimeDependencies.Add(
					Path.Combine("$(BinaryOutputDir)", "onnxruntime.dll"),
					Path.Combine(PlatformLib, "onnxruntime.dll"));
				RuntimeDependencies.Add(
					Path.Combine("$(BinaryOutputDir)", "onnxruntime_providers_shared.dll"),
					Path.Combine(PlatformLib, "onnxruntime_providers_shared.dll"));
			}

			PublicDefinitions.Add("SHERPA_ONNX_LINK_STATIC=0");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string PlatformLib = Path.Combine(LibPath, "Linux");

			bHaveNativeLibs = File.Exists(Path.Combine(PlatformLib, "libsherpa-onnx-c-api.a"));

			if (bHaveNativeLibs)
			{
				PublicAdditionalLibraries.Add(Path.Combine(PlatformLib, "libsherpa-onnx-c-api.a"));
				PublicAdditionalLibraries.Add(Path.Combine(PlatformLib, "libonnxruntime.a"));
			}

			PublicDefinitions.Add("SHERPA_ONNX_LINK_STATIC=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string ABI = Target.Architecture.LinuxName;
			string PlatformLib = Path.Combine(LibPath, "Android", ABI);

			bHaveNativeLibs = File.Exists(Path.Combine(PlatformLib, "libsherpa-onnx-c-api.a"));

			if (bHaveNativeLibs)
			{
				PublicAdditionalLibraries.Add(Path.Combine(PlatformLib, "libsherpa-onnx-c-api.a"));
				PublicAdditionalLibraries.Add(Path.Combine(PlatformLib, "libonnxruntime.a"));
			}

			PublicDefinitions.Add("SHERPA_ONNX_LINK_STATIC=1");
		}

		if (!bHaveNativeLibs)
		{
			System.Console.WriteLine(
				"SherpaONNX: Native libraries not found in ThirdParty/sherpa-onnx/lib/<platform>/.\n" +
				"  Build sherpa-onnx with CMake first, then place the output files in the lib directory.\n" +
				"  See: https://github.com/k2-fsa/sherpa-onnx for build instructions.");
		}

		// ---- Stage model files as NonUFS so ONNX Runtime can fopen() them ----
		string PluginContent = Path.Combine(PluginDirectory, "Content");
		string ModelsDir     = Path.Combine(PluginContent, "Models");

		// 仅打包 .onnx 模型 + tokens.txt，排除 test_wavs 等多余文件
		StageModelFiles(ModelsDir);
	}

	private void StageModelFiles(string ModelsDir)
	{
		if (!Directory.Exists(ModelsDir)) return;

		foreach (string FilePath in Directory.GetFiles(ModelsDir, "*", SearchOption.AllDirectories))
		{
			string Ext = Path.GetExtension(FilePath).ToLower();
			string Name = Path.GetFileName(FilePath).ToLower();

			// 只打包模型推理所需的文件
			if (Ext != ".onnx" && Name != "tokens.txt" && Name != "keywords.txt" && Ext != ".phone")
				continue;

			// 目标路径: 项目目录/Content/Models/<相对路径>
			string RelativePath = Path.GetRelativePath(ModelsDir, FilePath);
			string DestPath = Path.Combine("$(ProjectDir)", "Content", "Models", RelativePath);

			RuntimeDependencies.Add(DestPath, FilePath, StagedFileType.NonUFS);
		}
	}
}
