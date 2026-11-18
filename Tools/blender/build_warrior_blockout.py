"""
Builds the Ironveil male Warrior blockout from scratch in Blender 5.2.

Every piece is made from simple shapes so proportions can be changed by editing
numbers here (see the P dict of body landmarks) and rebuilding.

Run from a terminal (this replaces the open scene, so don't run it inside a file you care about):
    blender --background --python Tools/blender/build_warrior_blockout.py -- OUTPUT_FOLDER [front,three_quarter,side,back]
Outputs: Warrior_Male_Blockout.blend, FBX exports for Unreal, and preview renders.
"""
import math, os, sys
import bpy, bmesh
from mathutils import Vector, Matrix

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
OUT = ARGS[0] if ARGS else os.path.join(os.getcwd(), "warrior_out")
os.makedirs(OUT, exist_ok=True)
sys.argv = [sys.argv[0]] + ARGS

# ---------------------------------------------------------------- scene setup
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0

def collection(name):
    c = bpy.data.collections.new(name)
    scene.collection.children.link(c)
    return c

COL_CHAR = collection("Warrior_Character")
COL_WEAP = collection("Warrior_Weapons")
COL_PREV = collection("Preview_Only")

# ---------------------------------------------------------------- materials
def material(name, color, metallic=0.0, rough=0.5):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*color, 1)
    return m

M = {
    "Skin":      material("M_Warrior_Skin", (0.62, 0.42, 0.32), 0.0, 0.55),
    "Steel":     material("M_Warrior_Steel", (0.56, 0.57, 0.58), 1.0, 0.32),
    "DarkSteel": material("M_Warrior_DarkSteel", (0.24, 0.24, 0.25), 1.0, 0.45),
    "Mail":      material("M_Warrior_Mail", (0.33, 0.33, 0.34), 1.0, 0.6),
    "Leather":   material("M_Warrior_Leather", (0.16, 0.09, 0.05), 0.0, 0.65),
    "Cloth":     material("M_Warrior_Tabard", (0.30, 0.04, 0.035), 0.0, 0.85),
    "Brass":     material("M_Warrior_Brass", (0.72, 0.52, 0.22), 1.0, 0.35),
    "Wood":      material("M_Shield_Paint", (0.26, 0.05, 0.04), 0.0, 0.6),
    "Grip":      material("M_Sword_Grip", (0.10, 0.06, 0.04), 0.0, 0.7),
}

# ---------------------------------------------------------------- helpers
def T(x, y, z): return Matrix.Translation((x, y, z))
def S(x, y, z): return Matrix.Diagonal((x, y, z, 1))
def R(deg, axis): return Matrix.Rotation(math.radians(deg), 4, axis)
def align(a, b):
    """Matrix placing a Z-up primitive's centre between a and b, Z pointing a->b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    rot = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_matrix().to_4x4()
    return Matrix.Translation((a + b) / 2) @ rot, d.length
def lerp(a, b, t): return Vector(a).lerp(Vector(b), t)
def mirror(p): return (-p[0], p[1], p[2])

class Part:
    """Collects several primitives (each with a material) into one object."""
    def __init__(self, name, mats):
        self.name, self.bm, self.mats = name, bmesh.new(), [M[m] for m in mats]
    def _tag(self, verts, mi):
        faces = {f for v in verts for f in v.link_faces}
        for f in faces: f.material_index = mi
        return verts
    def sphere(self, matrix, mi=0, seg=24, rings=12, cut_below=None):
        r = bmesh.ops.create_uvsphere(self.bm, u_segments=seg, v_segments=rings, radius=1.0, matrix=matrix)
        verts = r["verts"]
        if cut_below is not None:
            inv = matrix.inverted()
            kill = [v for v in verts if (inv @ v.co).z < cut_below]
            bmesh.ops.delete(self.bm, geom=kill, context='VERTS')
            verts = [v for v in verts if v.is_valid]
        return self._tag(verts, mi)
    def cone(self, matrix, r1, r2, depth, mi=0, seg=24, caps=False):
        r = bmesh.ops.create_cone(self.bm, cap_ends=caps, cap_tris=False, segments=seg,
                                  radius1=r1, radius2=r2, depth=depth, matrix=matrix)
        return self._tag(r["verts"], mi)
    def lathe(self, rings, mi=0, seg=28):
        """rings: list of (z, rx, ry) from bottom to top -> open tube with that profile."""
        loops = []
        for z, rx, ry in rings:
            loops.append([self.bm.verts.new((rx * math.cos(2 * math.pi * i / seg),
                                             ry * math.sin(2 * math.pi * i / seg), z)) for i in range(seg)])
        for a, b in zip(loops, loops[1:]):
            for i in range(seg):
                j = (i + 1) % seg
                self.bm.faces.new((a[i], a[j], b[j], b[i]))
        verts = [v for loop in loops for v in loop]
        return self._tag(verts, mi)
    def cube(self, matrix, mi=0):
        r = bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)
        return self._tag(r["verts"], mi)
    def tube(self, a, b, r_a, r_b, mi=0, seg=20, t0=0.0, t1=1.0, caps=False):
        pa, pb = lerp(a, b, t0), lerp(a, b, t1)
        m, L = align(pa, pb)
        return self.cone(m, r_a, r_b, L, mi, seg, caps)
    def build(self, col, smooth=True, solidify=0.0, bevel=0.0, subsurf=0):
        me = bpy.data.meshes.new(self.name)
        self.bm.normal_update()
        self.bm.to_mesh(me); self.bm.free()
        for m in self.mats: me.materials.append(m)
        ob = bpy.data.objects.new(self.name, me)
        col.objects.link(ob)
        if smooth:
            for p in me.polygons: p.use_smooth = True
        if solidify:
            s = ob.modifiers.new("Solidify", 'SOLIDIFY'); s.thickness = solidify; s.offset = 1.0
        if bevel:
            bv = ob.modifiers.new("Bevel", 'BEVEL'); bv.width = bevel; bv.segments = 2
            bv.limit_method = 'ANGLE'
        if subsurf:
            ss = ob.modifiers.new("Subdivision", 'SUBSURF'); ss.levels = subsurf; ss.render_levels = subsurf
        return ob

# ---------------------------------------------------------------- skeleton landmarks (metres, Z up, facing -Y)
# Character's right side is -X. A-pose arms for easy rigging later.
P = dict(
    pelvis=(0, 0, 0.98), spine=(0, 0, 1.12), chest=(0, 0, 1.30), upchest=(0, 0, 1.42),
    neck=(0, 0.01, 1.52), necktop=(0, 0.01, 1.58),
    clav_l=(0.12, 0, 1.44), sho_l=(0.215, 0, 1.445), elb_l=(0.355, 0.03, 1.21),
    wri_l=(0.47, 0.01, 1.00), hand_l=(0.53, -0.01, 0.90),
    hip_l=(0.10, 0, 0.93), knee_l=(0.115, -0.01, 0.52), ank_l=(0.12, 0.02, 0.09), toe_l=(0.13, -0.12, 0.03),
)
for k in list(P):
    if k.endswith("_l"):
        P[k[:-2] + "_r"] = mirror(P[k])

# ---------------------------------------------------------------- body (Skin modifier mannequin, editable)
def build_body():
    names = list(P)
    radii = dict(pelvis=(0.16, 0.115), spine=(0.145, 0.11), chest=(0.18, 0.125), upchest=(0.17, 0.11),
                 neck=(0.06, 0.06), necktop=(0.055, 0.055), clav=(0.07, 0.06), sho=(0.065, 0.065),
                 elb=(0.045, 0.045), wri=(0.035, 0.028), hand=(0.04, 0.02), hip=(0.09, 0.09),
                 knee=(0.058, 0.058), ank=(0.042, 0.042), toe=(0.045, 0.03))
    edges = [("pelvis", "spine"), ("spine", "chest"), ("chest", "upchest"), ("upchest", "neck"), ("neck", "necktop")]
    for s in ("l", "r"):
        edges += [("upchest", f"clav_{s}"), (f"clav_{s}", f"sho_{s}"), (f"sho_{s}", f"elb_{s}"),
                  (f"elb_{s}", f"wri_{s}"), (f"wri_{s}", f"hand_{s}"),
                  ("pelvis", f"hip_{s}"), (f"hip_{s}", f"knee_{s}"), (f"knee_{s}", f"ank_{s}"), (f"ank_{s}", f"toe_{s}")]
    me = bpy.data.meshes.new("SK_Warrior_Body")
    me.from_pydata([P[n] for n in names], [(names.index(a), names.index(b)) for a, b in edges], [])
    ob = bpy.data.objects.new("SK_Warrior_Body", me)
    COL_CHAR.objects.link(ob)
    skin = ob.modifiers.new("Skin", 'SKIN')
    for i, n in enumerate(names):
        key = n if n in radii else n[:-2]
        me.skin_vertices[""].data[i].radius = radii[key]
        me.skin_vertices[""].data[i].use_root = (n == "pelvis")
    ss = ob.modifiers.new("Subdivision", 'SUBSURF'); ss.levels = 2; ss.render_levels = 2
    me.materials.append(M["Skin"])
    skin.use_smooth_shade = True
    # head
    h = Part("SK_Warrior_Head", ["Skin"])
    h.sphere(T(0, 0.005, 1.675) @ S(0.092, 0.108, 0.122))
    h.sphere(T(0, -0.035, 1.615) @ S(0.07, 0.07, 0.06))       # jaw
    h.sphere(T(0, -0.108, 1.66) @ S(0.016, 0.02, 0.03), seg=12, rings=8)  # nose
    h.build(COL_CHAR)
    return ob

# ---------------------------------------------------------------- armour pieces (origins at the feet)
def build_helmet():
    p = Part("SK_Warrior_Helmet", ["Steel", "Brass"])
    p.sphere(T(0, 0.005, 1.695) @ S(0.118, 0.13, 0.14), cut_below=-0.05)    # dome
    p.cone(T(0, 0.005, 1.648) @ S(1, 1.1, 1), 0.116, 0.118, 0.07)           # brow band
    p.cube(T(0, -0.137, 1.635) @ S(0.022, 0.012, 0.10), 0)                  # nasal guard
    p.cube(T(0, 0.005, 1.818) @ S(0.012, 0.22, 0.03), 1)                     # brass comb ridge
    for s in (1, -1):
        p.cube(T(0.10 * s, -0.06, 1.60) @ R(15 * s, 'Z') @ S(0.012, 0.07, 0.09), 0)  # cheek guards
    return p.build(COL_CHAR, solidify=0.006, bevel=0.002)

def build_gorget():
    p = Part("SK_Warrior_Gorget", ["Steel"])
    p.cone(T(0, 0.01, 1.505) @ S(1, 0.95, 1), 0.13, 0.075, 0.08)
    return p.build(COL_CHAR, solidify=0.005)

def build_cuirass():
    p = Part("SK_Warrior_Cuirass", ["Steel", "Brass"])
    profile = [(1.10, 0.172, 0.126), (1.16, 0.166, 0.122), (1.22, 0.176, 0.128), (1.29, 0.196, 0.138),
               (1.36, 0.21, 0.145), (1.42, 0.205, 0.138), (1.46, 0.186, 0.124), (1.485, 0.15, 0.104)]
    vs = p.lathe(profile, seg=32)
    for v in vs:                                    # breastplate ridge down the centre
        if v.co.y < 0 and 1.15 < v.co.z < 1.47:
            v.co.y -= 0.02 * max(0.0, 1 - abs(v.co.x) / 0.16)
    p.cone(T(0, 0, 1.105) @ S(1, 0.74, 1), 0.182, 0.176, 0.045, mi=0, seg=32)  # fauld lame
    p.sphere(T(0, -0.168, 1.37) @ S(0.028, 0.012, 0.028), mi=1, seg=12, rings=8)  # brass emblem
    return p.build(COL_CHAR, solidify=0.008, bevel=0.002)

def build_pauldrons():
    p = Part("SK_Warrior_Pauldrons", ["Steel"])
    for s, key in ((1, "sho_l"), (-1, "sho_r")):
        x, y, z = P[key]
        base = T(x + 0.025 * s, y, z + 0.03) @ R(-28 * s, 'Y')
        p.sphere(base @ S(0.11, 0.105, 0.09), cut_below=-0.15)
        p.cone(base @ T(0, 0, -0.035) @ S(1, 0.95, 1), 0.112, 0.105, 0.035)  # lame 1
        p.cone(base @ T(0, 0, -0.065) @ S(1, 0.95, 1), 0.108, 0.10, 0.03)    # lame 2
    return p.build(COL_CHAR, solidify=0.006, bevel=0.002)

def build_mail():
    p = Part("SK_Warrior_Hauberk", ["Mail"])
    p.cone(T(0, 0, 0.955) @ S(1, 0.8, 1), 0.225, 0.168, 0.36, seg=28)   # skirt
    for s in ("l", "r"):
        p.tube(P[f"sho_{s}"], P[f"elb_{s}"], 0.07, 0.054, t0=0.05, t1=1.08)  # sleeves
    return p.build(COL_CHAR, solidify=0.004)

def build_tabard():
    p = Part("SK_Warrior_Tabard", ["Cloth"])
    p.cube(T(0, -0.185, 0.86) @ R(-7, 'X') @ S(0.26, 0.006, 0.50))
    p.cube(T(0, 0.185, 0.86) @ R(7, 'X') @ S(0.26, 0.006, 0.50))
    return p.build(COL_CHAR, bevel=0.002)

def build_belt():
    p = Part("SK_Warrior_Belt", ["Leather", "Brass"])
    p.cone(T(0, 0, 1.095) @ S(1, 0.76, 1), 0.192, 0.19, 0.05, seg=28)
    p.cube(T(0, -0.148, 1.095) @ S(0.05, 0.012, 0.045), 1)
    p.cube(T(0.15, -0.06, 1.0) @ R(-20, 'Z') @ S(0.04, 0.03, 0.12), 0)  # pouch
    return p.build(COL_CHAR, solidify=0.006, bevel=0.002)

def build_arms():
    p = Part("SK_Warrior_Gauntlets", ["Steel", "Leather"])
    for s in ("l", "r"):
        sign = 1 if s == "l" else -1
        p.tube(P[f"elb_{s}"], P[f"wri_{s}"], 0.054, 0.043, t0=0.10, t1=0.96)        # vambrace
        p.sphere(T(*lerp(P[f"elb_{s}"], (P[f"elb_{s}"][0] + 0.03 * sign, 0.045, P[f"elb_{s}"][2]), 1.0))
                 @ S(0.05, 0.045, 0.05), seg=16, rings=8)                             # couter
        p.tube(P[f"wri_{s}"], P[f"hand_{s}"], 0.05, 0.04, t0=-0.15, t1=0.15)         # cuff
        m, L = align(P[f"wri_{s}"], P[f"hand_{s}"])
        p.cube(m @ T(0, 0, 0.01) @ S(0.075, 0.042, L * 1.15), 1)                      # leather glove
    return p.build(COL_CHAR, solidify=0.005, bevel=0.002)

def build_legs():
    p = Part("SK_Warrior_LegArmour", ["Steel"])
    for s in ("l", "r"):
        p.tube(P[f"hip_{s}"], P[f"knee_{s}"], 0.098, 0.072, t0=0.25, t1=0.97)   # cuisses
        k = Vector(P[f"knee_{s}"])
        p.sphere(T(k.x, k.y - 0.035, k.z) @ S(0.058, 0.04, 0.062), seg=16, rings=8)  # knee cop
        p.tube(P[f"knee_{s}"], P[f"ank_{s}"], 0.07, 0.054, t0=0.02, t1=0.86)  # greaves
    return p.build(COL_CHAR, solidify=0.005, bevel=0.002)

def build_boots():
    p = Part("SK_Warrior_Boots", ["Leather"])
    for s in (1, -1):
        p.cube(T(0.125 * s, -0.04, 0.05) @ S(0.105, 0.28, 0.10))                         # foot
        p.cube(T(0.121 * s, 0.012, 0.165) @ S(0.112, 0.12, 0.20))                        # shaft
    ob = p.build(COL_CHAR)
    bv = ob.modifiers.new("Bevel", 'BEVEL'); bv.width = 0.025; bv.segments = 4
    return ob

# ---------------------------------------------------------------- weapons (origin = grip)
def build_sword():
    p = Part("SM_ArmingSword", ["Steel", "Grip", "Brass"])
    p.cone(R(45, 'Z') @ S(1, 1, 1), 0.016, 0.014, 0.11, mi=1, seg=12, caps=True)      # grip (along Z)
    p.cube(T(0, 0, 0.065) @ S(0.20, 0.022, 0.02), 2)                                    # crossguard
    p.sphere(T(0, 0, -0.075) @ S(0.026, 0.02, 0.026), mi=2, seg=12, rings=8)            # pommel
    blade = T(0, 0, 0.075 + 0.35) @ S(1, 0.16, 1) @ R(45, 'Z')
    p.cone(blade, 0.036, 0.014, 0.70, mi=0, seg=4, caps=True)                           # blade
    p.cone(T(0, 0, 0.775 + 0.035) @ S(1, 0.16, 1) @ R(45, 'Z'), 0.014, 0.0, 0.07, mi=0, seg=4, caps=True)  # tip
    ob = p.build(COL_WEAP, smooth=False)
    ob.modifiers.new("WeightedNormal", 'WEIGHTED_NORMAL')
    return ob

def build_shield():
    bm = bmesh.new()
    pts = []
    for i in range(9):                                   # curved top edge, left to right
        t = i / 8
        pts.append((-0.25 + 0.5 * t, 0.36 + 0.035 * math.cos((t - 0.5) * math.pi)))
    for i in range(1, 12):                               # right side down to the point
        t = i / 12
        pts.append((0.25 * (1 - t ** 1.6), 0.36 - 0.80 * t ** 1.25))
    pts.append((0.0, -0.46))
    for i in range(11, 0, -1):                           # left side back up
        t = i / 12
        pts.append((-0.25 * (1 - t ** 1.6), 0.36 - 0.80 * t ** 1.25))
    face = bm.faces.new([bm.verts.new((x, 0, z)) for x, z in pts])
    res = bmesh.ops.inset_individual(bm, faces=[face], thickness=0.03, depth=0)
    for f in bm.faces:
        f.material_index = 1                             # steel rim
    face.material_index = 0                              # painted centre
    # brass hourglass emblem (the Veil Relic sign) on the front (-Y)
    for z, spin in ((0.11, 90), (-0.07, -90)):
        r = bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=True, segments=3, radius1=0.085, radius2=0.085,
                                  depth=0.008, matrix=T(0, -0.022, z) @ R(90, 'X') @ R(spin, 'Z'))
        for f in {f for v in r["verts"] for f in v.link_faces}:
            f.material_index = 2
    me = bpy.data.meshes.new("SM_KiteShield")
    bm.normal_update(); bm.to_mesh(me); bm.free()
    for m in ("Wood", "Steel", "Brass"):
        me.materials.append(M[m])
    ob = bpy.data.objects.new("SM_KiteShield", me)
    COL_WEAP.objects.link(ob)
    sol = ob.modifiers.new("Solidify", 'SOLIDIFY'); sol.thickness = 0.025; sol.offset = 0.0
    sol.material_offset_rim = 1
    bend = ob.modifiers.new("Curve", 'SIMPLE_DEFORM'); bend.deform_method = 'BEND'
    bend.deform_axis = 'Z'; bend.angle = math.radians(-35)
    ob.modifiers.new("Bevel", 'BEVEL').width = 0.003
    return ob

# ---------------------------------------------------------------- build everything
build_body()
for fn in (build_helmet, build_gorget, build_cuirass, build_pauldrons, build_mail, build_tabard,
           build_belt, build_arms, build_legs, build_boots):
    fn()
sword = build_sword()
shield = build_shield()

# Put weapons in the hands for the preview pose (object transforms only; meshes keep grip origins)
hand_r = lerp(P["wri_r"], P["hand_r"], 0.45)
sword.location = hand_r
sword.rotation_euler = (math.radians(160), math.radians(-18), 0)
shield.location = (0.43, -0.10, 1.12)
shield.rotation_euler = (math.radians(4), 0, math.radians(32))

# ---------------------------------------------------------------- preview stage (not exported)
floor = bpy.data.objects.new("Preview_Floor", bpy.data.meshes.new("Preview_Floor"))
bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=6); bm.to_mesh(floor.data); bm.free()
floor.data.materials.append(material("M_Preview_Floor", (0.07, 0.07, 0.075), 0, 0.8))
COL_PREV.objects.link(floor)

def light(name, kind, loc, rot, energy, size=1.0, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, kind); d.energy = energy; d.color = color
    if kind == 'AREA': d.size = size
    o = bpy.data.objects.new(name, d); o.location = loc; o.rotation_euler = [math.radians(a) for a in rot]
    COL_PREV.objects.link(o)
light("Key", 'AREA', (-2.2, -2.6, 3.0), (50, 0, -40), 650, 2.0, (1.0, 0.95, 0.88))
light("Rim", 'AREA', (2.4, 2.2, 2.6), (-55, 0, 135), 700, 1.5, (0.8, 0.88, 1.0))
light("Fill", 'AREA', (3.0, -2.0, 1.2), (80, 0, 55), 220, 3.0)

world = bpy.data.worlds.new("Preview_World"); world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.035, 0.037, 0.042, 1)
scene.world = world

cam_data = bpy.data.cameras.new("Preview_Camera"); cam_data.lens = 70
cam = bpy.data.objects.new("Preview_Camera", cam_data); COL_PREV.objects.link(cam)
scene.camera = cam
target = Vector((0, 0, 0.93))
def aim(loc):
    cam.location = loc
    cam.rotation_euler = (target - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()

scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 40
scene.cycles.use_denoising = True
scene.render.resolution_x, scene.render.resolution_y = 700, 1000
scene.view_settings.view_transform = 'AgX'
scene.view_settings.exposure = -0.8
for look in ("AgX - Medium High Contrast", "Medium High Contrast"):
    try:
        scene.view_settings.look = look
        break
    except TypeError:
        pass

bpy.ops.wm.save_as_mainfile(filepath=f"{OUT}/Warrior_Male_Blockout.blend")

# ---------------------------------------------------------------- polycount report
dg = bpy.context.evaluated_depsgraph_get()
total = 0
for ob in list(COL_CHAR.objects) + list(COL_WEAP.objects):
    ev = ob.evaluated_get(dg); me = ev.to_mesh()
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    total += tris if ob in COL_CHAR.objects.values() else 0
    print(f"POLY {ob.name}: {tris} tris")
    ev.to_mesh_clear()
print(f"POLY character total: {total} tris")

# ---------------------------------------------------------------- exports for Unreal
def export(objs, path):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_ALL', use_mesh_modifiers=True,
                             mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False,
                             axis_forward='-Y', axis_up='Z')

export(list(COL_CHAR.objects), f"{OUT}/SK_Warrior_Male_Blockout.fbx")
# weapons exported at their grip origin, without the preview pose
for w in (sword, shield):
    loc, rot = w.location.copy(), w.rotation_euler.copy()
    w.location = (0, 0, 0); w.rotation_euler = (0, 0, 0)
    export([w], f"{OUT}/{w.name}.fbx")
    w.location, w.rotation_euler = loc, rot

# ---------------------------------------------------------------- preview renders
views = {"front": (0, -4.6, 1.05), "three_quarter": (3.1, -3.5, 1.25), "side": (-4.6, 0.0, 1.05), "back": (-1.6, 4.3, 1.2)}
only = sys.argv[2].split(",") if len(sys.argv) > 2 else list(views)
for name in only:
    aim(views[name])
    scene.render.filepath = f"{OUT}/render_{name}.png"
    bpy.ops.render.render(write_still=True)
    print("RENDERED", name)
