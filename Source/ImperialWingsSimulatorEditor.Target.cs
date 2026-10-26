using UnrealBuildTool;
using System.Collections.Generic;

public class ImperialWingsSimulatorEditorTarget : TargetRules
{
    public ImperialWingsSimulatorEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("ImperialWingsSimulator");
    }
}
