#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "SherpaKws/SherpaKwsNative.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSherpaNativeRequiredExportsTest,
	"SherpaONNX.NativeLoader.RequiredExports",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSherpaNativeRequiredExportsTest::RunTest(const FString& Parameters)
{
	const FKwsNativeAPI& API = SherpaKws_GetAPI();
	TestTrue(TEXT("All required KWS exports are available"), API.IsLoaded());
	TestTrue(TEXT("All required VAD exports are available"), API.IsVadLoaded());
	TestTrue(TEXT("All required ASR exports are available"), API.IsAsrLoaded());
	return true;
}

#endif
