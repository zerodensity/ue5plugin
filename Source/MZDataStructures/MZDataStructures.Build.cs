// Copyright MediaZ AS. All Rights Reserved.


using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;

using UnrealBuildTool;

public class MZDataStructures : ModuleRules
{
	public MZDataStructures(ReadOnlyTargetRules Target) : base(Target)
	{
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
