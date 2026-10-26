using UnrealBuildTool;

public class ImperialWingsSimulator : ModuleRules
{
    public ImperialWingsSimulator(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput"
            , "HTTP", "Json", "JsonUtilities", "PhysicsCore"
        });
        bUseUnity = false;
        if (Target.Platform == UnrealTargetPlatform.Win64) PublicSystemLibraries.Add("winmm.lib");
    }
}
