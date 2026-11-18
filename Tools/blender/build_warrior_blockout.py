"""
Builds the Ironveil male Warrior (refined blockout, v2) from scratch in Blender 5.2.

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
VIEWS = ARGS[1].split(",") if len(ARGS) > 1 else ["front", "three_quarter", "side", "back"]
os.makedirs(OUT, exist_ok=True)

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

# ---------------------------------------------------------------- materials (procedural detail is for look-dev only)
def material(name, color, metallic=0.0, rough=0.5, detail=None, detail_scale=60.0, bump=0.2):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*color, 1)
    if detail:
        coord = nt.nodes.new("ShaderNodeTexCoord")
        if detail == "mail":
            tex = nt.nodes.new("ShaderNodeTexVoronoi")
            tex.feature = 'DISTANCE_TO_EDGE'
            out = tex.outputs["Distance"]
        else:
            tex = nt.nodes.new("ShaderNodeTexNoise")
            tex.inputs["Detail"].default_value = 8.0
            out = tex.outputs["Fac"]
        tex.inputs["Scale"].default_value = detail_scale
        nt.links.new(coord.outputs["Object"], tex.inputs["Vector"])
        bmp = nt.nodes.new("ShaderNodeBump")
        bmp.inputs["Strength"].default_value = bump
        nt.links.new(out, bmp.inputs["Height"])
        nt.links.new(bmp.outputs["Normal"], b.inputs["Normal"])
        if detail in ("steel", "leather"):
            rng = nt.nodes.new("ShaderNodeMapRange")
            rng.inputs["To Min"].default_value = rough * 0.75
            rng.inputs["To Max"].default_value = min(1.0, rough * 1.35)
            nt.links.new(out, rng.inputs["Value"])
            nt.links.new(rng.outputs["Result"], b.inputs["Roughness"])
    return m

M = {
    "Skin":      material("M_Warrior_Skin", (0.62, 0.42, 0.32), 0.0, 0.55),
    "Steel":     material("M_Warrior_Steel", (0.56, 0.57, 0.58), 1.0, 0.32, "steel", 18.0, 0.05),
    "DarkSteel": material("M_Warrior_DarkSteel", (0.22, 0.22, 0.23), 1.0, 0.45, "steel", 18.0, 0.05),
    "Mail":      material("M_Warrior_Mail", (0.30, 0.30, 0.31), 1.0, 0.55, "mail", 260.0, 0.6),
    "Leather":   material("M_Warrior_Leather", (0.11, 0.065, 0.04), 0.0, 0.65, "leather", 90.0, 0.25),
    "Cloth":     material("M_Warrior_Tabard", (0.30, 0.04, 0.035), 0.0, 0.85, "cloth", 300.0, 0.15),
    "Trim":      material("M_Warrior_Trim", (0.62, 0.52, 0.34), 0.0, 0.8, "cloth", 300.0, 0.15),
    "Wool":      material("M_Warrior_Cape", (0.13, 0.12, 0.105), 0.0, 0.9, "cloth", 200.0, 0.2),
    "Fur":       material("M_Warrior_Fur", (0.20, 0.14, 0.09), 0.0, 1.0, "leather", 400.0, 0.8),
    "Brass":     material("M_Warrior_Brass", (0.72, 0.52, 0.22), 1.0, 0.35, "steel", 18.0, 0.05),
    "Wood":      material("M_Shield_Paint", (0.26, 0.05, 0.04), 0.0, 0.6, "leather", 30.0, 0.1),
    "Paint2":    material("M_Shield_Paint_Cream", (0.62, 0.55, 0.40), 0.0, 0.6, "leather", 30.0, 0.1),
    "Grip":      material("M_Sword_Grip", (0.10, 0.06, 0.04), 0.0, 0.7, "leather", 120.0, 0.3),
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
def frame(a, b, front=(0, -1, 0)):
    """Matrix at a with Z along a->b, local +Y pointing to the character's front (-Y), X = Y x Z.
    The same on both sides, so 'front' arcs (around 90 degrees) mean the same thing left and right."""
    a, b = Vector(a), Vector(b)
    z = (b - a).normalized()
    y = (Vector(front) - z * Vector(front).dot(z)).normalized()
    x = y.cross(z)
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = a
    return m, (b - a).length
def lerp(a, b, t): return Vector(a).lerp(Vector(b), t)
def mirror(p): return (-p[0], p[1], p[2])

class Part:
    """Collects several primitives (each with a material) into one object."""
    def __init__(self, name, mats):
        self.name, self.bm, self.mats = name, bmesh.new(), [M[m] for m in mats]
    def _tag(self, verts, mi):
        for f in {f for v in verts for f in v.link_faces}:
            f.material_index = mi
        return verts
    def sphere(self, matrix, mi=0, seg=24, rings=12, cut_below=None, keep=None):
        r = bmesh.ops.create_uvsphere(self.bm, u_segments=seg, v_segments=rings, radius=1.0, matrix=matrix)
        verts = r["verts"]
        inv = matrix.inverted()
        if cut_below is not None or keep is not None:
            kill = [v for v in verts if (cut_below is not None and (inv @ v.co).z < cut_below)
                    or (keep is not None and not keep(inv @ v.co))]
            bmesh.ops.delete(self.bm, geom=kill, context='VERTS')
            verts = [v for v in verts if v.is_valid]
        return self._tag(verts, mi)
    def cone(self, matrix, r1, r2, depth, mi=0, seg=24, caps=False):
        r = bmesh.ops.create_cone(self.bm, cap_ends=caps, cap_tris=False, segments=seg,
                                  radius1=r1, radius2=r2, depth=depth, matrix=matrix)
        return self._tag(r["verts"], mi)
    def cube(self, matrix, mi=0):
        r = bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)
        return self._tag(r["verts"], mi)
    def lathe(self, rings, mi=0, seg=28, matrix=None, arc=None):
        """rings: (z, rx, ry) bottom to top. arc=(a0, a1) in degrees makes an open curved plate."""
        loops = []
        n = seg if arc is None else seg + 1
        for z, rx, ry in rings:
            loop = []
            for i in range(n):
                if arc is None:
                    ang = 2 * math.pi * i / seg
                else:
                    ang = math.radians(arc[0] + (arc[1] - arc[0]) * i / seg)
                co = Vector((rx * math.cos(ang), ry * math.sin(ang), z))
                loop.append(self.bm.verts.new(matrix @ co if matrix else co))
            loops.append(loop)
        for a, b in zip(loops, loops[1:]):
            for i in range(n if arc is None else n - 1):
                j = (i + 1) % n
                self.bm.faces.new((a[i], a[j], b[j], b[i]))
        return self._tag([v for l in loops for v in l], mi)
    def limb(self, a, b, profile, mi=0, seg=24, arc=None):
        """Shaped tube along a->b. profile: (t, rx, ry) with t from 0..1 along the segment."""
        m, L = frame(a, b)
        return self.lathe([(t * L, rx, ry) for t, rx, ry in profile], mi, seg, m, arc)
    def tube(self, a, b, r_a, r_b, mi=0, seg=20, t0=0.0, t1=1.0, caps=False):
        pa, pb = lerp(a, b, t0), lerp(a, b, t1)
        m, L = align(pa, pb)
        return self.cone(m, r_a, r_b, L, mi, seg, caps)
    def rivet(self, co, mi, r=0.007):
        """Rivets go into one shared object (SK_Warrior_Rivets) so they skip this part's modifiers."""
        RIVETS.sphere(T(*co) @ S(r, r, r), 0 if self.mats[mi] is M["Brass"] else 1, seg=8, rings=5)
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

RIVETS = None   # created after the Part class exists

def cloth_panel(part, mi, top_z, bot_z, top_w, bot_w, y_of, fold_amp, folds, hem="dagged", nx=14, nz=18,
                emblem=None):
    """A hanging cloth sheet: y_of(x, z) gives its depth; folds deepen toward the hem."""
    grid = []
    for j in range(nz + 1):
        t = j / nz
        z = top_z + (bot_z - top_z) * t
        w = top_w + (bot_w - top_w) * t
        row = []
        for i in range(nx + 1):
            u = i / nx
            x = -w / 2 + w * u
            if hem == "dagged" and j == nz:
                z_i = z + (0.04 if i % 2 else 0.0)
            else:
                z_i = z
            fold = fold_amp * (0.25 + t) * math.sin(u * folds * math.pi)
            row.append(part.bm.verts.new((x, y_of(x, z_i) + fold, z_i)))
        grid.append(row)
    for j in range(nz):
        for i in range(nx):
            f = part.bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
            f.material_index = mi

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
SIDES = (("l", 1), ("r", -1))

# ---------------------------------------------------------------- body (Skin modifier mannequin, editable)
def build_body():
    names = list(P)
    radii = dict(pelvis=(0.16, 0.115), spine=(0.145, 0.11), chest=(0.18, 0.125), upchest=(0.17, 0.11),
                 neck=(0.06, 0.06), necktop=(0.055, 0.055), clav=(0.07, 0.06), sho=(0.065, 0.065),
                 elb=(0.045, 0.045), wri=(0.035, 0.028), hand=(0.04, 0.02), hip=(0.09, 0.09),
                 knee=(0.058, 0.058), ank=(0.042, 0.042), toe=(0.045, 0.03))
    edges = [("pelvis", "spine"), ("spine", "chest"), ("chest", "upchest"), ("upchest", "neck"), ("neck", "necktop")]
    for s, _ in SIDES:
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
    h = Part("SK_Warrior_Head", ["Skin"])
    h.sphere(T(0, 0.005, 1.675) @ S(0.092, 0.108, 0.122))
    h.sphere(T(0, -0.035, 1.615) @ S(0.07, 0.07, 0.06))                    # jaw
    h.sphere(T(0, -0.108, 1.66) @ S(0.016, 0.02, 0.03), seg=12, rings=8)  # nose
    for s in (1, -1):
        h.sphere(T(0.04 * s, -0.096, 1.685) @ S(0.022, 0.01, 0.008), seg=12, rings=6)  # brow ridge
    h.build(COL_CHAR)

# ---------------------------------------------------------------- helmet with aventail and crest
def build_helmet():
    p = Part("SK_Warrior_Helmet", ["Steel", "Brass", "Mail"])
    dome = p.sphere(T(0, 0.005, 1.695) @ S(0.118, 0.13, 0.145), cut_below=-0.05)
    for v in dome:                                         # gentle bascinet point at the top
        if v.co.z > 1.80:
            v.co.z += (v.co.z - 1.80) * 0.6
    p.lathe([(1.615, 0.119, 0.131), (1.645, 0.12, 0.132), (1.675, 0.118, 0.13)], mi=0, seg=32)  # brow band
    p.cube(T(0, -0.14, 1.635) @ S(0.022, 0.012, 0.10), 0)                                       # nasal
    for s in (1, -1):                                                                           # curved cheek plates
        p.lathe([(1.555, 0.112, 0.122), (1.585, 0.118, 0.128), (1.62, 0.119, 0.131)], 0, seg=10,
                matrix=T(0, 0.005, 0), arc=(-58, -15) if s > 0 else (-165, -122))
    # crest: a brass band running front to back over the dome
    crest = p.lathe([(-0.006, 0.143, 0.157), (0.006, 0.143, 0.157)], 1, seg=40,
                    matrix=T(0, 0.005, 1.695) @ R(90, 'Y') @ R(90, 'Z'), arc=(0, 180))
    # aventail: mail curtain from the helmet rim down to the shoulders, open at the face
    aven = p.lathe([(1.45, 0.175, 0.155), (1.50, 0.15, 0.14), (1.555, 0.125, 0.135), (1.60, 0.118, 0.13)],
                   2, seg=32, matrix=T(0, 0.01, 0))
    kill = [f for f in p.bm.faces if f.material_index == 2 and f.calc_center_median().y < -0.07
            and f.calc_center_median().z > 1.535]
    bmesh.ops.delete(p.bm, geom=kill, context='FACES')
    for a in range(0, 360, 30):                                                                 # rivets on brow band
        ang = math.radians(a)
        p.rivet((0.122 * math.cos(ang), 0.005 + 0.135 * math.sin(ang), 1.645), 1, 0.006)
    return p.build(COL_CHAR, solidify=0.005, bevel=0.002)

def build_gorget():
    p = Part("SK_Warrior_Gorget", ["Steel"])
    p.lathe([(1.46, 0.165, 0.14), (1.485, 0.15, 0.125), (1.51, 0.11, 0.095), (1.535, 0.085, 0.08)], 0, 32)
    return p.build(COL_CHAR, solidify=0.005)

# ---------------------------------------------------------------- torso: cuirass, fauld, tassets
def build_cuirass():
    p = Part("SK_Warrior_Cuirass", ["Steel", "Brass", "DarkSteel"])
    profile = [(1.10, 0.172, 0.126), (1.16, 0.166, 0.122), (1.22, 0.176, 0.128), (1.29, 0.196, 0.138),
               (1.36, 0.21, 0.145), (1.42, 0.205, 0.138), (1.46, 0.186, 0.124), (1.485, 0.15, 0.104)]
    vs = p.lathe(profile, seg=36)
    for v in vs:                                            # breastplate ridge
        if v.co.y < 0 and 1.15 < v.co.z < 1.47:
            v.co.y -= 0.02 * max(0.0, 1 - abs(v.co.x) / 0.16)
    # plackart (belly plate) overlapping the breastplate, front only
    p.lathe([(1.10, 0.18, 0.136), (1.16, 0.174, 0.132), (1.23, 0.18, 0.137)], 0, 18, arc=(200, 340))
    # fauld: overlapping hoops below the waist
    for i, (z, rx, ry) in enumerate([(1.10, 0.184, 0.138), (1.065, 0.19, 0.143), (1.03, 0.196, 0.148)]):
        p.lathe([(z - 0.04, rx + 0.006, ry + 0.005), (z, rx, ry)], 0, 36)
        for a in range(205, 340, 22):                       # rivets along each lame
            ang = math.radians(a)
            p.rivet((rx * math.cos(ang), ry * math.sin(ang) - 0.004, z - 0.012), 1, 0.006)
    # tassets: curved plates hanging over the front of each thigh
    for s, sign in SIDES:
        hx = 0.105 * sign
        for k, (z0, z1) in enumerate([(0.995, 0.905), (0.925, 0.835)]):
            m = T(hx, -0.02 - 0.004 * k, 0) @ R(-6, 'X')
            p.lathe([(z1, 0.122, 0.138), (z0, 0.116, 0.132)], 0, 10, matrix=m, arc=(205, 335))
    p.sphere(T(0, -0.168, 1.37) @ S(0.028, 0.012, 0.028), mi=1, seg=12, rings=8)   # brass boss
    for z in (1.43, 1.25):                                  # rivets beside the ridge
        for s in (1, -1):
            p.rivet((0.09 * s, -0.142 if z > 1.3 else -0.13, z), 1, 0.006)
    return p.build(COL_CHAR, solidify=0.007, bevel=0.002)

def build_pauldrons():
    p = Part("SK_Warrior_Pauldrons", ["Steel", "Brass"])
    for s, sign in SIDES:
        x, y, z = P[f"sho_{s}"]
        base = T(x + 0.025 * sign, y, z + 0.03) @ R(-28 * sign, 'Y')
        p.sphere(base @ S(0.112, 0.108, 0.092), cut_below=-0.15)
        for k, dz in enumerate((-0.03, -0.058, -0.084)):
            r0 = 0.112 - 0.004 * k
            p.lathe([(dz - 0.03, r0 + 0.004, r0 * 0.96 + 0.004), (dz, r0, r0 * 0.96)], 0, 28, matrix=base)
        for a in range(0, 360, 45):                         # rivets on the top lame
            ang = math.radians(a)
            p.rivet(base @ Vector((0.113 * math.cos(ang), 0.108 * math.sin(ang), -0.04)), 1, 0.006)
    return p.build(COL_CHAR, solidify=0.006, bevel=0.002)

# ---------------------------------------------------------------- mail
def build_mail():
    p = Part("SK_Warrior_Hauberk", ["Mail"])
    p.lathe([(0.78, 0.225, 0.18), (0.90, 0.205, 0.165), (1.02, 0.18, 0.145), (1.13, 0.168, 0.128)], 0, 32)
    for s, sign in SIDES:
        p.limb(P[f"sho_{s}"], P[f"elb_{s}"], [(0.0, 0.072, 0.07), (0.6, 0.062, 0.06), (1.06, 0.055, 0.054)], 0, 20)
    return p.build(COL_CHAR, solidify=0.004)

# ---------------------------------------------------------------- arms: rerebrace, couter, vambrace, gauntlet
def build_arms():
    p = Part("SK_Warrior_Gauntlets", ["Steel", "Leather", "Brass"])
    for s, sign in SIDES:
        sho, elb, wri, hand = P[f"sho_{s}"], P[f"elb_{s}"], P[f"wri_{s}"], P[f"hand_{s}"]
        p.limb(sho, elb, [(0.38, 0.068, 0.066), (0.62, 0.066, 0.064), (0.86, 0.061, 0.059)], 0, 20)  # rerebrace
        e = Vector(elb)
        p.sphere(T(e.x + 0.012 * sign, e.y + 0.035, e.z) @ S(0.052, 0.046, 0.052), 0, 18, 10)           # couter cup
        p.sphere(T(e.x + 0.05 * sign, e.y + 0.02, e.z) @ R(25 * sign, 'Y') @ S(0.012, 0.06, 0.07), 0, 16, 8)  # fan wing
        p.limb(elb, wri, [(0.12, 0.056, 0.054), (0.38, 0.058, 0.055), (0.72, 0.048, 0.046), (0.97, 0.043, 0.041)],
               0, 20)                                                                                   # vambrace
        # gauntlet: flared cuff, back-of-hand plate, leather palm, steel finger lames, thumb
        p.limb(lerp(wri, hand, -0.25), lerp(wri, hand, 0.12), [(0.0, 0.046, 0.044), (1.0, 0.064, 0.06)], 0, 20)
        m, L = frame(wri, hand)                       # local +X = world +X on both sides; outward = sign
        p.cube(m @ T(-0.004 * sign, 0, 0.05) @ S(0.03, 0.08, 0.085), 1)                              # palm
        p.lathe([(0.012, 0.024, 0.045), (0.09, 0.022, 0.044)], 0, 10, matrix=m,
                arc=(-75, 75) if sign > 0 else (105, 255))                                             # back plate
        for k, yy in enumerate((-0.03, -0.01, 0.01, 0.03)):
            for seg_i, (z0, ln) in enumerate(((0.095, 0.03), (0.125, 0.028), (0.152, 0.024))):
                p.cube(m @ T(0.004 * sign, yy, z0 + ln / 2) @ R(-8 * seg_i * sign, 'Y') @ S(0.022, 0.018, ln), 0)
        p.cube(m @ T(-0.006 * sign, 0.05, 0.055) @ R(35, 'X') @ S(0.02, 0.02, 0.06), 0)              # thumb (front)
        p.rivet(m @ Vector((0.03 * sign, 0, 0.0)), 2, 0.006)
    return p.build(COL_CHAR, solidify=0.004, bevel=0.002)

# ---------------------------------------------------------------- legs: cuisses, poleyns, greaves, sabatons
def build_legs():
    p = Part("SK_Warrior_LegArmour", ["Steel", "Brass"])
    for s, sign in SIDES:
        hip, knee, ank = P[f"hip_{s}"], P[f"knee_{s}"], P[f"ank_{s}"]
        p.limb(hip, knee, [(0.22, 0.104, 0.1), (0.6, 0.088, 0.085), (0.96, 0.072, 0.07)], 0, 22, arc=(-30, 210))  # cuisse (front+sides)
        k = Vector(knee)
        p.sphere(T(k.x, k.y - 0.04, k.z) @ S(0.06, 0.045, 0.066), 0, 18, 10)                         # poleyn cup
        p.sphere(T(k.x + 0.05 * sign, k.y - 0.01, k.z) @ R(15 * sign, 'Z') @ S(0.012, 0.055, 0.06), 0, 16, 8)  # side wing
        for dz in (0.075, -0.075):                                                                       # poleyn lames
            p.lathe([(k.z + dz - 0.012, 0.075, 0.07), (k.z + dz + 0.012, 0.073, 0.068)], 0, 12,
                    matrix=T(k.x, k.y - 0.005, 0), arc=(200, 340))
        p.limb(knee, ank, [(0.04, 0.068, 0.066), (0.28, 0.075, 0.076), (0.55, 0.062, 0.062), (0.95, 0.052, 0.052)],
               0, 22)                                                                                    # greave
        p.rivet((k.x + 0.064 * sign, k.y - 0.01, k.z), 1, 0.007)
    return p.build(COL_CHAR, solidify=0.005, bevel=0.002)

def build_boots():
    p = Part("SK_Warrior_Boots", ["Leather", "Steel"])
    for s, sign in SIDES:
        x = 0.125 * sign
        p.cube(T(x, -0.04, 0.035) @ S(0.10, 0.27, 0.07), 0)                                    # leather boot / sole
        p.cube(T(x, 0.015, 0.10) @ S(0.096, 0.108, 0.09), 0)                                   # ankle
        for k in range(5):                                                                      # sabaton lames
            yc = 0.03 - 0.036 * k
            r = 0.064 - 0.003 * k
            p.lathe([(-0.02, r * 0.82, r), (0.02, r * 0.82, r)], 1, 14,
                    matrix=T(x, yc, 0.03) @ R(90, 'X') @ R(90, 'Z'), arc=(-90, 90))
        p.sphere(T(x, -0.155, 0.03) @ S(0.054, 0.055, 0.045), 1, 16, 8, cut_below=-0.05)       # toe cap
    ob = p.build(COL_CHAR)
    bv = ob.modifiers.new("Bevel", 'BEVEL'); bv.width = 0.012; bv.segments = 3
    sol = ob.modifiers.new("Solidify", 'SOLIDIFY'); sol.thickness = 0.004; sol.offset = 1.0
    return ob

# ---------------------------------------------------------------- belt and scabbard
def build_belt():
    p = Part("SK_Warrior_Belt", ["Leather", "Brass", "Steel"])
    p.lathe([(1.07, 0.206, 0.16), (1.12, 0.204, 0.158)], 0, 36)
    p.cube(T(0, -0.165, 1.095) @ S(0.05, 0.012, 0.045), 1)                                    # buckle
    p.cube(T(-0.16, -0.08, 1.0) @ R(25, 'Z') @ S(0.045, 0.035, 0.11), 0)                     # pouch (right hip)
    # scabbard hanging from the left hip, angled back
    top, tip = Vector((0.205, 0.03, 1.07)), Vector((0.30, 0.30, 0.36))
    m, L = align(top, tip)
    p.cone(m @ S(1, 0.35, 1), 0.035, 0.022, L, 0, 4, caps=True)                  # leather body
    p.cone(Matrix.Translation(top + (tip - top) * 0.04) @ m.to_3x3().to_4x4() @ S(1, 0.35, 1),
           0.04, 0.039, 0.06, 2, 4, caps=True)                                                 # steel locket
    p.cone(Matrix.Translation(tip - (tip - top).normalized() * 0.04) @ m.to_3x3().to_4x4() @ S(1, 0.35, 1),
           0.025, 0.012, 0.08, 2, 4, caps=True)                                                # steel chape
    p.cube(T(0.2, 0.0, 1.03) @ R(-20, 'X') @ S(0.012, 0.02, 0.09), 0)                          # hanger strap
    ob = p.build(COL_CHAR, bevel=0.002)
    ob.modifiers.new("Solidify", 'SOLIDIFY').thickness = 0.005
    return ob

# ---------------------------------------------------------------- cloth: tabard and cape
def skirt_y(z):
    """Depth of the mail skirt front at height z (matches build_mail)."""
    pts = [(0.78, 0.18), (0.90, 0.165), (1.02, 0.145), (1.13, 0.128)]
    if z <= pts[0][0]: return pts[0][1]
    for (z0, y0), (z1, y1) in zip(pts, pts[1:]):
        if z0 <= z <= z1:
            return y0 + (y1 - y0) * (z - z0) / (z1 - z0)
    return pts[-1][1]

def build_tabard():
    p = Part("SK_Warrior_Tabard", ["Cloth", "Trim"])
    for side in (-1, 1):   # -1 front, +1 back
        cloth_panel(p, 0, 1.11, 0.60, 0.25, 0.30,
                    lambda x, z, side=side: side * (skirt_y(max(z, 0.78)) + 0.022 + 0.04 * max(0, 0.78 - z)),
                    0.01 * -side, 3)
    # cream hourglass emblem on the front panel
    for z, spin in ((0.99, 180), (0.89, 0)):          # 180 = points down, 0 = points up
        p.cone(T(0, -skirt_y(z) - 0.042, z) @ R(90, 'X') @ R(spin, 'Z'), 0.05, 0.05, 0.004, 1, 3, caps=True)
    ob = p.build(COL_CHAR)
    sol = ob.modifiers.new("Solidify", 'SOLIDIFY'); sol.thickness = 0.005
    return ob

def build_cape():
    p = Part("SK_Warrior_Cape", ["Wool", "Fur"])
    def back_y(x, z):
        t = (1.46 - z) / (1.46 - 0.55)
        base = 0.15 + 0.07 * t
        return base + 0.18 * (x / 0.4) ** 2 * (1 - 0.6 * t) * -1 + 0.06
    cloth_panel(p, 0, 1.46, 0.55, 0.40, 0.66, back_y, 0.03, 7, hem="straight", nx=28, nz=22)
    # fur collar lying over the shoulders and upper back
    rng = [(1.47, 0.20, 0.15), (1.50, 0.21, 0.16), (1.53, 0.17, 0.13)]
    vs = p.lathe(rng, 1, 32, matrix=T(0, 0.025, 0))
    for v in vs:
        n = math.sin(v.co.x * 90) * math.cos(v.co.y * 70)
        v.co.x *= 1 + 0.06 * n; v.co.y *= 1 + 0.06 * n
    kill = [f for f in p.bm.faces if f.material_index == 1 and f.calc_center_median().y < 0.03]
    bmesh.ops.delete(p.bm, geom=kill, context='FACES')
    ob = p.build(COL_CHAR)
    sol = ob.modifiers.new("Solidify", 'SOLIDIFY'); sol.thickness = 0.008
    ss = ob.modifiers.new("Subdivision", 'SUBSURF'); ss.levels = 1; ss.render_levels = 1
    return ob

# ---------------------------------------------------------------- weapons (origin = grip)
def build_sword():
    p = Part("SM_ArmingSword", ["Steel", "Grip", "Brass"])
    p.cone(R(45, 'Z'), 0.016, 0.014, 0.11, mi=1, seg=12, caps=True)                       # grip
    for z in (-0.03, 0.0, 0.03):
        p.cone(T(0, 0, z), 0.0175, 0.0175, 0.004, mi=2, seg=12, caps=True)                 # brass wire bands
    p.cube(T(0, 0, 0.065) @ S(0.20, 0.022, 0.02), 2)                                       # crossguard
    for s in (1, -1):
        p.sphere(T(0.1 * s, 0, 0.065) @ S(0.014, 0.014, 0.014), 2, 10, 6)                  # quillon tips
    p.sphere(T(0, 0, -0.075) @ S(0.026, 0.018, 0.026), mi=2, seg=12, rings=8)              # wheel pommel
    p.cone(T(0, 0, 0.075 + 0.35) @ S(1, 0.16, 1) @ R(45, 'Z'), 0.036, 0.014, 0.70, 0, 4, caps=True)  # blade
    p.cone(T(0, 0, 0.775 + 0.035) @ S(1, 0.16, 1) @ R(45, 'Z'), 0.014, 0.0, 0.07, 0, 4, caps=True)    # tip
    ob = p.build(COL_WEAP, smooth=False)
    ob.modifiers.new("WeightedNormal", 'WEIGHTED_NORMAL')
    return ob

def shield_outline():
    pts = []
    for i in range(9):
        t = i / 8
        pts.append((-0.25 + 0.5 * t, 0.36 + 0.035 * math.cos((t - 0.5) * math.pi)))
    for i in range(1, 12):
        t = i / 12
        pts.append((0.25 * (1 - t ** 1.6), 0.36 - 0.80 * t ** 1.25))
    pts.append((0.0, -0.46))
    for i in range(11, 0, -1):
        t = i / 12
        pts.append((-0.25 * (1 - t ** 1.6), 0.36 - 0.80 * t ** 1.25))
    return pts

def build_shield():
    bm = bmesh.new()
    pts = shield_outline()
    outer = [bm.verts.new((x, -0.0125, z)) for x, z in pts]
    front = bm.faces.new(outer)
    bmesh.ops.inset_individual(bm, faces=[front], thickness=0.03, depth=0)
    for f in bm.faces:
        f.material_index = 1                    # steel rim
    front.material_index = 0                    # red field
    # back face + side wall, so the board has real thickness
    back_v = [bm.verts.new((x, 0.0125, z)) for x, z in pts]
    bm.faces.new(back_v).material_index = 0
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((outer[i], outer[j], back_v[j], back_v[i])).material_index = 1
    # cream chevron and brass hourglass, painted / mounted on the front
    chev = [(-0.15, 0.02), (0.0, -0.20), (0.15, 0.02), (0.15, 0.085), (0.0, -0.13), (-0.15, 0.085)]
    cf = bm.faces.new([bm.verts.new((x, -0.0145, z)) for x, z in chev]); cf.material_index = 3
    for z, spin in ((0.245, 180), (0.135, 0)):
        r = bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=True, segments=3, radius1=0.055, radius2=0.055,
                                  depth=0.008, matrix=T(0, -0.017, z) @ R(90, 'X') @ R(spin, 'Z'))
        for f in {f for v in r["verts"] for f in v.link_faces}: f.material_index = 2
    for i, (x, z) in enumerate(pts):            # rim rivets
        if i % 3 == 0:
            c = Vector((x * 0.9, -0.0135, z * 0.93 + 0.01))
            r = bmesh.ops.create_uvsphere(bm, u_segments=8, v_segments=5, radius=0.007, matrix=T(*c))
            for f in {f for v in r["verts"] for f in v.link_faces}: f.material_index = 1
    # back: leather straps (enarmes) and a wooden handle block
    for z in (0.12, -0.08):
        r = bmesh.ops.create_cube(bm, size=1.0, matrix=T(0, 0.028, z) @ S(0.16, 0.02, 0.035))
        for f in {f for v in r["verts"] for f in v.link_faces}: f.material_index = 4
    # curve the board (centre of curvature behind the shield); applied to the mesh so the paint stays on top
    Rb = 0.82
    for v in bm.verts:
        ang = v.co.x / Rb
        rad = Rb - v.co.y
        v.co.x, v.co.y = rad * math.sin(ang), Rb - rad * math.cos(ang)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new("SM_KiteShield")
    bm.normal_update(); bm.to_mesh(me); bm.free()
    for m in ("Wood", "Steel", "Brass", "Paint2", "Grip"):
        me.materials.append(M[m])
    ob = bpy.data.objects.new("SM_KiteShield", me)
    COL_WEAP.objects.link(ob)
    return ob

# ---------------------------------------------------------------- build everything
RIVETS = Part("SK_Warrior_Rivets", ["Brass", "Steel"])
build_body()
for fn in (build_helmet, build_gorget, build_cuirass, build_pauldrons, build_mail, build_tabard, build_cape,
           build_belt, build_arms, build_legs, build_boots):
    fn()
RIVETS.build(COL_CHAR)
sword = build_sword()
shield = build_shield()

# UV unwrap every mesh so the pieces are ready for texturing (body excluded: replaced by MetaHuman later)
for ob in list(COL_CHAR.objects) + list(COL_WEAP.objects):
    if ob.name == "SK_Warrior_Body":
        continue
    try:
        bpy.ops.object.select_all(action='DESELECT')
        bpy.context.view_layer.objects.active = ob
        ob.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
        bpy.ops.object.mode_set(mode='OBJECT')
    except Exception as e:
        print("UV unwrap skipped for", ob.name, e)
        if bpy.context.object and bpy.context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

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
scene.cycles.samples = 48
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

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "Warrior_Male_Blockout.blend"))

# ---------------------------------------------------------------- polycount report
dg = bpy.context.evaluated_depsgraph_get()
total = 0
for ob in list(COL_CHAR.objects) + list(COL_WEAP.objects):
    ev = ob.evaluated_get(dg); me = ev.to_mesh()
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    if ob.name in COL_CHAR.objects:
        total += tris
    print(f"POLY {ob.name}: {tris} tris, UV layers: {len(ob.data.uv_layers)}")
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

export(list(COL_CHAR.objects), os.path.join(OUT, "SK_Warrior_Male_Blockout.fbx"))
for w in (sword, shield):
    loc, rot = w.location.copy(), w.rotation_euler.copy()
    w.location = (0, 0, 0); w.rotation_euler = (0, 0, 0)
    export([w], os.path.join(OUT, f"{w.name}.fbx"))
    w.location, w.rotation_euler = loc, rot

# ---------------------------------------------------------------- preview renders
views = {"front": (0, -4.6, 1.05), "three_quarter": (3.1, -3.5, 1.25), "side": (-4.6, 0.0, 1.05),
         "back": (-1.6, 4.3, 1.2), "closeup": (1.1, -1.6, 1.5)}
for name in VIEWS:
    if name == "closeup":
        target = Vector((0, 0, 1.35)); cam_data.lens = 85
    aim(views[name])
    scene.render.filepath = os.path.join(OUT, f"render_{name}.png")
    bpy.ops.render.render(write_still=True)
    print("RENDERED", name)
