"""
Builds Ironveil's blockout meshes for the playable prototype, from the concept sheets in the repo's planning:
the Mage, Thief and Monk heroes (the Warrior has its own refined script, build_warrior_blockout.py), the five enemy
archetypes, the guardians and the Hourless King, every biome prop the floor generator places, and the Veil Spire.

Run from the repository root (this replaces the open scene):
    blender --background --python Tools/blender/build_ironveil_assets.py -- [OUTPUT_FOLDER] [--no-render] [--only Props]
Defaults to Art/Export. Writes:
    Art/Export/Characters/SM_Hero_*.fbx      Art/Export/Enemies/SM_Enemy_*.fbx, SM_Boss_*.fbx
    Art/Export/Props/SM_Prop_*.fbx           Art/Export/Tower/SM_VeilSpire.fbx
    Art/Export/Ironveil_Blockouts.blend      Art/Export/Previews/*.png (contact sheets)
Then import them into Unreal with Tools/unreal/setup_ironveil_content.py; the game uses them automatically.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
from ironveil_kit import Builder, export_fbx, hexcol, humanoid, mat, render_sheet, reset_scene  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
FLAGS = {a for a in ARGS if a.startswith("--")}
POSITIONAL = [a for a in ARGS if not a.startswith("--")]
ONLY = None
if "--only" in ARGS:
    i = ARGS.index("--only")
    ONLY = ARGS[i + 1] if i + 1 < len(ARGS) else None
    POSITIONAL = [a for a in POSITIONAL if a != ONLY]
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.abspath(POSITIONAL[0]) if POSITIONAL else os.path.join(ROOT, "Art", "Export")
RENDER = "--no-render" not in FLAGS

scene = reset_scene()

# ---------------------------------------------------------------- palette
C = {k: hexcol(v) for k, v in {
    "skin": "C08A6A", "skin_pale": "E8D2C8", "skin_tan": "A87050", "skin_green": "6A8A5A",
    "steel": "8A9098", "dark_steel": "3A3D44", "iron": "4A4A50", "brass": "B08A3A", "gold": "E8B030",
    "leather": "4A2E1E", "dark_leather": "15171C", "wood": "5A3A22", "bark": "3E2A1A",
    "red": "8E1E22", "purple": "3A1E6A", "violet": "B57BFF", "cyan": "2EC8EA", "white_wrap": "E8E2D2",
    "cloth": "5A4630", "moss": "3E5A2C", "foliage": "2F6B2A", "jungle": "1F7A2E", "stone": "6E6E66",
    "stone_dark": "4A4A46", "concrete": "8A8680", "rust": "8A4A22", "ice": "9ED0F0", "snow": "EEF2F8",
    "relic": "3FA0FF", "neon": "FF3CC8", "glass": "A8D8F0", "black": "101014",
}.items()}

M = {
    "skin": mat("M_Skin", C["skin"], rough=0.55), "skin_pale": mat("M_SkinPale", C["skin_pale"], rough=0.5),
    "skin_tan": mat("M_SkinTan", C["skin_tan"], rough=0.55), "skin_green": mat("M_SkinOgre", C["skin_green"], rough=0.7),
    "steel": mat("M_Steel", C["steel"], 1.0, 0.35), "dark_steel": mat("M_DarkSteel", C["dark_steel"], 1.0, 0.45),
    "iron": mat("M_Iron", C["iron"], 0.9, 0.55), "brass": mat("M_Brass", C["brass"], 1.0, 0.35), "gold": mat("M_Gold", C["gold"], 1.0, 0.3),
    "leather": mat("M_Leather", C["leather"], rough=0.7), "dark_leather": mat("M_DarkLeather", C["dark_leather"], rough=0.6),
    "wood": mat("M_Wood", C["wood"], rough=0.8), "bark": mat("M_Bark", C["bark"], rough=0.95),
    "red": mat("M_RedCloth", C["red"], rough=0.85), "purple": mat("M_Robe", C["purple"], rough=0.8),
    "violet_glow": mat("M_ArcaneGlow", C["violet"], emit=6.0), "cyan_glow": mat("M_CyanGlow", C["cyan"], emit=5.0),
    "gold_glow": mat("M_GoldGlow", C["gold"], 1.0, 0.3, emit=3.0), "relic": mat("M_RelicGlow", C["relic"], emit=8.0),
    "white_wrap": mat("M_Wrap", C["white_wrap"], rough=0.85), "cloth": mat("M_Cloth", C["cloth"], rough=0.9),
    "moss": mat("M_Moss", C["moss"], rough=0.95), "foliage": mat("M_Foliage", C["foliage"], rough=0.8),
    "jungle": mat("M_JungleLeaf", C["jungle"], rough=0.7), "stone": mat("M_Stone", C["stone"], rough=0.9),
    "stone_dark": mat("M_StoneDark", C["stone_dark"], rough=0.9), "concrete": mat("M_Concrete", C["concrete"], rough=0.95),
    "rust": mat("M_Rust", C["rust"], 0.6, 0.8), "ice": mat("M_Ice", C["ice"], 0.0, 0.08, emit=0.4), "snow": mat("M_Snow", C["snow"], rough=0.6),
    "neon": mat("M_Neon", C["neon"], emit=8.0), "glass": mat("M_Glass", C["glass"], 0.0, 0.05), "black": mat("M_Black", C["black"], rough=0.6),
    "hair_silver": mat("M_HairSilver", hexcol("D8D8E8"), rough=0.5), "lamp": mat("M_LampGlow", hexcol("FFD08A"), emit=6.0),
}


def collection(name):
    c = bpy.data.collections.new(name)
    scene.collection.children.link(c)
    return c


ASSETS = {}   # category -> list of (name, object)


def asset(category, name):
    """Decorator: builds, joins and registers one asset."""
    def wrap(fn):
        def run(col):
            b = Builder(name)
            fn(b)
            obj = b.join(col)
            ASSETS.setdefault(category, []).append((name, obj))
            return obj
        run.category = category
        run.asset_name = name
        BUILDERS.append(run)
        return run
    return wrap


BUILDERS = []

# ================================================================ HEROES (from the class concept sheet)


@asset("Characters", "SM_Hero_Mage")
def hero_mage(b):
    p = humanoid(b, M["skin_pale"], M["purple"], M["purple"], M["purple"], height=1.78, bulk=0.88, arm_pose=(20, 28))
    b.cone((0, 0, 0.55), 0.42, 1.1, M["purple"], tip=0.2)                       # long robe
    b.cone((0, 0, 0.52), 0.43, 0.06, M["gold"], tip=0.43)                       # hem trim
    b.box((0, -0.125, 1.15), (0.06, 0.02, 0.85), M["gold"])                      # front trim
    b.box((0, 0.02, 1.52), (0.5, 0.3, 0.12), M["purple"], rot=(8, 0, 0))         # high collar
    b.sph((0, 0.06, 1.7), (0.25, 0.27, 0.33), M["hair_silver"])                 # silver hair
    b.box((0, 0.13, 1.35), (0.26, 0.05, 0.6), M["hair_silver"], rot=(-6, 0, 0))
    for s in (-1, 1):
        b.cone((s * 0.115, 0.0, 1.69), 0.025, 0.12, M["skin_pale"], rot=(0, s * -70, 0))  # ears
        b.box((s * 0.3, 0.02, 1.5), (0.16, 0.22, 0.06), M["gold"], rot=(0, s * 20, 0))    # pauldrons
    # staff in the right hand, with a ring and orb
    hr = p["hand_r"]
    b.cyl((hr.x, hr.y - 0.02, hr.z + 0.25), 0.022, 1.9, M["black"])
    b.tor((hr.x, hr.y - 0.02, hr.z + 1.22), 0.14, 0.015, M["gold"], rot=(90, 0, 0))
    b.sph((hr.x, hr.y - 0.02, hr.z + 1.22), 0.13, M["violet_glow"])
    # arcane sigil around the left hand
    hl = p["hand_l"]
    b.tor((hl.x, hl.y - 0.1, hl.z + 0.05), 0.2, 0.01, M["violet_glow"], rot=(90, 0, 0))
    b.sph((hl.x, hl.y - 0.1, hl.z + 0.05), 0.07, M["violet_glow"])


@asset("Characters", "SM_Hero_Thief")
def hero_thief(b):
    p = humanoid(b, M["skin"], M["dark_leather"], M["dark_leather"], M["dark_leather"], height=1.8, bulk=0.9, arm_pose=(22, 22))
    b.cone((0, 0.02, 1.72), 0.17, 0.42, M["dark_leather"], tip=0.02, rot=(-8, 0, 0))   # hood
    b.box((0, -0.11, 1.62), (0.18, 0.06, 0.1), M["black"])                            # mask
    for s in (-1, 1):
        b.box((s * 0.12, -0.13, 1.66), (0.05, 0.01, 0.015), M["cyan_glow"])          # eye glints
        b.box((s * 0.16, 0.08, 0.75), (0.12, 0.03, 0.75), M["dark_leather"], rot=(8, 0, s * 6))  # coat tails
    b.box((0, -0.125, 1.25), (0.02, 0.01, 0.42), M["cyan_glow"])                       # glowing seam
    b.box((0, 0, 1.0), (0.44, 0.26, 0.06), M["leather"])                               # belt
    for x in (-0.15, 0.0, 0.15):
        b.box((x, -0.14, 0.97), (0.07, 0.05, 0.08), M["leather"])                      # pouches
    for hand in (p["hand_l"], p["hand_r"]):
        b.box((hand.x, hand.y - 0.18, hand.z - 0.02), (0.02, 0.36, 0.05), M["steel"], rot=(0, 0, 0))   # daggers
        b.box((hand.x, hand.y - 0.18, hand.z - 0.02), (0.022, 0.36, 0.01), M["cyan_glow"])
        b.box((hand.x, hand.y + 0.02, hand.z - 0.02), (0.08, 0.03, 0.03), M["dark_steel"])


@asset("Characters", "SM_Hero_Monk")
def hero_monk(b):
    p = humanoid(b, M["skin_tan"], M["white_wrap"], M["black"], M["skin_tan"], height=1.8, bulk=1.05, arm_pose=(18, 18))
    b.cone((0, 0, 0.45), 0.33, 0.8, M["black"], tip=0.2)                                     # wide trousers
    b.box((0, -0.01, 1.22), (0.46, 0.26, 0.12), M["gold"], rot=(0, 30, 0))                    # sash across chest
    b.box((0, 0, 0.98), (0.46, 0.28, 0.12), M["gold"])                                        # belt sash
    b.box((0.1, -0.15, 0.7), (0.12, 0.02, 0.5), M["gold"])                                    # sash tail
    for i in range(16):                                                                       # prayer beads hanging on the chest
        a = math.pi * i / 15
        b.sph((math.cos(a) * 0.16, -0.13, 1.5 - math.sin(a) * 0.26), 0.045, M["bark"])
    b.box((0, -0.115, 1.73), (0.12, 0.01, 0.015), M["gold_glow"])                             # forehead mark
    for hand in (p["hand_l"], p["hand_r"]):
        b.tor((hand.x, hand.y, hand.z + 0.08), 0.12, 0.012, M["gold_glow"], rot=(90, 0, 0))   # golden chakram rings
        b.tor((hand.x, hand.y, hand.z + 0.08), 0.16, 0.008, M["gold_glow"], rot=(80, 25, 0))
        b.cyl((hand.x, hand.y, hand.z + 0.14), 0.06, 0.1, M["white_wrap"])                    # hand wraps

# ================================================================ ENEMIES


@asset("Enemies", "SM_Enemy_Swordsman")
def enemy_swordsman(b):
    p = humanoid(b, M["skin"], M["leather"], M["cloth"], M["leather"], arm_pose=(15, 25))
    b.cone((0, 0.02, 1.75), 0.15, 0.3, M["moss"], tip=0.02, rot=(-10, 0, 0))
    hr = p["hand_r"]
    b.box((hr.x, hr.y - 0.45, hr.z), (0.03, 0.85, 0.07), M["steel"])
    b.box((hr.x, hr.y - 0.02, hr.z), (0.2, 0.04, 0.04), M["iron"])


@asset("Enemies", "SM_Enemy_Archer")
def enemy_archer(b):
    p = humanoid(b, M["skin"], M["moss"], M["cloth"], M["moss"], bulk=0.92, arm_pose=(30, 15))
    hl = p["hand_l"]
    b.limb((hl.x, hl.y - 0.1, hl.z + 0.6), (hl.x, hl.y - 0.25, hl.z), 0.018, M["wood"])
    b.limb((hl.x, hl.y - 0.25, hl.z), (hl.x, hl.y - 0.1, hl.z - 0.6), 0.018, M["wood"])
    b.limb((hl.x, hl.y - 0.1, hl.z + 0.6), (hl.x, hl.y - 0.1, hl.z - 0.6), 0.004, M["white_wrap"])
    b.cyl((0.08, 0.16, 1.3), 0.07, 0.55, M["leather"], rot=(15, 0, 10))


@asset("Enemies", "SM_Enemy_Duelist")
def enemy_duelist(b):
    p = humanoid(b, M["skin"], M["red"], M["black"], M["skin"], bulk=0.95, arm_pose=(35, 35))
    b.box((0, 0, 1.0), (0.46, 0.28, 0.08), M["gold"])
    for hand in (p["hand_l"], p["hand_r"]):
        b.box((hand.x, hand.y - 0.38, hand.z + 0.05), (0.02, 0.72, 0.05), M["steel"], rot=(12, 0, 0))


@asset("Enemies", "SM_Enemy_ShieldBrute")
def enemy_brute(b):
    p = humanoid(b, M["skin"], M["iron"], M["cloth"], M["leather"], height=1.95, bulk=1.3, arm_pose=(10, 20))
    b.sph((0, -0.01, 1.82), (0.26, 0.28, 0.24), M["iron"])
    hl, hr = p["hand_l"], p["hand_r"]
    b.box((hl.x + 0.05, hl.y - 0.25, hl.z + 0.35), (0.7, 0.08, 1.15), M["wood"], bevel=0.02)
    b.box((hl.x + 0.05, hl.y - 0.3, hl.z + 0.35), (0.12, 0.04, 0.9), M["iron"])
    b.cyl((hr.x, hr.y - 0.1, hr.z - 0.3), 0.035, 0.7, M["wood"])
    b.sph((hr.x, hr.y - 0.1, hr.z - 0.68), 0.2, M["iron"])


@asset("Enemies", "SM_Enemy_Ogre")
def enemy_ogre(b):
    p = humanoid(b, M["skin_green"], M["leather"], M["cloth"], M["skin_green"], height=3.0, bulk=1.7, arm_pose=(18, 22))
    b.sph((0, -0.15, 1.75), (1.0, 0.8, 0.9), M["skin_green"])                                  # belly
    hr = p["hand_r"]
    b.limb((hr.x, hr.y, hr.z), (hr.x + 0.1, hr.y - 0.3, hr.z - 1.1), 0.08, M["wood"])
    b.sph((hr.x + 0.12, hr.y - 0.33, hr.z - 1.2), (0.42, 0.42, 0.55), M["wood"])


@asset("Enemies", "SM_Boss_Warlord")
def boss_warlord(b):
    p = humanoid(b, M["skin"], M["dark_steel"], M["dark_steel"], M["dark_steel"], height=2.4, bulk=1.4, arm_pose=(12, 30))
    b.sph((0, 0, 2.25), (0.34, 0.36, 0.34), M["dark_steel"])                                   # helm
    for s in (-1, 1):
        b.cone((s * 0.2, 0, 2.45), 0.05, 0.4, M["brass"], rot=(0, s * 25, 0))                   # horns
        b.box((s * 0.42, 0.02, 1.95), (0.32, 0.4, 0.12), M["brass"], rot=(0, s * 18, 0))       # pauldrons
    b.sph((0, -0.2, 1.7), 0.18, M["relic"])                                                    # Veil Relic in the chest
    b.tor((0, -0.2, 1.7), 0.2, 0.025, M["brass"], rot=(90, 0, 0))
    b.box((0, 0.24, 1.2), (0.8, 0.04, 1.6), M["red"], rot=(-6, 0, 0))                           # cape
    hr = p["hand_r"]
    b.box((hr.x, hr.y - 0.8, hr.z + 0.05), (0.05, 1.6, 0.14), M["steel"])                      # greatsword
    b.box((hr.x, hr.y - 0.02, hr.z + 0.05), (0.4, 0.06, 0.06), M["brass"])


@asset("Enemies", "SM_Boss_Guardian")
def boss_guardian(b):
    p = humanoid(b, M["skin"], M["iron"], M["stone_dark"], M["iron"], height=2.5, bulk=1.5, arm_pose=(20, 20))
    for s in (-1, 1):
        b.cone((s * 0.22, 0, 2.5), 0.07, 0.5, M["bark"], rot=(0, s * 35, 0))
    b.sph((0, -0.22, 1.75), 0.2, M["relic"])
    b.box((0, 0.26, 1.2), (0.9, 0.05, 1.7), M["moss"], rot=(-6, 0, 0))
    hr = p["hand_r"]
    b.limb((hr.x, hr.y, hr.z), (hr.x, hr.y - 0.2, hr.z - 1.2), 0.07, M["wood"])
    b.box((hr.x, hr.y - 0.2, hr.z - 1.3), (0.6, 0.12, 0.35), M["iron"], bevel=0.03)


@asset("Enemies", "SM_Boss_HourlessKing")
def boss_king(b):
    p = humanoid(b, M["skin_pale"], M["black"], M["black"], M["black"], height=3.2, bulk=1.3, arm_pose=(25, 35))
    b.cone((0, 0, 1.0), 0.9, 2.0, M["black"], tip=0.25)                                       # robe
    b.cone((0, 0, 0.05), 0.92, 0.1, M["brass"], tip=0.9)
    b.sph((0, -0.25, 2.3), 0.22, M["relic"])                                                  # heart of time
    # Clock-face halo with hour marks
    head = p["head"]
    b.tor((0, 0.25, head.z + 0.1), 0.7, 0.025, M["brass"], rot=(90, 0, 0), segs=48)
    for i in range(12):
        a = 2 * math.pi * i / 12
        size = 0.16 if i % 3 == 0 else 0.09
        b.box((math.cos(a) * 0.7, 0.25, head.z + 0.1 + math.sin(a) * 0.7), (0.03, 0.03, size), M["gold_glow"], rot=(0, -math.degrees(a) + 90, 0))
    # Clock-hand blade
    hr = p["hand_r"]
    b.box((hr.x, hr.y - 0.02, hr.z - 1.0), (0.05, 0.22, 2.0), M["brass"])
    b.cone((hr.x, hr.y - 0.02, hr.z - 2.15), 0.2, 0.35, M["brass"], rot=(180, 0, 0), radius_y=0.04)
    # Floating gears
    for (x, z, r) in ((-1.1, 2.6, 0.25), (1.2, 2.0, 0.18), (-0.9, 1.4, 0.15)):
        b.tor((x, 0.1, z), r, 0.04, M["brass"], rot=(90, 0, 0), segs=24)

# ================================================================ PROPS (names match AArpgFloorGenerator::EProp)


@asset("Props", "SM_Prop_Tree")
def prop_tree(b):
    b.cyl((0, 0, 2.5), 0.28, 5.0, M["bark"], verts=10)
    b.sph((0, 0, 5.6), (3.8, 3.8, 3.0), M["foliage"], segs=12)
    b.sph((0.9, 0.6, 6.8), (2.6, 2.6, 2.2), M["foliage"], segs=12)
    for a in (0, 120, 240):
        r = math.radians(a)
        b.limb((0, 0, 0.3), (math.cos(r) * 0.8, math.sin(r) * 0.8, 0), 0.12, M["bark"], verts=8)


@asset("Props", "SM_Prop_Pine")
def prop_pine(b):
    b.cyl((0, 0, 1.2), 0.2, 2.4, M["bark"], verts=10)
    for i, (z, r, h) in enumerate(((3.0, 1.8, 2.6), (4.6, 1.3, 2.2), (6.0, 0.8, 1.8))):
        b.cone((0, 0, z), r, h, M["foliage"], verts=12)
        b.cone((0, 0, z + h * 0.18), r * 0.8, h * 0.6, M["snow"], verts=12)


@asset("Props", "SM_Prop_JungleTree")
def prop_jungle_tree(b):
    b.cyl((0, 0, 4.5), 0.32, 9.0, M["bark"], verts=10)
    b.sph((0, 0, 9.2), (6.0, 6.0, 1.6), M["jungle"], segs=14)
    for i in range(7):
        a = 2 * math.pi * i / 7
        b.cyl((math.cos(a) * 2.2, math.sin(a) * 2.2, 6.6), 0.05, 4.0 - (i % 3) * 0.6, M["jungle"], verts=6)
    for a in (0, 90, 180, 270):
        r = math.radians(a)
        b.limb((math.cos(r) * 0.25, math.sin(r) * 0.25, 1.2), (math.cos(r) * 1.1, math.sin(r) * 1.1, 0), 0.12, M["bark"], verts=8)


@asset("Props", "SM_Prop_Fern")
def prop_fern(b):
    for i in range(7):
        a = 2 * math.pi * i / 7
        b.cone((math.cos(a) * 0.3, math.sin(a) * 0.3, 0.45), 0.25, 0.9, M["jungle"], rot=(math.cos(a) * 30, math.sin(a) * -30, 0), verts=6, radius_y=0.06)


@asset("Props", "SM_Prop_Rock")
def prop_rock(b):
    b.sph((0, 0, 0.4), (2.6, 2.0, 1.6), M["stone"], rot=(8, 4, 20), segs=10)
    b.sph((1.1, 0.6, 0.3), (1.4, 1.2, 1.0), M["stone_dark"], rot=(0, 10, 40), segs=8)
    b.sph((-0.4, -0.2, 0.95), (1.2, 0.9, 0.5), M["moss"], segs=8)


@asset("Props", "SM_Prop_BrokenPillar")
def prop_pillar(b):
    b.box((0, 0, 0.15), (1.6, 1.6, 0.3), M["stone"], bevel=0.04)
    b.cyl((0, 0, 1.3), 0.5, 2.0, M["stone"], verts=12)
    b.cyl((0, 0, 2.45), 0.5, 0.3, M["stone"], rot=(6, 4, 0), verts=12)
    b.cyl((1.3, 0.4, 0.45), 0.45, 1.2, M["stone"], rot=(0, 90, 15), verts=12)
    b.sph((0.0, 0.0, 2.6), (0.9, 0.9, 0.25), M["moss"], segs=8)


@asset("Props", "SM_Prop_RuinArch")
def prop_arch(b):
    b.box((0, -2.2, 2.5), (0.9, 0.9, 5.0), M["stone"], bevel=0.04)
    b.box((0, 2.2, 2.0), (0.9, 0.9, 4.0), M["stone"], bevel=0.04)
    b.box((0, -0.6, 5.2), (1.0, 3.6, 0.7), M["stone"], rot=(-8, 0, 0), bevel=0.04)
    b.box((0, 2.2, 4.3), (1.0, 1.0, 0.4), M["stone_dark"], rot=(0, 0, 12))


@asset("Props", "SM_Prop_Crystal")
def prop_crystal(b):
    for (x, y, h, rx, ry) in ((0, 0, 3.0, 0, 0), (0.5, 0.2, 2.0, 15, -10), (-0.4, 0.3, 1.6, -18, 12), (0.1, -0.5, 1.2, 10, 20)):
        b.cone((x, y, h / 2), 0.35, h, M["relic"], rot=(rx, ry, 0), verts=6)
    b.sph((0, 0, 0.1), (1.6, 1.4, 0.4), M["stone_dark"], segs=8)


@asset("Props", "SM_Prop_StoneHead")
def prop_stone_head(b):
    b.box((0, 0, 1.6), (2.6, 2.2, 3.2), M["stone"], bevel=0.12)
    b.box((0, -1.12, 1.9), (2.0, 0.1, 0.2), M["stone_dark"])                               # brow
    for s in (-1, 1):
        b.box((s * 0.5, -1.12, 2.1), (0.4, 0.06, 0.3), M["relic"])                         # glowing eyes
    b.box((0, -1.2, 1.4), (0.5, 0.4, 0.8), M["stone"], bevel=0.05)                         # nose
    b.box((0, -1.12, 0.8), (1.2, 0.08, 0.15), M["stone_dark"])                             # mouth
    b.sph((0, 0, 3.3), (2.8, 2.4, 0.6), M["jungle"], segs=10)


@asset("Props", "SM_Prop_Car")
def prop_car(b):
    b.box((0, 0, 0.7), (4.2, 1.8, 0.9), M["rust"], bevel=0.08)
    b.box((-0.2, 0, 1.45), (2.2, 1.65, 0.7), M["rust"], bevel=0.08)
    b.box((0.9, 0, 1.45), (0.05, 1.5, 0.55), M["glass"], rot=(0, -25, 0))
    for x in (1.4, -1.4):
        for y in (0.9, -0.9):
            b.cyl((x, y, 0.35), 0.35, 0.25, M["black"], rot=(90, 0, 0), verts=12)
    b.box((2.12, 0, 0.75), (0.05, 1.6, 0.25), M["iron"])


@asset("Props", "SM_Prop_Barrier")
def prop_barrier(b):
    b.box((0, 0, 0.45), (3.8, 0.7, 0.9), M["concrete"], bevel=0.05)
    b.box((0, 0, 1.0), (3.8, 0.35, 0.2), M["concrete"])
    b.box((0, -0.36, 0.65), (3.6, 0.02, 0.12), M["gold"])


@asset("Props", "SM_Prop_Rubble")
def prop_rubble(b):
    pieces = ((0, 0, 0.3, 1.6, 1.2, 0.6, 10), (1.2, 0.6, 0.25, 1.0, 0.8, 0.5, 35), (-1.0, -0.5, 0.2, 0.9, 0.9, 0.4, -20),
              (0.3, -1.1, 0.35, 1.2, 0.6, 0.7, 60), (-0.4, 0.9, 0.5, 0.6, 0.6, 1.0, 15))
    for (x, y, z, sx, sy, sz, r) in pieces:
        b.box((x, y, z), (sx, sy, sz), M["concrete"], rot=(r * 0.3, r * -0.2, r))
    b.limb((0.2, 0.1, 0.5), (1.5, -0.4, 1.4), 0.02, M["rust"], verts=6)                   # rebar
    b.limb((-0.3, 0.3, 0.6), (-1.2, 1.2, 1.5), 0.02, M["rust"], verts=6)


@asset("Props", "SM_Prop_StreetLamp")
def prop_lamp(b):
    b.cyl((0, 0, 2.6), 0.09, 5.2, M["iron"], verts=8)
    b.box((0.5, 0, 5.2), (1.2, 0.25, 0.15), M["iron"])
    b.box((1.0, 0, 5.05), (0.4, 0.35, 0.12), M["lamp"])
    b.cyl((0, 0, 0.15), 0.25, 0.3, M["iron"], verts=8)


@asset("Props", "SM_Prop_NeonSign")
def prop_neon(b):
    b.box((0, 0, 2.0), (0.35, 0.35, 4.0), M["iron"])
    b.box((0, -0.05, 4.2), (0.25, 2.6, 1.4), M["dark_steel"])
    b.box((0, -0.19, 4.2), (0.04, 2.3, 1.1), M["neon"])
    b.box((0, -0.22, 4.2), (0.04, 2.0, 0.08), M["cyan_glow"])


@asset("Props", "SM_Prop_Barrel")
def prop_barrel(b):
    b.cyl((0, 0, 0.55), 0.35, 1.1, M["rust"], verts=14)
    b.cyl((0.85, 0.2, 0.55), 0.35, 1.1, M["dark_steel"], verts=14)
    for z in (0.2, 0.9):
        b.tor((0, 0, z), 0.355, 0.02, M["iron"], segs=20)


@asset("Props", "SM_Prop_Crate")
def prop_crate(b):
    b.box((0, 0, 0.6), (1.2, 1.2, 1.2), M["wood"], bevel=0.03)
    b.box((0.2, 0.1, 1.7), (1.0, 1.0, 1.0), M["wood"], rot=(0, 0, 20), bevel=0.03)
    for x in (-0.55, 0.55):
        b.box((x, -0.61, 0.6), (0.1, 0.02, 1.2), M["bark"])


@asset("Props", "SM_Prop_PierPosts")
def prop_pier(b):
    for x in (0, 2.2):
        for y in (0, 2.2):
            b.cyl((x, y, 1.0), 0.2, 3.0, M["bark"], verts=8)
    b.box((1.1, 1.1, 2.0), (3.0, 3.0, 0.2), M["wood"])
    for i in range(6):
        b.box((1.1, -0.2 + i * 0.5, 2.12), (3.0, 0.4, 0.04), M["wood"])


@asset("Props", "SM_Prop_Boat")
def prop_boat(b):
    b.box((0, 0, 0.4), (4.2, 1.5, 0.7), M["wood"], rot=(0, 0, 0), bevel=0.15)
    b.cone((2.5, 0, 0.4), 0.75, 0.9, M["wood"], rot=(0, 90, 0), radius_y=0.7, verts=8)
    for x in (-1.0, 0.0, 1.0):
        b.box((x, 0, 0.7), (0.2, 1.4, 0.06), M["bark"])
    b.limb((-0.5, 0.2, 0.7), (1.5, 1.4, 1.0), 0.04, M["bark"], verts=6)                   # oar


@asset("Props", "SM_Prop_IceSpike")
def prop_ice_spike(b):
    for (x, y, h, rx, ry) in ((0, 0, 6.0, 0, 0), (0.7, 0.3, 3.6, 12, -8), (-0.6, 0.4, 2.8, -10, 10)):
        b.cone((x, y, h / 2), 0.6, h, M["ice"], rot=(rx, ry, 0), verts=6)
    b.sph((0, 0, 0.1), (2.0, 1.8, 0.4), M["snow"], segs=10)


@asset("Props", "SM_Prop_SnowRock")
def prop_snow_rock(b):
    b.sph((0, 0, 0.5), (2.8, 2.2, 1.8), M["stone_dark"], segs=10)
    b.sph((0, 0, 1.1), (2.5, 2.0, 0.9), M["snow"], segs=10)


@asset("Props", "SM_Prop_FrozenPillar")
def prop_frozen_pillar(b):
    b.cyl((0, 0, 2.2), 0.55, 4.4, M["stone"], verts=12)
    b.cyl((0, 0, 2.0), 0.75, 3.0, M["ice"], verts=8)
    b.cone((0, 0, 4.6), 0.5, 0.6, M["snow"], verts=12)


@asset("Props", "SM_Prop_House")
def prop_house(b):
    b.box((0, 0, 2.2), (7.0, 6.0, 4.4), mat("M_Plaster", hexcol("8C8070"), rough=0.9), bevel=0.05)
    b.cone((0, 0, 6.0), 5.0, 3.2, mat("M_Roof", hexcol("52301F"), rough=0.85), rot=(0, 0, 45), verts=4)
    b.box((3.52, 0, 1.1), (0.1, 1.4, 2.2), M["wood"])
    for y in (-1.8, 1.8):
        b.box((3.52, y, 2.8), (0.1, 0.9, 0.9), M["lamp"])
    b.box((-1.5, 2.0, 6.2), (0.6, 0.6, 2.0), M["stone"])                                    # chimney
    for x in (-3.4, 3.4):
        b.box((x, 0, 2.2), (0.2, 6.0, 0.2), M["bark"])                                       # timber beams


@asset("Props", "SM_Prop_Stall")
def prop_stall(b):
    b.box((0, 0, 0.5), (3.0, 1.6, 1.0), M["wood"])
    b.box((0, 0.2, 2.6), (3.4, 2.0, 0.12), M["red"], rot=(8, 0, 0))
    for x in (-1.5, 1.5):
        b.cyl((x, 0.9, 1.3), 0.06, 2.6, M["bark"], verts=6)
    for x in (-0.9, 0.0, 0.9):
        b.sph((x, -0.2, 1.1), 0.25, M["gold"] if x == 0 else M["foliage"], segs=8)


@asset("Props", "SM_Prop_Well")
def prop_well(b):
    b.cyl((0, 0, 0.5), 1.1, 1.0, M["stone"], verts=16)
    b.cyl((0, 0, 1.0), 0.85, 0.05, mat("M_Water", hexcol("1A3A50"), rough=0.05), verts=16)
    for x in (-1.0, 1.0):
        b.cyl((x, 0, 1.6), 0.08, 2.2, M["wood"], verts=6)
    b.box((0, 0, 2.75), (2.4, 1.6, 0.1), M["wood"], rot=(0, 0, 0))
    b.cyl((0, 0, 2.4), 0.07, 2.0, M["bark"], rot=(0, 90, 0), verts=8)


@asset("Props", "SM_Prop_Platform")
def prop_platform(b):
    b.box((0, 0, 0.45), (5.0, 4.0, 0.9), M["stone"], bevel=0.05)
    b.box((0, 0, 0.93), (5.2, 4.2, 0.06), M["stone_dark"])
    b.box((0, -2.4, 0.22), (2.0, 0.8, 0.44), M["stone"])                                     # step

# ================================================================ THE VEIL SPIRE


@asset("Tower", "SM_VeilSpire")
def veil_spire(b):
    z = 0.0
    r = 14.0
    for tier in range(9):
        h = 16.0
        b.cyl((0, 0, z + h / 2), r, h, M["stone_dark"], verts=12)
        for k in range(6):
            a = 2 * math.pi * k / 6 + tier * 0.3
            b.box((math.cos(a) * r, math.sin(a) * r, z + h / 2), (2.6, 2.6, h * 0.92), M["black"], rot=(0, 0, math.degrees(a)))
            b.cone((math.cos(a) * r, math.sin(a) * r, z + h + 1.5), 1.2, 3.0, M["black"], verts=6)
        if tier % 2 == 1 or tier == 8:
            b.tor((0, 0, z + h * 0.6), r * 1.45, 0.35, M["relic"], rot=(math.sin(tier * 1.7) * 6, math.cos(tier * 1.3) * 6, 0), segs=48)
        z += h
        r *= 0.84
    b.cone((0, 0, z + 12), r, 24, M["black"], verts=12)
    b.cyl((0, 0, z / 2), 1.2, z, M["relic"], verts=12)                                       # the light seam
    for i, (a, d, hz, s) in enumerate(((0.3, 30, 20, 4), (1.9, 38, 45, 3), (3.1, 26, 70, 5), (4.4, 42, 30, 2.5), (5.5, 34, 95, 3.5))):
        b.sph((math.cos(a) * d, math.sin(a) * d, hz), (s * 1.4, s, s * 0.8), M["stone_dark"], segs=8)
        b.cone((math.cos(a) * d, math.sin(a) * d, hz - s * 0.9), s * 0.6, s * 1.4, M["stone_dark"], rot=(180, 0, 0), verts=6)

# ================================================================ run


def main():
    groups = {}
    for build in BUILDERS:
        if ONLY and build.category != ONLY:
            continue
        col = groups.get(build.category) or collection(build.category)
        groups[build.category] = col
        obj = build(col)
        path = os.path.join(OUT, build.category, f"{build.asset_name}.fbx")
        export_fbx(obj, path)
        print(f"Exported {path}")

    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "Ironveil_Blockouts.blend"))

    if RENDER:
        prev = os.path.join(OUT, "Previews")
        os.makedirs(prev, exist_ok=True)
        everything = [o for items in ASSETS.values() for _, o in items]
        sheets = []
        for cat, items in ASSETS.items():
            objs = [o for _, o in items]
            if len(objs) > 9:
                sheets += [(objs[i:i + 9], f"{cat}_{i // 9 + 1}.png") for i in range(0, len(objs), 9)]
            else:
                sheets.append((objs, f"{cat}.png"))
        for objs, filename in sheets:
            for o in everything:
                o.hide_render = o not in objs
            render_sheet(objs, os.path.join(prev, filename))
        for items in ASSETS.values():
            for _, o in items:
                o.hide_render = False


main()
