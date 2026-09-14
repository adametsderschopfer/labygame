using UnrealBuildTool;

public class labyEditor : ModuleRules
{
	public labyEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.bWithLiveCoding)
			PrivateDependencyModuleNames.Add("LiveCoding");

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core",
			                                                 "CoreUObject",
			                                                 "Engine",
			                                                 "laby",
			                                                 "UMG",
			                                                 "UMGEditor",
			                                                 "UnrealEd",
			                                                 "Slate",
			                                                 "SlateCore",
			                                                 "AssetRegistry" });
	}
}
