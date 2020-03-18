// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;
using System.Reflection;

// Platform-specific files implement this interface, returning their particular dependencies, defines, etc.
public abstract class AkUEPlatform
{
	protected ReadOnlyTargetRules Target;
	protected string ThirdPartyFolder;

	public AkUEPlatform(ReadOnlyTargetRules in_Target, string in_ThirdPartyFolder)
	{
		Target = in_Target;
		ThirdPartyFolder = in_ThirdPartyFolder;
	}

	public static AkUEPlatform GetAkUEPlatformInstance(ReadOnlyTargetRules Target, string ThirdPartyFolder)
	{
		var AkUEPlatformType = System.Type.GetType("AkUEPlatform_" + Target.Platform.ToString());
		if (AkUEPlatformType == null)
		{
			throw new BuildException("Wwise does not support platform " + Target.Platform.ToString());
		}

		var PlatformInstance = Activator.CreateInstance(AkUEPlatformType, Target, ThirdPartyFolder) as AkUEPlatform;
		if (PlatformInstance == null)
		{
			throw new BuildException("Wwise could not instantiate platform " + Target.Platform.ToString());
		}

		return PlatformInstance;
	}

	protected static List<string> GetAllLibrariesInFolder(string LibFolder, string ExtensionFilter, bool RemoveLibPrefix = true)
	{
		var ret = new List<string>();
		var FoundLibs = Directory.GetFiles(LibFolder, ExtensionFilter);

		foreach (var Library in FoundLibs)
		{
			var LibName = Path.GetFileNameWithoutExtension(Library);
			if (RemoveLibPrefix && LibName.StartsWith("lib"))
			{
				LibName = LibName.Remove(0, 3);
			}
			ret.Add(LibName);
		}

		return ret;
	}

	public virtual string akConfigurationDir
	{
		get
		{
			switch (Target.Configuration)
			{
				case UnrealTargetConfiguration.Debug:
					var akConfiguration = Target.bDebugBuildsActuallyUseDebugCRT ? "Debug" : "Profile";
					return akConfiguration;

				case UnrealTargetConfiguration.Development:
				case UnrealTargetConfiguration.Test:
				case UnrealTargetConfiguration.DebugGame:
					return "Profile";
				default:
					return "Release";
			}
		}
	}
	
	public abstract string GetLibraryFullPath(string LibName, string LibPath);
	public abstract bool SupportsAkAutobahn { get; }
	public abstract bool SupportsCommunication { get; }
	public virtual bool SupportsOpus { get { return true; } }

	public abstract List<string> GetPublicLibraryPaths();
	public abstract List<string> GetAdditionalWwiseLibs();
	public abstract List<string> GetPublicSystemLibraries();
	public abstract List<string> GetPublicDelayLoadDLLs();
	public abstract List<string> GetPublicDefinitions();
	public abstract Tuple<string, string> GetAdditionalPropertyForReceipt(string ModuleDirectory);
	public abstract List<string> GetPublicAdditionalFrameworks();
	
#if UE_4_24_OR_LATER
	public virtual List<string> GetSanitizedAkLibList(List<string> AkLibs)
	{
		List<string> SanitizedLibs = new List<string>();
		foreach(var lib in AkLibs)
		{
			foreach(var libPath in GetPublicLibraryPaths())
			{
				SanitizedLibs.Add(GetLibraryFullPath(lib, libPath));
			}
		}
		
		return SanitizedLibs;
	}
#else
	public abstract string SanitizeLibName(string in_libName);
	public virtual List<string> GetSanitizedAkLibList(List<string> AkLibs)
	{
		List<string> SanitizedLibs = new List<string>();
		foreach(var lib in AkLibs)
		{
			SanitizedLibs.Add(SanitizeLibName(lib));
		}
		
		return SanitizedLibs;
	}
#endif

}

public class AkAudio : ModuleRules
{
	private static AkUEPlatform AkUEPlatformInstance;
	private List<string> AkLibs = new List<string> 
	{
		"AkSoundEngine",
		"AkMemoryMgr",
		"AkStreamMgr",
		"AkMusicEngine",
		"AkSpatialAudio",
		"AkAudioInputSource",
		"AkVorbisDecoder",
	};
	
	public AkAudio(ReadOnlyTargetRules Target) : base(Target)
	{
		string ThirdPartyFolder = Path.Combine(ModuleDirectory, "../../ThirdParty");
		AkUEPlatformInstance = AkUEPlatform.GetAkUEPlatformInstance(Target, ThirdPartyFolder);
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePathModuleNames.AddRange(new[] { "Settings", "UMG", "TargetPlatform"});

		PublicDependencyModuleNames.AddRange(new[] { "UMG", "PhysX", "APEX" });

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SlateCore",
			"NetworkReplayStreaming",
			"MovieScene",
			"MovieSceneTracks",
			"Projects",
			"Json",
			"Slate",
			"InputCore",
			"Projects"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "DesktopPlatform" });

			foreach (var Platform in GetAvailablePlatforms(ModuleDirectory))
			{
				PublicDefinitions.Add("AK_PLATFORM_" + Platform.ToUpper());
			}
		}

		PrivateIncludePaths.Add("AkAudio/Private");
		PublicIncludePaths.Add(Path.Combine(ThirdPartyFolder, "include"));

		PublicDefinitions.Add("AK_UNREAL_MAX_CONCURRENT_IO=32");
		PublicDefinitions.Add("AK_UNREAL_IO_GRANULARITY=32768");
		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			PublicDefinitions.Add("AK_OPTIMIZED");
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping && AkUEPlatformInstance.SupportsCommunication)
		{
			AkLibs.Add("CommunicationCentral");
			PublicDefinitions.Add("AK_ENABLE_COMMUNICATION=1");
		}
		else
		{
			PublicDefinitions.Add("AK_ENABLE_COMMUNICATION=0");
		}

		if (AkUEPlatformInstance.SupportsAkAutobahn)
		{
			AkLibs.Add("AkAutobahn");
			PublicDefinitions.Add("AK_SUPPORT_WAAPI=1");
		}
		else
		{
			PublicDefinitions.Add("AK_SUPPORT_WAAPI=0");
		}

		if (AkUEPlatformInstance.SupportsOpus)
		{
			AkLibs.Add("AkOpusDecoder");
			PublicDefinitions.Add("AK_SUPPORT_OPUS=1");
		}
		else
		{
			PublicDefinitions.Add("AK_SUPPORT_OPUS=0");
		}

		// Platform-specific dependencies
#if UE_4_24_OR_LATER
		PublicSystemLibraries.AddRange(AkUEPlatformInstance.GetPublicSystemLibraries());
#else
		PublicLibraryPaths.AddRange(AkUEPlatformInstance.GetPublicLibraryPaths());
		PublicAdditionalLibraries.AddRange(AkUEPlatformInstance.GetPublicSystemLibraries());
#endif
		AkLibs.AddRange(AkUEPlatformInstance.GetAdditionalWwiseLibs());
		PublicDefinitions.AddRange(AkUEPlatformInstance.GetPublicDefinitions());
		PublicDefinitions.Add(string.Format("AK_CONFIGURATION=\"{0}\"", AkUEPlatformInstance.akConfigurationDir));
		var AdditionalProperty = AkUEPlatformInstance.GetAdditionalPropertyForReceipt(ModuleDirectory);
		if (AdditionalProperty != null)
		{
			AdditionalPropertiesForReceipt.Add(AdditionalProperty.Item1, AdditionalProperty.Item2);
		}

		var Frameworks = AkUEPlatformInstance.GetPublicAdditionalFrameworks();
		foreach (var fw in Frameworks)
		{
#if UE_4_22_OR_LATER
			PublicAdditionalFrameworks.Add(new ModuleRules.Framework(fw));
#else
			PublicAdditionalFrameworks.Add(new UEBuildFramework(fw));
#endif
		}

		PublicAdditionalLibraries.AddRange(AkUEPlatformInstance.GetSanitizedAkLibList(AkLibs));
	}

	private static List<string> GetAvailablePlatforms(string ModuleDir)
	{
		var FoundPlatforms = new List<string>();
		const string StartPattern = "AkAudio_";
		const string EndPattern = ".Build.cs";
		foreach (var BuildCsFile in System.IO.Directory.GetFiles(ModuleDir, "*" + EndPattern))
		{
			if (BuildCsFile.Contains("AkAudio_"))
			{
				int StartIndex = BuildCsFile.IndexOf(StartPattern) + StartPattern.Length;
				int StopIndex = BuildCsFile.IndexOf(EndPattern);
				FoundPlatforms.Add(BuildCsFile.Substring(StartIndex, StopIndex - StartIndex));
			}
		}

		return FoundPlatforms;
	}
}
