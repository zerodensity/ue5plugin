// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

using UnrealBuildTool;

public class MZClient : ModuleRules
{
	public MZClient(ReadOnlyTargetRules Target) : base(Target)
	{
		//PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.Platform == UnrealTargetPlatform.Win64)
        {
			CppStandard = CppStandardVersion.Cpp20;
		

			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
				);
				

			PrivateIncludePaths.AddRange(
				new string[] {
					// ... add other private include paths required here ...
				}
				);
			
		
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Projects",
					"RemoteControl",
					"RenderCore",
					"RHI",
					"RHICore",
					"D3D11RHI",
					"D3D12RHI",
					"VulkanRHI",
					"MZProto",
					"UnrealEd"
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Projects",
					"RemoteControl",
					"RenderCore",
					"RHI",
					"RHICore",
					"D3D11RHI",
					"D3D12RHI",
					"VulkanRHI",
					"MZProto",
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
                        // Path.Combine(EngineDirectory, "Source/Runtime/D3D12RHI/Private/Windows")
                });

            DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"MZRemoteControl"
				}
				);
        }
		
	}
}
