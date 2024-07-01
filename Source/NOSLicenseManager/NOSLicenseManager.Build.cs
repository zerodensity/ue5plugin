// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;

using UnrealBuildTool;

public class NOSLicenseManager : ModuleRules
{
	public NOSLicenseManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"NOSSceneTreeManager",
				"NOSClient",
			});
		PublicDependencyModuleNames.AddRange(
			new string[]
				{
				"Core",
				"CoreUObject",
				"Engine",
				}
			);
	}
}
