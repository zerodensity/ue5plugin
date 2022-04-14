// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using System.Collections;
using System.Collections.Generic;
using UnrealBuildTool;

public class MZProto : ModuleRules
{
    private void CopyToBinaries(string Filepath)
    {
        string BinaryDir = Path.Combine(PluginDirectory, "Binaries/" + Target.Platform.ToString());

        string filename = Path.GetFileName(Filepath);

        if (!Directory.Exists(BinaryDir))
        {
            Directory.CreateDirectory(BinaryDir);
        }
        try
        {
            File.Copy(Filepath, Path.Combine(BinaryDir, filename), true);
        }
        catch { }
    }

    public MZProto(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "Projects"
                // ... add other public dependencies that you statically link with here ...
            }
        );

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Add the import library
            var Libs =  new HashSet<string>(Directory.GetFiles(Path.Combine(PluginDirectory, "shared", "lib"),"*.lib"));

            string[] ShippingBlackList =
            {
                // "libssl",
                "zlib",
                "libcrypto",
            };
        
  
            //if (Target.Configuration == UnrealTargetConfiguration.Shipping)
            {
                foreach (string lib in ShippingBlackList)
                {
                    Libs.Remove(Path.Combine(PluginDirectory, "shared", "lib", lib + ".lib"));
                }
            }

            foreach (string lib in Libs)
            {
                PublicAdditionalLibraries.Add(lib);
            }

            PublicDefinitions.Add("GOOGLE_PROTOBUF_NO_RTTI");
            PublicDefinitions.Add("GPR_FORBID_UNREACHABLE_CODE");
            PublicDefinitions.Add("GRPC_ALLOW_EXCEPTIONS=0");
            PublicDefinitions.Add("GOOGLE_PROTOBUF_INTERNAL_DONATE_STEAL_INLINE");

            PublicIncludePaths.Add(ModuleDirectory);
            PublicIncludePaths.Add(Path.Combine(PluginDirectory, "shared"));
            PublicIncludePaths.Add(Path.Combine(PluginDirectory, "shared", "include"));
            PublicIncludePaths.Add(Path.Combine(PluginDirectory, "shared", "generated"));
            
            PublicDependencyModuleNames.AddRange(new string[] { "WebSockets" });
            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL", "zlib");

            string BinaryDir = Path.Combine(
                PluginDirectory,
                "Binaries",
                Target.Platform.ToString()
            );

            foreach (
                string dll in Directory.GetFiles(
                    Path.Combine(PluginDirectory, "shared", "bin"),
                    "*.dll"
                )
            )
            {
                // PublicDelayLoadDLLs.Add(Path.GetFileName(dll) + ".dll");
                RuntimeDependencies.Add(dll);
                CopyToBinaries(dll);
            }
        }
    }
}
