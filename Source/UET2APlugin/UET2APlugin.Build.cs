// Copyright UET2A Team. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

[SupportedTargetTypes(TargetType.Editor, TargetType.Game, TargetType.Client, TargetType.Server)]
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

		// FBX SDK - Use the engine's built-in FBX external module.
		// UE5.6/5.7 在不同安装形态下 ThirdParty 目录版本号可能不同，不要把版本号写死。
		string FBXSDKRootDir = Path.Combine(EngineDirectory, "Source", "ThirdParty", "FBX");
		if (Directory.Exists(FBXSDKRootDir))
		{
			// Add the engine's FBX module as a dependency - it handles platform-specific linking.
			AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");
			PublicDefinitions.Add("WITH_FBX_SDK=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_FBX_SDK=0");
			System.Console.WriteLine("UET2APlugin: FBX SDK root not found in engine ThirdParty directory. Runtime FBX import will be disabled.");
		}
	}
}
