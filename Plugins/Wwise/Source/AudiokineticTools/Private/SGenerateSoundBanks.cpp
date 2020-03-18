// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*------------------------------------------------------------------------------------
	SGenerateSoundBanks.cpp
------------------------------------------------------------------------------------*/

#include "SGenerateSoundBanks.h"
#include "AkAudioBank.h"
#include "AkAudioDevice.h"
#include "AkAudioBankGenerationHelpers.h"
#include "AkWaapiClient.h"
#include "Platforms/AkUEPlatform.h"
#include "Platforms/AkPlatformInfo.h"
#include "AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Dialogs/Dialogs.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"


#define LOCTEXT_NAMESPACE "AkAudio"

SGenerateSoundBanks::SGenerateSoundBanks()
{
}

void SGenerateSoundBanks::Construct(const FArguments& InArgs, TArray<TWeakObjectPtr<UAkAudioBank>>* in_pSoundBanks)
{
	// Generate the list of banks and platforms
	PopulateList();
	if (PlatformNames.Num() == 0)
	{
		OpenMsgDlgInt(EAppMsgType::Ok, NSLOCTEXT("AkAudio", "Warning_Ak_PlatformSupported", "Unable to generate SoundBanks. Please select a valid Wwise supported platform in the 'Project Settings > Project > Supported Platforms' dialog."), LOCTEXT("AkAudio_Error", "Error"));
		return;
	}

	// Build the form
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(0, 8)
		.FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(0, 8)
			.AutoWidth()
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SAssignNew(BankList, SListView<TSharedPtr<FString>>)
					.ListItemsSource(&Banks)
					.SelectionMode(ESelectionMode::Multi)
					.OnGenerateRow(this, &SGenerateSoundBanks::MakeBankListItemWidget)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column("Available Banks")
						[
							SNew(SBorder)
							.Padding(5)
							.Content()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AkAvailableBanks", "Available Banks"))
							]
						]
					)
				]
			]
			+SHorizontalBox::Slot()
			.Padding(0, 8)
			.AutoWidth()
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SAssignNew(PlatformList, SListView<TSharedPtr<FString>>)
					.ListItemsSource(&PlatformNames)
					.SelectionMode(ESelectionMode::Multi)
					.OnGenerateRow(this, &SGenerateSoundBanks::MakePlatformListItemWidget)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column("Available Platforms")
						[
							SNew(SBorder)
							.Padding(5)
							.Content()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AkAvailablePlatforms", "Available Platforms"))
							]
						]
					)
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4)
		.HAlign(HAlign_Right)
		[
			SNew(SButton)
			.Text(LOCTEXT("AkGenerate", "Generate"))
			.OnClicked(this, &SGenerateSoundBanks::OnGenerateButtonClicked)
		]
	];


	// Select all the banks, or only the banks passed with pObjects
	if(in_pSoundBanks == nullptr)
	{
		// Select all the banks
		for (auto& BankName : Banks)
		{
			BankList->SetItemSelection(BankName, true);
		}
	}
	else
	{
		// Select given banks
		for (const auto& SoundBank : *in_pSoundBanks)
		{
			const auto inBankName = SoundBank->GetName();
			for (auto& BankName : Banks)
			{
				if (*BankName == inBankName)
				{
					BankList->SetItemSelection(BankName, true);
				}
			}
		}
	}

	// Select all the platforms
	for (int32 ItemIdx = 0; ItemIdx < PlatformNames.Num(); ItemIdx++)
	{
		PlatformList->SetItemSelection(PlatformNames[ItemIdx], true);
	}
}

void SGenerateSoundBanks::SetShouldSaveWwiseProject(bool in_bShouldSaveBeforeGeneration) { m_bShouldSaveWwiseProject = in_bShouldSaveBeforeGeneration; }

void SGenerateSoundBanks::PopulateList()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	Banks.Empty();

	// Force load of bank assets so that the list is fully populated.
	{
		TArray<FAssetData> BankAssets;
		AssetRegistryModule.Get().GetAssetsByClass(UAkAudioBank::StaticClass()->GetFName(), BankAssets);

		for (int32 AssetIndex = 0; AssetIndex < BankAssets.Num(); ++AssetIndex)
		{	
			Banks.Add(MakeShared<FString>(BankAssets[AssetIndex].AssetName.ToString()));
		}
	}
	Banks.Sort([](const TSharedPtr<FString>& LHS, const TSharedPtr<FString>& RHS)
	{
		return *LHS < *RHS;
	});

	// Get available platforms
	PlatformNames.Empty();
	auto AvailablePlatformNames = AkUnrealPlatformHelper::GetAllSupportedWwisePlatforms();
	for (const auto Platform : AvailablePlatformNames)
	{
		auto* PlatformInfo = UAkPlatformInfo::GetAkPlatformInfo(*Platform);
		if (!PlatformInfo)
		{
			continue;
		}

		PlatformNames.Add(TSharedPtr<FString>(new FString(PlatformInfo->WwisePlatform)));
	}
}

FReply SGenerateSoundBanks::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent )
{
	if (InKeyboardEvent.GetKey() == EKeys::Enter)
	{
		return OnGenerateButtonClicked();
	}
	else if (InKeyboardEvent.GetKey() == EKeys::Escape)
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		ParentWindow->RequestDestroyWindow();
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyboardEvent);
}

FReply SGenerateSoundBanks::OnGenerateButtonClicked()
{
	auto BanksToGenerate = BankList->GetSelectedItems();
	if (BanksToGenerate.Num() <= 0)
	{
		OpenMsgDlgInt(EAppMsgType::Ok, NSLOCTEXT("AkAudio", "Warning_Ak_NoAkBanksSelected", "At least one bank must be selected."), LOCTEXT("AkAudio_Error", "Error"));
		return FReply::Handled();
	}

	auto PlatformsToGenerate = PlatformList->GetSelectedItems();
	if (PlatformsToGenerate.Num() <= 0)
	{
		OpenMsgDlgInt(EAppMsgType::Ok, NSLOCTEXT("AkAudio", "Warning_Ak_NoAkPlatformsSelected", "At least one platform must be selected."), LOCTEXT("AkAudio_Error", "Error"));
		return FReply::Handled();
	}

	TMap<FString, TSet<UAkAudioEvent*>> BankToEventSet;
	if (WwiseBnkGenHelper::GenerateDefinitionFile(BanksToGenerate, BankToEventSet))
	{
		/* We only save the wwise project when banks are generated from sequencer, since it would be a very big behavior change to save every time from the main UE toolbar. */
		if (m_bShouldSaveWwiseProject)
		{
			/* If we have an open connection to Authoring via WAAPI, save the project first and wait until it is not dirty before generating sounbanks */
			FAkWaapiClient* pWaapiClient = FAkWaapiClient::Get();
			if (pWaapiClient != nullptr)
			{
				FAkWaapiClient::SaveProject();
				bool bProjectDirty = true;
				while (bProjectDirty)
				{
					bProjectDirty = FAkWaapiClient::IsProjectDirty();
				}
			}
		}

		if (BanksToGenerate.Num() != 0)
		{
			auto OnSoundBanksGenerated = WwiseBnkGenHelper::FOnBankGenerationComplete::CreateLambda([this, BankToEventSet](bool)
			{
				if (auto AudioDevice = FAkAudioDevice::Get())
				{
					UE_LOG(LogAkBanks, Log, TEXT("Reloading all SoundBanks..."));
					AudioDevice->ReloadAllReferencedBanks();
				}
				WwiseBnkGenHelper::FetchAttenuationInfo(BankToEventSet);
			});

			WwiseBnkGenHelper::GenerateSoundBanksNonBlocking(BanksToGenerate, PlatformsToGenerate, OnSoundBanksGenerated);
			TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
			ParentWindow->RequestDestroyWindow();
		}
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AkAudio_SoundBankGenFailNoDefFile", "SoundBank generation failed: could not open the Wwise SoundBank definition file."));
	}

	return FReply::Handled();
}

TSharedRef<ITableRow> SGenerateSoundBanks::MakeBankListItemWidget(TSharedPtr<FString> Bank, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(Bank.IsValid());

	return
		SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				SNew(STextBlock)
				.Text(FText::FromString(*Bank))
			]
		];
}

TSharedRef<ITableRow> SGenerateSoundBanks::MakePlatformListItemWidget(TSharedPtr<FString> Platform, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				SNew(STextBlock)
				.Text(FText::FromString(*Platform))
			]
		];
}

#undef LOCTEXT_NAMESPACE
