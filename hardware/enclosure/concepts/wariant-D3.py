"""Wariant D3: kolumna ze ściętymi krawędziami, dwa kolory, wsuwana w podstawkę.
Render Cycles (CPU). Wymiary modułów przybliżone (nie zmierzone).
  flatpak run --filesystem=host org.blender.Blender --background --python "$PWD/wariant-D3.py"
"""
import bpy, math, os
from mathutils import Vector

OUT = os.path.dirname(os.path.abspath(__file__))
MATS = {}

def pm(name, color, metal=0.0, rough=0.5, emit=0.0):
    if name in MATS: return MATS[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = color
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if emit:
        b.inputs["Emission Color"].default_value = color
        b.inputs["Emission Strength"].default_value = emit
    MATS[name] = m
    return m

def charcoal(): return pm("grafit", (0.075, 0.08, 0.085, 1), 0.0, 0.55)
def teal():     return pm("morski", (0.02, 0.17, 0.25, 1), 0.2, 0.42)
def silver():   return pm("srebro", (0.75, 0.76, 0.78, 1), 1.0, 0.28)
def black():    return pm("czern", (0.005, 0.005, 0.006, 1), 0.0, 0.25)
def red():      return pm("dioda", (0.9, 0.04, 0.005, 1), 0.0, 0.4, emit=1.8)

def box(size, loc, material, chamfer=0.0, name="box", rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot)
    o = bpy.context.active_object
    o.scale, o.name = size, name
    bpy.ops.object.transform_apply(scale=True)
    if chamfer:
        m = o.modifiers.new("b", "BEVEL")
        m.width, m.segments, m.limit_method = chamfer, 1, "NONE"
        bpy.ops.object.modifier_apply(modifier="b")
    o.data.materials.append(material)
    return o

def cyl(r, h, loc, material, axis="z", name="cyl"):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=h, location=loc, vertices=48)
    o = bpy.context.active_object
    if axis == "y": o.rotation_euler = (math.pi / 2, 0, 0)
    o.name = name
    o.data.materials.append(material)
    bpy.ops.object.shade_smooth()
    return o

def build(explode=0):
    W, D, H = 70, 50, 155
    BASE_H, INS = 28, 25
    z0 = BASE_H - INS + explode
    zt = z0 + H
    box((W, D, H), (0, 0, z0 + H / 2), charcoal(), 6, "kolumna")
    # szczelina podziału przód/tył na prawym boku
    box((0.6, 1.0, H - 22), (W / 2 + 0.02, 6, z0 + H / 2), black(), 0, "szczelina")
    # kołnierz morski przy podstawce
    box((W + 5, D + 5, 9), (0, 0, z0 + 4.5), teal(), 2, "kolnierz")
    # śrubki w narożnikach przedniej ścianki
    for sx in (-1, 1):
        for zz in (z0 + 22, zt - 9):
            cyl(2.0, 1.2, (sx * (W / 2 - 7), -D / 2 - 0.2, zz), black(), "y", "srubka")
    # OLED: ramka morska + ekran
    zo = z0 + 108
    box((41, 3, 30), (0, -D / 2 - 0.6, zo), teal(), 2, "oled-ramka")
    box((30, 0.8, 15.5), (0, -D / 2 - 2.2, zo), black(), 0, "oled-ekran")
    # matryca: czarna ramka + diody
    zm = z0 + 66
    box((38, 3, 38), (0, -D / 2 - 0.6, zm), black(), 2, "matryca-ramka")
    for i in range(8):
        for j in range(8):
            cyl(1.35, 0.8, ((i - 3.5) * 3.6, -D / 2 - 2.4, zm + (j - 3.5) * 3.6), red(), "y", "dioda")
    # USB-C na prawym boku
    box((1.2, 9.5, 3.4), (W / 2, -2, z0 + 48), black(), 0, "USB-C")
    # dziurki z tyłu
    for zz in (z0 + 128, z0 + 48):
        cyl(3.5, 1, (0, D / 2 + 0.2, zz), black(), "y", "dziurka")
        box((3, 1, 8), (0, D / 2 + 0.2, zz + 5), black(), 0, "dziurka")
    # głowica: szyjka serwa + korpus sonaru
    box((30, 26, 22), (0, 0, zt + 11), teal(), 3, "szyjka")
    zh = zt + 22 + 13
    box((60, 28, 26), (0, 0, zh), charcoal(), 5, "glowica")
    box((52, 2, 20), (0, -14.5, zh), teal(), 2, "glowica-rama")
    for sx in (-14, 14):
        cyl(8.8, 7, (sx, -16.5, zh), silver(), "y", "przetwornik")
        cyl(6.8, 7.6, (sx, -16.6, zh), black(), "y", "siatka")
    box((6, 1.2, 6), (0, -15.8, zh), black(), 0, "czujnik")
    # podstawka
    box((120, 110, BASE_H), (0, -14, BASE_H / 2), charcoal(), 9, "podstawka")
    box((W + 16, D + 16, 1.2), (0, 0, BASE_H + 0.2), pm("plyta", (0.015, 0.016, 0.018, 1), 0.2, 0.4), 1, "plyta")
    for sx in (-1, 1):
        for sy in (-1, 1):
            cyl(2.2, 1.2, (sx * (W / 2 + 4), sy * (D / 2 + 4), BASE_H + 0.9), black(), "z", "srubka")

def scene(w=1100, h=1100, samples=48):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = samples
    sc.cycles.use_denoising = True
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.view_settings.view_transform = "Standard"
    sc.world = bpy.data.worlds.new("w")
    sc.world.use_nodes = True
    bg = sc.world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.55, 0.57, 0.6, 1)
    bg.inputs["Strength"].default_value = 1.1
    bpy.ops.mesh.primitive_plane_add(size=40000, location=(0, 0, 0))
    fl = bpy.context.active_object
    fl.data.materials.append(pm("podloze", (0.5, 0.52, 0.55, 1), 0, 0.8))
    for loc, en, ang in (((-1, -1, 2), 1.6, 0.35), ((1.5, -0.5, 1), 0.9, 0.6)):
        bpy.ops.object.light_add(type="SUN")
        s = bpy.context.active_object
        s.data.energy, s.data.angle = en, ang
        s.rotation_euler = Vector(loc).to_track_quat("-Z", "Y").to_euler()

def camera(back=False, c=Vector((0, -8, 100)), dist=430):
    sc = bpy.context.scene
    cd = bpy.data.cameras.new("cam")
    cd.lens, cd.clip_end = 62, 5000
    cam = bpy.data.objects.new("cam", cd)
    sc.collection.objects.link(cam)
    d = Vector((-0.55, 0.8, 0.45)) if back else Vector((0.62, -0.78, 0.42))
    cam.location = c + d.normalized() * dist
    cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam

if __name__ == "__main__":
    import sys
    for tag, back, ex in (("front", False, 0),):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        MATS.clear()
        scene()
        build(ex)
        camera(back)
        bpy.context.scene.render.filepath = os.path.join(OUT, f"wariant-D3-{tag}.png")
        bpy.ops.render.render(write_still=True)
        if tag == "front":
            bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "wariant-D3.blend"))
