// The packaged game build target.

using UnrealBuildTool;
using System.Collections.Generic;

public class IronveilTarget : TargetRules
{
	public IronveilTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Ironveil");
	}
}
