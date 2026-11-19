#include "Data/ArpgGameData.h"

using namespace ArpgColors;

namespace
{
	FArpgSkillDef Skill(const TCHAR* Name, const TCHAR* Desc, float Mana, float Cd, FLinearColor Color)
	{
		FArpgSkillDef S;
		S.Name = Name;
		S.Description = Desc;
		S.ManaCost = Mana;
		S.Cooldown = Cd;
		S.Color = Color;
		return S;
	}

	TArray<FArpgClassDef> BuildClasses()
	{
		TArray<FArpgClassDef> Out;

		{
			FArpgClassDef C;
			C.Class = EArpgClass::Warrior;
			C.Name = TEXT("Warrior");
			C.Tagline = TEXT("Frontline  -  Versatile  -  Resilient");
			C.Description = TEXT("Sword and shield. Heavy hits, a breakable guard and a barrier when the line has to hold.");
			C.Color = Hex(0xD8383A);
			C.Strength = 32.f; C.Dexterity = 14.f; C.Intelligence = 14.f;
			C.StrPerLevel = 2.f; C.DexPerLevel = 1.f; C.IntPerLevel = 0.f;
			C.MoveSpeed = 480.f;
			C.StartingItems = { TEXT("RustedSword"), TEXT("KiteShield"), TEXT("MailHauberk") };
			C.Skills[0] = Skill(TEXT("Power Strike"), TEXT("Overhead blow: 250% weapon damage in a small area, huge poise damage."), 8.f, 2.f, Hex(0xFF5A3C));
			C.Skills[1] = Skill(TEXT("Battle Fury"), TEXT("8 s: 30% more attack speed and 20% more melee damage."), 15.f, 14.f, Hex(0xFF2A2A));
			C.Skills[2] = Skill(TEXT("Shield Barrier"), TEXT("6 s barrier absorbing 30% of max Health; blocking costs no stamina."), 12.f, 12.f, Hex(0x7FB2FF));
			C.Skills[3] = Skill(TEXT("Charged Slash"), TEXT("Dash 9 m, cutting everything in the path for 160% weapon damage."), 10.f, 5.f, Hex(0x4FA8FF));
			C.MeshPath = TEXT("/Game/_Game/Art/Characters/SM_Hero_Warrior.SM_Hero_Warrior");
			Out.Add(C);
		}
		{
			FArpgClassDef C;
			C.Class = EArpgClass::Mage;
			C.Name = TEXT("Mage");
			C.Tagline = TEXT("Control  -  Burst  -  Support");
			C.Description = TEXT("Staff bolts at range, explosive arcane spells, and an energy barrier instead of armour.");
			C.Color = Hex(0x9B5CFF);
			C.Strength = 14.f; C.Dexterity = 14.f; C.Intelligence = 32.f;
			C.StrPerLevel = 0.f; C.DexPerLevel = 1.f; C.IntPerLevel = 2.f;
			C.MoveSpeed = 470.f;
			C.bRangedBasicAttack = true;
			C.StartingItems = { TEXT("OakStaff"), TEXT("LinenRobe") };
			C.Skills[0] = Skill(TEXT("Arcane Blast"), TEXT("Arcane orb: 22-32 lightning damage, explodes in a 2.5 m radius."), 7.f, 0.f, Hex(0xB57BFF));
			C.Skills[1] = Skill(TEXT("Mana Surge"), TEXT("Restore 35% mana and blast nearby enemies away."), 0.f, 12.f, Hex(0x6F8BFF));
			C.Skills[2] = Skill(TEXT("Telekinesis"), TEXT("Lift every enemy in front of you, then slam them down. Staggers."), 14.f, 6.f, Hex(0xD08CFF));
			C.Skills[3] = Skill(TEXT("Spell Barrier"), TEXT("8 s barrier: absorbs 25 + 60% of max Mana as damage."), 18.f, 16.f, Hex(0x8E7BFF));
			C.MeshPath = TEXT("/Game/_Game/Art/Characters/SM_Hero_Mage.SM_Hero_Mage");
			Out.Add(C);
		}
		{
			FArpgClassDef C;
			C.Class = EArpgClass::Thief;
			C.Name = TEXT("Thief");
			C.Tagline = TEXT("Stealth  -  Mobility  -  Precision");
			C.Description = TEXT("Twin blades, long dodges and guaranteed crits from the shadows. Picks the locks nobody else can.");
			C.Color = Hex(0x2EC8EA);
			C.Strength = 14.f; C.Dexterity = 32.f; C.Intelligence = 14.f;
			C.StrPerLevel = 0.f; C.DexPerLevel = 2.f; C.IntPerLevel = 1.f;
			C.DodgeIFrames = 0.35f;
			C.MoveSpeed = 540.f;
			C.StartingItems = { TEXT("TwinStilettos"), TEXT("LeatherJerkin") };
			C.Skills[0] = Skill(TEXT("Shadow Step"), TEXT("Teleport behind your target. Your next hit within 2 s is a critical strike."), 8.f, 3.f, Hex(0x3FE0FF));
			C.Skills[1] = Skill(TEXT("Twin Strike"), TEXT("Two fast cuts for 90% weapon damage each; 30% chance to Bleed, adds Poison."), 6.f, 0.f, Hex(0x2AB8FF));
			C.Skills[2] = Skill(TEXT("Smoke Bomb"), TEXT("4 m cloud for 5 s. Enemies inside lose you; hits on them are backstabs."), 10.f, 8.f, Hex(0x8FA3B0));
			C.Skills[3] = Skill(TEXT("Lockpick"), TEXT("Open a nearby locked coffer for extra loot. In a fight: Expose a target (no armour, 20% more damage taken)."), 4.f, 1.f, Hex(0x9AF0FF));
			C.MeshPath = TEXT("/Game/_Game/Art/Characters/SM_Hero_Thief.SM_Hero_Thief");
			Out.Add(C);
		}
		{
			FArpgClassDef C;
			C.Class = EArpgClass::Monk;
			C.Name = TEXT("Monk");
			C.Tagline = TEXT("Balance  -  Flow  -  Discipline");
			C.Description = TEXT("Fast unarmed chains, self-healing and an unshakeable stance. Strikes flow from enemy to enemy.");
			C.Color = Hex(0xF2B233);
			C.Strength = 22.f; C.Dexterity = 22.f; C.Intelligence = 16.f;
			C.StrPerLevel = 1.f; C.DexPerLevel = 1.f; C.IntPerLevel = 1.f;
			C.DodgeIFrames = 0.3f;
			C.MoveSpeed = 520.f;
			C.StartingItems = { TEXT("WarWraps"), TEXT("Brigandine") };
			C.Skills[0] = Skill(TEXT("Flow Strike"), TEXT("Dash to an enemy and chain to up to 2 more, 140% weapon damage each."), 7.f, 0.f, Hex(0xFFC94A));
			C.Skills[1] = Skill(TEXT("Chi Heal"), TEXT("Heal 30% of max Health over 2 s and clear ailments."), 14.f, 10.f, Hex(0x8CFF9A));
			C.Skills[2] = Skill(TEXT("Zen Resolve"), TEXT("6 s: can't be staggered, +25% to all resistances, 15% less damage taken."), 12.f, 18.f, Hex(0xFFE08A));
			C.Skills[3] = Skill(TEXT("Divine Palm"), TEXT("A golden palm wave that pierces everything: 180% weapon damage, half as fire."), 12.f, 4.f, Hex(0xFFB020));
			C.MeshPath = TEXT("/Game/_Game/Art/Characters/SM_Hero_Monk.SM_Hero_Monk");
			Out.Add(C);
		}
		return Out;
	}

	TArray<FArpgBiomeDef> BuildBiomes()
	{
		TArray<FArpgBiomeDef> Out;
		{
			FArpgBiomeDef B;
			B.Biome = EArpgBiome::Forest;
			B.Name = TEXT("Forest");
			B.Tagline = TEXT("A serene yet mysterious forest, where nature and magic intertwine.");
			B.Floor = Hex(0x4A5A3A); B.FloorAlt = Hex(0x5B5C4E); B.Wall = Hex(0x6E6E66); B.WallAlt = Hex(0x55604A);
			B.Accent = Hex(0x3FA9FF); B.Foliage = Hex(0x2F6B2A); B.Ground = Hex(0x334426);
			B.SunColor = Hex(0xFFF0C8); B.SunIntensity = 7.f; B.SunPitch = -35.f; B.SunYaw = 40.f; B.SkyIntensity = 1.1f;
			B.FogColor = Hex(0x9FC2B0); B.FogDensity = 0.03f; B.SceneTint = FLinearColor(0.95f, 1.0f, 0.95f); B.Saturation = 1.05f;
			B.WallHeightMin = 250.f; B.WallHeightMax = 700.f;
			B.EnemyNames[0] = TEXT("Bandit Swordsman"); B.EnemyNames[1] = TEXT("Bandit Archer"); B.EnemyNames[2] = TEXT("Mercenary Duelist");
			B.EnemyNames[3] = TEXT("Tower-Shield Bandit"); B.EnemyNames[4] = TEXT("Ogre");
			B.EnemyPrimary = Hex(0x5A4630); B.EnemySecondary = Hex(0x3E5A2C);
			B.Element = EArpgDamageType::Physical; B.ElementShare = 0.f;
			Out.Add(B);
		}
		{
			FArpgBiomeDef B;
			B.Biome = EArpgBiome::Jungle;
			B.Name = TEXT("Jungle");
			B.Tagline = TEXT("A dense and dangerous jungle, filled with ancient ruins.");
			B.Floor = Hex(0x3F4A2C); B.FloorAlt = Hex(0x6B6248); B.Wall = Hex(0x7C7A62); B.WallAlt = Hex(0x55663C);
			B.Accent = Hex(0x6CFF8C); B.Foliage = Hex(0x1F7A2E); B.Ground = Hex(0x24391C);
			B.SunColor = Hex(0xFFE7B0); B.SunIntensity = 9.f; B.SunPitch = -55.f; B.SunYaw = 120.f; B.SkyIntensity = 1.0f;
			B.FogColor = Hex(0xB9D6A8); B.FogDensity = 0.045f; B.SceneTint = FLinearColor(0.97f, 1.0f, 0.9f); B.Saturation = 1.15f;
			B.WallHeightMin = 300.f; B.WallHeightMax = 800.f;
			B.EnemyNames[0] = TEXT("Vine Cultist"); B.EnemyNames[1] = TEXT("Blowgun Hunter"); B.EnemyNames[2] = TEXT("Jaguar Warrior");
			B.EnemyNames[3] = TEXT("Stone-Mask Guard"); B.EnemyNames[4] = TEXT("Swamp Hulk");
			B.EnemyPrimary = Hex(0x6B4A2A); B.EnemySecondary = Hex(0x2E7A3A);
			B.Element = EArpgDamageType::Chaos; B.ElementShare = 0.35f;
			Out.Add(B);
		}
		{
			FArpgBiomeDef B;
			B.Biome = EArpgBiome::Wasteland;
			B.Name = TEXT("Post-Apocalyptic City");
			B.Tagline = TEXT("A shattered metropolis, where remnants of the old world still stand.");
			B.Floor = Hex(0x55504A); B.FloorAlt = Hex(0x6A5A48); B.Wall = Hex(0x7A746C); B.WallAlt = Hex(0x8A5A3A);
			B.Accent = Hex(0xFF3CC8); B.Foliage = Hex(0x5A5A3A); B.Ground = Hex(0x4A4038);
			B.SunColor = Hex(0xFFB27A); B.SunIntensity = 6.f; B.SunPitch = -18.f; B.SunYaw = -60.f; B.SkyIntensity = 0.9f;
			B.FogColor = Hex(0xC07A6A); B.FogDensity = 0.035f; B.SceneTint = FLinearColor(1.0f, 0.93f, 0.88f); B.Saturation = 0.9f;
			B.WallHeightMin = 350.f; B.WallHeightMax = 900.f;
			B.EnemyNames[0] = TEXT("Raider"); B.EnemyNames[1] = TEXT("Crossbow Scavenger"); B.EnemyNames[2] = TEXT("Pit Fighter");
			B.EnemyNames[3] = TEXT("Riot-Shield Raider"); B.EnemyNames[4] = TEXT("Mutant Brute");
			B.EnemyPrimary = Hex(0x6A4A34); B.EnemySecondary = Hex(0x3A3A3E);
			B.Element = EArpgDamageType::Fire; B.ElementShare = 0.25f;
			Out.Add(B);
		}
		{
			FArpgBiomeDef B;
			B.Biome = EArpgBiome::Flood;
			B.Name = TEXT("Flood");
			B.Tagline = TEXT("A rising flood washes over the land, turning the tower into an island.");
			B.Floor = Hex(0x4E5560); B.FloorAlt = Hex(0x5C5248); B.Wall = Hex(0x5E6672); B.WallAlt = Hex(0x47525E);
			B.Accent = Hex(0x48B8FF); B.Foliage = Hex(0x3A5A4A); B.Ground = Hex(0x1A2A3A);
			B.SunColor = Hex(0xC8DAFF); B.SunIntensity = 4.f; B.SunPitch = -30.f; B.SunYaw = 200.f; B.SkyIntensity = 0.8f;
			B.FogColor = Hex(0x6A86A8); B.FogDensity = 0.05f; B.SceneTint = FLinearColor(0.88f, 0.94f, 1.0f); B.Saturation = 0.85f;
			B.WallHeightMin = 200.f; B.WallHeightMax = 500.f;
			B.bWater = true;
			B.EnemyNames[0] = TEXT("Drowned Sailor"); B.EnemyNames[1] = TEXT("Harpooner"); B.EnemyNames[2] = TEXT("Tide Reaver");
			B.EnemyNames[3] = TEXT("Barnacle Guard"); B.EnemyNames[4] = TEXT("Deep Brute");
			B.EnemyPrimary = Hex(0x3A5060); B.EnemySecondary = Hex(0x5A6A4A);
			B.Element = EArpgDamageType::Lightning; B.ElementShare = 0.3f;
			Out.Add(B);
		}
		{
			FArpgBiomeDef B;
			B.Biome = EArpgBiome::IceLands;
			B.Name = TEXT("Ice Lands");
			B.Tagline = TEXT("A frozen wasteland, where ice and snow reign supreme.");
			B.Floor = Hex(0xC9D6E2); B.FloorAlt = Hex(0xA9B8C8); B.Wall = Hex(0x8FA6C0); B.WallAlt = Hex(0xB8D8F0);
			B.Accent = Hex(0x5AC8FF); B.Foliage = Hex(0x2E4A3E); B.Ground = Hex(0xDDE6EE);
			B.SunColor = Hex(0xE0EEFF); B.SunIntensity = 7.f; B.SunPitch = -25.f; B.SunYaw = 160.f; B.SkyIntensity = 1.2f;
			B.FogColor = Hex(0xCADCF0); B.FogDensity = 0.04f; B.SceneTint = FLinearColor(0.92f, 0.96f, 1.0f); B.Saturation = 0.9f;
			B.WallHeightMin = 300.f; B.WallHeightMax = 750.f;
			B.EnemyNames[0] = TEXT("Frost Raider"); B.EnemyNames[1] = TEXT("Rime Archer"); B.EnemyNames[2] = TEXT("Ice Dancer");
			B.EnemyNames[3] = TEXT("Glacier Guard"); B.EnemyNames[4] = TEXT("Frost Troll");
			B.EnemyPrimary = Hex(0x4A5A70); B.EnemySecondary = Hex(0xA8C8E0);
			B.Element = EArpgDamageType::Cold; B.ElementShare = 0.35f;
			Out.Add(B);
		}
		{
			FArpgBiomeDef B;
			B.Biome = EArpgBiome::FracturedTime;
			B.Name = TEXT("Fractured Time");
			B.Tagline = TEXT("Eras collide. Something old waits at the centre.");
			B.Floor = Hex(0x4A4652); B.FloorAlt = Hex(0x6A5A3A); B.Wall = Hex(0x5A5866); B.WallAlt = Hex(0x8A7040);
			B.Accent = Hex(0x3FA0FF); B.Foliage = Hex(0x3A4A3A); B.Ground = Hex(0x2A2830);
			B.SunColor = Hex(0xBFD4FF); B.SunIntensity = 5.f; B.SunPitch = -30.f; B.SunYaw = 0.f; B.SkyIntensity = 0.8f;
			B.FogColor = Hex(0x5A6A9A); B.FogDensity = 0.04f; B.SceneTint = FLinearColor(0.92f, 0.94f, 1.0f); B.Saturation = 0.8f;
			B.WallHeightMin = 400.f; B.WallHeightMax = 900.f;
			for (int32 i = 0; i < 5; ++i) { B.EnemyNames[i] = TEXT("Echo"); }
			B.EnemyPrimary = Hex(0x4A4A5A); B.EnemySecondary = Hex(0x8A7040);
			Out.Add(B);
		}
		{
			FArpgBiomeDef B;
			B.Biome = EArpgBiome::Greyhollow;
			B.Name = TEXT("Greyhollow");
			B.Tagline = TEXT("The town at the foot of the Veil Spire. Climb. Survive. Ascend.");
			B.Floor = Hex(0x6A665E); B.FloorAlt = Hex(0x5A5650); B.Wall = Hex(0x7A7468); B.WallAlt = Hex(0x5A4A3A);
			B.Accent = Hex(0x3FA9FF); B.Foliage = Hex(0x3A5A2E); B.Ground = Hex(0x3E4A30);
			B.SunColor = Hex(0xFFE2B8); B.SunIntensity = 6.f; B.SunPitch = -22.f; B.SunYaw = 70.f; B.SkyIntensity = 1.0f;
			B.FogColor = Hex(0x8AA0C8); B.FogDensity = 0.02f;
			for (int32 i = 0; i < 5; ++i) { B.EnemyNames[i] = TEXT("Villager"); }
			B.EnemyPrimary = Hex(0x5A4A3A); B.EnemySecondary = Hex(0x3A3A3A);
			Out.Add(B);
		}
		return Out;
	}

	FArpgEnemyAttackDef Atk(EArpgEnemyAttack Kind, float MinR, float MaxR, float Windup, float Recovery, float Dmg, float Poise,
		float Weight = 1.f, int32 MinPhase = 1, float Cooldown = 0.f)
	{
		FArpgEnemyAttackDef A;
		A.Kind = Kind; A.MinRange = MinR; A.MaxRange = MaxR; A.Windup = Windup; A.Recovery = Recovery;
		A.DamageMult = Dmg; A.Poise = Poise; A.Weight = Weight; A.MinPhase = MinPhase; A.Cooldown = Cooldown;
		return A;
	}

	TArray<FArpgArchetypeDef> BuildArchetypes()
	{
		using K = EArpgEnemyAttack;
		TArray<FArpgArchetypeDef> Out;
		{
			FArpgArchetypeDef D; D.Archetype = EArpgArchetype::Swordsman;
			D.Health10 = 90.f; D.Hit10 = 12.f; D.Armour = 40.f; D.Poise = 30.f; D.MoveSpeed = 390.f; D.XP = 10.f;
			D.Attacks = { Atk(K::Combo3, 0, 230, 0.45f, 0.7f, 1.f, 8.f, 2.f), Atk(K::Lunge, 350, 650, 0.55f, 0.8f, 1.3f, 12.f, 1.f, 1, 3.f) };
			D.MeshPath = TEXT("/Game/_Game/Art/Enemies/SM_Enemy_Swordsman.SM_Enemy_Swordsman");
			Out.Add(D);
		}
		{
			FArpgArchetypeDef D; D.Archetype = EArpgArchetype::Archer;
			D.Health10 = 60.f; D.Hit10 = 14.f; D.Armour = 20.f; D.Evasion = 60.f; D.Poise = 30.f; D.MoveSpeed = 400.f;
			D.PreferredRange = 1100.f; D.AggroRange = 2400.f; D.XP = 10.f;
			D.Attacks = { Atk(K::Shot, 0, 2200, 0.8f, 0.6f, 1.f, 6.f, 3.f), Atk(K::Volley, 400, 1600, 1.0f, 0.9f, 0.7f, 4.f, 1.f, 1, 5.f) };
			D.MeshPath = TEXT("/Game/_Game/Art/Enemies/SM_Enemy_Archer.SM_Enemy_Archer");
			Out.Add(D);
		}
		{
			FArpgArchetypeDef D; D.Archetype = EArpgArchetype::Duelist;
			D.Health10 = 110.f; D.Hit10 = 10.f; D.Armour = 30.f; D.Evasion = 80.f; D.Poise = 30.f; D.MoveSpeed = 460.f; D.bParries = true; D.XP = 12.f;
			D.Attacks = { Atk(K::Combo3, 0, 220, 0.28f, 0.5f, 1.f, 6.f, 3.f), Atk(K::Lunge, 300, 700, 0.35f, 0.6f, 1.2f, 8.f, 1.f, 1, 2.5f) };
			D.MeshPath = TEXT("/Game/_Game/Art/Enemies/SM_Enemy_Duelist.SM_Enemy_Duelist");
			Out.Add(D);
		}
		{
			FArpgArchetypeDef D; D.Archetype = EArpgArchetype::ShieldBrute;
			D.Health10 = 180.f; D.Hit10 = 20.f; D.Armour = 200.f; D.Poise = 80.f; D.PoiseRecoverDelay = 4.f; D.MoveSpeed = 300.f;
			D.Scale = 1.15f; D.bFrontBlock = true; D.XP = 18.f;
			D.Attacks = { Atk(K::Swipe, 0, 240, 0.6f, 0.8f, 1.f, 15.f, 2.f), Atk(K::Slam, 0, 380, 1.0f, 1.1f, 1.6f, 25.f, 1.f, 1, 4.f) };
			D.MeshPath = TEXT("/Game/_Game/Art/Enemies/SM_Enemy_ShieldBrute.SM_Enemy_ShieldBrute");
			Out.Add(D);
		}
		{
			FArpgArchetypeDef D; D.Archetype = EArpgArchetype::Ogre;
			D.Health10 = 420.f; D.Hit10 = 45.f; D.Armour = 120.f; D.Poise = 150.f; D.PoiseRecoverDelay = 5.f; D.MoveSpeed = 330.f;
			D.Scale = 1.7f; D.XP = 45.f;
			D.Attacks = { Atk(K::Swipe, 0, 400, 0.9f, 1.0f, 1.f, 30.f, 2.f), Atk(K::Stomp, 0, 450, 1.1f, 1.2f, 0.9f, 30.f, 1.f, 1, 5.f),
				Atk(K::Charge, 500, 1400, 0.9f, 1.2f, 1.2f, 30.f, 1.f, 1, 7.f) };
			D.MeshPath = TEXT("/Game/_Game/Art/Enemies/SM_Enemy_Ogre.SM_Enemy_Ogre");
			Out.Add(D);
		}
		{
			FArpgArchetypeDef D; D.Archetype = EArpgArchetype::Guardian;
			D.Health10 = 2400.f; D.Hit10 = 30.f; D.Armour = 250.f; D.Poise = 400.f; D.PoiseRecoverDelay = 4.f; D.MoveSpeed = 420.f;
			D.Scale = 1.5f; D.AggroRange = 6000.f; D.XP = 400.f;
			D.Attacks = {
				Atk(K::Combo3, 0, 320, 0.5f, 0.8f, 1.f, 15.f, 3.f),
				Atk(K::Charge, 450, 2000, 0.8f, 1.2f, 1.4f, 40.f, 1.5f, 1, 6.f),
				Atk(K::Shockwave, 0, 900, 1.0f, 1.0f, 1.2f, 25.f, 1.5f, 1, 7.f),
				Atk(K::SlowDome, 0, 1200, 0.8f, 0.6f, 0.f, 0.f, 1.f, 2, 14.f),
				Atk(K::Summon, 0, 9000, 1.0f, 0.6f, 0.f, 0.f, 3.f, 2, 9999.f),
				Atk(K::Leap, 500, 2200, 0.7f, 1.2f, 1.8f, 40.f, 1.5f, 2, 8.f) };
			D.MeshPath = TEXT("/Game/_Game/Art/Enemies/SM_Boss_Guardian.SM_Boss_Guardian");
			Out.Add(D);
		}
		{
			FArpgArchetypeDef D; D.Archetype = EArpgArchetype::FinalBoss;
			D.Health10 = 6000.f; D.Hit10 = 34.f; D.Armour = 400.f; D.Poise = 900.f; D.PoiseRecoverDelay = 4.f; D.MoveSpeed = 450.f;
			D.Scale = 2.0f; D.AggroRange = 9000.f; D.XP = 5000.f;
			D.Attacks = {
				Atk(K::Combo3, 0, 420, 0.5f, 0.7f, 1.f, 20.f, 3.f),
				Atk(K::HourHand, 0, 3000, 0.9f, 1.0f, 0.6f, 5.f, 1.5f, 1, 6.f),
				Atk(K::RingBurst, 0, 1500, 1.0f, 1.2f, 1.0f, 20.f, 1.5f, 1, 9.f),
				Atk(K::SlowDome, 0, 1500, 0.8f, 0.5f, 0.f, 0.f, 1.f, 2, 12.f),
				Atk(K::Summon, 0, 9000, 1.0f, 0.6f, 0.f, 0.f, 2.f, 2, 25.f),
				Atk(K::Leap, 500, 2600, 0.7f, 1.0f, 1.6f, 40.f, 1.5f, 2, 7.f),
				Atk(K::Volley, 0, 3000, 0.7f, 0.8f, 0.7f, 5.f, 1.f, 3, 4.f) };
			D.MeshPath = TEXT("/Game/_Game/Art/Enemies/SM_Boss_HourlessKing.SM_Boss_HourlessKing");
			Out.Add(D);
		}
		return Out;
	}

	TArray<FArpgGuardianDef> BuildGuardians()
	{
		TArray<FArpgGuardianDef> Out;
		auto G = [&Out](const TCHAR* Name, const TCHAR* Title, EArpgBiome Biome, EArpgDamageType Element, uint32 P, uint32 S, const TCHAR* Mesh)
		{
			FArpgGuardianDef D;
			D.Name = Name; D.Title = Title; D.Biome = Biome; D.Element = Element; D.Primary = Hex(P); D.Secondary = Hex(S);
			D.MeshPath = Mesh;
			Out.Add(D);
		};
		// Index 0 is always floor 10 (GDD).
		G(TEXT("Warlord Varrick Dunmar"), TEXT("Bound to the Spire, half-fused with a relic"), EArpgBiome::Forest, EArpgDamageType::Physical, 0x5A3A2A, 0xB08A3A,
			TEXT("/Game/_Game/Art/Enemies/SM_Boss_Warlord.SM_Boss_Warlord"));
		G(TEXT("Ysra, the Thornmother"), TEXT("The jungle remembers her name"), EArpgBiome::Jungle, EArpgDamageType::Chaos, 0x2E5A2A, 0x9AFF5A,
			TEXT("/Game/_Game/Art/Enemies/SM_Boss_Guardian.SM_Boss_Guardian"));
		G(TEXT("The Rust Colossus"), TEXT("A war machine that forgot its war"), EArpgBiome::Wasteland, EArpgDamageType::Fire, 0x6A3A22, 0xFF6A2A,
			TEXT("/Game/_Game/Art/Enemies/SM_Boss_Guardian.SM_Boss_Guardian"));
		G(TEXT("The Drowned Admiral"), TEXT("His fleet sank. He did not."), EArpgBiome::Flood, EArpgDamageType::Lightning, 0x2A3A5A, 0x6ACBFF,
			TEXT("/Game/_Game/Art/Enemies/SM_Boss_Guardian.SM_Boss_Guardian"));
		G(TEXT("Jarl Rimefang"), TEXT("King of a winter that never ended"), EArpgBiome::IceLands, EArpgDamageType::Cold, 0x6A8AB0, 0xDDF2FF,
			TEXT("/Game/_Game/Art/Enemies/SM_Boss_Guardian.SM_Boss_Guardian"));
		return Out;
	}
}

const FArpgClassDef& ArpgData::Class(EArpgClass C)
{
	static const TArray<FArpgClassDef> Classes = BuildClasses();
	return Classes[FMath::Clamp((int32)C, 0, Classes.Num() - 1)];
}

const FArpgBiomeDef& ArpgData::Biome(EArpgBiome B)
{
	static const TArray<FArpgBiomeDef> Biomes = BuildBiomes();
	for (const FArpgBiomeDef& D : Biomes)
	{
		if (D.Biome == B) { return D; }
	}
	return Biomes[0];
}

const FArpgArchetypeDef& ArpgData::Archetype(EArpgArchetype A)
{
	static const TArray<FArpgArchetypeDef> Defs = BuildArchetypes();
	for (const FArpgArchetypeDef& D : Defs)
	{
		if (D.Archetype == A) { return D; }
	}
	return Defs[0];
}

const TArray<EArpgBiome>& ArpgData::FloorBiomes()
{
	static const TArray<EArpgBiome> List = { EArpgBiome::Forest, EArpgBiome::Jungle, EArpgBiome::Wasteland, EArpgBiome::Flood, EArpgBiome::IceLands };
	return List;
}

const FArpgGuardianDef& ArpgData::GuardianForFloor(int32 Floor, int32 RunSeed)
{
	static const TArray<FArpgGuardianDef> Guardians = BuildGuardians();
	if (Floor <= 10)
	{
		return Guardians[0];
	}
	// Floors 20..90: a seeded shuffle so each run meets the guardians in a different order.
	FRandomStream Rng(RunSeed * 7919 + Floor);
	return Guardians[Rng.RandRange(0, Guardians.Num() - 1)];
}

const FArpgGuardianDef& ArpgData::FinalBoss()
{
	static const FArpgGuardianDef King = []()
	{
		FArpgGuardianDef D;
		D.Name = TEXT("The Hourless King");
		D.Title = TEXT("Builder of the Veil Spire");
		D.Biome = EArpgBiome::FracturedTime;
		D.Element = EArpgDamageType::Lightning;
		D.Primary = Hex(0x1E2A4A);
		D.Secondary = Hex(0xC8A040);
		D.MeshPath = TEXT("/Game/_Game/Art/Enemies/SM_Boss_HourlessKing.SM_Boss_HourlessKing");
		return D;
	}();
	return King;
}

FString ArpgData::MonsterModName(EArpgMonsterMod Mod)
{
	switch (Mod)
	{
	case EArpgMonsterMod::Hasted:        return TEXT("Hasted");
	case EArpgMonsterMod::FireEnchanted: return TEXT("Fire Enchanted");
	case EArpgMonsterMod::Armoured:      return TEXT("Armoured");
	case EArpgMonsterMod::Regenerating:  return TEXT("Regenerating");
	case EArpgMonsterMod::Volatile:      return TEXT("Volatile");
	case EArpgMonsterMod::Vampiric:      return TEXT("Vampiric");
	default:                             return TEXT("");
	}
}

float ArpgData::HealthMultiplier(int32 Level)
{
	return FMath::Pow(1.07f, (float)FMath::Max(0, Level - 1));
}

float ArpgData::DamageMultiplier(int32 Level)
{
	return FMath::Pow(1.05f, (float)FMath::Max(0, Level - 1));
}

FString ArpgData::RareMonsterName(FRandomStream& Rng)
{
	static const TCHAR* A[] = { TEXT("Grim"), TEXT("Ash"), TEXT("Rot"), TEXT("Iron"), TEXT("Hollow"), TEXT("Blood"), TEXT("Storm"), TEXT("Gloom"),
		TEXT("Thorn"), TEXT("Rust"), TEXT("Frost"), TEXT("Bone"), TEXT("Dusk"), TEXT("Cinder"), TEXT("Tide") };
	static const TCHAR* B[] = { TEXT("maw"), TEXT("jaw"), TEXT("fang"), TEXT("hide"), TEXT("brand"), TEXT("spine"), TEXT("gut"), TEXT("eye"),
		TEXT("hand"), TEXT("skull"), TEXT("wake"), TEXT("howl") };
	static const TCHAR* C[] = { TEXT("the Unbroken"), TEXT("the Hungry"), TEXT("of the Long Stair"), TEXT("the Faithless"), TEXT("the Drowned"),
		TEXT("the Kinslayer"), TEXT("the Last"), TEXT("of Greyhollow"), TEXT("the Hourbound"), TEXT("the Pale") };
	return FString::Printf(TEXT("%s%s %s"), A[Rng.RandRange(0, UE_ARRAY_COUNT(A) - 1)], B[Rng.RandRange(0, UE_ARRAY_COUNT(B) - 1)],
		C[Rng.RandRange(0, UE_ARRAY_COUNT(C) - 1)]);
}

FString ArpgData::RareItemName(FRandomStream& Rng)
{
	static const TCHAR* A[] = { TEXT("Dread"), TEXT("Gale"), TEXT("Woe"), TEXT("Ember"), TEXT("Rune"), TEXT("Grave"), TEXT("Hate"), TEXT("Storm"),
		TEXT("Tempest"), TEXT("Veil"), TEXT("Sorrow"), TEXT("Wrath"), TEXT("Hour"), TEXT("Rift"), TEXT("Ghoul") };
	static const TCHAR* B[] = { TEXT("Bite"), TEXT("Song"), TEXT("Ward"), TEXT("Mark"), TEXT("Coil"), TEXT("Grasp"), TEXT("Veil"), TEXT("Edge"),
		TEXT("Crest"), TEXT("Shell"), TEXT("Spiral"), TEXT("Hold"), TEXT("Knell") };
	return FString::Printf(TEXT("%s %s"), A[Rng.RandRange(0, UE_ARRAY_COUNT(A) - 1)], B[Rng.RandRange(0, UE_ARRAY_COUNT(B) - 1)]);
}

FString ArpgData::MagicItemPrefix(FRandomStream& Rng)
{
	static const TCHAR* A[] = { TEXT("Tempered"), TEXT("Gleaming"), TEXT("Burnished"), TEXT("Runed"), TEXT("Honed"), TEXT("Etched"),
		TEXT("Shimmering"), TEXT("Veiled"), TEXT("Sturdy"), TEXT("Keen") };
	return A[Rng.RandRange(0, UE_ARRAY_COUNT(A) - 1)];
}

float ArpgData::XPToNext(int32 Level)
{
	return FMath::RoundToFloat(60.f * FMath::Pow((float)Level, 1.7f) + 40.f);
}
