"""
Shared helpers for Ironveil's Blender blockout scripts (Blender 5.x).

Blockouts are built from simple shapes so every proportion is a number you can edit. They are static meshes:
the game's C++ loads them by name (SM_Hero_*, SM_Enemy_*, SM_Boss_*, SM_Prop_*, SM_VeilSpire) and falls back to
gray-box shapes when a mesh hasn't been imported yet. Conventions:
  - 1 Blender unit = 1 metre; characters stand with their feet at the origin and face -Y (Blender's front view).
  - Each asset is joined into one object with a few colour materials, applied transforms and auto-smooth off.
"""
import math
import os

import bpy
from mathutils import Vector


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0
    return scene


_MATS = {}


def mat(name, color, metal=0.0, rough=0.6, emit=0.0):
    """A Principled material; Base Color carries into Unreal on FBX import."""
    key = name
    if key in _MATS:
        return _MATS[key]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if emit > 0:
        b.inputs["Emission Color"].default_value = (*color, 1)
        b.inputs["Emission Strength"].default_value = emit
    m.diffuse_color = (*color, 1)
    _MATS[key] = m
    return m


def hexcol(h):
    """sRGB hex -> linear RGB tuple."""
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)


class Builder:
    """Collects parts for one asset, then joins them into a single mesh object."""

    def __init__(self, name):
        self.name = name
        self.parts = []

    def _finish(self, obj, loc, rot, scale, material):
        obj.location = loc
        obj.rotation_euler = tuple(math.radians(r) for r in rot)
        obj.scale = scale
        if material is not None:
            obj.data.materials.append(material)
        self.parts.append(obj)
        return obj

    def box(self, loc, size, material, rot=(0, 0, 0), bevel=0.0):
        bpy.ops.mesh.primitive_cube_add(size=1)
        o = self._finish(bpy.context.active_object, loc, rot, size, material)
        if bevel > 0:
            mod = o.modifiers.new("Bevel", 'BEVEL')
            mod.width = bevel
            mod.segments = 2
        return o

    def cyl(self, loc, radius, depth, material, rot=(0, 0, 0), verts=16, radius_y=None):
        bpy.ops.mesh.primitive_cylinder_add(radius=1, depth=1, vertices=verts)
        return self._finish(bpy.context.active_object, loc, rot, (radius, radius_y or radius, depth), material)

    def cone(self, loc, radius, depth, material, rot=(0, 0, 0), tip=0.0, verts=16, radius_y=None):
        bpy.ops.mesh.primitive_cone_add(radius1=1, radius2=tip, depth=1, vertices=verts)
        return self._finish(bpy.context.active_object, loc, rot, (radius, radius_y or radius, depth), material)

    def sph(self, loc, size, material, rot=(0, 0, 0), segs=16):
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, segments=segs, ring_count=max(6, segs // 2))
        if isinstance(size, (int, float)):
            size = (size, size, size)
        return self._finish(bpy.context.active_object, loc, rot, size, material)

    def tor(self, loc, major, minor, material, rot=(0, 0, 0), segs=32):
        bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, major_segments=segs, minor_segments=8)
        return self._finish(bpy.context.active_object, loc, rot, (1, 1, 1), material)

    def limb(self, a, b, radius, material, verts=12):
        """A cylinder from point a to point b (arms, legs, bow limbs)."""
        a, b = Vector(a), Vector(b)
        d = b - a
        bpy.ops.mesh.primitive_cylinder_add(radius=1, depth=1, vertices=verts)
        o = bpy.context.active_object
        o.location = (a + b) / 2
        o.rotation_mode = 'QUATERNION'
        o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
        o.scale = (radius, radius, d.length)
        o.data.materials.append(material)
        self.parts.append(o)
        return o

    def join(self, collection):
        for o in self.parts:
            for mod in list(o.modifiers):
                bpy.context.view_layer.objects.active = o
                bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.ops.object.select_all(action='DESELECT')
        for o in self.parts:
            o.select_set(True)
        bpy.context.view_layer.objects.active = self.parts[0]
        bpy.ops.object.join()
        obj = bpy.context.active_object
        obj.name = self.name
        obj.data.name = self.name
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        # Origin at the base centre so it stands on the floor in Unreal.
        bpy.context.scene.cursor.location = (0, 0, 0)
        bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
        for c in obj.users_collection:
            c.objects.unlink(obj)
        collection.objects.link(obj)
        return obj


def humanoid(b, m_skin, m_torso, m_legs, m_arms, height=1.8, bulk=1.0, arm_pose=(15, 15), leg_spread=0.11):
    """A simple standing body in A-pose; returns key points (hand positions etc.)."""
    h = height / 1.8
    w = bulk
    hip_z = 0.95 * h
    # legs
    for side in (-1, 1):
        b.limb((side * leg_spread * w, 0, 0.08 * h), (side * 0.1 * w, 0, hip_z), 0.075 * w, m_legs)
        b.box((side * leg_spread * w, -0.05, 0.04 * h), (0.12 * w, 0.26, 0.08 * h), m_legs, bevel=0.01)
    # pelvis, torso, neck, head
    b.box((0, 0, hip_z + 0.05 * h), (0.36 * w, 0.22 * w, 0.2 * h), m_legs, bevel=0.02)
    b.box((0, 0, 1.25 * h), (0.42 * w, 0.24 * w, 0.46 * h), m_torso, bevel=0.03)
    b.cyl((0, 0, 1.53 * h), 0.06 * w, 0.12 * h, m_skin)
    b.sph((0, -0.01, 1.66 * h), (0.21, 0.23, 0.27), m_skin)
    hands = []
    for side, ang in zip((-1, 1), arm_pose):
        sh = Vector((side * 0.25 * w, 0, 1.44 * h))
        r = math.radians(ang)
        hand = sh + Vector((side * math.sin(r) * 0.62 * h, 0, -math.cos(r) * 0.62 * h))
        b.sph(tuple(sh), 0.15 * w, m_torso)
        b.limb(tuple(sh), tuple(hand), 0.055 * w, m_arms)
        b.sph(tuple(hand), 0.09 * w, m_skin)
        hands.append(hand)
    return {"hand_l": hands[0], "hand_r": hands[1], "head": Vector((0, -0.01, 1.66 * h)), "chest": Vector((0, 0, 1.3 * h))}


def export_fbx(obj, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS',
                             object_types={'MESH'}, mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False)


def render_sheet(objects, path, title_spacing=None, res=(1600, 700), samples=24):
    """Lays the objects out in a row and renders a preview with Cycles (works headless)."""
    scene = bpy.context.scene
    xs = 0.0
    placed = []
    for o in objects:
        dims = o.dimensions
        width = max(dims.x, dims.y)
        o.location.x = xs + width / 2
        xs += width + (title_spacing or max(0.6, width * 0.25))
        placed.append(o)
    total = xs
    height = max(o.dimensions.z for o in objects)

    world = bpy.data.worlds.new("PreviewWorld")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.05, 0.06, 0.08, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6
    scene.world = world

    bpy.ops.mesh.primitive_plane_add(size=1, location=(total / 2, 0, 0))
    floor = bpy.context.active_object
    floor.scale = (total * 2 + 10, 40, 1)
    floor.data.materials.append(mat("PreviewFloor", (0.18, 0.18, 0.2), rough=0.9))

    sun = bpy.data.lights.new("Sun", 'SUN')
    sun.energy = 3.5
    sun_obj = bpy.data.objects.new("Sun", sun)
    sun_obj.rotation_euler = (math.radians(50), 0, math.radians(30))
    scene.collection.objects.link(sun_obj)

    cam = bpy.data.cameras.new("Cam")
    cam.type = 'ORTHO'
    cam.ortho_scale = max(total * 1.05, height * res[0] / res[1] * 1.15)
    cam_obj = bpy.data.objects.new("Cam", cam)
    cam_obj.location = (total / 2, -60, height * 0.5 + 18)
    cam_obj.rotation_euler = (math.radians(73), 0, 0)
    scene.collection.objects.link(cam_obj)
    scene.camera = cam_obj

    scene.render.engine = 'CYCLES'
    scene.cycles.samples = samples
    scene.cycles.device = 'CPU'
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.filepath = path
    scene.view_settings.view_transform = 'AgX' if 'AgX' in [v.identifier for v in scene.view_settings.bl_rna.properties['view_transform'].enum_items] else 'Standard'
    bpy.ops.render.render(write_still=True)

    # Put things back where they were for the .blend.
    for o in placed:
        o.location.x = 0
    for o in (floor, sun_obj, cam_obj):
        bpy.data.objects.remove(o, do_unlink=True)
