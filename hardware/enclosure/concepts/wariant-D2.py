"""Wariant D2: opływowa kolumna wsuwana w osobną podstawkę (Blender, mm).
Wymiary modułów przybliżone (nie zmierzone). Uruchomienie:
  flatpak run --filesystem=host org.blender.Blender --background --python "$PWD/wariant-D2.py"
"""
import bpy, os, sys, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import concepts as C

def rbox(size, loc, r, color, name):
    o = C.box(size, loc, color, None, name)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(scale=True)
    m = o.modifiers.new("bevel", "BEVEL")
    m.width, m.segments, m.limit_method = r, 10, "NONE"
    bpy.ops.object.modifier_apply(modifier="bevel")
    bpy.ops.object.shade_smooth()
    return o

def build(explode=0):
    # kolumna: przekrój 70x50, wysokość 155, dolne 25 mm wsunięte w podstawkę
    W, D, H, R = 70, 50, 155, 14
    BASE_H, INS = 28, 25
    z0 = BASE_H - INS + explode
    col = rbox((W, D, H), (0, 0, z0 + H / 2), R, "body", "kolumna")
    top = z0 + H
    C.head(0, 0, top)
    g1 = C.empty((0, -D / 2, z0 + 105), name="oled"); C.oled(g1)
    g2 = C.empty((0, -D / 2, z0 + 62), name="matrix"); C.matrix(g2)
    C.keyholes((0,), z0 + 128, D / 2 + 0.5)
    C.keyholes((0,), z0 + 48, D / 2 + 0.5)
    C.box((1, 9, 3.2), (W / 2, 0, z0 + 48), "dark", None, "USB-C")
    # podstawka: szeroka, z przedłużeniem do przodu, kieszeń na kolumnę
    base = rbox((120, 110, BASE_H), (0, -14, BASE_H / 2), 12, "silver", "podstawka")
    C.box((W + 3, D + 3, 0.6), (0, 0, BASE_H + 0.1), "dark", None, "kieszen")

def run(tag, explode=0, views=("front", "tyl")):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    C._mats.clear()
    C.setup_render()
    build(explode)
    for v in views:
        C.shoot(f"wariant-D2-{tag}-{v}", back=(v == "tyl"))
    return

if __name__ == "__main__":
    run("zlozony")
    run("rozlozony", explode=75, views=("front",))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(C.OUT, "wariant-D2.blend"))
