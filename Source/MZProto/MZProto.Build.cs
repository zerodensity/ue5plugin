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
            string SDKIncdeps = Path.Combine(SDKdir, "installed", "x64-windows");
            string SDKLibdeps = Path.Combine(SDKdir, "installed", "x64-windows");

            // Add the import library
            var Libs =  new HashSet<string>(Directory.GetFiles(Path.Combine(SDKdir, "lib"),"*.lib"));
            Libs.UnionWith(new HashSet<string>(Directory.GetFiles(Path.Combine(SDKLibdeps, "lib"),"*.lib")));

            var Dlls =  new HashSet<string>(Directory.GetFiles(Path.Combine(SDKdir, "bin"),"*.dll"));
            Dlls.UnionWith(new HashSet<string>(Directory.GetFiles(Path.Combine(SDKLibdeps, "bin"),"*.dll")));

            var Pdbs = new HashSet<string>(Directory.GetFiles(Path.Combine(SDKdir, "bin"), "*.pdb"));
            Pdbs.UnionWith(new HashSet<string>(Directory.GetFiles(Path.Combine(SDKLibdeps, "bin"), "*.pdb")));

            foreach (string pdb in Pdbs)
            {
                CopyToBinaries(pdb);
            }

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
                    Libs.Remove(Path.Combine(SDKLibdeps, "lib", lib + ".lib"));
                    foreach(string dll in Directory.GetFiles(Path.Combine(SDKLibdeps, "bin"), lib + "*.dll")) 
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
            // PublicDefinitions.Add("GOOGLE_PROTOBUF_INTERNAL_DONATE_STEAL_INLINE");
            // PrivateDefinitions.Add("PROTOBUF_FORCE_COPY_DEFAULT_STRING");
            // PrivateDefinitions.Add("PROTOBUF_FORCE_COPY_IN_RELEASE");
            // PrivateDefinitions.Add("PROTOBUF_FORCE_COPY_IN_SWAP");
            // PrivateDefinitions.Add("PROTOBUF_FORCE_COPY_IN_MOVE");

            PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
            PublicIncludePaths.Add(Path.Combine(SDKdir, "include"));
            PublicIncludePaths.Add(Path.Combine(SDKIncdeps, "include"));
            
            // PublicDependencyModuleNames.AddRange(new string[] { "WebSockets" });
            // AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL", "zlib");

        }
    }
}
