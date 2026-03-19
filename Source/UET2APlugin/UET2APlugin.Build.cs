// Copyright UET2A Team. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UET2APlugin : ModuleRules
{
	public UET2APlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"JsonUtilities",
			"AnimationCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
			"AssetRegistry"
		});

		// FBX SDK - Use the engine's built-in FBX external module
		// This handles all platform-specific library linking automatically
		string FBXSDKDir = Path.Combine(EngineDirectory, "Source", "ThirdParty", "FBX", "2020.2");
		if (Directory.Exists(FBXSDKDir))
		{
			// Add the engine's FBX module as a dependency - it handles lib/dylib linking
			AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");
			PublicDefinitions.Add("WITH_FBX_SDK=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_FBX_SDK=0");
			System.Console.WriteLine("UET2APlugin: FBX SDK not found in engine ThirdParty directory. Runtime FBX import will be disabled.");
		}
	}
}
