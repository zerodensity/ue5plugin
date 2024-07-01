// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;
using EpicGames.Core;
using UnrealBuildTool;

public class NOSClient : ModuleRules
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

	public static string GetSDKDir(string PluginDirectory)
	{
		var ConfigFilePath = Path.Combine(PluginDirectory, "Config\\config.json");
		if (!File.Exists(ConfigFilePath))
		{
			string errorMessage = "Please verify Config/config.json";
			System.Console.WriteLine(errorMessage);
			throw new BuildException(errorMessage);
		}

		FileReference ConfigFile = new FileReference(ConfigFilePath);
		var Config = JsonObject.Read(ConfigFile);
		
		string NosmanPath = Config.GetStringField("NOSMAN_PATH");

		//execute shell command
		System.Diagnostics.Process process = new System.Diagnostics.Process();
		process.StartInfo.FileName = NosmanPath;
		process.StartInfo.ArgumentList.Add("sdk-info");
		process.StartInfo.ArgumentList.Add("1.2.0");
		process.StartInfo.UseShellExecute = false;
		process.StartInfo.WorkingDirectory = Path.Combine(NosmanPath, "..");
		process.StartInfo.RedirectStandardOutput = true;
		process.StartInfo.RedirectStandardError = true;
		process.StartInfo.CreateNoWindow = true;
		process.Start();
		process.WaitForExit();
		string output = process.StandardOutput.ReadToEnd();

		var SDKInfo = JsonObject.Parse(output);
		string SDKdir = SDKInfo.GetStringField("path");

		return SDKdir;
	}

	public NOSClient(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.bBuildEditor)
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				CppStandard = CppStandardVersion.Cpp20;

				string SDKdir = GetSDKDir(PluginDirectory);

				if (String.IsNullOrEmpty(SDKdir))
				{
					string errorMessage = "Please update NODOS_SDK_DIR environment variable";
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
					"Json"
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
