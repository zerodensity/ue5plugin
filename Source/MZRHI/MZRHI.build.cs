// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

using UnrealBuildTool;

public class MZRHI : ModuleRules
{
    public MZRHI(ReadOnlyTargetRules Target) : base(Target)
    {
        //PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);

            PublicDependencyModuleNames.AddRange(new string[] { "Core", });

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "MediaIOCore",
                    "InputCore",
                    "RHI",
                    "RHICore",
                    "RenderCore",
                }
            );

            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PrivateDependencyModuleNames.AddRange(new string[] { "D3D12RHI", });

                AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");

                PrivateIncludePaths.AddRange(
                    new string[]
                    {
                        EnginePath + "Source/Runtime/D3D12RHI/Public",
                        EnginePath + "Source/Runtime/D3D12RHI/Private",
                    }
                );
            }
        }
    }
}
