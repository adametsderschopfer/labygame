// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class laby : ModuleRules
{
	public laby(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core",
			                                                "CoreUObject",
			                                                "Engine",
			                                                "InputCore",
			                                                "UMG",
			                                                "SlateCore",
			                                                "EnhancedInput",
			                                                "ProceduralMeshComponent",
			                                                "MassCore",
			                                                "MassEntity",
			                                                "RenderCore" });

		PublicDependencyModuleNames.AddRange(new string[] { "OnlineSubsystem", "OnlineSubsystemUtils" });
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate", "SlateCore", "ApplicationCore", "Sockets", "MoviePlayer", "AnimGraphRuntime", "AnimationCore"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
