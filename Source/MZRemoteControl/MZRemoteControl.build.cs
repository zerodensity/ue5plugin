// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

using UnrealBuildTool;

public class MZRemoteControl : ModuleRules
{
	public MZRemoteControl(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					//"Core",
					//"CoreUObject",
					//"UnrealEd",
					//"MZProto",
					"Engine",
					"Core",
					"CoreUObject",
					"Slate",
					"SlateCore",
					"InputCore",
					"EditorFramework",
					"UnrealEd",
					"GraphEditor",
					"EditorStyle",
					"PropertyEditor",
					"AppFramework",
					"Projects",
					"Sequencer",
					"EditorWidgets",
					"ApplicationCore",
					"CurveEditor",
					"ToolWidgets",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
				"RemoteControl",
				"MZProto",
				"MZClient",
				}
			);
		}
	}
}
