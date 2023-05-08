// Copyright MediaZ AS. All Rights Reserved.


using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;

using UnrealBuildTool;

public class MZClient : ModuleRules
{
	private string CopyToBinaries(string Filepath)
	{
		string BinaryDir = Path.Combine(PluginDirectory, "Binaries", Target.Platform.ToString());

		string path = Path.Combine(BinaryDir, Path.GetFileName(Filepath));

		if (!Directory.Exists(BinaryDir))
		{
			Directory.CreateDirectory(BinaryDir);
		}

		File.Copy(Filepath, path, true);
		return path;
	}

	public MZClient(ReadOnlyTargetRules Target) : base(Target)
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
					"Projects",
					"Slate",
					"SlateCore",
					"UnrealEd",
					}
					);

				PrivateDependencyModuleNames.AddRange(
					new string[]
					{

					"Core",
					"CoreUObject",
					"Engine",
					"Projects",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"ToolMenus",
					"UnrealEd",
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
