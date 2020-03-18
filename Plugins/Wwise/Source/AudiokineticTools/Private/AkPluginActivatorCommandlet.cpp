// Fill out your copyright notice in the Description page of Project Settings.

#include "AkPluginActivatorCommandlet.h"

#if WITH_EDITOR

#include "AssetRegistryModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "XmlFile.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IPluginManager.h"

#include "AkUnrealHelper.h"
#include "AkActivatedPlugins.h"
#include "AkAudioBankGenerationHelpers.h"
#include "Platforms/AkPlatformInfo.h"
#include "GenericPlatform/GenericPlatformMisc.h"

#if UE_4_24_OR_LATER
#include "TargetReceipt.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAkPluginActivator, Log, All);
#endif


UAkPluginActivatorCommandlet::UAkPluginActivatorCommandlet()
{
	IsClient = IsEditor = IsServer = false;
	LogToConsole = true;

	HelpDescription = TEXT("Commandlet that activates Wwise plug-ins.");

	HelpParamNames.Add(TEXT("platform"));
	HelpParamDescriptions.Add(TEXT("Specifies the UE4 platform that is being built."));

	HelpParamNames.Add(TEXT("configuration"));
	HelpParamDescriptions.Add(TEXT("(Optional) Specifies the configuration to be built. Possible values are Debug, Profile or Release. Defaults to Profile."));

	HelpParamNames.Add(TEXT("help"));
	HelpParamDescriptions.Add(TEXT("(Optional) Print this help message. This will quit the commandlet immediately."));

	HelpUsage = TEXT("<Editor.exe> <path_to_uproject> -run=AkPluginActivator [-platform=PlatformToActivatePlugins] [-configuration=Debug|Profile|Release]");
	HelpWebLink = TEXT("https://www.audiokinetic.com/library/edge/?source=UE4&id=using_features_pluginactivatorcommandlet.html");
}

void UAkPluginActivatorCommandlet::PrintHelp() const
{
	UE_LOG(LogAkPluginActivator, Display, TEXT("%s"), *HelpDescription);
	UE_LOG(LogAkPluginActivator, Display, TEXT("Usage: %s"), *HelpUsage);
	UE_LOG(LogAkPluginActivator, Display, TEXT("Parameters:"));
	for (int32 i = 0; i < HelpParamNames.Num(); ++i)
	{
		UE_LOG(LogAkPluginActivator, Display, TEXT("\t- %s: %s"), *HelpParamNames[i], *HelpParamDescriptions[i]);
	}
	UE_LOG(LogAkPluginActivator, Display, TEXT("For more information, see %s"), *HelpWebLink);
}

FString UAkPluginActivatorCommandlet::GetTargetFilePath(const FString& BaseDir, const FString& TargetName, const FString& Platform, const FString& Configuration)
{
#if UE_4_24_OR_LATER
	EBuildConfiguration TargetConfiguration;
	LexTryParseString(TargetConfiguration, *Configuration);
	return FTargetReceipt::GetDefaultPath(*BaseDir, *TargetName, *Platform, TargetConfiguration, nullptr);
#else
	if (Configuration == "Development")
	{
		return FPaths::Combine(BaseDir, FString::Printf(TEXT("Binaries/%s/%s.target"), *Platform, *TargetName));
	}
	else
	{
		return FPaths::Combine(BaseDir, FString::Printf(TEXT("Binaries/%s/%s-%s-%s.target"), *Platform, *TargetName, *Platform, *Configuration));
	}
#endif
}

int32 UAkPluginActivatorCommandlet::Main(const FString& Params)
{
	// @todo: Determine whether files should be written here at all. It is possible to create these files right after successful bank generation.

#if !WITH_EDITOR
	return -1;
#else
	if (!FApp::HasProjectName())
	{
		UE_LOG(LogAkPluginActivator, Display, TEXT("Project name could not be determined."));
		return 0;
	}

	const FString TargetName = FApp::GetProjectName();
	if (TargetName.IsEmpty() || TargetName.Compare("UE4Game") == 0 || TargetName.Compare("UE4Editor") == 0)
	{
		UE_LOG(LogAkPluginActivator, Display, TEXT("No files will be modified when building the UE4 Wwise engine plug-in."));
		return 0;
	}

	FString TargetPlatform;
	FString Configuration = "Profile";
	FString TargetConfiguration = "Development";
	{
		TArray<FString> Tokens;
		TArray<FString> Switches;
		TMap<FString, FString> ParamVals;
		UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);
		if (Switches.Contains("help"))
		{
			PrintHelp();
			return 0;
		}

		auto* Platform = ParamVals.Find(FString(TEXT("platform")));
		if (!Platform)
		{
			UE_LOG(LogAkPluginActivator, Error, TEXT("No platform specified."));
			PrintHelp();
			return -1;
		}

		TargetPlatform = *Platform;

		const auto* configParam = ParamVals.Find(FString(TEXT("configuration")));
		if (configParam && (configParam->Compare("Debug", ESearchCase::IgnoreCase) == 0 ||
			configParam->Compare("Profile", ESearchCase::IgnoreCase) == 0 ||
			configParam->Compare("Release", ESearchCase::IgnoreCase) == 0))
		{
			Configuration = *configParam;
		}
	
		const auto* targetConfigParam = ParamVals.Find(FString(TEXT("targetconfig")));
		if (targetConfigParam)
		{
			TargetConfiguration = *targetConfigParam;
		}
	}

	auto* PlatformInfo = UAkPlatformInfo::GetAkPlatformInfo(TargetPlatform);
	if (!PlatformInfo)
	{
		UE_LOG(LogAkPluginActivator, Error, TEXT("Unsupported target name specified: %s"), *TargetPlatform);
		return -1;
	}

	if (PlatformInfo->bSupportsUPL || PlatformInfo->bUsesStaticLibraries)
	{
		return 0;
	}

	if (PlatformInfo->bForceReleaseConfig)
	{
		Configuration = "Release";
	}

	TArray<FString> WwisePlugins;
	auto* Package = LoadPackage(nullptr, *UAkActivatedPlugins::GetPackageName(), LOAD_None);
	const auto& FilePath = UAkActivatedPlugins::GetFilePath();
	const auto* ActivatedPlugins = LoadObject<UAkActivatedPlugins>(Package, *FString("ActivatedPlugins"), *FilePath, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone);
	if (!ActivatedPlugins)
	{
		UE_LOG(LogAkPluginActivator, Display, TEXT("Can't find UAkActivatedPlugins"));
	}
	else if (const FAkPluginList* PluginListStruct = ActivatedPlugins->Platforms.Find(PlatformInfo->WwisePlatform))
	{
		WwisePlugins = PluginListStruct->PluginNames;
		auto PluginList = FString::Join(PluginListStruct->PluginNames, TEXT(", "));
		UE_LOG(LogAkPluginActivator, Display, TEXT("Plug-ins required for <%s> platform are: %s"), *PlatformInfo->WwisePlatform, *PluginList);
	}
	else
	{
		UE_LOG(LogAkPluginActivator, Display, TEXT("Can't find FAkPluginList for <%s> platform"), *PlatformInfo->WwisePlatform);
	}

	const FString TargetFileName = GetTargetFilePath(AkUnrealHelper::GetProjectDirectory(), TargetName, TargetPlatform, TargetConfiguration);
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *TargetFileName))
	{
		UE_LOG(LogAkPluginActivator, Error, TEXT("Could not read file: %s"), *TargetFileName);
		return -1;
	}

	auto JsonReader = TJsonReaderFactory<>::Create(Json);
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject))
	{
		UE_LOG(LogAkPluginActivator, Error, TEXT("Could not deserialize Json file: %s"), *TargetFileName);
		return -1;
	}

	auto RuntimeDependencies = JsonObject->GetArrayField("RuntimeDependencies");
	bool ModifiedRuntimeDependencies = false;

	{
		const FString ThirdPartyDirectory = AkUnrealHelper::GetThirdPartyDirectory();
		const auto ArchitectureDirectory = ThirdPartyDirectory / PlatformInfo->Architecture;
		const auto PluginDirectory = ArchitectureDirectory / Configuration / "bin";

		for (auto i = 0; i < RuntimeDependencies.Num(); )
		{
			auto RuntimeDependency = RuntimeDependencies[i]->AsObject();
			if (RuntimeDependency && RuntimeDependency->GetStringField("Path").StartsWith(ArchitectureDirectory))
			{
				RuntimeDependencies.RemoveAt(i);
				ModifiedRuntimeDependencies = true;
			}
			else
				++i;
		}

		auto AddRuntimeDependencies = [&](const FString& FileNameFormat, const FString& UfsType)
		{
			if (FileNameFormat.IsEmpty())
				return;

			for (const auto& PluginName : WwisePlugins)
			{
				auto RuntimeDependency = new FJsonObject();
				RuntimeDependency->SetStringField("Path", PluginDirectory / FString::Format(*FileNameFormat, { PluginName }));
				RuntimeDependency->SetStringField("Type", UfsType);
				RuntimeDependencies.Add(MakeShareable(new FJsonValueObject(MakeShareable(RuntimeDependency))));
				ModifiedRuntimeDependencies = true;
			}
		};

		AddRuntimeDependencies(PlatformInfo->LibraryFileNameFormat, "SystemNonUFS");
		AddRuntimeDependencies(PlatformInfo->DebugFileNameFormat, "DebugNonUFS");
	}

	if (!ModifiedRuntimeDependencies)
	{
		UE_LOG(LogAkPluginActivator, Display, TEXT("No Wwise plug-ins required for <%s> platform."), *PlatformInfo->WwisePlatform);
		return 0;
	}

	JsonObject->SetArrayField("RuntimeDependencies", RuntimeDependencies);

	Json.Empty();
	auto Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	if (!FFileHelper::SaveStringToFile(Json, *TargetFileName))
	{
		UE_LOG(LogAkPluginActivator, Error, TEXT("Could not modify file: %s"), *TargetFileName);
		return -1;
	}

	UE_LOG(LogAkPluginActivator, Display, TEXT("Successfully modified file: %s"), *TargetFileName);
	return 0;
#endif
}
