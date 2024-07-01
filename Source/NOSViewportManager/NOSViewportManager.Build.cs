// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;

using UnrealBuildTool;

public class NOSViewportManager : ModuleRules
{
	public NOSViewportManager(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.bBuildEditor)
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				CppStandard = CppStandardVersion.Cpp20;

				PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

				PublicDependencyModuleNames.AddRange(
					new string[]
					{
					"Core",
					"CoreUObject",
					"InputCore",
					"Engine",
					"RenderCore",
					"RHI"
					}
					);

				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
					"Core",
					"CoreUObject",
					"InputCore",
					"Engine",
					"RenderCore",
					"RHI"
					}
					);

			}
		}
		else 
		{
			PublicDependencyModuleNames.AddRange(
					new string[]
					{
					"Core",
					"CoreUObject",
					"InputCore",
					"Engine",
					"RenderCore",
					"RHI"
					}
					);
			PrivateDependencyModuleNames.AddRange(new string[] { 
					"Core",
					"CoreUObject",
					"InputCore",
					"Engine",
					"RenderCore",
					"RHI" });
		}
	}
}
