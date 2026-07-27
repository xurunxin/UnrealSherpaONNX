#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Model/SherpaModelPathResolver.h"
#include "Kws/SherpaKwsLibrary.h"

namespace
{
	FString NormalizeTestPath(const FString& Path)
	{
		FString Normalized = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Normalized);
		return Normalized;
	}

	TFunction<bool(const FString&)> MakeFileExists(const TArray<FString>& ExistingPaths)
	{
		TSet<FString> Existing;
		for (const FString& Path : ExistingPaths)
		{
			Existing.Add(NormalizeTestPath(Path));
		}

		return [Existing = MoveTemp(Existing)](const FString& Path)
		{
			return Existing.Contains(NormalizeTestPath(Path));
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSherpaModelPathResolverPriorityTest,
	"SherpaONNX.ModelPaths.CompleteSetPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSherpaModelPathResolverPriorityTest::RunTest(const FString& Parameters)
{
	using namespace SherpaModelPathResolver;

	const FString ProjectRoot = TEXT("C:/Project/Content/Models");
	const FString PluginRoot = TEXT("C:/Project/Plugins/SherpaONNX/Content/Models");
	const TArray<FString> Required = {
		TEXT("Asr/model/encoder.onnx"),
		TEXT("Asr/model/decoder.onnx"),
		TEXT("Asr/model/joiner.onnx"),
		TEXT("Asr/model/tokens.txt")
	};

	TArray<FString> BothRootsComplete;
	for (const FString& RelativePath : Required)
	{
		BothRootsComplete.Add(FPaths::Combine(ProjectRoot, RelativePath));
		BothRootsComplete.Add(FPaths::Combine(PluginRoot, RelativePath));
	}
	const FModelPathResolution ProjectWins = ResolveCompleteModelSet(
		ProjectRoot, PluginRoot, Required, MakeFileExists(BothRootsComplete));
	TestTrue(TEXT("A complete project model set resolves"), ProjectWins.bSuccess);
	TestEqual(TEXT("The project root has priority"), ProjectWins.Root, NormalizeTestPath(ProjectRoot));
	for (const FString& Path : ProjectWins.Paths)
	{
		TestTrue(TEXT("Every resolved path comes from the project root"), Path.StartsWith(NormalizeTestPath(ProjectRoot)));
	}

	TArray<FString> PartialProjectCompletePlugin = {
		FPaths::Combine(ProjectRoot, Required[0])
	};
	for (const FString& RelativePath : Required)
	{
		PartialProjectCompletePlugin.Add(FPaths::Combine(PluginRoot, RelativePath));
	}
	const FModelPathResolution PluginFallback = ResolveCompleteModelSet(
		ProjectRoot, PluginRoot, Required, MakeFileExists(PartialProjectCompletePlugin));
	TestTrue(TEXT("A complete plugin model set resolves"), PluginFallback.bSuccess);
	TestEqual(TEXT("A partial project set falls back as a whole"), PluginFallback.Root, NormalizeTestPath(PluginRoot));
	for (const FString& Path : PluginFallback.Paths)
	{
		TestTrue(TEXT("Fallback never mixes roots"), Path.StartsWith(NormalizeTestPath(PluginRoot)));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSherpaModelPathResolverFailureTest,
	"SherpaONNX.ModelPaths.IncompleteSets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSherpaModelPathResolverFailureTest::RunTest(const FString& Parameters)
{
	using namespace SherpaModelPathResolver;

	const FString ProjectRoot = TEXT("C:/Project/Content/Models");
	const FString PluginRoot = TEXT("C:/Project/Plugins/SherpaONNX/Content/Models");
	const TArray<FString> Required = {
		TEXT("Asr/bpe/encoder.onnx"),
		TEXT("Asr/bpe/decoder.onnx"),
		TEXT("Asr/bpe/joiner.onnx"),
		TEXT("Asr/bpe/tokens.txt"),
		TEXT("Asr/bpe/bpe.model")
	};
	const TArray<FString> SplitAcrossRoots = {
		FPaths::Combine(ProjectRoot, Required[0]),
		FPaths::Combine(ProjectRoot, Required[1]),
		FPaths::Combine(ProjectRoot, Required[4]),
		FPaths::Combine(PluginRoot, Required[2]),
		FPaths::Combine(PluginRoot, Required[3])
	};

	const FModelPathResolution Failed = ResolveCompleteModelSet(
		ProjectRoot, PluginRoot, Required, MakeFileExists(SplitAcrossRoots));
	TestFalse(TEXT("Two partial roots do not resolve"), Failed.bSuccess);
	TestTrue(TEXT("A failed resolution does not expose mixed paths"), Failed.Paths.IsEmpty());
	TestTrue(
		TEXT("The diagnostic lists an absolute project-side missing path"),
		Failed.ErrorMessage.Contains(NormalizeTestPath(FPaths::Combine(ProjectRoot, Required[2]))));
	TestTrue(
		TEXT("The diagnostic lists an absolute plugin-side missing BPE model"),
		Failed.ErrorMessage.Contains(NormalizeTestPath(FPaths::Combine(PluginRoot, Required[4]))));

	const TArray<FString> VadRequired = { TEXT("Vad/Silero/silero_vad.int8.onnx") };
	const FModelPathResolution VadResolved = ResolveCompleteModelSet(
		ProjectRoot,
		PluginRoot,
		VadRequired,
		MakeFileExists({ FPaths::Combine(PluginRoot, VadRequired[0]) }));
	TestTrue(TEXT("The generic resolver supports a one-file VAD set"), VadResolved.bSuccess);
	TestEqual(TEXT("A one-file VAD set uses the complete plugin root"), VadResolved.Root, NormalizeTestPath(PluginRoot));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSherpaKwsAbsoluteCustomPathTest,
	"SherpaONNX.ModelPaths.AbsoluteCustomKws",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSherpaKwsAbsoluteCustomPathTest::RunTest(const FString& Parameters)
{
	const FString AbsoluteModelDir = TEXT("C:/External/SherpaModels/CustomKws");
	const FSherpaKwsModelConfig Config = USherpaKwsLibrary::MakeCustomConfig(
		AbsoluteModelDir,
		TEXT("encoder.onnx"),
		TEXT("decoder.onnx"),
		TEXT("joiner.onnx"),
		TEXT("tokens.txt"),
		TEXT("D:/Explicit/keywords.txt"));

	TestEqual(
		TEXT("An absolute custom KWS directory is not prefixed by a default root"),
		Config.EncoderPath,
		NormalizeTestPath(FPaths::Combine(AbsoluteModelDir, TEXT("encoder.onnx"))));
	TestEqual(
		TEXT("All core files stay under the absolute custom KWS directory"),
		Config.TokensPath,
		NormalizeTestPath(FPaths::Combine(AbsoluteModelDir, TEXT("tokens.txt"))));
	TestEqual(
		TEXT("The explicit keywords file remains unchanged"),
		Config.KeywordsFile,
		FString(TEXT("D:/Explicit/keywords.txt")));
	return true;
}

#endif
