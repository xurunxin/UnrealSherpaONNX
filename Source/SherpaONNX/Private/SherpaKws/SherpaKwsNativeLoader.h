#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace SherpaKwsNativeLoader
{
enum class ELoadedModuleDecision : uint8
{
	LoadExpected,
	AcquireExisting,
	RejectPathUnavailable,
	RejectPathMismatch,
	RejectIncompatible
};

struct FLoadedModuleState
{
	bool bIsLoaded = false;
	bool bPathAvailable = false;
	bool bCompatible = true;
	FString Path;
};

inline FString NormalizeWindowsModulePath(const FString& Path)
{
	FString Normalized = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeFilename(Normalized);
	FPaths::CollapseRelativeDirectories(Normalized);
	return Normalized;
}

inline bool AreSameWindowsModulePath(const FString& Left, const FString& Right)
{
	return NormalizeWindowsModulePath(Left).Equals(
		NormalizeWindowsModulePath(Right),
		ESearchCase::IgnoreCase);
}

inline ELoadedModuleDecision DecideLoadedModule(
	const FLoadedModuleState& State,
	const FString& ExpectedPath,
	bool bRequireCompatibility)
{
	if (!State.bIsLoaded)
	{
		return ELoadedModuleDecision::LoadExpected;
	}
	if (!State.bPathAvailable)
	{
		return ELoadedModuleDecision::RejectPathUnavailable;
	}
	if (bRequireCompatibility && !State.bCompatible)
	{
		return ELoadedModuleDecision::RejectIncompatible;
	}
	if (bRequireCompatibility || AreSameWindowsModulePath(State.Path, ExpectedPath))
	{
		return ELoadedModuleDecision::AcquireExisting;
	}
	return ELoadedModuleDecision::RejectPathMismatch;
}
}
