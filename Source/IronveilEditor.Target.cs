// The editor build target (what Visual Studio / Rider builds when you press Play or Build).

using UnrealBuildTool;
using System.Collections.Generic;

public class IronveilEditorTarget : TargetRules
{
	public IronveilEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Ironveil");
	}
}
