// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved

#include "Platforms/AkPlatformBase.h"
#include "AkAudioDevice.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

FString FAkPlatformBase::GetWwisePluginDirectory()
{
	return FPaths::ConvertRelativePathToFull(IPluginManager::Get().FindPlugin("Wwise")->GetBaseDir());
}
