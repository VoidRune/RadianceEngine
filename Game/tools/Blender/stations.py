import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import hubprops

NAME = 'Stations'


def arch_wall(width, height, depth, opening_width, opening_height, segments=10):
    bm = bmesh.new()
    half = width * 0.5
    ow = opening_width * 0.5
    spring = opening_height - ow
    outline = [(-half, 0.0), (-ow, 0.0)]
    for i in range(segments + 1):
        angle = math.pi - math.pi * i / segments
        outline.append((math.cos(angle) * ow, spring + math.sin(angle) * ow))
    outline += [(ow, 0.0), (half, 0.0), (half, height), (-half, height)]
    front = [bm.verts.new((x, -depth * 0.5, z)) for x, z in outline]
    back = [bm.verts.new((x, depth * 0.5, z)) for x, z in outline]
    count = len(outline)
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    for i in range(count):
        j = (i + 1) % count
        bm.faces.new((front[i], front[j], back[j], back[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=[face for face in bm.faces if len(face.verts) > 4])
    return bm


def forge(m, target, rng):
    parts = []
    hearth = pk.part('Forge_Front', arch_wall(1.9, 0.92, 0.3, 0.62, 0.56), target, m['masonry'], matrix=Matrix.Translation((0.0, -0.6, 0.0)), rng=rng, bevel=(0.03, 3, 30.0))
    parts.append(hearth)
    parts.append(pk.part('Forge_Back', pk.box((1.9, 0.3, 0.92), (0.0, 0.6, 0.46)), target, m['masonry'], rng=rng, bevel=(0.03, 3, 30.0)))
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'Forge_Side{int(side)}', pk.box((0.32, 0.9, 0.92), (side * 0.79, 0.0, 0.46)), target, m['masonry'], rng=rng, bevel=(0.03, 3, 30.0)))
    parts.append(pk.part('Forge_Floor', pk.box((1.3, 0.9, 0.12), (0.0, 0.0, 0.06)), target, m['masonry'], rng=rng))
    parts.append(pk.part('Forge_Slab', pk.chipped((2.04, 1.5, 0.14), rng, chips=3, depth=0.03, center=(0.0, 0.0, 0.99)), target, m['stone'], rng=rng, bevel=(0.03, 3, 30.0)))
    pit = pk.lathe([(0.52, 1.06), (0.5, 1.16), (0.46, 1.18), (0.4, 1.12), (0.0, 1.1)], 14)
    parts.append(pk.part('Forge_Pit', pit, target, m['stone'], matrix=Matrix.Translation((-0.25, -0.05, 0.0)), rng=rng, bevel=(0.02, 2, 30.0)))
    parts += hubprops.coals('Forge_Coal', (-0.25, -0.05, 1.1), 0.4, 0.09, m, target, rng, 16)
    parts += hubprops.coals('Forge_Firebox', (0.0, -0.3, 0.12), 0.32, 0.12, m, target, rng, 12)
    chimney = pk.lathe([(0.0, 0.0), (0.42, 0.0), (0.42, 1.0), (0.32, 1.6), (0.3, 2.2), (0.0, 2.2)], 4, math.pi / 4.0)
    parts.append(pk.part('Forge_Chimney', chimney, target, m['masonry'], matrix=Matrix.Translation((0.45, 0.45, 1.06)), rng=rng, bevel=(0.03, 3, 30.0)))
    parts.append(pk.part('Forge_ChimneyCap', pk.chipped((0.62, 0.62, 0.1), rng, chips=2, depth=0.02, center=(0.45, 0.45, 3.3)), target, m['stone'], rng=rng, bevel=(0.025, 2, 30.0)))
    hood = pk.lathe([(0.0, 1.55), (0.62, 1.55), (0.62, 1.6), (0.3, 2.05), (0.0, 2.05)], 4, math.pi / 4.0)
    parts.append(pk.part('Forge_Hood', hood, target, m['iron'], matrix=Matrix.Translation((-0.25, -0.05, 0.0)), rng=rng, bevel=(0.01, 2, 30.0)))
    parts.append(pk.part('Forge_Flue', pk.cylinder(0.11, 0.7, 10, z=2.0), target, m['iron'], matrix=Matrix.Translation((-0.25, -0.05, 0.0)), rng=rng, smooth=40.0))
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'Forge_Strut{int(side)}', pk.rod((-0.25 + side * 0.5, -0.05 - 0.45, 1.12), (-0.25 + side * 0.55, -0.05 - 0.5, 1.58), 0.018, 6), target, m['iron'], rng=rng, smooth=40.0))
    bellows_base = Matrix.Translation((1.18, 0.05, 0.5)) @ Matrix.Rotation(-0.15, 4, 'Y')
    for k, (dz, scale) in enumerate(((0.0, 1.0), (0.18, 0.92))):
        board = pk.part(f'Forge_BellowsBoard{k}', pk.box((0.6, 0.42, 0.035)), target, m['wood'], rng=rng, bevel=(0.01, 2, 30.0))
        board.matrix_world = bellows_base @ Matrix.Translation((0.0, 0.0, dz)) @ Matrix.Rotation(0.12 * k, 4, 'Y') @ Matrix.Scale(scale, 4)
        parts.append(board)
    pleats = pk.lathe([(0.2, 0.0), (0.24, 0.04), (0.19, 0.07), (0.23, 0.11), (0.18, 0.14), (0.16, 0.17)], 12)
    bellows = pk.part('Forge_Bellows', pleats, target, m['leather'], rng=rng, smooth=30.0)
    bellows.matrix_world = bellows_base @ Matrix.Translation((0.05, 0.0, 0.02)) @ Matrix.Diagonal((1.3, 1.0, 1.0, 1.0))
    parts.append(bellows)
    parts.append(pk.part('Forge_Nozzle', pk.rod((0.85, 0.05, 0.52), (0.78, 0.05, 0.62), 0.03, 8, end_radius=0.018), target, m['iron'], rng=rng, smooth=40.0))
    for k in range(3):
        tool = pk.part(f'Forge_Tool{k}', pk.rod((-0.98, -0.3 + k * 0.2, 0.98), (-1.0, -0.32 + k * 0.2, 0.35), 0.012, 6), target, m['iron'], rng=rng, smooth=40.0)
        parts.append(tool)
    parts.append(pk.part('Forge_Rail', pk.rod((-0.99, -0.42, 0.95), (-0.99, 0.22, 0.95), 0.015, 6), target, m['iron'], rng=rng, smooth=40.0))
    return kit.join(parts, 'Forge')


def anvil(m, target, rng):
    parts = []
    stump = pk.lathe([(0.3, 0.0), (0.31, 0.03), (0.28, 0.08), (0.27, 0.36), (0.26, 0.4), (0.0, 0.41)], 16)
    stump_obj = pk.part('Anvil_Stump', stump, target, m['bark'], rng=rng, smooth=35.0)
    pk.tag(stump_obj)
    parts.append(stump_obj)
    parts.append(pk.part('Anvil_Feet', pk.box((0.34, 0.24, 0.07), (0.0, 0.0, 0.445)), target, m['iron'], rng=rng, bevel=(0.012, 2, 30.0)))
    waist = pk.lathe([(0.11, 0.48), (0.075, 0.52), (0.07, 0.58), (0.1, 0.62)], 4, math.pi / 4.0)
    parts.append(pk.part('Anvil_Waist', waist, target, m['iron'], matrix=Matrix.Diagonal((1.5, 1.0, 1.0, 1.0)), rng=rng, bevel=(0.01, 2, 30.0)))
    parts.append(pk.part('Anvil_Body', pk.box((0.58, 0.18, 0.13), (-0.04, 0.0, 0.685)), target, m['iron'], rng=rng, bevel=(0.012, 2, 30.0)))
    horn = pk.cone_bm((0.25, 0.0, 0.69), (0.5, 0.0, 0.72), 0.075, sides=12)
    parts.append(pk.part('Anvil_Horn', horn, target, m['iron'], rng=rng, smooth=40.0))
    parts.append(pk.part('Anvil_Heel', pk.box((0.14, 0.14, 0.08), (-0.38, 0.0, 0.71)), target, m['iron'], rng=rng, bevel=(0.01, 2, 30.0)))
    hammer_head = pk.part('Anvil_HammerHead', pk.box((0.12, 0.05, 0.05), (0.0, 0.0, 0.0)), target, m['iron'], rng=rng, bevel=(0.008, 1, 30.0))
    hammer_head.matrix_world = Matrix.Translation((-0.1, 0.05, 0.78)) @ Matrix.Rotation(0.5, 4, 'Z')
    parts.append(hammer_head)
    handle = pk.part('Anvil_Handle', pk.rod((0.0, 0.0, 0.0), (0.0, 0.32, 0.0), 0.014, 6), target, m['wood'], rng=rng, smooth=40.0)
    handle.matrix_world = Matrix.Translation((-0.1, 0.05, 0.78)) @ Matrix.Rotation(0.5, 4, 'Z')
    parts.append(handle)
    return kit.join(parts, 'Anvil')


def crystal_shard(radius, height, sides=6, tip=0.35, rng=None):
    ring = []
    bm = bmesh.new()
    for i in range(sides):
        angle = 2.0 * math.pi * i / sides + (rng.uniform(-0.08, 0.08) if rng else 0.0)
        r = radius * (rng.uniform(0.88, 1.08) if rng else 1.0)
        ring.append((math.cos(angle) * r, math.sin(angle) * r))
    bottom = [bm.verts.new((x, y, -0.05)) for x, y in ring]
    top = [bm.verts.new((x, y, height * (1.0 - tip))) for x, y in ring]
    apex = bm.verts.new((radius * 0.08, -radius * 0.05, height))
    bm.faces.new(list(reversed(bottom)))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
        bm.faces.new((top[i], top[j], apex))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def gem_altar(m, target, rng):
    parts = [pk.part('Altar_Base', pk.lathe([(hubprops.octagon(0.6), 0.0), (hubprops.octagon(0.6), 0.14), (hubprops.octagon(0.52), 0.2), (hubprops.octagon(0.5), 0.24)], 8, math.pi / 8.0), target, m['stone'], rng=rng, bevel=(0.03, 3, 30.0))]
    column = pk.lathe([(hubprops.octagon(0.3), 0.24), (hubprops.octagon(0.27), 0.32), (hubprops.octagon(0.24), 0.9), (hubprops.octagon(0.3), 0.98), (hubprops.octagon(0.42), 1.04), (hubprops.octagon(0.46), 1.12)], 8, math.pi / 8.0)
    parts.append(pk.part('Altar_Column', column, target, m['stone'], rng=rng, bevel=(0.025, 2, 30.0)))
    dish = pk.lathe([(0.0, 1.1), (0.46, 1.12), (0.48, 1.16), (0.4, 1.18), (0.0, 1.15)], 16)
    parts.append(pk.part('Altar_Dish', dish, target, m['stone'], rng=rng, smooth=40.0))
    for k in range(3):
        angle = k * 2.0 * math.pi / 3.0 + 0.3
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        parts.append(pk.tube([out * 0.36 + Vector((0.0, 0.0, 1.15)), out * 0.3 + Vector((0.0, 0.0, 1.32)), out * 0.16 + Vector((0.0, 0.0, 1.46)), out * 0.06 + Vector((0.0, 0.0, 1.5))], 0.016, target, m['brass'], f'Altar_Claw{k}', resolution=6, sides=6, rng=rng))
    gem = pk.part('Altar_Gem', pk.lathe([(0.0, 1.28), (0.14, 1.44), (0.13, 1.56), (0.0, 1.8)], 6), target, m['crystal'], rng=rng)
    parts.append(gem)
    for k in range(5):
        angle = k * 2.0 * math.pi / 5.0 + rng.uniform(-0.3, 0.3)
        shard = pk.part(f'Altar_Shard{k}', crystal_shard(rng.uniform(0.035, 0.055), rng.uniform(0.12, 0.22), rng=rng), target, m['crystal'], rng=rng)
        shard.matrix_world = Matrix.Translation((math.cos(angle) * 0.3, math.sin(angle) * 0.3, 1.14)) @ Matrix.Rotation(rng.uniform(-0.4, 0.4), 4, 'X') @ Matrix.Rotation(rng.uniform(-0.4, 0.4), 4, 'Y')
        parts.append(shard)
    return kit.join(parts, 'GemAltar')


def mission_board(m, target, rng):
    parts = []
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'Mission_Post{int(side)}', pk.chipped((0.15, 0.15, 2.35), rng, chips=2, depth=0.015, center=(side * 1.12, 0.0, 1.175)), target, m['wood'], rng=rng, bevel=(0.018, 2, 30.0)))
    parts.append(pk.part('Mission_Back', pk.box((2.1, 0.05, 1.3), (0.0, 0.025, 1.4)), target, m['wood'], rng=rng, bevel=(0.012, 2, 30.0)))
    for k, (size, center) in enumerate((((2.26, 0.09, 0.09), (0.0, 0.0, 2.08)), ((2.26, 0.09, 0.09), (0.0, 0.0, 0.72)), ((0.09, 0.09, 1.3), (-1.06, 0.0, 1.4)), ((0.09, 0.09, 1.3), (1.06, 0.0, 1.4)))):
        parts.append(pk.part(f'Mission_Frame{k}', pk.chipped(size, rng, chips=1, depth=0.012, center=center), target, m['wood'], rng=rng, bevel=(0.014, 2, 30.0)))
    for side in (-1.0, 1.0):
        slope = Matrix.Translation((0.0, side * 0.21, 2.46)) @ Matrix.Rotation(-side * 0.55, 4, 'X')
        parts.append(pk.part(f'Mission_Roof{int(side)}', pk.box((2.6, 0.5, 0.04)), target, m['wood'], matrix=slope, rng=rng, bevel=(0.012, 2, 30.0)))
    parts.append(pk.part('Mission_Ridge', pk.box((2.66, 0.08, 0.07), (0.0, 0.0, 2.58)), target, m['wood'], rng=rng, bevel=(0.014, 2, 30.0)))
    for face, side in enumerate((-1.0, 1.0)):
        flip = Matrix.Rotation(0.0 if side < 0.0 else math.pi, 4, 'Z')
        board_y = -0.006 if side < 0.0 else 0.056
        map_sheet = pk.part(f'Mission_Map{face}', pk.box((0.82, 0.004, 0.6)), target, m['map'], rng=rng)
        map_sheet.matrix_world = flip @ Matrix.Translation((-0.4, board_y if side < 0.0 else -board_y, 1.55)) @ Matrix.Rotation(0.03, 4, 'Y')
        parts.append(map_sheet)
        sheets = [((0.35, 1.7), 0.3, 0.36, 0.06), ((0.72, 1.62), 0.26, 0.32, -0.08), ((0.5, 1.15), 0.34, 0.28, 0.04), ((-0.5, 1.0), 0.3, 0.24, -0.05), ((-0.05, 1.02), 0.24, 0.3, 0.09), ((0.85, 1.1), 0.22, 0.26, -0.03)]
        for k, ((x, z), w, h, roll) in enumerate(sheets):
            sheet = pk.part(f'Mission_Sheet{face}_{k}', pk.box((w, 0.004, h)), target, m['paper'], rng=rng)
            depth = -(abs(board_y) + 0.002 + 0.002 * k) if side < 0.0 else -(board_y + 0.002 + 0.002 * k)
            sheet.matrix_world = flip @ Matrix.Translation((x, depth, z)) @ Matrix.Rotation(roll, 4, 'Y')
            parts.append(sheet)
        pins = [(-0.78, 1.82), (-0.02, 1.82), (-0.78, 1.28), (-0.02, 1.28), (0.35, 1.86), (0.72, 1.76), (0.5, 1.27), (-0.5, 1.1), (-0.05, 1.15), (0.85, 1.21)]
        lift = -0.024 if side < 0.0 else -(board_y + 0.02)
        for k, (x, z) in enumerate(pins):
            pin = pk.part(f'Mission_Pin{face}_{k}', pk.ellipsoid_bm((x, lift, z), (0.014, 0.012, 0.014), segments=8, rings=6), target, m['red'], rng=rng, smooth=60.0)
            pin.matrix_world = flip
            parts.append(pin)
        string = [(-0.55, lift - 0.002, 1.7), (-0.2, lift - 0.006, 1.45), (0.35, lift - 0.006, 1.86), (0.72, lift - 0.006, 1.76)]
        rope = pk.tube(string, 0.004, target, m['red'], f'Mission_String{face}', resolution=1, sides=4, rng=rng)
        rope.matrix_world = flip
        parts.append(rope)
    parts.append(pk.part('Mission_Lantern', pk.box((0.14, 0.14, 0.2), (0.0, -0.08, 2.24)), target, m['glass'], rng=rng))
    return kit.join(parts, 'MissionBoard')


def lectern(m, target, rng):
    parts = [pk.part('Lectern_Foot', pk.chipped((0.5, 0.4, 0.08), rng, chips=2, depth=0.015, center=(0.0, 0.0, 0.04)), target, m['wood'], rng=rng, bevel=(0.015, 2, 30.0))]
    parts.append(pk.part('Lectern_Post', pk.lathe([(0.09, 0.08), (0.07, 0.14), (0.06, 0.4), (0.075, 0.48), (0.06, 0.56), (0.06, 0.86), (0.09, 0.92)], 8, math.pi / 8.0), target, m['wood'], rng=rng, bevel=(0.008, 1, 30.0)))
    desk = pk.box((0.62, 0.46, 0.05))
    desk_obj = pk.part('Lectern_Desk', desk, target, m['wood'], rng=rng, bevel=(0.015, 2, 30.0))
    tilt = Matrix.Translation((0.0, 0.02, 1.0)) @ Matrix.Rotation(-0.42, 4, 'X')
    desk_obj.matrix_world = tilt
    parts.append(desk_obj)
    lip = pk.part('Lectern_Lip', pk.box((0.62, 0.035, 0.05), (0.0, -0.235, 0.03)), target, m['wood'], rng=rng, bevel=(0.01, 1, 30.0))
    lip.matrix_world = tilt
    parts.append(lip)
    parts.append(pk.part('Lectern_Brace', pk.box((0.4, 0.06, 0.14), (0.0, 0.03, 0.9)), target, m['wood'], rng=rng, bevel=(0.012, 2, 30.0)))
    for side in (-1.0, 1.0):
        cover = pk.part(f'Lectern_Cover{int(side)}', pk.box((0.26, 0.34, 0.012), (side * 0.13, 0.0, 0.0)), target, m['leather'], rng=rng, bevel=(0.004, 1, 30.0))
        cover.matrix_world = tilt @ Matrix.Translation((0.0, 0.0, 0.032)) @ Matrix.Rotation(side * -0.08, 4, 'Y')
        parts.append(cover)
        block = pk.part(f'Lectern_Pages{int(side)}', pk.box((0.23, 0.31, 0.03), (side * 0.12, 0.0, 0.0)), target, m['paper_plain'], rng=rng, bevel=(0.004, 1, 30.0))
        block.matrix_world = tilt @ Matrix.Translation((0.0, 0.0, 0.052)) @ Matrix.Rotation(side * -0.12, 4, 'Y')
        parts.append(block)
        page = pk.part(f'Lectern_Page{int(side)}', pk.box((0.22, 0.3, 0.002), (side * 0.12, 0.0, 0.0)), target, m['runes'], rng=rng)
        page.matrix_world = tilt @ Matrix.Translation((0.0, 0.0, 0.068)) @ Matrix.Rotation(side * -0.12, 4, 'Y')
        parts.append(page)
    parts.append(pk.tube([(0.0, -0.15, 0.0), (0.02, -0.22, -0.02), (0.01, -0.3, -0.1)], 0.006, target, m['red'], 'Lectern_Ribbon', resolution=4, sides=4, rng=rng))
    parts[-1].matrix_world = tilt @ Matrix.Translation((0.0, 0.0, 0.07))
    return kit.join(parts, 'Lectern')


def build(target):
    rng = random.Random(31)
    m = {
        'iron': pk.iron_material('Iron', bake_size=1024),
        'wood': pk.wood_material('Wood', light=(0.27, 0.185, 0.11), dark=(0.12, 0.08, 0.05), weathered=0.4, bake_size=2048),
        'stone': pk.stone_material('Stone', bake_size=2048),
        'masonry': pk.masonry_material('Masonry', light=(0.29, 0.265, 0.235), dark=(0.13, 0.12, 0.105), soot=0.85, bake_size=2048),
        'ember': pk.ember_material('Ember'),
        'leather': pk.leather_material('Leather'),
        'bark': pk.bark_material('Bark', bake_size=512),
        'brass': pk.brass_material('Brass', bake_size=512),
        'crystal': pk.crystal_material('Crystal', core=(0.62, 0.38, 1.0), strength=3.0),
        'paper': pk.paper_material('Paper'),
        'paper_plain': pk.paper_material('PageEdges', tone=(0.8, 0.74, 0.6), lines=False, bake_size=256),
        'map': pk.map_material('Map'),
        'runes': pk.rune_page_material('Runes'),
        'red': pk.rubber_material('RedCloth', tone=(0.5, 0.05, 0.04), bake_size=128, roughness=0.6),
        'glass': pk.glow_material('LanternGlass', color=(1.0, 0.66, 0.3), strength=3.0, base=(0.9, 0.66, 0.36)),
    }
    return [forge(m, target, rng), anvil(m, target, rng), gem_altar(m, target, rng), mission_board(m, target, rng), lectern(m, target, rng)]


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    pk.unwrap_all(objects, {'Crystal': 0.01, 'RedCloth': 0.02, 'LanternGlass': 0.02, 'Runes': 0.01, 'PageEdges': 0.01})
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.4)
    if args.get('--preview'):
        pk.ground_preview(12.0)
        for index, obj in enumerate(objects):
            obj.matrix_world = Matrix.Translation(((index % 3) * 3.0 - 3.0, (index // 3) * 3.0 - 1.5, 0.0))
        pk.preview(objects[:3], args['--preview'], 'stations_a', fill=0.95)
        pk.preview(objects[3:], args['--preview'], 'stations_b', fill=0.95)


if __name__ == '__main__':
    main()
