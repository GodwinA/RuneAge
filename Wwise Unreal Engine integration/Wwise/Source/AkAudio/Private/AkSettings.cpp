// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved

#include "AkSettings.h"
#include "AkAudioDevice.h"
#include "AkUnrealHelper.h"
#include "AkSettingsPerUser.h"

#if WITH_EDITOR
#include "Settings/ProjectPackagingSettings.h"
#include "Platforms/AkUEPlatform.h"
#include "InitializationSettings/AkInitializationSettings.h"
#endif

#include "UObject/UnrealType.h"
#include "AssetRegistryModule.h"
#include "AkAcousticTexture.h"

#include "StringMatchAlgos/StringMatching.h"
#include "StringMatchAlgos/Array2D.h"

//////////////////////////////////////////////////////////////////////////
// UAkSettings

namespace AkSettings_Helper
{
	void MigrateMultiCoreRendering(bool EnableMultiCoreRendering, const FString& PlatformName)
	{
		FString SettingsClassName = FString::Format(TEXT("Ak{0}InitializationSettings"), { *PlatformName });
		auto* SettingsClass = FindObject<UClass>(ANY_PACKAGE, *SettingsClassName);
		if (!SettingsClass)
		{
			return;
		}

		auto* MigrationFunction = SettingsClass->FindFunctionByName(TEXT("MigrateMultiCoreRendering"));
		auto* Settings = SettingsClass->GetDefaultObject();
		if (!MigrationFunction || !Settings)
		{
			return;
		}

		Settings->ProcessEvent(MigrationFunction, &EnableMultiCoreRendering);
		Settings->UpdateDefaultConfigFile();
	}

	void MatchAcousticTextureNamesToPhysMaterialNames(
		const TArray<FAssetData>& PhysicalMaterials,
		const TArray<FAssetData>& AcousticTextures,
		TArray<int32>& assignments)
	{
		uint32 NumPhysMat = (uint32)PhysicalMaterials.Num();
		uint32 NumAcousticTex = (uint32)AcousticTextures.Num();

		// Create a scores matrix
		Array2D<float> scores(NumPhysMat, NumAcousticTex, 0);

		for (uint32 i = 0; i < NumPhysMat; ++i)
		{
			TArray<bool> perfectObjectMatches;
			perfectObjectMatches.Init(false, NumAcousticTex);

			if (PhysicalMaterials[i].GetAsset())
			{
				FString physMaterialName = PhysicalMaterials[i].GetAsset()->GetFName().ToString();

				if (physMaterialName.Len() == 0)
					continue;

				for (uint32 j = 0; j < NumAcousticTex; ++j)
				{
					// Skip objects for which we already found a perfect match
					if (perfectObjectMatches[j] == true)
						continue;

					if (AcousticTextures[j].GetAsset())
					{
						FString acousticTextureName = AcousticTextures[j].GetAsset()->GetFName().ToString();

						if (acousticTextureName.Len() == 0)
							continue;

						// Calculate longest common substring length
						float lcs = LCS::GetLCSScore(
							physMaterialName.ToLower(),
							acousticTextureName.ToLower());

						scores(i, j) = lcs;

						if (FMath::IsNearlyEqual(lcs, 1.f))
						{
							assignments[i] = j;
							perfectObjectMatches[j] = true;
							break;
						}
					}
				}
			}
		}

		for (uint32 i = 0; i < NumPhysMat; ++i)
		{
			if (assignments[i] == -1)
			{
				float bestScore = 0.f;
				int32 matchedIdx = -1;
				for (uint32 j = 0; j < NumAcousticTex; ++j)
				{
					if (scores(i, j) > bestScore)
					{
						bestScore = scores(i, j);
						matchedIdx = j;
					}
				}
				if (bestScore >= 0.2f)
					assignments[i] = matchedIdx;
			}
		}
	}
}

FString UAkSettings::DefaultSoundBankFolder = TEXT("WwiseAudio");

UAkSettings::UAkSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WwiseSoundBankFolder.Path = DefaultSoundBankFolder;

#if WITH_EDITOR
	AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	//register to asset modification delegates
	auto& AssetRegistry = AssetRegistryModule->Get();
	AssetRegistry.OnAssetAdded().AddUObject(this, &UAkSettings::OnAssetAdded);
	AssetRegistry.OnAssetRemoved().AddUObject(this, &UAkSettings::OnAssetRemoved);
#endif
}

void UAkSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	UAkSettingsPerUser* AkSettingsPerUser = GetMutableDefault<UAkSettingsPerUser>();

	if (AkSettingsPerUser)
	{
		bool didChanges = false;

		if (!WwiseWindowsInstallationPath_DEPRECATED.Path.IsEmpty())
		{
			AkSettingsPerUser->WwiseWindowsInstallationPath = WwiseWindowsInstallationPath_DEPRECATED;
			WwiseWindowsInstallationPath_DEPRECATED.Path.Reset();
			didChanges = true;
		}

		if (!WwiseMacInstallationPath_DEPRECATED.FilePath.IsEmpty())
		{
			AkSettingsPerUser->WwiseMacInstallationPath = WwiseMacInstallationPath_DEPRECATED;
			WwiseMacInstallationPath_DEPRECATED.FilePath.Reset();
			didChanges = true;
		}

		if (didChanges)
		{
			UpdateDefaultConfigFile();
			AkSettingsPerUser->SaveConfig();
		}
	}

	if (!MigratedEnableMultiCoreRendering)
	{
		MigratedEnableMultiCoreRendering = true;

		for (const auto& PlatformName : AkUnrealPlatformHelper::GetAllSupportedWwisePlatforms())
		{
			AkSettings_Helper::MigrateMultiCoreRendering(bEnableMultiCoreRendering_DEPRECATED, *PlatformName);
		}
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UAkSettings::PreEditChange(UProperty* PropertyAboutToChange)
{
	PreviousWwiseProjectPath = WwiseProjectPath.FilePath;
	PreviousSoundBankFolder = WwiseSoundBankFolder.Path;
}

void UAkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if ( PropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, MaxSimultaneousReverbVolumes) )
	{
		MaxSimultaneousReverbVolumes = FMath::Clamp<uint8>( MaxSimultaneousReverbVolumes, 0, AK_MAX_AUX_PER_OBJ );
		FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
		if( AkAudioDevice )
		{
			AkAudioDevice->SetMaxAuxBus(MaxSimultaneousReverbVolumes);
		}
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, WwiseProjectPath))
	{
		AkUnrealHelper::SanitizeProjectPath(WwiseProjectPath.FilePath, PreviousWwiseProjectPath, FText::FromString("Please enter a valid Wwise project"), bRequestRefresh);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, bAutoConnectToWAAPI))
	{
		OnAutoConnectChanged.Broadcast();
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, WwiseSoundBankFolder))
	{
		EnsureSoundBankPathIsInPackagingSettings();
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, AkGeometryMap))
	{
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FAkGeometrySurfacePropertiesToMap, AcousticTexture))
		{
			for (auto& elem : AkGeometryMap)
			{
				PhysicalMaterialAcousticTextureMap[elem.Key.LoadSynchronous()] = elem.Value.AcousticTexture.LoadSynchronous();
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAkGeometrySurfacePropertiesToMap, OcclusionValue))
		{
			for (auto& elem : AkGeometryMap)
			{
				PhysicalMaterialOcclusionMap[elem.Key.LoadSynchronous()] = elem.Value.OcclusionValue;
			}
		}
	}

	PreviousSoundBankFolder.Empty();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAkSettings::EnsureSoundBankPathIsInPackagingSettings() 
{
	UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();

	bool foundPackageDirectory = false;
	bool packageSettingsNeedUpdate = false;

	for (int32 i = 0; i < PackagingSettings->DirectoriesToAlwaysStageAsUFS.Num(); i++)
	{
		if (PackagingSettings->DirectoriesToAlwaysStageAsUFS[i].Path == WwiseSoundBankFolder.Path)
		{
			foundPackageDirectory = true;
			break;
		}
	}

	if (!foundPackageDirectory)
	{
		PackagingSettings->DirectoriesToAlwaysStageAsUFS.Add(WwiseSoundBankFolder);
		packageSettingsNeedUpdate = true;
	}

	if (!PreviousSoundBankFolder.IsEmpty())
	{
		for (int i = 0; i < PackagingSettings->DirectoriesToAlwaysStageAsUFS.Num(); i++)
		{
			if (PackagingSettings->DirectoriesToAlwaysStageAsUFS[i].Path == PreviousSoundBankFolder)
			{
				PackagingSettings->DirectoriesToAlwaysStageAsUFS.RemoveAt(i);
				packageSettingsNeedUpdate = true;
			}
		}
	}

	if (packageSettingsNeedUpdate)
	{
		PackagingSettings->UpdateDefaultConfigFile();
	}
}

void UAkSettings::FillAkGeometryMap(
	const TArray<FAssetData>& PhysicalMaterialAssets,
	const TArray<FAssetData>& AcousticTextureAssets)
{
	TArray<int32> assignments;
	assignments.Init(-1, PhysicalMaterialAssets.Num());

	AkSettings_Helper::MatchAcousticTextureNamesToPhysMaterialNames(PhysicalMaterialAssets, AcousticTextureAssets, assignments);

	for (int i = 0; i < PhysicalMaterialAssets.Num(); i++)
	{
		auto physicalMaterial = Cast<UPhysicalMaterial>(PhysicalMaterialAssets[i].GetAsset());
		if (!PhysicalMaterialAcousticTextureMap.Contains(physicalMaterial))
		{
			if (assignments[i] != -1)
			{
				int32 acousticTextureIdx = assignments[i];
				auto acousticTexture = Cast<UAkAcousticTexture>(AcousticTextureAssets[acousticTextureIdx].GetAsset());
				PhysicalMaterialAcousticTextureMap.Add(physicalMaterial, acousticTexture);
			}
			else
			{
				PhysicalMaterialAcousticTextureMap.Add(physicalMaterial);
			}
		}
		else
		{
			if (assignments[i] != -1)
			{
				if (!PhysicalMaterialAcousticTextureMap[physicalMaterial])
				{
					int32 acousticTextureIdx = assignments[i];
					auto acousticTexture = Cast<UAkAcousticTexture>(AcousticTextureAssets[acousticTextureIdx].GetAsset());
					PhysicalMaterialAcousticTextureMap[physicalMaterial] = acousticTexture;
				}
			}
		}

		if (!PhysicalMaterialOcclusionMap.Contains(physicalMaterial))
			PhysicalMaterialOcclusionMap.Add(physicalMaterial, 1.f);
	}

	UpdateAkGeometryMap();
}

void UAkSettings::UpdateAkGeometryMap()
{
	AkGeometryMap.Empty();
	for (auto& elem : PhysicalMaterialAcousticTextureMap)
	{
		TSoftObjectPtr<class UPhysicalMaterial> physMatPtr(elem.Key);
		TSoftObjectPtr<class UAkAcousticTexture> acousticTexPtr(elem.Value);
		FAkGeometrySurfacePropertiesToMap props;
		props.AcousticTexture = acousticTexPtr;
		props.OcclusionValue = PhysicalMaterialOcclusionMap[elem.Key];
		AkGeometryMap.Emplace(physMatPtr, props);
	}

	AkGeometryMap.KeySort([](const TSoftObjectPtr<class UPhysicalMaterial>& Lhs, const TSoftObjectPtr<class UPhysicalMaterial>& Rhs) {
		UPhysicalMaterial* lhs = Lhs.Get();
		UPhysicalMaterial* rhs = Rhs.Get();
		if (lhs && rhs)
		{
#if UE_4_23_OR_LATER
			return Lhs.Get()->GetFName().LexicalLess(Rhs.Get()->GetFName());
#else
			return Lhs.Get()->GetFName() < Rhs.Get()->GetFName();
#endif
		}
		else
			return !lhs ? true : false;
	});
}

void UAkSettings::InitAkGeometryMap()
{
	PhysicalMaterialAcousticTextureMap.Empty();
	PhysicalMaterialOcclusionMap.Empty();

	// copy everything from the ini file
	for (auto& elem : AkGeometryMap)
	{
		auto physMat = elem.Key.LoadSynchronous();
		auto surfaceProps = elem.Value;
		PhysicalMaterialAcousticTextureMap.Add(physMat, surfaceProps.AcousticTexture.LoadSynchronous());
		PhysicalMaterialOcclusionMap.Add(physMat, surfaceProps.OcclusionValue);
	}
	bAkGeometryMapInitialized = true;

	// Obtain the 2 list of children we want to match
	TArray<FAssetData> PhysicalMaterials, AcousticTextures;
	AssetRegistryModule->Get().GetAssetsByClass(UPhysicalMaterial::StaticClass()->GetFName(), PhysicalMaterials);
	AssetRegistryModule->Get().GetAssetsByClass(UAkAcousticTexture::StaticClass()->GetFName(), AcousticTextures);

	FillAkGeometryMap(PhysicalMaterials, AcousticTextures);
}

void UAkSettings::OnAssetAdded(const FAssetData& NewAssetData)
{
	if (!bAkGeometryMapInitialized)
		return;

	if (auto physicalMaterial = Cast<UPhysicalMaterial>(NewAssetData.GetAsset()))
	{
		TArray<FAssetData> PhysicalMaterials, AcousticTextures;
		PhysicalMaterials.Add(NewAssetData);
		AssetRegistryModule->Get().GetAssetsByClass(UAkAcousticTexture::StaticClass()->GetFName(), AcousticTextures);

		FillAkGeometryMap(PhysicalMaterials, AcousticTextures);
	}

	if (auto acousticTexture = Cast<UAkAcousticTexture>(NewAssetData.GetAsset()))
	{
		TArray<FAssetData> PhysicalMaterials, AcousticTextures;
		AssetRegistryModule->Get().GetAssetsByClass(UPhysicalMaterial::StaticClass()->GetFName(), PhysicalMaterials);
		AcousticTextures.Add(NewAssetData);

		FillAkGeometryMap(PhysicalMaterials, AcousticTextures);
	}
}

void UAkSettings::OnAssetRemoved(const struct FAssetData& AssetData)
{
	if (auto physicalMaterial = Cast<UPhysicalMaterial>(AssetData.GetAsset()))
	{
		PhysicalMaterialAcousticTextureMap.Remove(physicalMaterial);
		PhysicalMaterialOcclusionMap.Remove(physicalMaterial);
		UpdateAkGeometryMap();
	}
}
#endif // WITH_EDITOR

bool UAkSettings::GetAssociatedAcousticTexture(const UPhysicalMaterial* physMaterial, UAkAcousticTexture*& acousticTexture) const
{
	TSoftObjectPtr<class UPhysicalMaterial> physMatPtr(physMaterial);
	auto props = AkGeometryMap.Find(physMatPtr);

	if (!props)
		return false;

	TSoftObjectPtr<class UAkAcousticTexture> texturePtr = props->AcousticTexture;
	acousticTexture = texturePtr.LoadSynchronous();
	return true;
}

bool UAkSettings::GetAssociatedOcclusionValue(const UPhysicalMaterial* physMaterial, float& occlusionValue) const
{
	TSoftObjectPtr<class UPhysicalMaterial> physMatPtr(physMaterial);
	auto props = AkGeometryMap.Find(physMatPtr);

	if (!props)
		return false;

	occlusionValue = props->OcclusionValue;
	return true;
}
