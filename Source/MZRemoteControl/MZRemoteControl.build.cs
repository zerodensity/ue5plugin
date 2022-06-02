// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

using UnrealBuildTool;

public class MZRemoteControl : ModuleRules
{
	public MZRemoteControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"UnrealEd",
				"MZProto",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"RemoteControl",
				"MZClient",
			}
		);

	}
}
