// Copyright MediaZ AS. All Rights Reserved.


using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;

using UnrealBuildTool;

public class MZLicenseManager : ModuleRules
{
	public MZLicenseManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"MZSceneTreeManager",
				"MZClient",
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
