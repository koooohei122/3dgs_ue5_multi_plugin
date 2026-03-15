// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

using UnrealBuildTool;

public class GaussianSplatting : ModuleRules
{
	public GaussianSplatting(ReadOnlyTargetRules Target) : base(Target)
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
			"RenderCore",
			"RHI",
			"Renderer",
			"Projects",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"RHICore",
			"zlib",
			"ImageWrapper",
		});

		// Allow access to renderer internals for custom passes
		PrivateIncludePathModuleNames.AddRange(new string[]
		{
			"Renderer",
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
			});
		}
	}
}
