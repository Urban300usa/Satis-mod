using UnrealBuildTool;

public class BuildingOwnership : ModuleRules
{
	public BuildingOwnership(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",
			"OnlineSubsystemUtils",
			"FactoryGame",
			"SML",
		});
	}
}
