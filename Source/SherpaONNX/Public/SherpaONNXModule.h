#pragma once

#include "Modules/ModuleManager.h"

class SHERPAONNX_API FSherpaONNXModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static bool IsAvailable();

private:
	void ShutdownKwsWorkers();
};
