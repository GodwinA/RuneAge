// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved

#include "AkActivatedPlugins.h"
#include "AkAudioDevice.h"
#include "Misc/PackageName.h"

namespace UAkActivatedPlugins_Helper
{
	const FString PackageName = TEXT("/Game/Wwise/EditorOnly/ActivatedPlugins");
	FString FilePath;
}

//////////////////////////////////////////////////////////////////////////
// UAkActivatedPlugins

UAkActivatedPlugins::UAkActivatedPlugins(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FString& UAkActivatedPlugins::GetPackageName() { return UAkActivatedPlugins_Helper::PackageName; }

const FString& UAkActivatedPlugins::GetFilePath()
{
	if (UAkActivatedPlugins_Helper::FilePath.IsEmpty())
	{
		UAkActivatedPlugins_Helper::FilePath = FString::Format(TEXT("/Game/Wwise/EditorOnly/ActivatedPlugins{0}"), { FPackageName::GetAssetPackageExtension() });
	}

	return UAkActivatedPlugins_Helper::FilePath;
}
