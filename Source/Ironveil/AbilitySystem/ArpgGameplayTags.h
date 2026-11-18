// Native Gameplay Tags: tags defined in C++ so code and Blueprints share one list
// and a typo becomes a compile error instead of a silent bug.
//
// Use in C++:        ArpgTags::State_Invulnerable
// Use in Blueprint:  pick "State.Invulnerable" from any Gameplay Tag dropdown.
//
// To add a tag: declare it here, define it in ArpgGameplayTags.cpp, compile.

#pragma once

#include "NativeGameplayTags.h"

namespace ArpgTags
{
	// ---- Input: which button an ability is bound to (set on each ability's InputTag) ----
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Dodge);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Attack_Light);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Attack_Heavy);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Block);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Grab);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Flask);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_1);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_2);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_3);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_4);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_5);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_6);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_7);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Skill_8);

	// ---- Ability: what kind of ability this is (support gems check these) ----
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dodge);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Light);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Heavy);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Grab);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Spell);

	// ---- State: what is happening to a character right now ----
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Blocking);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Staggered);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Grabbed);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	// ---- Damage types ----
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Physical);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Fire);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Cold);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Lightning);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Chaos);

	// ---- Status: ailments ----
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Ignited);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Chilled);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Frozen);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Shocked);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Bleeding);
	IRONVEIL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Poisoned);
}
