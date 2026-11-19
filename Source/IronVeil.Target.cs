using UnrealBuildTool;
using System.Collections.Generic;

public class IronVeilTarget : TargetRules
{
	public IronVeilTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("IronVeil");
	}
}
