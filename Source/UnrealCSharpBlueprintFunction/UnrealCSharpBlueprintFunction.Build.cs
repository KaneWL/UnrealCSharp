using UnrealBuildTool;

public class UnrealCSharpBlueprintFunction : ModuleRules
{
    public UnrealCSharpBlueprintFunction(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(["Core", "CoreUObject", "Engine"]);

        PrivateDependencyModuleNames.AddRange(["UnrealCSharp", "UnrealCSharpCore"]);

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(["UnrealEd", "BlueprintGraph"]);
        }
    }
}
