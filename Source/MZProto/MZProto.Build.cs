// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;
using UnrealBuildTool;

public class MZProto : ModuleRules
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

    public MZProto(ReadOnlyTargetRules Target) : base(Target)
    {
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "RemoteControl",
            }
        );


        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
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

			var Libs =  new HashSet<string>(Directory.GetFiles(SDKParentDir, "*.lib", eo));
			var Dlls =  new HashSet<string>(Directory.GetFiles(SDKParentDir, "*.dll", eo));
            var Pdbs = new HashSet<string>(Directory.GetFiles(SDKParentDir, "*.pdb", eo));

			foreach (string pdb in Pdbs)
            {
                CopyToBinaries(pdb);
            }

            Console.WriteLine("MZProto: Adding additional libs");
            foreach (string lib in Libs)
            {
                string copied = CopyToBinaries(lib);
                Console.WriteLine("MZProto: " + copied);
                PublicAdditionalLibraries.Add(copied);
            }

            Console.WriteLine("MZProto: Adding runtime dependencies");
            foreach (string dll in Dlls)
            {
                // PublicDelayLoadDLLs.Add(Path.GetFileName(dll) + ".dll");
                string copied = CopyToBinaries(dll);
                Console.WriteLine("MZProto: " + copied);
                RuntimeDependencies.Add(copied);
            }

            PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
            PublicIncludePaths.Add(Path.Combine(SDKdir, "include"));
            PublicIncludePaths.Add(Path.Combine(SDKdir, "../include"));
        }
    }
}
