using UnrealBuildTool;
using System.Collections.Generic;

public class IronVeilEditorTarget : TargetRules
{
	public IronVeilEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("IronVeil");
	}
}
