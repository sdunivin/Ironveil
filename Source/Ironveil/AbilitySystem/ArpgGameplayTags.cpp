#include "AbilitySystem/ArpgGameplayTags.h"

namespace ArpgTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Dodge, "InputTag.Dodge", "Tap the Dodge/Sprint button.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Attack_Light, "InputTag.Attack.Light", "Light attack button.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Attack_Heavy, "InputTag.Attack.Heavy", "Heavy attack button (hold to charge).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Block, "InputTag.Block", "Block / parry button.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Grab, "InputTag.Grab", "Grab button.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Flask, "InputTag.Flask", "Health flask button.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_1, "InputTag.Skill.1", "Skill bar slot 1.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_2, "InputTag.Skill.2", "Skill bar slot 2.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_3, "InputTag.Skill.3", "Skill bar slot 3.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_4, "InputTag.Skill.4", "Skill bar slot 4.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_5, "InputTag.Skill.5", "Skill bar slot 5 (Skill Modifier + 1).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_6, "InputTag.Skill.6", "Skill bar slot 6 (Skill Modifier + 2).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_7, "InputTag.Skill.7", "Skill bar slot 7 (Skill Modifier + 3).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_8, "InputTag.Skill.8", "Skill bar slot 8 (Skill Modifier + 4).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dodge, "Ability.Dodge", "The dodge ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Light, "Ability.Attack.Light", "Light combo attacks.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Heavy, "Ability.Attack.Heavy", "Heavy and charged attacks.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Grab, "Ability.Grab", "Grabs, throws, slams and shoves.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee, "Ability.Melee", "Any melee ability; melee support gems require this.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Spell, "Ability.Spell", "Any spell; spell support gems require this.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "Ignores incoming hits (dodge i-frames).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging", "Currently dodging.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Blocking, "State.Blocking", "Holding block.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Staggered, "State.Staggered", "Poise broken: takes more damage and can be grabbed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Grabbed, "State.Grabbed", "Held by another character.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Out of Health.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Physical, "Damage.Physical", "Physical damage, reduced by Armour.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Fire, "Damage.Fire", "Fire damage, reduced by Fire Resistance.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Cold, "Damage.Cold", "Cold damage, reduced by Cold Resistance.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Lightning, "Damage.Lightning", "Lightning damage, reduced by Lightning Resistance.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Chaos, "Damage.Chaos", "Chaos damage, reduced by Chaos Resistance.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Ignited, "Status.Ignited", "Burning: fire damage over time.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Chilled, "Status.Chilled", "Slowed by cold.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Frozen, "Status.Frozen", "Can't act.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Shocked, "Status.Shocked", "Takes increased damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Bleeding, "Status.Bleeding", "Physical damage over time, worse while moving.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Poisoned, "Status.Poisoned", "Stacking chaos damage over time.");
}
