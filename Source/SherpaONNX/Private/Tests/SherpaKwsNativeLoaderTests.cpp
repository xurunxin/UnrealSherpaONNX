#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "SherpaKws/SherpaKwsNativeLoader.h"

using namespace SherpaKwsNativeLoader;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSherpaNativeLoaderDecisionTest,
	"SherpaONNX.NativeLoader.Decision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSherpaNativeLoaderDecisionTest::RunTest(const FString& Parameters)
{
	const FString ExpectedPath = TEXT("C:/Plugin/Source/ThirdParty/sherpa-onnx/lib/Win64/onnxruntime.dll");

	FLoadedModuleState State;
	TestEqual(
		TEXT("No preloaded module loads the bundled DLL"),
		DecideLoadedModule(State, ExpectedPath, true),
		ELoadedModuleDecision::LoadExpected);

	State.bIsLoaded = true;
	State.bPathAvailable = true;
	State.Path = ExpectedPath;
	State.bCompatible = true;
	TestEqual(
		TEXT("Same-path compatible ORT gets a controlled reference"),
		DecideLoadedModule(State, ExpectedPath, true),
		ELoadedModuleDecision::AcquireExisting);

	State.bCompatible = false;
	TestEqual(
		TEXT("Same-path incompatible ORT is rejected"),
		DecideLoadedModule(State, ExpectedPath, true),
		ELoadedModuleDecision::RejectIncompatible);

	State.Path = TEXT("C:/Engine/Plugins/NNE/onnxruntime.dll");
	State.bCompatible = true;
	TestEqual(
		TEXT("Different-path ORT gets a controlled reference when it supports the required C API"),
		DecideLoadedModule(State, ExpectedPath, true),
		ELoadedModuleDecision::AcquireExisting);

	State.bCompatible = false;
	TestEqual(
		TEXT("Different-path incompatible ORT is rejected"),
		DecideLoadedModule(State, ExpectedPath, true),
		ELoadedModuleDecision::RejectIncompatible);

	State.bPathAvailable = false;
	TestEqual(
		TEXT("A module whose path cannot be inspected is rejected"),
		DecideLoadedModule(State, ExpectedPath, true),
		ELoadedModuleDecision::RejectPathUnavailable);

	State.bPathAvailable = true;
	State.Path = ExpectedPath;
	State.bCompatible = false;
	TestEqual(
		TEXT("Same-path non-ORT module gets a controlled reference without an ORT compatibility check"),
		DecideLoadedModule(State, ExpectedPath, false),
		ELoadedModuleDecision::AcquireExisting);

	State.Path = TEXT("C:/Other/sherpa-onnx-c-api.dll");
	TestEqual(
		TEXT("Different-path non-ORT module is rejected"),
		DecideLoadedModule(State, ExpectedPath, false),
		ELoadedModuleDecision::RejectPathMismatch);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSherpaNativeLoaderPathTest,
	"SherpaONNX.NativeLoader.WindowsPathSemantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSherpaNativeLoaderPathTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Path comparison ignores case, separators, and collapsed segments"),
		AreSameWindowsModulePath(
			TEXT("C:\\PLUGIN\\Source\\ThirdParty\\sherpa-onnx\\lib\\Win64\\temp\\..\\onnxruntime.dll"),
			TEXT("c:/plugin/source/thirdparty/sherpa-onnx/lib/win64/onnxruntime.dll")));
	TestFalse(
		TEXT("Different canonical paths remain different"),
		AreSameWindowsModulePath(
			TEXT("C:/Plugin/onnxruntime.dll"),
			TEXT("C:/Windows/System32/onnxruntime.dll")));
	return true;
}

#endif
