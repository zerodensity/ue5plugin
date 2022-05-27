// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;
using UnrealBuildTool;

public class MZProto : ModuleRules
{
    private void CopyToBinaries(string Filepath)
    {
        string BinaryDir = Path.Combine(PluginDirectory, "Binaries", Target.Platform.ToString());

        string path = Path.Combine(BinaryDir, Path.GetFileName(Filepath));

        if (!Directory.Exists(BinaryDir))
        {
            Directory.CreateDirectory(BinaryDir);
        }   

        try { File.Copy(Filepath, path, true); } catch { }
    }

    public MZProto(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "RemoteControl",
            }
        );

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string SDKdir = Environment.GetEnvironmentVariable("MZ_SDK_DIR");
            string SDKdeps = Path.Combine(SDKdir, "installed", "x64-windows");

            // Add the import library
            var Libs =  new HashSet<string>(Directory.GetFiles(Path.Combine(SDKdir, "lib"),"*.lib"));
            Libs.UnionWith(new HashSet<string>(Directory.GetFiles(Path.Combine(SDKdeps, "lib"),"*.lib")));

            var Dlls =  new HashSet<string>(Directory.GetFiles(Path.Combine(SDKdir, "bin"),"*.dll"));
            Dlls.UnionWith(new HashSet<string>(Directory.GetFiles(Path.Combine(SDKdeps, "bin"),"*.dll")));

            foreach (string dll in Dlls)
            {
                // PublicDelayLoadDLLs.Add(Path.GetFileName(dll) + ".dll");
                RuntimeDependencies.Add(dll);
                CopyToBinaries(dll);
            }

            string[] ShippingBlackList =
            {
                "libssl",
                "zlib",
                "libcrypto",
            };
        
  
            if (Target.Configuration == UnrealTargetConfiguration.Shipping)
            {
                foreach (string lib in ShippingBlackList)
                {
                    Libs.Remove(Path.Combine(SDKdeps, "lib", lib + ".lib"));
                    foreach(string dll in Directory.GetFiles(Path.Combine(SDKdeps, "bin"), lib + "*.dll")) 
                    {
                      Dlls.Remove(dll);
                    }
                }
            }

            foreach (string lib in Libs)
            {
                PublicAdditionalLibraries.Add(lib);
            }

            PublicDefinitions.Add("GOOGLE_PROTOBUF_NO_RTTI");
            PublicDefinitions.Add("GPR_FORBID_UNREACHABLE_CODE");
            PublicDefinitions.Add("GRPC_ALLOW_EXCEPTIONS=0");
            //PublicDefinitions.Add("GOOGLE_PROTOBUF_INTERNAL_DONATE_STEAL_INLINE");

            PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
            PublicIncludePaths.Add(Path.Combine(SDKdir, "include"));
            PublicIncludePaths.Add(Path.Combine(SDKdeps, "include"));
            
            // PublicDependencyModuleNames.AddRange(new string[] { "WebSockets" });
            // AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL", "zlib");

        }
    }
}
