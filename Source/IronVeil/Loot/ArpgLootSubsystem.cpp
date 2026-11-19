#include "Loot/ArpgLootSubsystem.h"

#include "IronVeil.h"
#include "Data/ArpgGameData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// Minimal CSV reader: comma separated, optional double quotes, first row is the header.
	TArray<TArray<FString>> ParseCsv(const FString& Text)
	{
		TArray<TArray<FString>> Rows;
		TArray<FString> Row;
		FString Cell;
		bool bQuoted = false;
		for (int32 i = 0; i < Text.Len(); ++i)
		{
			const TCHAR C = Text[i];
			if (bQuoted)
			{
				if (C == TEXT('"'))
				{
					if (i + 1 < Text.Len() && Text[i + 1] == TEXT('"')) { Cell.AppendChar(TEXT('"')); ++i; }
					else { bQuoted = false; }
				}
				else { Cell.AppendChar(C); }
				continue;
			}
			if (C == TEXT('"')) { bQuoted = true; }
			else if (C == TEXT(',')) { Row.Add(Cell); Cell.Reset(); }
			else if (C == TEXT('\n'))
			{
				Row.Add(Cell); Cell.Reset();
				if (Row.Num() > 1 || !Row[0].IsEmpty()) { Rows.Add(Row); }
				Row.Reset();
			}
			else if (C != TEXT('\r')) { Cell.AppendChar(C); }
		}
		if (!Cell.IsEmpty() || Row.Num() > 0)
		{
			Row.Add(Cell);
			Rows.Add(Row);
		}
		return Rows;
	}

	struct FCsvTable
	{
		TArray<TArray<FString>> Rows;
		TMap<FString, int32> Columns;

		bool Load(const FString& Path)
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path))
			{
				UE_LOG(LogIronveil, Error, TEXT("Loot: could not read %s"), *Path);
				return false;
			}
			Rows = ParseCsv(Text);
			if (Rows.Num() == 0) { return false; }
			for (int32 i = 0; i < Rows[0].Num(); ++i) { Columns.Add(Rows[0][i].TrimStartAndEnd(), i); }
			return true;
		}
		FString Get(int32 Row, const TCHAR* Column) const
		{
			const int32* Idx = Columns.Find(Column);
			return (Idx && Rows[Row].IsValidIndex(*Idx)) ? Rows[Row][*Idx].TrimStartAndEnd() : FString();
		}
		float F(int32 Row, const TCHAR* Column) const { return FCString::Atof(*Get(Row, Column)); }
		int32 I(int32 Row, const TCHAR* Column) const { return FCString::Atoi(*Get(Row, Column)); }
	};

	template <typename TEnum>
	TEnum ParseEnum(const FString& Value, TEnum Default)
	{
		const int64 V = StaticEnum<TEnum>()->GetValueByNameString(Value);
		return V == INDEX_NONE ? Default : (TEnum)V;
	}

	bool HasAnyTag(const FString& ItemTags, const FString& AllowedTags)
	{
		TArray<FString> A, B;
		ItemTags.ParseIntoArray(A, TEXT("|"));
		AllowedTags.ParseIntoArray(B, TEXT("|"));
		for (const FString& T : A)
		{
			if (B.Contains(T)) { return true; }
		}
		return false;
	}

	float RollValue(FRandomStream& Rng, float Min, float Max)
	{
		return (float)FMath::RoundToInt(Rng.FRandRange(Min, Max));
	}

	struct FUniqueDef
	{
		FName Base;
		const TCHAR* Name;
		FName Effect;
		const TCHAR* Text;
		TArray<FName> Rows;
	};

	const TArray<FUniqueDef>& Uniques()
	{
		static const TArray<FUniqueDef> List = {
			{ TEXT("Gauntlets"), TEXT("Varrick's Grip"), TEXT("VarricksGrip"), TEXT("Thrown enemies explode for 50 fire damage on impact"),
				{ TEXT("MaxHealth_T5"), TEXT("AttackAddedPhys_T5"), TEXT("ThrowSlamDamageInc_T3") } },
			{ TEXT("LeatherHood"), TEXT("The Unseen Hand"), TEXT("UnseenHand"), TEXT("Backstabs refresh Smoke Bomb's cooldown"),
				{ TEXT("Dexterity_T5"), TEXT("LocalEvasionInc_T4"), TEXT("ItemRarityInc_T4") } },
			{ TEXT("OakStaff"), TEXT("Emberheart"), TEXT("Emberheart"), TEXT("Ignited enemies spread Ignite to a nearby enemy on death"),
				{ TEXT("SpellAddedFire_T5"), TEXT("SpellDamageInc_T4"), TEXT("CastSpeed_T4") } },
			{ TEXT("ChronoFocus"), TEXT("Stolen Hour"), TEXT("StolenHour"), TEXT("For 3 s after a dodge, cooldowns recover twice as fast"),
				{ TEXT("CooldownRecovery_T3"), TEXT("SkillDurationInc_T3"), TEXT("MaxMana_T5") } },
			{ TEXT("WarWraps"), TEXT("Palm of the Still Lake"), TEXT("StillLake"), TEXT("Divine Palm heals you for 10% of the damage it deals"),
				{ TEXT("LocalAttackSpeed_T5"), TEXT("LocalAddedFire_T5"), TEXT("HealthOnHit_T4") } },
		};
		return List;
	}
}

void UArpgLootSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const FString Dir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"));
	LoadBases(FPaths::Combine(Dir, TEXT("DT_ItemBases.csv")));
	LoadAffixes(FPaths::Combine(Dir, TEXT("DT_Affixes.csv")));
	UE_LOG(LogIronveil, Log, TEXT("Loot: %d item bases, %d affix tiers loaded from %s"), Bases.Num(), Affixes.Num(), *Dir);
}

UArpgLootSubsystem* UArpgLootSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UArpgLootSubsystem>() : nullptr;
}

void UArpgLootSubsystem::LoadBases(const FString& Path)
{
	FCsvTable T;
	if (!T.Load(Path)) { return; }
	for (int32 r = 1; r < T.Rows.Num(); ++r)
	{
		const FName Name(*T.Get(r, TEXT("Name")));
		if (Name.IsNone()) { continue; }
		FArpgItemBaseRow B;
		B.DisplayName = FText::FromString(T.Get(r, TEXT("DisplayName")));
		B.Slot = ParseEnum<EArpgEquipSlot>(T.Get(r, TEXT("Slot")), EArpgEquipSlot::MainHand);
		B.Handedness = ParseEnum<EArpgHandedness>(T.Get(r, TEXT("Handedness")), EArpgHandedness::None);
		B.ItemTags = T.Get(r, TEXT("ItemTags"));
		B.RequiredLevel = T.I(r, TEXT("RequiredLevel"));
		B.ReqStrength = T.I(r, TEXT("ReqStrength"));
		B.ReqDexterity = T.I(r, TEXT("ReqDexterity"));
		B.ReqIntelligence = T.I(r, TEXT("ReqIntelligence"));
		B.PhysicalMin = T.F(r, TEXT("PhysicalMin"));
		B.PhysicalMax = T.F(r, TEXT("PhysicalMax"));
		B.AttacksPerSecond = T.F(r, TEXT("AttacksPerSecond"));
		B.CritChance = T.F(r, TEXT("CritChance"));
		B.Armour = T.F(r, TEXT("Armour"));
		B.Evasion = T.F(r, TEXT("Evasion"));
		B.EnergyShield = T.F(r, TEXT("EnergyShield"));
		B.BlockChance = T.F(r, TEXT("BlockChance"));
		B.ImplicitText = FText::FromString(T.Get(r, TEXT("ImplicitText")));
		B.ImplicitStat = FName(*T.Get(r, TEXT("ImplicitStat")));
		B.ImplicitMin = T.F(r, TEXT("ImplicitMin"));
		B.ImplicitMax = T.F(r, TEXT("ImplicitMax"));
		B.InventoryWidth = T.I(r, TEXT("InventoryWidth"));
		B.InventoryHeight = T.I(r, TEXT("InventoryHeight"));
		B.bCanDrop = T.Get(r, TEXT("bCanDrop")).ToBool();
		B.DropWeight = T.I(r, TEXT("DropWeight"));
		Bases.Add(Name, B);
		BaseOrder.Add(Name);
	}
}

void UArpgLootSubsystem::LoadAffixes(const FString& Path)
{
	FCsvTable T;
	if (!T.Load(Path)) { return; }
	for (int32 r = 1; r < T.Rows.Num(); ++r)
	{
		const FName Name(*T.Get(r, TEXT("Name")));
		if (Name.IsNone()) { continue; }
		FArpgAffixRow A;
		A.AffixType = ParseEnum<EArpgAffixType>(T.Get(r, TEXT("AffixType")), EArpgAffixType::Prefix);
		A.ModGroup = FName(*T.Get(r, TEXT("ModGroup")));
		A.Tier = T.I(r, TEXT("Tier"));
		A.MinItemLevel = T.I(r, TEXT("MinItemLevel"));
		A.Stat = FName(*T.Get(r, TEXT("Stat")));
		A.ModType = ParseEnum<EArpgModType>(T.Get(r, TEXT("ModType")), EArpgModType::Flat);
		A.ValueMin = T.F(r, TEXT("ValueMin"));
		A.ValueMax = T.F(r, TEXT("ValueMax"));
		A.Value2Min = T.F(r, TEXT("Value2Min"));
		A.Value2Max = T.F(r, TEXT("Value2Max"));
		A.Weight = T.I(r, TEXT("Weight"));
		A.AllowedTags = T.Get(r, TEXT("AllowedTags"));
		A.DisplayText = FText::FromString(T.Get(r, TEXT("DisplayText")));
		Affixes.Add(Name, A);
		AffixOrder.Add(Name);
	}
}

FString UArpgLootSubsystem::FormatAffix(const FString& Template, float V, float V2)
{
	FString S = Template.Replace(TEXT("{0}"), *FString::Printf(TEXT("%d"), FMath::RoundToInt(V)));
	return S.Replace(TEXT("{1}"), *FString::Printf(TEXT("%d"), FMath::RoundToInt(V2)));
}

FArpgItem UArpgLootSubsystem::MakeBaseItem(FName BaseName, int32 ItemLevel) const
{
	FArpgItem Item;
	const FArpgItemBaseRow* Base = Bases.Find(BaseName);
	if (!Base) { return Item; }
	Item.BaseName = BaseName;
	Item.ItemLevel = ItemLevel;
	Item.Rarity = EArpgRarity::Normal;
	Item.DisplayName = Base->DisplayName.ToString();
	Item.ImplicitValue = FMath::RoundToFloat((Base->ImplicitMin + Base->ImplicitMax) * 0.5f);
	return Item;
}

void UArpgLootSubsystem::AddAffixes(FRandomStream& Rng, FArpgItem& Item, const FArpgItemBaseRow& Base, int32 Count, int32 MaxPrefix, int32 MaxSuffix) const
{
	for (int32 n = 0; n < Count; ++n)
	{
		int32 NumP = 0, NumS = 0;
		TSet<FName> Used;
		for (const FArpgRolledAffix& A : Item.Affixes)
		{
			(A.AffixType == EArpgAffixType::Prefix ? NumP : NumS)++;
			Used.Add(A.ModGroup);
		}
		TArray<const FName*> Options;
		int64 TotalWeight = 0;
		for (const FName& RowName : AffixOrder)
		{
			const FArpgAffixRow& A = Affixes[RowName];
			if (A.MinItemLevel > Item.ItemLevel || Used.Contains(A.ModGroup)) { continue; }
			if (A.AffixType == EArpgAffixType::Prefix ? NumP >= MaxPrefix : NumS >= MaxSuffix) { continue; }
			if (!HasAnyTag(Base.ItemTags, A.AllowedTags)) { continue; }
			Options.Add(&RowName);
			TotalWeight += A.Weight;
		}
		if (Options.Num() == 0 || TotalWeight <= 0) { return; }
		int64 Pick = (int64)(Rng.FRand() * (double)TotalWeight);
		const FName* Chosen = Options.Last();
		for (const FName* RowName : Options)
		{
			Pick -= Affixes[*RowName].Weight;
			if (Pick < 0) { Chosen = RowName; break; }
		}
		const FArpgAffixRow& Row = Affixes[*Chosen];
		FArpgRolledAffix R;
		R.RowName = *Chosen;
		R.ModGroup = Row.ModGroup;
		R.Stat = Row.Stat;
		R.AffixType = Row.AffixType;
		R.ModType = Row.ModType;
		R.Tier = Row.Tier;
		R.Value = RollValue(Rng, Row.ValueMin, Row.ValueMax);
		R.Value2 = RollValue(Rng, Row.Value2Min, Row.Value2Max);
		R.Text = FormatAffix(Row.DisplayText.ToString(), R.Value, R.Value2);
		Item.Affixes.Add(R);
	}
}

FArpgItem UArpgLootSubsystem::RollBase(FRandomStream& Rng, FName BaseName, int32 ItemLevel, EArpgRarity Rarity) const
{
	FArpgItem Item = MakeBaseItem(BaseName, ItemLevel);
	const FArpgItemBaseRow* Base = Bases.Find(BaseName);
	if (!Base) { return Item; }
	Item.ImplicitValue = RollValue(Rng, Base->ImplicitMin, Base->ImplicitMax);
	Item.Rarity = Rarity;
	if (Rarity == EArpgRarity::Magic)
	{
		AddAffixes(Rng, Item, *Base, Rng.FRand() < 0.5f ? 1 : 2, 1, 1);
		Item.DisplayName = FString::Printf(TEXT("%s %s"), *ArpgData::MagicItemPrefix(Rng), *Base->DisplayName.ToString());
	}
	else if (Rarity == EArpgRarity::Rare)
	{
		// 4 / 5 / 6 affixes at 45 / 35 / 20 (GDD, Tools/loot_sim.py).
		const float Roll = Rng.FRand();
		AddAffixes(Rng, Item, *Base, Roll < 0.45f ? 4 : (Roll < 0.8f ? 5 : 6), 3, 3);
		Item.DisplayName = ArpgData::RareItemName(Rng);
	}
	else if (Rarity == EArpgRarity::Unique)
	{
		return RollUnique(Rng, ItemLevel);
	}
	return Item;
}

FArpgItem UArpgLootSubsystem::RollUnique(FRandomStream& Rng, int32 ItemLevel) const
{
	const TArray<FUniqueDef>& List = Uniques();
	const FUniqueDef& U = List[Rng.RandRange(0, List.Num() - 1)];
	FArpgItem Item = MakeBaseItem(U.Base, ItemLevel);
	if (!Item.IsValid()) { return Item; }
	Item.Rarity = EArpgRarity::Unique;
	Item.DisplayName = U.Name;
	Item.UniqueEffect = U.Effect;
	Item.UniqueText = U.Text;
	for (const FName& RowName : U.Rows)
	{
		if (const FArpgAffixRow* Row = Affixes.Find(RowName))
		{
			FArpgRolledAffix R;
			R.RowName = RowName; R.ModGroup = Row->ModGroup; R.Stat = Row->Stat; R.AffixType = Row->AffixType; R.ModType = Row->ModType;
			R.Tier = Row->Tier;
			R.Value = RollValue(Rng, Row->ValueMin, Row->ValueMax);
			R.Value2 = RollValue(Rng, Row->Value2Min, Row->Value2Max);
			R.Text = FormatAffix(Row->DisplayText.ToString(), R.Value, R.Value2);
			Item.Affixes.Add(R);
		}
	}
	return Item;
}

FArpgItem UArpgLootSubsystem::RollItem(FRandomStream& Rng, int32 ItemLevel, float IncRarity, EArpgRarity MinRarity) const
{
	// Pick a base by DropWeight among bases the item level allows.
	TArray<const FName*> Options;
	int64 Total = 0;
	for (const FName& Name : BaseOrder)
	{
		const FArpgItemBaseRow& B = Bases[Name];
		if (!B.bCanDrop || B.RequiredLevel > ItemLevel + 2) { continue; }
		Options.Add(&Name);
		Total += B.DropWeight;
	}
	if (Options.Num() == 0) { return FArpgItem(); }
	int64 Pick = (int64)(Rng.FRand() * (double)Total);
	FName BaseName = *Options.Last();
	for (const FName* Name : Options)
	{
		Pick -= Bases[*Name].DropWeight;
		if (Pick < 0) { BaseName = *Name; break; }
	}

	// Rarity: Normal 70, Magic 25, Rare 4.7, Unique 0.3. Increased rarity multiplies the non-normal weights.
	const float RarityMult = 1.f + IncRarity / 100.f;
	float W[4] = { 70.f, 25.f * RarityMult, 4.7f * RarityMult, 0.3f * RarityMult };
	for (int32 i = 0; i < (int32)MinRarity; ++i) { W[i] = 0.f; }
	const float Sum = W[0] + W[1] + W[2] + W[3];
	float R = Rng.FRand() * Sum;
	EArpgRarity Rarity = EArpgRarity::Rare;
	for (int32 i = 0; i < 4; ++i)
	{
		R -= W[i];
		if (R <= 0.f) { Rarity = (EArpgRarity)i; break; }
	}
	if (Rarity == EArpgRarity::Unique)
	{
		return RollUnique(Rng, ItemLevel);
	}
	return RollBase(Rng, BaseName, ItemLevel, Rarity);
}

FString UArpgLootSubsystem::SlotName(const FArpgItem& Item) const
{
	const FArpgItemBaseRow* Base = Bases.Find(Item.BaseName);
	if (!Base) { return TEXT(""); }
	FString Slot = StaticEnum<EArpgEquipSlot>()->GetNameStringByValue((int64)Base->Slot);
	if (Base->Handedness == EArpgHandedness::TwoHand) { Slot += TEXT(" (two-handed)"); }
	return Slot;
}

TArray<FString> UArpgLootSubsystem::DescribeItem(const FArpgItem& Item) const
{
	TArray<FString> Lines;
	const FArpgItemBaseRow* Base = Bases.Find(Item.BaseName);
	if (!Base) { return Lines; }
	Lines.Add(Item.DisplayName);
	if (Item.Rarity == EArpgRarity::Rare || Item.Rarity == EArpgRarity::Unique)
	{
		Lines.Add(Base->DisplayName.ToString());
	}
	Lines.Add(FString::Printf(TEXT("%s  -  item level %d"), *SlotName(Item), Item.ItemLevel));
	if (Base->PhysicalMax > 0.f)
	{
		Lines.Add(FString::Printf(TEXT("Physical %d-%d  -  %.2f attacks/s  -  %.1f%% crit"), FMath::RoundToInt(Base->PhysicalMin),
			FMath::RoundToInt(Base->PhysicalMax), Base->AttacksPerSecond, Base->CritChance));
	}
	if (Base->Armour > 0.f) { Lines.Add(FString::Printf(TEXT("Armour %d"), FMath::RoundToInt(Base->Armour))); }
	if (Base->Evasion > 0.f) { Lines.Add(FString::Printf(TEXT("Evasion %d"), FMath::RoundToInt(Base->Evasion))); }
	if (Base->EnergyShield > 0.f) { Lines.Add(FString::Printf(TEXT("Energy Shield %d"), FMath::RoundToInt(Base->EnergyShield))); }
	if (Base->BlockChance > 0.f) { Lines.Add(FString::Printf(TEXT("Block %d%%"), FMath::RoundToInt(Base->BlockChance))); }
	const int32 ReqAttr = Base->ReqStrength + Base->ReqDexterity + Base->ReqIntelligence;
	if (ReqAttr > 0 || Base->RequiredLevel > 1)
	{
		Lines.Add(FString::Printf(TEXT("Requires level %d, %d Str, %d Dex, %d Int"), Base->RequiredLevel, Base->ReqStrength, Base->ReqDexterity,
			Base->ReqIntelligence));
	}
	if (!Base->ImplicitText.IsEmpty())
	{
		Lines.Add(TEXT("~ ") + FormatAffix(Base->ImplicitText.ToString(), Item.ImplicitValue, 0.f));
	}
	for (const FArpgRolledAffix& A : Item.Affixes)
	{
		Lines.Add(FString::Printf(TEXT("%s  (%s T%d)"), *A.Text, A.AffixType == EArpgAffixType::Prefix ? TEXT("P") : TEXT("S"), A.Tier));
	}
	if (!Item.UniqueText.IsEmpty())
	{
		Lines.Add(TEXT("* ") + Item.UniqueText);
	}
	return Lines;
}
