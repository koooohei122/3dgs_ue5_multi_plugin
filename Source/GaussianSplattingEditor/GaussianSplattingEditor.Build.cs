// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

using UnrealBuildTool;

public class GaussianSplattingEditor : ModuleRules
{
	public GaussianSplattingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[]
		{
			ModuleDirectory + "/Public",
		});

		PrivateIncludePaths.AddRange(new string[]
		{
			ModuleDirectory + "/Private",
		});

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GaussianSplatting",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"ContentBrowser",
			"EditorStyle",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"EditorWidgets",
			"Projects",
			"PropertyEditor",
		});
	}
}
