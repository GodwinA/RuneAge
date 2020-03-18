// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class AkUEPlatform_XboxOne : AkUEPlatform
{
	public AkUEPlatform_XboxOne(ReadOnlyTargetRules in_TargetRules, string in_ThirdPartyFolder) : base(in_TargetRules, in_ThirdPartyFolder) {}

#if !UE_4_24_OR_LATER
	public override string SanitizeLibName(string in_libName)
	{
		return in_libName + ".lib";
	}
#endif

	public override string GetLibraryFullPath(string LibName, string LibPath)
	{
		return Path.Combine(LibPath, LibName + ".lib");
	}

	public override bool SupportsAkAutobahn { get { return false; } }

	public override bool SupportsCommunication { get { return true; } }

	public override List<string> GetPublicLibraryPaths()
	{
		var akPlatformLibDir = "XboxOne_" + GetVisualStudioVersion();

		return new List<string>
		{
			Path.Combine(ThirdPartyFolder, akPlatformLibDir, akConfigurationDir, "lib")
		};
	}

	public override List<string> GetAdditionalWwiseLibs()
	{
		return new List<string>();
	}
	
	public override List<string> GetPublicSystemLibraries()
	{
		return new List<string>
		{
			"AcpHal.lib",
			"MMDevApi.lib"
		};
	}

	public override List<string> GetPublicDelayLoadDLLs()
	{
		return new List<string>();
	}

	public override List<string> GetPublicDefinitions()
	{
		return new List<string>
		{
			"_XBOX_ONE",
			"AK_XBOXONE_VS_VERSION=\"" + GetVisualStudioVersion() + "\""
		};
	}

	public override Tuple<string, string> GetAdditionalPropertyForReceipt(string ModuleDirectory)
	{
		return null;
	}

	public override List<string> GetPublicAdditionalFrameworks()
	{
		return new List<string>();
	}

	private string GetVisualStudioVersion()
	{
		// Use reflection because the GitHub version of UE is missing things.
		var XboxOnePlatformType = System.Type.GetType("XboxOnePlatform", false);
		if (XboxOnePlatformType != null)
		{
			var XboxOneCompilerField = XboxOnePlatformType.GetField("Compiler");
			if (XboxOneCompilerField != null)
			{
				var XboxOneCompilerValue = XboxOneCompilerField.GetValue(null);
				switch (XboxOneCompilerValue.ToString())
				{
					case "VisualStudio2012":
						return "vc110";
					case "VisualStudio2017":
						return "vc150";
					case "VisualStudio2015":
					default:
						return "vc140";
				}
			}
		}

		return "vc140";
	}
}
