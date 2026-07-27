#include "Model/SherpaModelPathResolver.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace SherpaModelPathResolver
{
	namespace
	{
		FString MakeAbsoluteModelPath(const FString& Root, const FString& RelativePath)
		{
			FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(Root, RelativePath));
			FPaths::NormalizeFilename(Path);
			return Path;
		}

		FString MakeAbsoluteModelRoot(const FString& Root)
		{
			FString Path = FPaths::ConvertRelativePathToFull(Root);
			FPaths::NormalizeDirectoryName(Path);
			return Path;
		}

		TArray<FString> MakePaths(const FString& Root, const TArray<FString>& RelativePaths)
		{
			TArray<FString> Paths;
			Paths.Reserve(RelativePaths.Num());
			for (const FString& RelativePath : RelativePaths)
			{
				Paths.Add(MakeAbsoluteModelPath(Root, RelativePath));
			}
			return Paths;
		}

		TArray<FString> FindMissingPaths(const TArray<FString>& Paths, FFileExists FileExists)
		{
			TArray<FString> MissingPaths;
			for (const FString& Path : Paths)
			{
				if (!FileExists(Path))
				{
					MissingPaths.Add(Path);
				}
			}
			return MissingPaths;
		}

		void AppendMissingPaths(
			FString& ErrorMessage,
			const TCHAR* RootLabel,
			const TArray<FString>& MissingPaths)
		{
			ErrorMessage += FString::Printf(TEXT("\n%s:"), RootLabel);
			for (const FString& MissingPath : MissingPaths)
			{
				ErrorMessage += FString::Printf(TEXT("\n  - %s"), *MissingPath);
			}
		}
	}

	FString GetProjectModelsRoot()
	{
		return MakeAbsoluteModelPath(FPaths::ProjectContentDir(), TEXT("Models"));
	}

	FString GetPluginModelsRoot()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SherpaONNX"));
		if (Plugin.IsValid())
		{
			return MakeAbsoluteModelPath(Plugin->GetContentDir(), TEXT("Models"));
		}

		// Keep diagnostics actionable even if the plugin manager is unavailable.
		return MakeAbsoluteModelPath(
			FPaths::ProjectPluginsDir(),
			TEXT("SherpaONNX/Content/Models"));
	}

	FModelPathResolution ResolveCompleteModelSet(
		const FString& ProjectModelsRoot,
		const FString& PluginModelsRoot,
		const TArray<FString>& RequiredRelativePaths,
		FFileExists FileExists)
	{
		FModelPathResolution Result;

		const TArray<FString> ProjectPaths = MakePaths(ProjectModelsRoot, RequiredRelativePaths);
		const TArray<FString> ProjectMissingPaths = FindMissingPaths(ProjectPaths, FileExists);
		if (ProjectMissingPaths.IsEmpty())
		{
			Result.bSuccess = true;
			Result.Root = MakeAbsoluteModelRoot(ProjectModelsRoot);
			Result.Paths = ProjectPaths;
			return Result;
		}

		const TArray<FString> PluginPaths = MakePaths(PluginModelsRoot, RequiredRelativePaths);
		const TArray<FString> PluginMissingPaths = FindMissingPaths(PluginPaths, FileExists);
		if (PluginMissingPaths.IsEmpty())
		{
			Result.bSuccess = true;
			Result.Root = MakeAbsoluteModelRoot(PluginModelsRoot);
			Result.Paths = PluginPaths;
			return Result;
		}

		Result.MissingPaths = ProjectMissingPaths;
		Result.MissingPaths.Append(PluginMissingPaths);
		Result.ErrorMessage = TEXT("No complete SherpaONNX model set was found.");
		AppendMissingPaths(Result.ErrorMessage, TEXT("Missing from project Content/Models"), ProjectMissingPaths);
		AppendMissingPaths(Result.ErrorMessage, TEXT("Missing from plugin Content/Models"), PluginMissingPaths);
		return Result;
	}

	FModelPathResolution ResolveDefaultModelSet(const TArray<FString>& RequiredRelativePaths)
	{
		return ResolveCompleteModelSet(
			GetProjectModelsRoot(),
			GetPluginModelsRoot(),
			RequiredRelativePaths,
			[](const FString& Path)
			{
				return FPaths::FileExists(Path);
			});
	}
}
