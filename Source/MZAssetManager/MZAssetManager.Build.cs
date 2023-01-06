// Copyright Epic Games, Inc. All Rights Reserved.


using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;

using UnrealBuildTool;

public class MZAssetManager : ModuleRules
{

	public MZAssetManager(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.bBuildEditor)
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				CppStandard = CppStandardVersion.Cpp20;

				string SDKdir = Environment.GetEnvironmentVariable("MZ_SDK_DIR");

				if (String.IsNullOrEmpty(SDKdir))
				{
					string errorMessage = "Please update MZ_SDK_DIR environment variable";
					System.Console.WriteLine(errorMessage);
					throw new BuildException(errorMessage);
				}

				var SDKIncludeDir = Path.Combine(SDKdir, "include");

				PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
				PublicIncludePaths.Add(SDKIncludeDir);
				

				PublicDependencyModuleNames.AddRange(
					new string[]
					{
					"Core",
					"CoreUObject",
					"Engine",
					"AssetRegistry",
					"EditorFramework",
                    "TypedElementFramework",
					"UMG",
					"UnrealEd",
					"MZClient",
					}
					);

				PrivateDependencyModuleNames.AddRange(
					new string[]
					{

					"Core",
					"CoreUObject",
					"Engine",
					"AssetRegistry",
					"EditorFramework",
                    "TypedElementFramework",
					"UMG",
					"UnrealEd",
					"MZClient",
					}
					);
			}
		}
		else 
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		}
	}
}
