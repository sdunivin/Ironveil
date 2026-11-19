"""
One-click content setup for the Ironveil prototype. Run it inside the Unreal Editor:
    Tools > Execute Python Script... > Tools/unreal/setup_ironveil_content.py
(needs the Python Editor Script Plugin, which IronVeil.uproject enables.)

It is safe to run again after re-exporting from Blender; it overwrites what it made before.
  1. Creates /Game/_Game/Materials/M_IronveilBase: a Color + Emissive material, so crystals, portals, runes and
     loot beams glow. The C++ uses it automatically (and falls back to the engine's BasicShapeMaterial without it).
  2. Imports every FBX under Art/Export/{Characters,Enemies,Props,Tower} into /Game/_Game/Art/<same folder>.
     The game swaps these in for the gray-box shapes by name (SM_Hero_*, SM_Enemy_*, SM_Boss_*, SM_Prop_*, SM_VeilSpire).
     The Warrior export from build_warrior_blockout.py (SK_Warrior_Male_Blockout.fbx) is imported as SM_Hero_Warrior.
  3. Creates the map /Game/_Game/Maps/L_Spire with ArpgGameMode and makes it the editor and game start map.
"""
import os
import re

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
EXPORT_DIR = os.path.join(PROJECT_DIR, "Art", "Export")
GAME_ROOT = "/Game/_Game"
MAP_PATH = GAME_ROOT + "/Maps/L_Spire"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[Ironveil setup] " + msg)


# ---------------------------------------------------------------- 1. base material
def make_base_material():
    path = GAME_ROOT + "/Materials/M_IronveilBase"
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset("M_IronveilBase", GAME_ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())

    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))

    emissive = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 250)
    emissive.set_editor_property("parameter_name", "Emissive")
    emissive.set_editor_property("default_value", 0.0)

    glow = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 200)
    mel.connect_material_expressions(color, "", glow, "A")
    mel.connect_material_expressions(emissive, "", glow, "B")

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 100)
    rough.set_editor_property("r", 0.65)

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("Material ready: " + path)


# ---------------------------------------------------------------- 2. meshes
def import_fbx(fbx_path, dest_folder, dest_name, collision=True):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx_path)
    task.set_editor_property("destination_path", dest_folder)
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    smd = options.get_editor_property("static_mesh_import_data")
    smd.set_editor_property("combine_meshes", True)
    smd.set_editor_property("auto_generate_collision", collision)
    task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])
    log("Imported {} -> {}/{}".format(os.path.basename(fbx_path), dest_folder, dest_name))


def import_meshes():
    if not os.path.isdir(EXPORT_DIR):
        log("No Art/Export folder yet. Run Tools/blender/build_ironveil_assets.py in Blender first.")
        return
    count = 0
    for category in ("Characters", "Enemies", "Props", "Tower"):
        folder = os.path.join(EXPORT_DIR, category)
        if not os.path.isdir(folder):
            continue
        for root, _dirs, files in os.walk(folder):
            for f in sorted(files):
                if not f.lower().endswith(".fbx"):
                    continue
                name = os.path.splitext(f)[0]
                if name.startswith("SK_Warrior"):
                    name = "SM_Hero_Warrior"
                elif not name.startswith("SM_"):
                    continue
                # Characters don't need collision (the capsule handles it); props and the tower do.
                import_fbx(os.path.join(root, f), "{}/Art/{}".format(GAME_ROOT, category), name, collision=category in ("Props", "Tower"))
                count += 1
    log("{} meshes imported.".format(count))


# ---------------------------------------------------------------- 3. map
def make_map():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if eal.does_asset_exist(MAP_PATH):
        les.load_level(MAP_PATH)
    else:
        les.new_level(MAP_PATH)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    game_mode = unreal.load_class(None, "/Script/IronVeil.ArpgGameMode")
    if game_mode:
        world.get_world_settings().set_editor_property("default_game_mode", game_mode)
    else:
        log("ArpgGameMode not found: build the C++ project (IronVeil) first, then run this script again.")
    les.save_current_level()

    # Point the project's start maps at L_Spire.
    ini = os.path.join(PROJECT_DIR, "Config", "DefaultEngine.ini")
    if os.path.exists(ini):
        with open(ini, "r", encoding="utf-8") as fh:
            text = fh.read()
        target = MAP_PATH + "." + MAP_PATH.rsplit("/", 1)[1]
        for key in ("GameDefaultMap", "EditorStartupMap"):
            if re.search(r"^{}=".format(key), text, flags=re.M):
                text = re.sub(r"^{}=.*$".format(key), "{}={}".format(key, target), text, flags=re.M)
            else:
                text = text.replace("[/Script/EngineSettings.GameMapsSettings]", "[/Script/EngineSettings.GameMapsSettings]\n{}={}".format(key, target), 1)
        with open(ini, "w", encoding="utf-8") as fh:
            fh.write(text)
    log("Map ready: " + MAP_PATH + " (now the start map). Press Play.")


make_base_material()
import_meshes()
make_map()
