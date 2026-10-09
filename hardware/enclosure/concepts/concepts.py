"""Orientacyjne koncepcje bryły obudowy radaru (Blender, jednostki: mm).

Wymiary modułów są przybliżone (typowe wartości katalogowe, NIE zmierzone na
posiadanych egzemplarzach). Służą tylko do porównania proporcji i układu.
Uruchomienie (bez okna):
  flatpak run --filesystem=host org.blender.Blender --background --python concepts.py
"""
import bpy, math, os, sys
from mathutils import Vector

OUT = os.path.dirname(os.path.abspath(__file__))

COL = {
    "body": (0.78, 0.80, 0.84, 1), "dark": (0.05, 0.05, 0.06, 1),
    "pcb": (0.05, 0.22, 0.45, 1), "red": (0.95, 0.12, 0.08, 1),
    "servo": (0.10, 0.25, 0.75, 1), "silver": (0.75, 0.75, 0.78, 1),
    "screen": (0.02, 0.02, 0.03, 1), "oled": (0.15, 0.55, 0.85, 1),
    "floor": (0.93, 0.93, 0.93, 1), "board": (0.04, 0.35, 0.18, 1),
}
_mats = {}
def mat(name):
    if name not in _mats:
        m = bpy.data.materials.new(name)
        m.diffuse_color = COL[name]
        _mats[name] = m
    return _mats[name]

def box(size, loc, color, parent=None, name="box"):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.active_object
    o.scale = size
    o.name = name
    o.data.materials.append(mat(color))
    if parent: o.parent = parent
    return o

def cyl(r, h, loc, color, axis="z", parent=None, name="cyl"):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=h, location=loc, vertices=32)
    o = bpy.context.active_object
    if axis == "y": o.rotation_euler = (math.pi / 2, 0, 0)
    o.name = name
    o.data.materials.append(mat(color))
    if parent: o.parent = parent
    return o

def empty(loc, rot=(0, 0, 0), name="grp"):
    e = bpy.data.objects.new(name, None)
    bpy.context.collection.objects.link(e)
    e.location = loc
    e.rotation_euler = rot
    return e

def prism_yz(pts, width, color, name="body"):
    """Bryła o profilu w płaszczyźnie y-z, wyciągnięta wzdłuż x."""
    n = len(pts)
    v = [(-width / 2, y, z) for y, z in pts] + [(width / 2, y, z) for y, z in pts]
    f = [list(range(n))[::-1], list(range(n, 2 * n))]
    for i in range(n):
        j = (i + 1) % n
        f.append([i, j, n + j, n + i])
    me = bpy.data.meshes.new(name)
    me.from_pydata(v, [], f)
    me.update()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    o.data.materials.append(mat(color))
    return o

# ---- moduły (przybliżone; front modułu = -y) -------------------------------
def head(cx, cy, z0, parent=None):
    """Serwo SG90 + HC-SR04 na orczyku. z0 = wierzch obudowy pod serwem."""
    g = empty((cx, cy, z0), name="head")
    if parent: g.parent = parent
    box((23, 12, 23), (0, 0, 11.5), "servo", g, "SG90")
    box((32.5, 12, 2), (0, 0, 17), "servo", g, "SG90-uszy")
    cyl(10, 3, (0, 0, 24.5), "silver", "z", g, "orczyk")
    zc = 24.5 + 1.5 + 11
    box((45, 1.6, 20), (0, -6, zc), "pcb", g, "HC-SR04")
    for sx in (-13, 13):
        cyl(8, 12, (sx, -13, zc), "silver", "y", g, "przetwornik")
    return g

def oled(parent):
    box((35, 4, 33), (0, 0, 0), "oled", parent, "OLED")
    box((29, 0.6, 15), (0, -2.2, 4), "screen", parent, "OLED-ekran")

def matrix(parent):
    box((32, 4, 32), (0, 0, 0), "board", parent, "MAX7219")
    box((32, 0.8, 32), (0, -2.2, 0), "dark", parent, "matryca")
    pitch = 3.6
    for i in range(8):
        for j in range(8):
            cyl(1.3, 0.8, ((i - 3.5) * pitch, -2.9, (j - 3.5) * pitch), "red", "y", parent, "dioda")

def keyholes(cx_list, z, y_back, body_parent=None):
    for cx in cx_list:
        cyl(3.5, 1, (cx, y_back, z), "dark", "y", body_parent, "dziurka-sciana")
        box((3, 1, 8), (cx, y_back, z + 5), "dark", body_parent, "dziurka-sciana")

def usb(loc, side):
    if side == "x":
        box((1, 9, 3.2), loc, "dark", None, "USB-C")
    else:
        box((9, 3.2, 1), loc, "dark", None, "USB-C")

# ---- warianty ----------------------------------------------------------------
def variant_A():
    W, D, H = 170, 90, 30
    box((W, D, H), (0, 0, H / 2), "body", None, "obudowa")
    head(0, 0, H)
    for sx, fn in ((-1, oled), (1, matrix)):
        g = empty((sx * 58, -10, H + 8), (math.radians(-60), 0, 0), "panel")
        # wyświetlacz nachylony do widza, leżący na górnej ściance
        fn(g)
    keyholes((-45, 45), 22, D / 2 + 0.5)
    usb((W / 2, 0, 15), "x")
    return "A"

def variant_B():
    W, D, H = 95, 70, 55
    box((W, D, H), (0, 0, H / 2), "body", None, "obudowa")
    head(0, 0, H)
    g1 = empty((-23, -D / 2, 27), name="oled"); oled(g1)
    g2 = empty((24, -D / 2, 27), name="matrix"); matrix(g2)
    keyholes((-25, 25), 42, D / 2 + 0.5)
    usb((W / 2, 0, 20), "x")
    return "B"

def variant_C():
    W, D = 110, 90
    pts = [(-D / 2, 0), (D / 2, 0), (D / 2, 60), (0, 60), (-D / 2, 15)]
    prism_yz(pts, W, "body", "obudowa")
    ang = math.atan2(60 - 15, D / 2)
    head(0, 22, 60)
    mid_y, mid_z = -D / 4, (60 + 15) / 2
    off = 3
    n = Vector((0, -math.sin(ang), math.cos(ang)))
    for sx, fn in ((-26, oled), (26, matrix)):
        g = empty((sx, mid_y - n.y * -0, mid_z), (ang + math.pi / 2 * 0 - math.pi / 2 + math.pi / 2 * 0, 0, 0), "panel")
        g.rotation_euler = (-(math.pi / 2 - ang), 0, 0)
        g.location = (sx, mid_y + n.y * off, mid_z + n.z * off)
        fn(g)
    keyholes((-30, 30), 40, D / 2 + 0.5)
    usb((W / 2, 20, 18), "x")
    return "C"

def variant_D():
    W, D, H = 60, 45, 150
    box((W, D, H), (0, 0, H / 2), "body", None, "obudowa")
    head(0, 0, H)
    g1 = empty((0, -D / 2, 100), name="oled"); oled(g1)
    g2 = empty((0, -D / 2, 55), name="matrix"); matrix(g2)
    keyholes((0,), 125, D / 2 + 0.5)
    keyholes((0,), 20, D / 2 + 0.5)
    usb((0, -5, 0), "z")
    return "D"

# ---- render ---------------------------------------------------------------
def setup_render(w=1100, h=1100):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "MATERIAL"
    sc.display.shading.show_shadows = False
    sc.display.shading.show_cavity = True
    sc.display.shading.show_object_outline = True
    sc.render.film_transparent = False
    sc.world = bpy.data.worlds.new("w")
    sc.world.color = (1, 1, 1)
    sc.display.render_aa = "8"

def bbox():
    pts = []
    for o in bpy.context.scene.objects:
        if o.type == "MESH" and o.name != "podloga":
            pts += [o.matrix_world @ Vector(c) for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi

def shoot(tag, back=False):
    sc = bpy.context.scene
    bpy.context.view_layer.update()
    lo, hi = bbox()
    c = (lo + hi) / 2
    size = (hi - lo).length
    d = Vector((-0.8, 1, 0.75)) if back else Vector((0.8, -1, 0.75))
    d.normalize()
    cam_d = bpy.data.cameras.new("cam")
    cam_d.type = "ORTHO"
    cam_d.ortho_scale = size * 1.25
    cam_d.clip_end = size * 10
    cam = bpy.data.objects.new("cam", cam_d)
    sc.collection.objects.link(cam)
    cam.location = c + d * size * 3
    tgt = empty(c, name="tgt")
    t = cam.constraints.new("TRACK_TO")
    t.target, t.track_axis, t.up_axis = tgt, "TRACK_NEGATIVE_Z", "UP_Y"
    sc.camera = cam
    sc.render.filepath = os.path.join(OUT, f"{tag}.png")
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)

if __name__ == "__main__":
    for fn in (variant_A, variant_B, variant_C, variant_D):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        _mats.clear()
        setup_render()
        tag = fn()
        shoot(f"wariant-{tag}-front")
        shoot(f"wariant-{tag}-tyl", back=True)
        if tag == "A":
            bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "wariant-A.blend"))
