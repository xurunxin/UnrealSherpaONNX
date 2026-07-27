#pragma once

#include "CoreMinimal.h"

namespace SherpaModelPathResolver
{
	struct FModelPathResolution
	{
		bool bSuccess = false;
		FString Root;
		TArray<FString> Paths;
		TArray<FString> MissingPaths;
		FString ErrorMessage;
	};

	using FFileExists = TFunctionRef<bool(const FString&)>;

	FString GetProjectModelsRoot();
	FString GetPluginModelsRoot();

	FModelPathResolution ResolveCompleteModelSet(
		const FString& ProjectModelsRoot,
		const FString& PluginModelsRoot,
		const TArray<FString>& RequiredRelativePaths,
		FFileExists FileExists);

	FModelPathResolution ResolveDefaultModelSet(const TArray<FString>& RequiredRelativePaths);
}
