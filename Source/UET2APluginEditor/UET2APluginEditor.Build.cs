// Copyright UET2A Team. All Rights Reserved.

using UnrealBuildTool;

[SupportedTargetTypes(TargetType.Editor)]
public class UET2APluginEditor : ModuleRules

{
	public UET2APluginEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UET2APlugin" // Depend on the runtime module
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
			"InputCore",
			"ToolMenus",
			"WorkspaceMenuStructure",
			"DesktopPlatform",
			"ApplicationCore"
		});
	}
}
