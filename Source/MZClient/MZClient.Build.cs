// Copyright Epic Games, Inc. All Rights Reserved.


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

			var SDKParentDir = Path.Combine(SDKdir, "..");

			EnumerationOptions eo = new EnumerationOptions();
			eo.RecurseSubdirectories = true;

			var Libs = new HashSet<string>(Directory.GetFiles(SDKParentDir, "*.lib", eo));
			var Dlls = new HashSet<string>(Directory.GetFiles(SDKParentDir, "*.dll", eo));
			var Pdbs = new HashSet<string>(Directory.GetFiles(SDKParentDir, "*.pdb", eo));

			foreach (string pdb in Pdbs)
			{
				CopyToBinaries(pdb);
			}

			Console.WriteLine("MZClient: Adding additional libs");
			foreach (string lib in Libs)
			{
				string copied = CopyToBinaries(lib);
				Console.WriteLine("MZClient: " + copied);
				PublicAdditionalLibraries.Add(copied);
			}

			Console.WriteLine("MZClient: Adding runtime dependencies");
			foreach (string dll in Dlls)
			{
				string copied = CopyToBinaries(dll);
				Console.WriteLine("MZClient: " + copied);
				RuntimeDependencies.Add(copied);
			}

			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
			PublicIncludePaths.Add(Path.Combine(SDKdir, "include"));
			PublicIncludePaths.Add(Path.Combine(SDKdir, "../include"));

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Projects",
					"RenderCore",
					"RHI",
					"RHICore",
					"D3D11RHI",
					"D3D12RHI",
					"VulkanRHI",
					"AssetRegistry",
				}
				);

			if (Target.bBuildEditor)
			{
				// we only want this to be included for editor builds
				//PublicDependencyModuleNames.Add("UnrealEd");
				PrivateDependencyModuleNames.Add("SlateCore");
				PrivateDependencyModuleNames.Add("Slate");
				PrivateDependencyModuleNames.Add("EditorStyle");
				PrivateDependencyModuleNames.Add("ToolMenus");
			}

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Projects",
					"RenderCore",
					"RHI",
					"RHICore",
					"D3D11RHI",
					"D3D12RHI",
					"VulkanRHI",
					"AssetRegistry",
				}
				);

            PrivateIncludePathModuleNames.Add("D3D11RHI");
            PrivateIncludePathModuleNames.Add("D3D12RHI");

            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");

			PublicDefinitions.Add("PLATFORM_WIN64");
			PrivateIncludePaths.AddRange(
                new string[]{
						//required for "D3D12RHIPrivate.h"
						Path.Combine(EngineDirectory, "Source/Runtime/D3D12RHI/Private"),
                });


        }
		
	}
}
