using UnrealBuildTool;
using System.Collections.Generic;

public class ImperialWingsSimulatorTarget : TargetRules
{
    public ImperialWingsSimulatorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("ImperialWingsSimulator");
    }
}
