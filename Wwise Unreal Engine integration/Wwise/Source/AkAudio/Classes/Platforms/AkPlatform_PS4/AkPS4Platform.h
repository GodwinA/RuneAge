#pragma once

#if PLATFORM_PS4

#include "Platforms/AkPlatformBase.h"
#include "AkPS4InitializationSettings.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"

#define TCHAR_TO_AK(Text) (const ANSICHAR*)(TCHAR_TO_ANSI(Text))

using UAkInitializationSettings = UAkPS4InitializationSettings;

struct FAkPS4Platform : FAkPlatformBase
{
	static const UAkInitializationSettings* GetInitializationSettings()
	{
		return GetDefault<UAkPS4InitializationSettings>();
	}

	static const FString GetPlatformBasePath()
	{
		return FString("PS4");
	}

	static FString GetWwisePluginDirectory()
	{
		// Is it not possible to get an absolute path on PS4, so we build it ourselves...
		FString BaseDirectory = TEXT("/app0");
		if (IPluginManager::Get().FindPlugin("Wwise")->GetType() == EPluginType::Engine)
		{
			BaseDirectory /= TEXT("engine");
		}
		else
		{
			BaseDirectory /= FApp::GetProjectName();
		}

		return BaseDirectory / TEXT("Plugins") / TEXT("Wwise");
	}
};

using FAkPlatform = FAkPS4Platform;

#endif
