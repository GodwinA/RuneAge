// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*------------------------------------------------------------------------------------
	AkAudioBankGenerationHelpers.cpp: Wwise Helpers to generate banks from the editor and when cooking.
------------------------------------------------------------------------------------*/

#include "AkAudioBankGenerationHelpers.h"
#include "FileHelpers.h"
#include "Async/Async.h"
#include "AkAudioStyle.h"
#include "AkAuxBus.h"
#include "AkAudioBank.h"
#include "AkAudioEvent.h"
#include "AkUnrealHelper.h"
#include "AkSettingsPerUser.h"
#include "AkActivatedPlugins.h"
#include "Platforms/AkPlatformInfo.h"
#include "SGenerateSoundBanks.h"
#include "AkSettings.h"
#include "AssetRegistryModule.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/FileHelper.h"
#include "Misc/FeedbackContext.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ProjectDescriptor.h"
#include "PlatformInfo.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/MonitoredProcess.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "XmlFile.h"
#include "AssetToolsModule.h"
#include "Framework/Docking/TabManager.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Interfaces/IPluginManager.h"
#include "PackageHelperFunctions.h"

#include "Platforms/AkUEPlatform.h"

#define LOCTEXT_NAMESPACE "AkAudio"

/** Whether we want the Cooking process to use Wwise to Re-generate banks.			*/
bool GIsWwiseCookingSoundBanks = true;

DEFINE_LOG_CATEGORY(LogAkBanks);

namespace WwiseBnkGenHelper
{
	// known packaged plug-in IDs
	enum PluginID
	{
		AkCompressor = 0x006C0003,
		AkDelay = 0x006A0003,
		AkExpander = 0x006D0003,
		AkGain = 0x008B0003,
		AkMatrixReverb = 0x00730003,
		AkMeter = 0x00810003,
		AkParametricEQ = 0x00690003,
		AkPeakLimiter = 0x006E0003,
		AkRoomVerb = 0x00760003,
		SineGenerator = 0x00640002,
		SinkAuxiliary = 0xB40007, // *
		SinkCommunication = 0xB00007, // *
		SinkControllerHeadphones = 0xB10007, // *
		SinkControllerSpeaker = 0xB30007, // *
		SinkDVRByPass = 0xAF0007, // *
		SinkNoOutput = 0xB50007, // *
		SinkSystem = 0xAE0007, // *
		ToneGenerator = 0x00660002,
		WwiseSilence = 0x00650002,

		AkAudioInput = 0xC80002,
		AkConvolutionReverb = 0x7F0003,
		AkFlanger = 0x7D0003,
		AkGuitarDistortion = 0x7E0003,
		AkHarmonizer = 0x8A0003,
		AkMotionSink = 0x1FB0007,
		AkMotionSource = 0x1990002,
		AkMotionGeneratorSource = 0x1950002,
		AkPitchShifter = 0x880003,
		AkRecorder = 0x840003,
		AkReflect = 0xAB0003,
		AkSoundSeedGrain = 0xB70002,
		AkSoundSeedImpact = 0x740003,
		AkSoundSeedWind = 0x770002,
		AkSoundSeedWoosh = 0x780002,
		AkStereoDelay = 0x870003,
		AkSynthOne = 0x940002,
		AkTimeStretch = 0x820003,
		AkTremolo = 0x830003,
		AuroHeadphone = 0x44C1073,
		CrankcaseAudioREVModelPlayer = 0x1A01052,
		iZHybridReverb = 0x21033,
		iZTrashBoxModeler = 0x71033,
		iZTrashDelay = 0x41033,
		iZTrashDistortion = 0x31033,
		iZTrashDynamics = 0x51033,
		iZTrashFilters = 0x61033,
		iZTrashMultibandDistortion = 0x91033,
		McDSPFutzBox = 0x6E1003,
		McDSPLimiter = 0x671003,
		ResonanceAudio = 0x641103,
	};

	const TMap<PluginID, const char*> PluginIDToStaticLibraryName =
	{
		{ PluginID::AkAudioInput, "AkAudioInputSource" },
		{ PluginID::AkCompressor, "AkCompressorFX" },
		{ PluginID::AkConvolutionReverb, "AkConvolutionReverbFX" },
		{ PluginID::AkDelay, "AkDelayFX" },
		{ PluginID::AkExpander, "AkExpanderFX" },
		{ PluginID::AkFlanger, "AkFlangerFX" },
		{ PluginID::AkGain, "AkGainFX" },
		{ PluginID::AkGuitarDistortion, "AkGuitarDistortionFX" },
		{ PluginID::AkHarmonizer, "AkHarmonizerFX" },
		{ PluginID::AkMatrixReverb, "AkMatrixReverbFX" },
		{ PluginID::AkMeter, "AkMeterFX" },
		{ PluginID::AkMotionSink, "AkMotionSink" },
		{ PluginID::AkMotionSource, "AkMotionSourceSource" },
		{ PluginID::AkMotionGeneratorSource, "AkMotionGeneratorSource" },
		{ PluginID::AkParametricEQ, "AkParametricEQFX" },
		{ PluginID::AkPeakLimiter, "AkPeakLimiterFX" },
		{ PluginID::AkPitchShifter, "AkPitchShifterFX" },
		{ PluginID::AkRecorder, "AkRecorderFX" },
		{ PluginID::AkReflect, "AkReflectFX" },
		{ PluginID::AkRoomVerb, "AkRoomVerbFX" },
		{ PluginID::WwiseSilence, "AkSilenceSource" },
		{ PluginID::SineGenerator, "AkSineSource" },
		{ PluginID::AkSoundSeedGrain, "AkSoundSeedGrainSource" },
		{ PluginID::AkSoundSeedImpact, "AkSoundSeedImpactFX" },
		{ PluginID::AkSoundSeedWind, "AkSoundSeedWindSource" },
		{ PluginID::AkSoundSeedWoosh, "AkSoundSeedWooshSource" },
		{ PluginID::AkStereoDelay, "AkStereoDelayFX" },
		{ PluginID::AkSynthOne, "AkSynthOneSource" },
		{ PluginID::ToneGenerator, "AkToneSource" },
		{ PluginID::AkTimeStretch, "AkTimeStretchFX" },
		{ PluginID::AkTremolo, "AkTremoloFX" },
		{ PluginID::AuroHeadphone, "AuroHeadphoneFX" },
		{ PluginID::CrankcaseAudioREVModelPlayer, "CrankcaseAudioREVModelPlayerFX" },
		{ PluginID::iZHybridReverb, "iZHybridReverbFX" },
		{ PluginID::iZTrashBoxModeler, "iZTrashBoxModelerFX" },
		{ PluginID::iZTrashDelay, "iZTrashDelayFX" },
		{ PluginID::iZTrashDistortion, "iZTrashDistortionFX" },
		{ PluginID::iZTrashDynamics, "iZTrashDynamicsFX" },
		{ PluginID::iZTrashFilters, "iZTrashFiltersFX" },
		{ PluginID::iZTrashMultibandDistortion, "iZTrashMultibandDistortionFX" },
		{ PluginID::McDSPFutzBox, "McDSPFutzBoxFX" },
		{ PluginID::McDSPLimiter, "McDSPLimiterFX" },
		{ PluginID::ResonanceAudio, "ResonanceAudioFX" },
	};

	FString GetWwiseApplicationPath()
	{
		const UAkSettingsPerUser* AkSettingsPerUser = GetDefault<UAkSettingsPerUser>();
		FString ApplicationToRun;
		ApplicationToRun.Empty();

		if (AkSettingsPerUser)
		{
#if PLATFORM_WINDOWS
			ApplicationToRun = AkSettingsPerUser->WwiseWindowsInstallationPath.Path;
#else
			ApplicationToRun = AkSettingsPerUser->WwiseMacInstallationPath.FilePath;
#endif
			if (FPaths::IsRelative(ApplicationToRun))
			{
				ApplicationToRun = FPaths::ConvertRelativePathToFull(AkUnrealHelper::GetProjectDirectory(), ApplicationToRun);
			}
			if (!(ApplicationToRun.EndsWith(TEXT("/")) || ApplicationToRun.EndsWith(TEXT("\\"))))
			{
				ApplicationToRun += TEXT("/");
			}

#if PLATFORM_WINDOWS
			if (FPaths::FileExists(ApplicationToRun + TEXT("Authoring/x64/Release/bin/WwiseCLI.exe")))
			{
				ApplicationToRun += TEXT("Authoring/x64/Release/bin/WwiseCLI.exe");
			}
			else
			{
				ApplicationToRun += TEXT("Authoring/Win32/Release/bin/WwiseCLI.exe");
			}
			ApplicationToRun.ReplaceInline(TEXT("/"), TEXT("\\"));
#elif PLATFORM_MAC
			ApplicationToRun += TEXT("Contents/Tools/WwiseCLI.sh");
			ApplicationToRun = TEXT("\"") + ApplicationToRun + TEXT("\"");
#endif
		}

		return ApplicationToRun;
	}

	FString GetLinkedProjectPath()
	{
		// Get the Wwise Project Name from directory.
		const UAkSettings* AkSettings = GetDefault<UAkSettings>();
		FString ProjectPath;
		ProjectPath.Empty();

		if (AkSettings)
		{
			ProjectPath = AkSettings->WwiseProjectPath.FilePath;

			ProjectPath = FPaths::ConvertRelativePathToFull(AkUnrealHelper::GetProjectDirectory(), ProjectPath);
#if PLATFORM_WINDOWS
			ProjectPath.ReplaceInline(TEXT("/"), TEXT("\\"));
#endif
		}

		return ProjectPath;
	}

	FString GetDefaultSBDefinitionFilePath()
	{
		FString TempFileName = AkUnrealHelper::GetProjectDirectory();
		TempFileName += TEXT("TempDefinitionFile.txt");
		return TempFileName;
	}

	/**
	 * Dump the bank definition to file
	 *
	 * @param in_DefinitionFileContent	Banks to include in file
	 */
	FString DumpBankContentString(TMap<FString, TSet<UAkAudioEvent*> >& in_DefinitionFileContent)
	{
		// This generate the Bank definition file.
		// 
		FString FileContent;
		if (in_DefinitionFileContent.Num())
		{
			for (TMap<FString, TSet<UAkAudioEvent*> >::TIterator It(in_DefinitionFileContent); It; ++It)
			{
				if (It.Value().Num())
				{
					FString BankName = It.Key();
					for (TSet<UAkAudioEvent*>::TIterator ItEvent(It.Value()); ItEvent; ++ItEvent)
					{
						FString EventName = (*ItEvent)->GetName();
						FileContent += BankName + "\t\"" + EventName + "\"\n";
					}
				}
			}
		}

		return FileContent;
	}

	FString DumpBankContentString(TMap<FString, TSet<UAkAuxBus*> >& in_DefinitionFileContent)
	{
		// This generate the Bank definition file.
		// 
		FString FileContent;
		if (in_DefinitionFileContent.Num())
		{
			for (TMap<FString, TSet<UAkAuxBus*> >::TIterator It(in_DefinitionFileContent); It; ++It)
			{
				if (It.Value().Num())
				{
					FString BankName = It.Key();
					for (TSet<UAkAuxBus*>::TIterator ItAuxBus(It.Value()); ItAuxBus; ++ItAuxBus)
					{
						FString AuxBusName = (*ItAuxBus)->GetName();
						FileContent += BankName + "\t-AuxBus\t\"" + AuxBusName + "\"\n";
					}
				}
			}
		}

		return FileContent;
	}

	bool GenerateDefinitionFile(TArray< TSharedPtr<FString> >& BanksToGenerate, TMap<FString, TSet<UAkAudioEvent*> >& BankToEventSet)
	{
		TMap<FString, TSet<UAkAuxBus*> > BankToAuxBusSet;
		{
			// Force load of event assets to make sure definition file is complete
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			TArray<FAssetData> EventAssets;
			AssetRegistryModule.Get().GetAssetsByClass(UAkAudioEvent::StaticClass()->GetFName(), EventAssets);

			for (int32 AssetIndex = 0; AssetIndex < EventAssets.Num(); ++AssetIndex)
			{
				FString EventAssetPath = EventAssets[AssetIndex].ObjectPath.ToString();
				UAkAudioEvent * pEvent = LoadObject<UAkAudioEvent>(NULL, *EventAssetPath, NULL, 0, NULL);
				if (BanksToGenerate.ContainsByPredicate([&](TSharedPtr<FString> Bank) {
					if (pEvent->RequiredBank)
					{
						return pEvent->RequiredBank->GetName() == *Bank;
					}
					return false;

				}))
				{
					if (pEvent->RequiredBank)
					{
						TSet<UAkAudioEvent*>& EventPtrSet = BankToEventSet.FindOrAdd(pEvent->RequiredBank->GetName());
						EventPtrSet.Add(pEvent);
					}
				}
			}

			// Force load of AuxBus assets to make sure definition file is complete
			TArray<FAssetData> AuxBusAssets;
			AssetRegistryModule.Get().GetAssetsByClass(UAkAuxBus::StaticClass()->GetFName(), AuxBusAssets);

			for (int32 AssetIndex = 0; AssetIndex < AuxBusAssets.Num(); ++AssetIndex)
			{
				FString AuxBusAssetPath = AuxBusAssets[AssetIndex].ObjectPath.ToString();
				UAkAuxBus * pAuxBus = LoadObject<UAkAuxBus>(NULL, *AuxBusAssetPath, NULL, 0, NULL);
				if (BanksToGenerate.ContainsByPredicate([&](TSharedPtr<FString> Bank) {
					if (pAuxBus->RequiredBank)
					{
						return pAuxBus->RequiredBank->GetName() == *Bank;
					}
					return false;
				}))
				{
					if (pAuxBus->RequiredBank)
					{
						TSet<UAkAuxBus*>& EventPtrSet = BankToAuxBusSet.FindOrAdd(pAuxBus->RequiredBank->GetName());
						EventPtrSet.Add(pAuxBus);
					}
				}
			}
		}

		FString DefFileContent = DumpBankContentString(BankToEventSet);
		DefFileContent += DumpBankContentString(BankToAuxBusSet);

		if (DefFileContent.IsEmpty())
		{
			UE_LOG(LogAkBanks, Error, TEXT("Your Wwise SoundBank definition file is empty, you should add a reference to a bank in your event assets."));
			return false;
		}

		return FFileHelper::SaveStringToFile(DefFileContent, *GetDefaultSBDefinitionFilePath());
	}

	FString GetBankGenerationFullDirectory(const TCHAR * in_szPlatformDir)
	{
		FString TargetDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(AkUnrealHelper::GetSoundBankDirectory(), in_szPlatformDir));

#if PLATFORM_WINDOWS
		TargetDir.ReplaceInline(TEXT("/"), TEXT("\\"));
#else
		TargetDir.ReplaceInline(TEXT("\\"), TEXT("/"));
#endif

		return TargetDir;
	}

	struct PlatformData
	{
		FString Name;
		FString BankPath;
	};

	UAkActivatedPlugins* ActivatedPlugins = nullptr;
	UPackage* Package = nullptr;
	bool ActivatedPluginsMapHasChanged = false;

	struct CommandLineArguments
	{
		FString Url;
		FString Parameters;

		TArray<PlatformData> Platforms;

		template<bool bStaticLibraries>
		FString GetLibraryName(const FXmlNode* Plugin) const
		{
			if (!Plugin)
				return {};

			if (bStaticLibraries)
			{
				auto IdString = Plugin->GetAttribute("ID");
				if (auto Id = (PluginID)FCString::Atoi(*IdString))
					if (auto StaticLibraryName = PluginIDToStaticLibraryName.FindRef(Id))
						return StaticLibraryName;
			}

			auto DllName = Plugin->GetAttribute("DLL");
			if (DllName.Compare("DefaultSink") == 0)
				return {};

			return DllName;
		}

		template<bool bStaticLibraries>
		TArray<FString> GetLibraryNames(const PlatformData& Platform) const
		{
			FString Xml;
			auto PluginInfoPath = Platform.BankPath / "PluginInfo.xml";
			if (!FFileHelper::LoadFileToString(Xml, *PluginInfoPath))
			{
				UE_LOG(LogAkBanks, Error, TEXT("File not found: %s"), *PluginInfoPath);
				return {};
			}

			FXmlFile File(Xml, EConstructMethod::ConstructFromBuffer);
			if (!File.IsValid())
			{
				UE_LOG(LogAkBanks, Error, TEXT("Malformed file: %s"), *PluginInfoPath);
				return {};
			}

			auto PluginInfo = File.GetRootNode();
			if (!PluginInfo || PluginInfo->GetTag().Compare("PluginInfo") != 0)
			{
				UE_LOG(LogAkBanks, Error, TEXT("Malformed file: %s"), *PluginInfoPath);
				return {};
			}

			auto Plugins = PluginInfo->GetFirstChildNode();
			if (!Plugins || Plugins->GetTag().Compare("Plugins") != 0)
			{
				UE_LOG(LogAkBanks, Error, TEXT("Malformed file: %s"), *PluginInfoPath);
				return {};
			}

			TArray<FString> PluginArray;
			for (const auto& Plugin : Plugins->GetChildrenNodes())
			{
				if (!Plugin || Plugin->GetTag().Compare("Plugin") != 0)
				{
					UE_LOG(LogAkBanks, Error, TEXT("Malformed file: %s"), *PluginInfoPath);
					return {};
				}

				auto LibraryName = GetLibraryName<bStaticLibraries>(Plugin);
				if (!LibraryName.IsEmpty())
					PluginArray.Add(LibraryName);
			}

			return PluginArray;
		}

		void ModifySourceCode(const TCHAR* FilePath, const TCHAR* Tag, const TCHAR* FormatString, const TArray<FString>& PluginArray, const FString& PlatformName) const
		{
			UE_LOG(LogAkBanks, Warning, TEXT("Ensure this is source-control compatible!"));
			TArray<FString> Lines;
			const TArray<FStringFormatArg> TagFormatArgArray = { Tag };
			const auto BeginToken = FString::Format(TEXT("/// <{0}>"), TagFormatArgArray);
			const auto EndToken = FString::Format(TEXT("/// </{0}>"), TagFormatArgArray);
			if (!FFileHelper::LoadFileToStringArray(Lines, FilePath))
			{
				Lines = {
					FString::Format(TEXT("#if PLATFORM_{0}"), { PlatformName.ToUpper() }),
					BeginToken,
					EndToken,
					TEXT("#endif")
				};
			}

			auto LineCount = Lines.Num();
			auto InsertionLineNumber = 0;
			for ( ; InsertionLineNumber < LineCount; ++InsertionLineNumber)
			{
				if (Lines[InsertionLineNumber].Contains(BeginToken))
					break;
			}

			if (InsertionLineNumber >= LineCount)
			{
				UE_LOG(LogAkBanks, Warning, TEXT("Could not add Wwise plug-ins to <%s> for <%s> platform. Token not found: %s"), FilePath, *PlatformName, *BeginToken);
				return;
			}

			++InsertionLineNumber;

			auto RemoveLineCount = 0;
			for (auto i = InsertionLineNumber; i < LineCount; ++i)
			{
				if (Lines[i].Contains(EndToken))
					break;

				++RemoveLineCount;
			}

			if (InsertionLineNumber + RemoveLineCount >= LineCount)
			{
				UE_LOG(LogAkBanks, Warning, TEXT("Could not add Wwise plug-ins to <%s> for <%s> platform. Token not found: %s"), FilePath, *PlatformName, *EndToken);
				return;
			}

			if (RemoveLineCount)
			{
				Lines.RemoveAt(InsertionLineNumber, RemoveLineCount);
			}

			for (const auto& Plugin : PluginArray)
			{
				Lines.Insert(FString::Format(FormatString, { Plugin }), InsertionLineNumber++);
			}

			if (FFileHelper::SaveStringArrayToFile(Lines, FilePath))
			{
				UE_LOG(LogAkBanks, Display, TEXT("Modified <%s> for <%s> platform."), FilePath, *PlatformName);
			}
			else
			{
				UE_LOG(LogAkBanks, Warning, TEXT("Could not modify <%s> for <%s> platform."), FilePath, *PlatformName);
			}
		}

		void OutputPluginInformation() const
		{
			const auto WwisePluginInfo = IPluginManager::Get().FindPlugin("Wwise");
			for (const auto& Platform : Platforms)
			{
				// todo slaptiste: REMOVE ONCE TESTED!!!
				UE_LOG(LogAkBanks, Display, TEXT("Attempting to modify files for <%s> platform."), *Platform.Name);

				auto* PlatformInfo = UAkPlatformInfo::GetAkPlatformInfo(Platform.Name);
				if (!PlatformInfo)
				{
					UE_LOG(LogAkBanks, Warning, TEXT("AkPlatformInfo class not found for <%s> platform."), *Platform.Name);
					continue;
				}

				if (PlatformInfo->bUsesStaticLibraries)
				{
					enum { bStaticLibraries = true };
					const auto PluginArray = GetLibraryNames<bStaticLibraries>(Platform);
					const auto Tag = FString::Format(TEXT("INSERT_{0}_PLUGINS"), { PlatformInfo->WwisePlatform.ToUpper() });

					const FString AkPluginIncludeFileName = FString::Format(TEXT("Ak{0}Plugins.h"), { *PlatformInfo->WwisePlatform });
					const FString AkPluginIncludeFilePath = FPaths::Combine(WwisePluginInfo->GetBaseDir(), TEXT("Source"), TEXT("AkAudio"), TEXT("Private"), TEXT("Generated"), AkPluginIncludeFileName);
					const auto AkFactoryHeaderFormatString = TEXT("#include <AK/Plugin/{0}Factory.h>");
					ModifySourceCode(*AkPluginIncludeFilePath, *Tag, AkFactoryHeaderFormatString, PluginArray, PlatformInfo->WwisePlatform);
				}

				if (ActivatedPlugins)
				{
					enum { bStaticLibraries = false };
					const auto PluginArray = GetLibraryNames<bStaticLibraries>(Platform);
					if (PluginArray.Num() == 0)
					{
						UE_LOG(LogAkBanks, Display, TEXT("No Wwise plug-ins required for <%s> platform."), *PlatformInfo->WwisePlatform);
					}

					auto& PluginListStruct = ActivatedPlugins->Platforms.FindOrAdd(PlatformInfo->WwisePlatform);
					if (PluginListStruct.PluginNames != PluginArray)
					{
						ActivatedPluginsMapHasChanged = true;
						PluginListStruct.PluginNames = PluginArray;
					}
				}
			}

			if (ActivatedPlugins && ActivatedPluginsMapHasChanged)
			{
				ActivatedPlugins->MarkPackageDirty();

				FString PackageFilename;
				if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
				{
					AsyncTask(ENamedThreads::GameThread, [PackageFilename]
					{
						SavePackageHelper(Package, PackageFilename);
					});
				}
			}
		}
	};

	CommandLineArguments GenerateCommandLineArguments(const TArray<TSharedPtr<FString>>& BankNames, TArray<TSharedPtr<FString>>& PlatformNames, const FString* WwisePathOverride = nullptr)
	{
		//////////////////////////////////////////////////////////////////////////////////////
		// For more information about how to generate banks using the command line, 
		// look in the Wwise SDK documentation 
		// in the section "Generating Banks from the Command Line"
		//////////////////////////////////////////////////////////////////////////////////////

		if (auto* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>())
		{
			static const FDirectoryPath WwiseEditorOnlyPath{ "/Game/Wwise/EditorOnly" };

			auto i = 0;
			auto n = PackagingSettings->DirectoriesToNeverCook.Num();
			for (; i < n; ++i)
			{
				if (PackagingSettings->DirectoriesToNeverCook[i].Path == WwiseEditorOnlyPath.Path)
				{
					break;
				}
			}

			if (i >= n)
			{
				PackagingSettings->DirectoriesToNeverCook.Add(WwiseEditorOnlyPath);
				PackagingSettings->UpdateDefaultConfigFile();
			}
		}

		Package = CreatePackage(nullptr, *UAkActivatedPlugins::GetPackageName());
		const auto FilePath = *UAkActivatedPlugins::GetFilePath();
		ActivatedPlugins = LoadObject<UAkActivatedPlugins>(Package, *FString("ActivatedPlugins"), FilePath, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone);
		ActivatedPluginsMapHasChanged = !ActivatedPlugins;

		if (!ActivatedPlugins)
		{
			UE_LOG(LogAkBanks, Display, TEXT("<%s> not found."), FilePath);

			ActivatedPlugins = NewObject<UAkActivatedPlugins>(Package, *FString("ActivatedPlugins"), EObjectFlags::RF_Public | EObjectFlags::RF_Standalone);
			FAssetRegistryModule::AssetCreated(ActivatedPlugins);
		}

		CommandLineArguments Arguments;

#if PLATFORM_WINDOWS
		FString& ApplicationPath = Arguments.Url;
#else
		Arguments.Url = TEXT("/bin/sh");
		FString& ApplicationPath = Arguments.Parameters;
#endif

		ApplicationPath = WwisePathOverride ? *WwisePathOverride : GetWwiseApplicationPath();

		Arguments.Parameters += FString::Printf(TEXT(" \"%s\""), *IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*GetLinkedProjectPath()));
		Arguments.Parameters += TEXT(" -GenerateSoundBanks");

		for (auto i = 0; i < BankNames.Num(); i++)
		{
			Arguments.Parameters += FString::Printf(TEXT(" -Bank %s"), **BankNames[i]);
		}

		Arguments.Parameters += FString::Printf(TEXT(" -ImportDefinitionFile \"%s\""), *IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*GetDefaultSBDefinitionFilePath()));

		// We could also specify the -Save flag.
		// It would cause the newly imported definition files to be persisted in the Wwise project files.
		// On the other hand, saving the project could cause the project currently being edited to be 
		// dirty if Wwise application is already running along with UnrealEditor, and the user would be 
		// prompted to either discard changes and reload the project, losing local changes.
		// You can uncomment the following line if you want the Wwise project to be saved in this process.
		// By default, we prefer to not save it.
		// 
		// Arguments.Parameters += TEXT(" -Save");
		//

		if (PlatformNames.Num() == 0)
		{
			PlatformNames = AkUnrealPlatformHelper::GetAllSupportedWwisePlatforms();
		}

		for (auto i = 0; i < PlatformNames.Num(); i++)
		{
			auto PlatformName = *PlatformNames[i];
			auto BankPath = GetBankGenerationFullDirectory(*PlatformName);
			Arguments.Platforms.Add(PlatformData{ PlatformName, BankPath });

#if PLATFORM_MAC
			// Workaround: This parameter does not work with Unix-style paths. convert it to Windows style.
			Arguments.Parameters += FString::Printf(TEXT(" -Platform %s -SoundBankPath %s \"Z:%s\""), *PlatformName, *PlatformName, *BankPath);
#else
			Arguments.Parameters += FString::Printf(TEXT(" -Platform %s -SoundBankPath %s \"%s\""), *PlatformName, *PlatformName, *BankPath);
#endif
		}

		// Here you can specify languages, if no language is specified, all languages from the Wwise project.
		// will be built.
		//#if PLATFORM_WINDOWS
		//		Arguments.Parameters += TEXT(" -Language English(US)");
		//#else
		//		Arguments.Parameters += TEXT(" -Language English\\(US\\)");
		//#endif

		return Arguments;
	}

	class FSoundBanksNotificationHandler
	{
	public:
		FSoundBanksNotificationHandler(TWeakPtr<SNotificationItem> InNotificationItemPtr, SNotificationItem::ECompletionState InCompletionState, const FText& InText, FOnBankGenerationComplete InOnSoundBankGenerationComplete, bool InExpireAndFadeout = true)
			: CompletionState(InCompletionState)
			, NotificationItemPtr(InNotificationItemPtr)
			, Text(InText)
			, bExpireAndFadeout(InExpireAndFadeout)
			, OnSoundBankGenerationComplete(InOnSoundBankGenerationComplete)
		{ }

		static void HandleDismissButtonClicked()
		{
			TSharedPtr<SNotificationItem> NotificationItem = ExpireNotificationItemPtr.Pin();
			if (NotificationItem.IsValid())
			{
				NotificationItem->SetExpireDuration(0.0f);
				NotificationItem->SetFadeOutDuration(0.0f);
				NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
				NotificationItem->ExpireAndFadeout();
				ExpireNotificationItemPtr.Reset();
			}
		}

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			if (NotificationItemPtr.IsValid())
			{
				TSharedPtr<SNotificationItem> NotificationItem = NotificationItemPtr.Pin();
				NotificationItem->SetText(Text);

				if (bExpireAndFadeout)
				{
					ExpireNotificationItemPtr.Reset();
					NotificationItem->SetExpireDuration(3.0f);
					NotificationItem->SetFadeOutDuration(0.5f);
					NotificationItem->ExpireAndFadeout();
				}
				else
				{
					// Handling the notification expiration in callback
					ExpireNotificationItemPtr = NotificationItem;
				}
				NotificationItem->SetCompletionState(CompletionState);
				if (CompletionState == SNotificationItem::CS_Fail)
				{
					GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
				}
				else
				{
					GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
				}

				OnSoundBankGenerationComplete.ExecuteIfBound(CompletionState == SNotificationItem::CS_Success);
			}
		}

		static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
		ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FSoundBanksNotificationHandler, STATGROUP_TaskGraphTasks);
		}

	private:
		static TWeakPtr<SNotificationItem> ExpireNotificationItemPtr;

		SNotificationItem::ECompletionState CompletionState;
		TWeakPtr<SNotificationItem> NotificationItemPtr;
		FText Text;
		bool bExpireAndFadeout;
		FOnBankGenerationComplete OnSoundBankGenerationComplete;
	};

	TWeakPtr<SNotificationItem> FSoundBanksNotificationHandler::ExpireNotificationItemPtr;

	void HandleWwiseCliProcessCompleted(int32 ReturnCode, TWeakPtr<SNotificationItem> NotificationItemPtr, FOnBankGenerationComplete OnSoundBankGenerationComplete)
	{
		if (ReturnCode == 0 || ReturnCode == 2)
		{
			TGraphTask<FSoundBanksNotificationHandler>::CreateTask().ConstructAndDispatchWhenReady(
				NotificationItemPtr,
				SNotificationItem::CS_Success,
				LOCTEXT("SoundBankGenerationSucceeded", "SoundBank generation complete!"),
				OnSoundBankGenerationComplete
			);
		}
		else
		{
			TGraphTask<FSoundBanksNotificationHandler>::CreateTask().ConstructAndDispatchWhenReady(
				NotificationItemPtr,
				SNotificationItem::CS_Fail,
				LOCTEXT("SoundBankGenerationFailed", "SoundBank generation failed!"),
				OnSoundBankGenerationComplete
			);
		}
	}
	
	void HandleWwiseCliProcessCanceled(TWeakPtr<SNotificationItem> NotificationItemPtr, FOnBankGenerationComplete OnSoundBankGenerationComplete)
	{
		TGraphTask<FSoundBanksNotificationHandler>::CreateTask().ConstructAndDispatchWhenReady(
			NotificationItemPtr,
			SNotificationItem::CS_Fail,
			LOCTEXT("SoundBankGenerationCanceled", "SoundBank generation canceled!"),
			OnSoundBankGenerationComplete
		);
	}

	static void ProcessWwiseCliOutput(FString Output, TWeakPtr<SNotificationItem> NotificationItemPtr)
	{
		if (!Output.IsEmpty() && !Output.Equals("\r"))
		{
			UE_LOG(LogAkBanks, Display, TEXT("%s"), *Output);
		}
	}

	int32 GenerateSoundBanksBlocking(TArray< TSharedPtr<FString> >& in_rBankNames, TArray< TSharedPtr<FString> >& in_PlatformNames, const FString* WwisePathOverride/* = nullptr*/)
	{
		auto Arguments = GenerateCommandLineArguments(in_rBankNames, in_PlatformNames, WwisePathOverride);
		UE_LOG(LogAkBanks, Log, TEXT("Starting Wwise SoundBank generation with the following command line:"));
		UE_LOG(LogAkBanks, Log, TEXT("%s %s"), *Arguments.Url, *Arguments.Parameters);

		int32 ReturnCode;
		// Create a pipe for the child process's STDOUT.
		void* WritePipe;
		void* ReadPipe;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
		FProcHandle ProcHandle = FPlatformProcess::CreateProc(*Arguments.Url, *Arguments.Parameters, true, false, false, nullptr, 0, nullptr, WritePipe);
		if (ProcHandle.IsValid())
		{
			FString NewLine;
			FPlatformProcess::Sleep(0.1f);
			// Wait for it to finish and get return code
			while (FPlatformProcess::IsProcRunning(ProcHandle) == true)
			{
				NewLine = FPlatformProcess::ReadPipe(ReadPipe);
				if (NewLine.Len() > 0)
				{
					UE_LOG(LogAkBanks, Display, TEXT("%s"), *NewLine);
					NewLine.Empty();
				}
				FPlatformProcess::Sleep(0.25f);
			}

			NewLine = FPlatformProcess::ReadPipe(ReadPipe);
			if (NewLine.Len() > 0)
			{
				UE_LOG(LogAkBanks, Display, TEXT("%s"), *NewLine);
			}

			FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
			Arguments.OutputPluginInformation();

			switch (ReturnCode)
			{
			case 2:
				UE_LOG(LogAkBanks, Warning, TEXT("Wwise command-line completed with warnings."));
				break;
			case 0:
				UE_LOG(LogAkBanks, Display, TEXT("Wwise command-line successfully completed."));
				break;
			default:
				UE_LOG(LogAkBanks, Error, TEXT("Wwise command-line failed with error %d."), ReturnCode);
				break;
			}
		}
		else
		{
			ReturnCode = -1;
			// Most chances are the path to the .exe or the project were not set properly in GEditorIni file.
			UE_LOG(LogAkBanks, Error, TEXT("Failed to run Wwise command-line: %s %s"), *Arguments.Url, *Arguments.Parameters);
		}

		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		return ReturnCode;
	}

	void GenerateSoundBanksNonBlocking(TArray< TSharedPtr<FString> >& in_rBankNames, TArray< TSharedPtr<FString> >& in_PlatformNames, FOnBankGenerationComplete OnSoundBankGenerationComplete)
	{
		if (in_rBankNames.Num() == 0)
		{
			return;
		}

		auto Arguments = GenerateCommandLineArguments(in_rBankNames, in_PlatformNames);

		// To get more information on how banks can be generated from the command lines.
		// Refer to the section: Generating Banks from the Command Line in the Wwise SDK documentation.
		UE_LOG(LogAkBanks, Log, TEXT("Starting Wwise SoundBank generation with the following command line:"));
		UE_LOG(LogAkBanks, Log, TEXT("%s %s"), *Arguments.Url, *Arguments.Parameters);

		TSharedPtr<FMonitoredProcess> WwiseCliProcess = MakeShareable(new FMonitoredProcess(Arguments.Url, Arguments.Parameters, true));
		FNotificationInfo Info(LOCTEXT("GeneratingSoundBanks", "Generating SoundBanks..."));

		Info.Image = FAkAudioStyle::GetBrush(TEXT("AudiokineticTools.AkPickerTabIcon"));
		Info.bFireAndForget = false;
		Info.FadeOutDuration = 0.0f;
		Info.ExpireDuration = 0.0f;
		Info.Hyperlink = FSimpleDelegate::CreateLambda([] { FGlobalTabmanager::Get()->InvokeTab(FName("OutputLog")); });
		Info.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
		Info.ButtonDetails.Add(
			FNotificationButtonInfo(
				LOCTEXT("UatTaskCancel", "Cancel"),
				LOCTEXT("UatTaskCancelToolTip", "Cancels execution of this task."),
				FSimpleDelegate::CreateLambda([=]
		{
			if (WwiseCliProcess.IsValid())
			{
				WwiseCliProcess->Cancel(true);
			}
		}),
				SNotificationItem::CS_Pending
			)
		);
		Info.ButtonDetails.Add(
			FNotificationButtonInfo(
				LOCTEXT("UatTaskDismiss", "Dismiss"),
				FText(),
				FSimpleDelegate::CreateStatic(&FSoundBanksNotificationHandler::HandleDismissButtonClicked),
				SNotificationItem::CS_Fail
			)
		);

		TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
		if (!NotificationItem.IsValid())
		{
			return;
		}

		NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);

		// launch the packager
		TWeakPtr<SNotificationItem> NotificationItemPtr(NotificationItem);

		WwiseCliProcess->OnCanceled().BindStatic(&HandleWwiseCliProcessCanceled, NotificationItemPtr, OnSoundBankGenerationComplete);
		WwiseCliProcess->OnCompleted().BindLambda([Arguments, NotificationItemPtr, OnSoundBankGenerationComplete](int32 ReturnCode)
		{
			HandleWwiseCliProcessCompleted(ReturnCode, NotificationItemPtr, OnSoundBankGenerationComplete);
			Arguments.OutputPluginInformation();
		});

		WwiseCliProcess->OnOutput().BindStatic(&ProcessWwiseCliOutput, NotificationItemPtr);

		if (WwiseCliProcess->Launch())
		{
			GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileStart_Cue.CompileStart_Cue"));
			return;
		}

		GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
		NotificationItem->SetText(LOCTEXT("UatLaunchFailedNotification", "Failed to launch Unreal Automation Tool (UAT)!"));
		NotificationItem->SetExpireDuration(3.0f);
		NotificationItem->SetFadeOutDuration(0.5f);
		NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
		NotificationItem->ExpireAndFadeout();
	}

	// Gets most recently modified JSON SoundBank metadata file
	struct BankNameToPath : private IPlatformFile::FDirectoryVisitor
	{
		FString BankPath;

		BankNameToPath(const FString& BankName, const TCHAR* BaseDirectory, IPlatformFile& PlatformFile)
			: BankName(BankName), PlatformFile(PlatformFile)
		{
			Visit(BaseDirectory, true);
			PlatformFile.IterateDirectory(BaseDirectory, *this);
		}

		bool IsValid() const { return StatData.bIsValid; }

	private:
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (bIsDirectory)
			{
				FString NewBankPath = FilenameOrDirectory;
				NewBankPath += TEXT("\\");
				NewBankPath += BankName + TEXT(".json");

				FFileStatData NewStatData = PlatformFile.GetStatData(*NewBankPath);
				if (NewStatData.bIsValid && !NewStatData.bIsDirectory)
				{
					if (!StatData.bIsValid || NewStatData.ModificationTime > StatData.ModificationTime)
					{
						StatData = NewStatData;
						BankPath = NewBankPath;
					}
				}
			}

			return true;
		}

		const FString& BankName;
		IPlatformFile& PlatformFile;
		FFileStatData StatData;
	};

	void FetchAttenuationInfo(const TMap<FString, TSet<UAkAudioEvent*> >& BankToEventSet)
	{
		FString PlatformName = GetTargetPlatformManagerRef().GetRunningTargetPlatform()->PlatformName();
		FString BankBasePath = WwiseBnkGenHelper::GetBankGenerationFullDirectory(*PlatformName);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const TCHAR* BaseDirectory = *BankBasePath;

		FString FileContents; // cache the file contents - in case we are opening large files

		for (TMap<FString, TSet<UAkAudioEvent*> >::TConstIterator BankIt(BankToEventSet); BankIt; ++BankIt)
		{
			FString BankName = BankIt.Key();
			BankNameToPath NameToPath(BankName, BaseDirectory, PlatformFile);

			if (NameToPath.IsValid())
			{
				const TCHAR* BankPath = *NameToPath.BankPath;
				const TSet<UAkAudioEvent*>& EventsInBank = BankIt.Value();

				FileContents.Reset();
				if (!FFileHelper::LoadFileToString(FileContents, BankPath))
				{
					UE_LOG(LogAkBanks, Warning, TEXT("Failed to load file contents of JSON SoundBank metadata file: %s"), BankPath);
					continue;
				}

				TSharedPtr<FJsonObject> JsonObject;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);

				if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
				{
					UE_LOG(LogAkBanks, Warning, TEXT("Unable to parse JSON SoundBank metadata file: %s"), BankPath);
					continue;
				}

				const TSharedPtr<FJsonObject>* SoundBanksInfo = nullptr;
				if (!JsonObject->TryGetObjectField("SoundBanksInfo", SoundBanksInfo))
				{
					UE_LOG(LogAkBanks, Warning, TEXT("Malformed JSON SoundBank metadata file: %s"), BankPath);
					continue;
				}

				const TArray<TSharedPtr<FJsonValue>>* SoundBanks = nullptr;
				if (!(*SoundBanksInfo)->TryGetArrayField("SoundBanks", SoundBanks))
				{
					UE_LOG(LogAkBanks, Warning, TEXT("Malformed JSON SoundBank metadata file: %s"), BankPath);
					continue;
				}

				TSharedPtr<FJsonObject> Obj = (*SoundBanks)[0]->AsObject();
				const TArray<TSharedPtr<FJsonValue>>* Events;
				if (!Obj->TryGetArrayField("IncludedEvents", Events))
				{
					// If we get here, it is because we are parsing a SoundBank that has no events - possibly containing external sources
					continue;
				}

				for (int i = 0; i < Events->Num(); i++)
				{
					TSharedPtr<FJsonObject> EventObj = (*Events)[i]->AsObject();

					FString EventName;
					if (!EventObj->TryGetStringField("Name", EventName))
						continue;

					UAkAudioEvent* Event = nullptr;
					for (auto TestEvent : EventsInBank)
					{
						if (TestEvent->GetName() == EventName)
						{
							Event = TestEvent;
							break;
						}
					}

					if (Event == nullptr)
						continue;

					bool Changed = false;
					FString ValueString;
					if (EventObj->TryGetStringField("MaxAttenuation", ValueString))
					{
						const float EventRadius = FCString::Atof(*ValueString);
						if (Event->MaxAttenuationRadius != EventRadius)
						{
							Event->MaxAttenuationRadius = EventRadius;
							Changed = true;
						}
					}
					else if (Event->MaxAttenuationRadius != 0)
					{
						// No attenuation info in json file, set to 0.
						Event->MaxAttenuationRadius = 0;
						Changed = true;
					}

					// if we can't find "DurationType", then we assume infinite
					const bool IsInfinite = !EventObj->TryGetStringField("DurationType", ValueString) || (ValueString == "Infinite") || (ValueString == "Unknown");
					if (Event->IsInfinite != IsInfinite)
					{
						Event->IsInfinite = IsInfinite;
						Changed = true;
					}

					if (!IsInfinite)
					{
						if (EventObj->TryGetStringField("DurationMin", ValueString))
						{
							const float DurationMin = FCString::Atof(*ValueString);
							if (Event->MinimumDuration != DurationMin)
							{
								Event->MinimumDuration = DurationMin;
								Changed = true;
							}
						}

						if (EventObj->TryGetStringField("DurationMax", ValueString))
						{
							const float DurationMax = FCString::Atof(*ValueString);
							if (Event->MaximumDuration != DurationMax)
							{
								Event->MaximumDuration = DurationMax;
								Changed = true;
							}
						}
					}

					if (Changed)
					{
						Event->Modify(true);
					}
				}
			}
		}
	}

	void CreateGenerateSoundBankWindow(TArray<TWeakObjectPtr<UAkAudioBank>>* pSoundBanks, bool in_bShouldSaveWwiseProject/* = false*/)
	{
		TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
			.Title(LOCTEXT("AkAudioGenerateSoundBanks", "Generate SoundBanks"))
			//.CenterOnScreen(true)
			.ClientSize(FVector2D(600.f, 332.f))
			.SupportsMaximize(false).SupportsMinimize(false)
			.SizingRule(ESizingRule::FixedSize)
			.FocusWhenFirstShown(true);

		TSharedRef<SGenerateSoundBanks> WindowContent = SNew(SGenerateSoundBanks, pSoundBanks);
		WindowContent->SetShouldSaveWwiseProject(in_bShouldSaveWwiseProject);
		if (!WindowContent->ShouldDisplayWindow())
		{
			return;
		}

		// Add our SGenerateSoundBanks to the window
		WidgetWindow->SetContent(WindowContent);

		// Set focus to our SGenerateSoundBanks widget, so our keyboard keys work right off the bat
		WidgetWindow->SetWidgetToFocusOnActivate(WindowContent);

		// This creates a windows that blocks the rest of the UI. You can only interact with the "Generate SoundBanks" window.
		// If you choose to use this, comment the rest of the function.
		//GEditor->EditorAddModalWindow(WidgetWindow);

		// This creates a window that still allows you to interact with the rest of the editor. If there is an attempt to delete
		// a UAkAudioBank (from the content browser) while this window is opened, the editor will generate a (cryptic) error message
		TSharedPtr<SWindow> ParentWindow;
		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow = MainFrame.GetParentWindow();
		}

		if (ParentWindow.IsValid())
		{
			// Parent the window to the main frame 
			FSlateApplication::Get().AddWindowAsNativeChild(WidgetWindow, ParentWindow.ToSharedRef());
		}
		else
		{
			// Spawn new window
			FSlateApplication::Get().AddWindow(WidgetWindow);
		}
	}

}

#undef LOCTEXT_NAMESPACE
