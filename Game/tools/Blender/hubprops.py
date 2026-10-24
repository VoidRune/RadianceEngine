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

NAME = 'HubProps'


def octagon(apothem):
    return apothem / math.cos(math.pi / 8.0)


def lamp_post(m, target, rng):
    parts = []
    foot = pk.part('Lamp_Foot', pk.lathe([(octagon(0.17), 0.0), (octagon(0.17), 0.05), (octagon(0.15), 0.07), (octagon(0.11), 0.1), (octagon(0.085), 0.2), (octagon(0.07), 0.32), (octagon(0.075), 0.34), (octagon(0.06), 0.36)], 8, math.pi / 8.0), target, m['iron'], rng=rng, bevel=(0.008, 2, 30.0))
    parts.append(foot)
    shaft = pk.part('Lamp_Shaft', pk.lathe([(0.048, 0.35), (0.045, 1.2), (0.042, 2.24), (0.06, 2.27), (0.06, 2.3), (0.05, 2.33), (0.05, 2.36)], 12), target, m['iron'], rng=rng, smooth=40.0)
    parts.append(shaft)
    for z in (0.62, 1.4):
        parts.append(pk.part(f'Lamp_Ring{z}', pk.torus(0.05, 0.012, 16, 6), target, m['iron'], matrix=Matrix.Translation((0.0, 0.0, z)), rng=rng, smooth=50.0))
    for k in range(4):
        angle = k * math.pi * 0.5 + math.pi * 0.25
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        parts.append(pk.tube([out * 0.05 + Vector((0.0, 0.0, 2.08)), out * 0.12 + Vector((0.0, 0.0, 2.22)), out * 0.13 + Vector((0.0, 0.0, 2.33)), out * 0.1 + Vector((0.0, 0.0, 2.38))], 0.011, target, m['iron'], f'Lamp_Scroll{k}', resolution=6, sides=6, rng=rng))
    parts.append(pk.part('Lamp_Plate', pk.lathe([(0.17, 2.36), (0.17, 2.385), (0.15, 2.4), (0.0, 2.4)], 4, math.pi / 4.0), target, m['iron'], rng=rng, bevel=(0.005, 1, 30.0)))
    half = 0.105
    for k in range(4):
        sx, sy = (1, 1, -1, -1)[k], (1, -1, -1, 1)[k]
        parts.append(pk.part(f'Lamp_Bar{k}', pk.box((0.022, 0.022, 0.38), (sx * half, sy * half, 2.59)), target, m['iron'], rng=rng, bevel=(0.004, 1, 30.0)))
    glass = pk.part('Lamp_Glass', pk.box((2.0 * half - 0.01, 2.0 * half - 0.01, 0.34), (0.0, 0.0, 2.59)), target, m['glass'], rng=rng)
    parts.append(glass)
    for z in (2.41, 2.775):
        parts.append(pk.part(f'Lamp_Frame{z}', pk.box((2.0 * half + 0.04, 2.0 * half + 0.04, 0.025), (0.0, 0.0, z)), target, m['iron'], rng=rng, bevel=(0.005, 1, 30.0)))
    roof = pk.part('Lamp_Roof', pk.lathe([(0.19, 2.785), (0.19, 2.8), (0.05, 2.93), (0.03, 2.95), (0.0, 2.95)], 4, math.pi / 4.0), target, m['iron'], rng=rng, bevel=(0.006, 1, 30.0))
    parts.append(roof)
    parts.append(pk.part('Lamp_Finial', pk.lathe([(0.025, 2.94), (0.03, 2.97), (0.0, 3.03)], 8), target, m['iron'], rng=rng, smooth=50.0))
    parts.append(pk.part('Lamp_Ring', pk.torus(0.035, 0.009, 12, 6), target, m['iron'], matrix=Matrix.Translation((0.0, 0.0, 3.07)) @ Matrix.Rotation(math.pi * 0.5, 4, 'X'), rng=rng, smooth=50.0))
    return kit.join(parts, 'LampPost')


def bench(m, target, rng):
    parts = []
    for side in (-1.0, 1.0):
        leg = pk.part(f'Bench_Leg{int(side)}', pk.chipped((0.13, 0.42, 0.36), rng, chips=2, depth=0.02, center=(side * 0.54, 0.0, 0.18)), target, m['stone'], rng=rng, bevel=(0.025, 3, 30.0))
        parts.append(leg)
        cap = pk.part(f'Bench_Cap{int(side)}', pk.box((0.16, 0.44, 0.03), (side * 0.54, 0.0, 0.375)), target, m['stone'], rng=rng, bevel=(0.012, 2, 30.0))
        parts.append(cap)
    for k, y in enumerate((-0.135, 0.0, 0.135)):
        frame = Matrix.Translation((rng.uniform(-0.01, 0.01), y, 0.415)) @ Matrix.Rotation(rng.uniform(-0.01, 0.01), 4, 'Z')
        plank = pk.part(f'Bench_Plank{k}', pk.chipped((1.46 + rng.uniform(-0.02, 0.02), 0.125, 0.046), rng, chips=1, depth=0.01), target, m['wood'], rng=rng, bevel=(0.012, 2, 30.0))
        plank.matrix_world = frame
        parts.append(plank)
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'Bench_Strap{int(side)}', pk.box((0.05, 0.43, 0.012), (side * 0.54, 0.0, 0.444)), target, m['iron'], rng=rng, bevel=(0.003, 1, 30.0)))
    return kit.join(parts, 'Bench')


def arrow(length, height, thickness, point):
    bm = bmesh.new()
    h = height * 0.5
    outline = [(0.0, -h), (length - point, -h), (length, 0.0), (length - point, h), (0.0, h)]
    bottom = [bm.verts.new((x, -thickness * 0.5, z)) for x, z in outline]
    top = [bm.verts.new((x, thickness * 0.5, z)) for x, z in outline]
    bm.faces.new(bottom)
    bm.faces.new(list(reversed(top)))
    for i in range(len(outline)):
        j = (i + 1) % len(outline)
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def signpost(m, target, rng):
    parts = [pk.part('Sign_Post', pk.chipped((0.09, 0.09, 1.86), rng, chips=2, depth=0.012, center=(0.0, 0.0, 0.93)), target, m['wood'], rng=rng, bevel=(0.012, 2, 30.0))]
    parts.append(pk.part('Sign_Cap', pk.lathe([(octagon(0.065), 1.86), (octagon(0.065), 1.88), (0.0, 1.95)], 4, math.pi / 4.0), target, m['wood'], rng=rng, bevel=(0.006, 1, 30.0)))
    for k, (z, yaw, length) in enumerate(((1.48, 0.15, 0.68), (1.2, math.pi - 0.25, 0.6), (0.96, 0.6, 0.5))):
        board = pk.part(f'Sign_Board{k}', arrow(length, 0.17, 0.035, 0.12), target, m['wood'], rng=rng, bevel=(0.008, 1, 30.0))
        board.matrix_world = Matrix.Rotation(yaw, 4, 'Z') @ Matrix.Translation((0.03, 0.0, z)) @ Matrix.Rotation(rng.uniform(-0.04, 0.04), 4, 'Y')
        parts.append(board)
        nail = pk.part(f'Sign_Nail{k}', pk.cylinder(0.012, 0.012, 6, z=0.0), target, m['iron'], rng=rng, smooth=40.0)
        nail.matrix_world = Matrix.Rotation(yaw, 4, 'Z') @ Matrix.Translation((0.08, -0.02, z)) @ Matrix.Rotation(math.pi * 0.5, 4, 'X')
        parts.append(nail)
    return kit.join(parts, 'Signpost')


def notice_board(m, target, rng):
    parts = []
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'Notice_Post{int(side)}', pk.chipped((0.1, 0.1, 2.15), rng, chips=2, depth=0.012, center=(side * 0.68, 0.0, 1.075)), target, m['wood'], rng=rng, bevel=(0.014, 2, 30.0)))
    parts.append(pk.part('Notice_Back', pk.box((1.26, 0.04, 0.92), (0.0, 0.02, 1.36)), target, m['wood'], rng=rng, bevel=(0.01, 2, 30.0)))
    for k, (size, center) in enumerate((((1.4, 0.07, 0.07), (0.0, 0.0, 1.85)), ((1.4, 0.07, 0.07), (0.0, 0.0, 0.88)), ((0.07, 0.07, 1.0), (-0.66, 0.0, 1.365)), ((0.07, 0.07, 1.0), (0.66, 0.0, 1.365)))):
        parts.append(pk.part(f'Notice_Frame{k}', pk.chipped(size, rng, chips=1, depth=0.01, center=center), target, m['wood'], rng=rng, bevel=(0.012, 2, 30.0)))
    for side in (-1.0, 1.0):
        slope = Matrix.Translation((0.0, side * 0.17, 2.24)) @ Matrix.Rotation(-side * 0.5, 4, 'X')
        parts.append(pk.part(f'Notice_Roof{int(side)}', pk.box((1.62, 0.4, 0.035)), target, m['wood'], matrix=slope, rng=rng, bevel=(0.01, 2, 30.0)))
    parts.append(pk.part('Notice_Ridge', pk.box((1.66, 0.07, 0.06), (0.0, 0.0, 2.33)), target, m['wood'], rng=rng, bevel=(0.012, 2, 30.0)))
    sheets = [((-0.36, 1.52), 0.36, 0.42, -0.06), ((0.12, 1.58), 0.3, 0.36, 0.05), ((0.42, 1.32), 0.28, 0.32, -0.08), ((-0.3, 1.1), 0.34, 0.26, 0.04), ((0.1, 1.15), 0.24, 0.3, 0.1)]
    for k, ((x, z), w, h, roll) in enumerate(sheets):
        sheet = pk.part(f'Notice_Sheet{k}', pk.box((w, 0.004, h)), target, m['paper'], rng=rng)
        sheet.matrix_world = Matrix.Translation((x, -0.004 - 0.002 * k, z)) @ Matrix.Rotation(roll, 4, 'Y')
        parts.append(sheet)
        pin = pk.part(f'Notice_Pin{k}', pk.ellipsoid_bm((0.0, 0.0, 0.0), (0.012, 0.01, 0.012), segments=8, rings=6), target, m['iron'], rng=rng, smooth=60.0)
        pin.matrix_world = Matrix.Translation((x, -0.016 - 0.002 * k, z + h * 0.42))
        parts.append(pin)
    return kit.join(parts, 'NoticeBoard')


def candle(name, base, height, radius, m, target, rng):
    profile = [(radius * 1.15, 0.0), (radius, 0.012), (radius * 0.98, height * 0.6), (radius, height - 0.012), (radius * 0.85, height), (0.0, height - 0.008)]
    body = pk.part(name, pk.lathe(profile, 10), target, m['wax'], rng=rng, smooth=40.0)
    body.matrix_world = Matrix.Translation(base)
    drips = []
    for i in range(3):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        drop = pk.part(f'{name}_Drip{i}', pk.capsule_bm((math.cos(angle) * radius * 0.95, math.sin(angle) * radius * 0.95, height - 0.005), (math.cos(angle) * radius * 1.05, math.sin(angle) * radius * 1.05, height - rng.uniform(0.03, 0.08)), 0.006, 0.008, segments=6, rings=2), target, m['wax'], rng=rng, smooth=50.0)
        drop.matrix_world = Matrix.Translation(base)
        drips.append(drop)
    wick = pk.part(f'{name}_Wick', pk.cylinder(0.003, 0.018, 4, z=height - 0.006), target, m['iron'], rng=rng, smooth=40.0)
    wick.matrix_world = Matrix.Translation(base)
    return [body, wick] + drips


def candle_stand(m, target, rng):
    parts = []
    for k in range(3):
        angle = k * 2.0 * math.pi / 3.0 + math.pi / 2.0
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        parts.append(pk.tube([out * 0.2 + Vector((0.0, 0.0, 0.0)), out * 0.16 + Vector((0.0, 0.0, 0.08)), out * 0.06 + Vector((0.0, 0.0, 0.14)), out * 0.02 + Vector((0.0, 0.0, 0.22))], 0.014, target, m['iron'], f'Stand_Leg{k}', resolution=6, sides=6, rng=rng))
        parts.append(pk.part(f'Stand_Toe{k}', pk.ellipsoid_bm(out * 0.2 + Vector((0.0, 0.0, 0.008)), (0.022, 0.022, 0.01), segments=8, rings=5), target, m['iron'], rng=rng, smooth=60.0))
    twist = [Vector((math.cos(t * 1.4) * 0.008, math.sin(t * 1.4) * 0.008, 0.2 + t * 0.06)) for t in range(14)]
    parts.append(pk.tube(twist, 0.018, target, m['iron'], 'Stand_Shaft', resolution=4, sides=8, rng=rng))
    parts.append(pk.part('Stand_Knot', pk.ellipsoid_bm((0.0, 0.0, 0.62), (0.035, 0.035, 0.03), segments=10, rings=6), target, m['iron'], rng=rng, smooth=60.0))
    parts.append(pk.part('Stand_Pan', pk.lathe([(0.0, 1.0), (0.05, 1.0), (0.13, 1.025), (0.15, 1.05), (0.14, 1.055), (0.12, 1.035), (0.0, 1.03)], 16), target, m['iron'], rng=rng, smooth=45.0))
    for k, (dx, h) in enumerate(((0.0, 0.22), (-0.075, 0.15), (0.075, 0.17))):
        if k:
            parts.append(pk.tube([(0.0, 0.0, 1.0), (dx * 0.7, 0.0, 1.04), (dx, 0.0, 1.09)], 0.009, target, m['iron'], f'Stand_Arm{k}', resolution=4, sides=6, rng=rng))
            parts.append(pk.part(f'Stand_Cup{k}', pk.lathe([(0.0, 1.085), (0.03, 1.09), (0.034, 1.115), (0.026, 1.115), (0.0, 1.11)], 10), target, m['iron'], matrix=Matrix.Translation((dx, 0.0, 0.0)), rng=rng, smooth=45.0))
            parts += candle(f'Stand_Candle{k}', (dx, 0.0, 1.11), h, 0.019, m, target, rng)
        else:
            parts += candle('Stand_Candle0', (0.0, 0.0, 1.04), h, 0.022, m, target, rng)
    return kit.join(parts, 'CandleStand')


def coals(name, center, radius, height, m, target, rng, count=16):
    pieces = []
    for i in range(count):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        distance = radius * math.sqrt(rng.uniform(0.0, 1.0))
        size = rng.uniform(0.035, 0.06)
        position = Vector(center) + Vector((math.cos(angle) * distance, math.sin(angle) * distance, height * (1.0 - (distance / radius) ** 2) * rng.uniform(0.6, 1.0)))
        lump = pk.chipped((size, size * rng.uniform(0.8, 1.2), size * rng.uniform(0.6, 0.9)), rng, chips=3, depth=size * 0.3)
        piece = pk.part(f'{name}{i}', lump, target, m['ember'], rng=rng)
        piece.matrix_world = Matrix.Translation(position) @ Matrix.Rotation(rng.uniform(0.0, 6.28), 4, 'Z') @ Matrix.Rotation(rng.uniform(-0.6, 0.6), 4, 'X')
        pieces.append(piece)
    bed = pk.part(f'{name}Bed', pk.lathe([(radius * 1.02, 0.0), (radius * 0.7, height * 0.35), (0.0, height * 0.5)], 14), target, m['ember'], rng=rng, smooth=40.0)
    bed.matrix_world = Matrix.Translation(center)
    return pieces + [bed]


def brazier(m, target, rng):
    parts = []
    for k in range(3):
        angle = k * 2.0 * math.pi / 3.0 + math.pi / 6.0
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        parts.append(pk.tube([out * 0.3 + Vector((0.0, 0.0, 0.0)), out * 0.26 + Vector((0.0, 0.0, 0.18)), out * 0.17 + Vector((0.0, 0.0, 0.42)), out * 0.22 + Vector((0.0, 0.0, 0.62)), out * 0.27 + Vector((0.0, 0.0, 0.7))], 0.022, target, m['iron'], f'Brazier_Leg{k}', resolution=6, sides=6, rng=rng))
        parts.append(pk.part(f'Brazier_Foot{k}', pk.ellipsoid_bm(out * 0.31 + Vector((0.0, 0.0, 0.015)), (0.04, 0.04, 0.02), segments=8, rings=5), target, m['iron'], rng=rng, smooth=60.0))
    parts.append(pk.part('Brazier_Ring', pk.torus(0.2, 0.016, 24, 6), target, m['iron'], matrix=Matrix.Translation((0.0, 0.0, 0.42)), rng=rng, smooth=50.0))
    bowl = pk.lathe([(0.0, 0.6), (0.12, 0.605), (0.22, 0.64), (0.29, 0.7), (0.33, 0.78), (0.345, 0.82), (0.33, 0.83), (0.31, 0.79), (0.27, 0.72), (0.2, 0.67), (0.0, 0.65)], 20)
    parts.append(pk.part('Brazier_Bowl', bowl, target, m['iron'], rng=rng, smooth=40.0))
    for k in range(10):
        angle = k * 2.0 * math.pi / 10.0
        parts.append(pk.part(f'Brazier_Rivet{k}', pk.ellipsoid_bm((math.cos(angle) * 0.335, math.sin(angle) * 0.335, 0.795), (0.012, 0.012, 0.012), segments=6, rings=4), target, m['iron'], rng=rng, smooth=60.0))
    parts += coals('Brazier_Coal', (0.0, 0.0, 0.68), 0.27, 0.12, m, target, rng, 18)
    return kit.join(parts, 'Brazier')


def fire_bowl(m, target, rng):
    parts = [pk.part('FireBowl_Base', pk.chipped((0.5, 0.5, 0.1), rng, chips=2, depth=0.02, center=(0.0, 0.0, 0.05)), target, m['stone'], rng=rng, bevel=(0.025, 3, 30.0))]
    column = pk.lathe([(octagon(0.2), 0.1), (octagon(0.19), 0.14), (octagon(0.155), 0.2), (octagon(0.15), 0.7), (octagon(0.17), 0.76), (octagon(0.2), 0.8)], 4, math.pi / 4.0)
    parts.append(pk.part('FireBowl_Column', column, target, m['stone'], rng=rng, bevel=(0.02, 2, 30.0)))
    bowl = pk.lathe([(0.0, 0.79), (0.2, 0.8), (0.3, 0.85), (0.36, 0.93), (0.38, 1.0), (0.36, 1.015), (0.32, 0.97), (0.26, 0.92), (0.0, 0.9)], 16, math.pi / 16.0)
    parts.append(pk.part('FireBowl_Bowl', bowl, target, m['stone'], rng=rng, bevel=(0.015, 2, 30.0)))
    parts += coals('FireBowl_Coal', (0.0, 0.0, 0.92), 0.3, 0.1, m, target, rng, 16)
    return kit.join(parts, 'FireBowl')


def offering_bowl(m, target, rng):
    parts = [pk.part('Offering_Plinth', pk.chipped((0.58, 0.58, 0.14), rng, chips=3, depth=0.025, center=(0.0, 0.0, 0.07)), target, m['stone'], rng=rng, bevel=(0.03, 3, 30.0))]
    column = pk.lathe([(0.17, 0.14), (0.15, 0.2), (0.12, 0.28), (0.12, 0.44), (0.16, 0.5)], 12)
    parts.append(pk.part('Offering_Stem', column, target, m['stone'], rng=rng, smooth=40.0))
    bowl = pk.lathe([(0.0, 0.48), (0.18, 0.5), (0.28, 0.56), (0.33, 0.65), (0.34, 0.7), (0.32, 0.715), (0.29, 0.66), (0.23, 0.6), (0.0, 0.58)], 20)
    parts.append(pk.part('Offering_Basin', bowl, target, m['stone'], rng=rng, smooth=35.0))
    for i in range(46):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        distance = 0.26 * math.sqrt(rng.uniform(0.0, 1.0))
        heap = 0.07 * (1.0 - (distance / 0.27) ** 2)
        coin = pk.part(f'Offering_Coin{i}', pk.cylinder(0.024, 0.006, 10), target, m['gold'], rng=rng, smooth=30.0)
        coin.matrix_world = Matrix.Translation((math.cos(angle) * distance, math.sin(angle) * distance, 0.585 + heap * rng.uniform(0.5, 1.0))) @ Matrix.Rotation(rng.uniform(-0.5, 0.5), 4, 'X') @ Matrix.Rotation(rng.uniform(-0.5, 0.5), 4, 'Y')
        parts.append(coin)
    return kit.join(parts, 'OfferingBowl')


def build(target):
    rng = random.Random(23)
    m = {
        'iron': pk.iron_material('Iron', bake_size=1024),
        'wood': pk.wood_material('Wood', light=(0.27, 0.185, 0.11), dark=(0.12, 0.08, 0.05), weathered=0.45, bake_size=2048),
        'stone': pk.stone_material('Stone', bake_size=2048),
        'glass': pk.glow_material('LanternGlass', color=(1.0, 0.66, 0.3), strength=3.2, base=(0.9, 0.66, 0.36)),
        'wax': pk.wax_material('Wax'),
        'ember': pk.ember_material('Ember'),
        'paper': pk.paper_material('Paper'),
        'gold': pk.brass_material('Gold', tone=(0.95, 0.72, 0.25), bake_size=512),
    }
    return [
        lamp_post(m, target, rng),
        bench(m, target, rng),
        signpost(m, target, rng),
        notice_board(m, target, rng),
        candle_stand(m, target, rng),
        brazier(m, target, rng),
        fire_bowl(m, target, rng),
        offering_bowl(m, target, rng),
    ]


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    pk.unwrap_all(objects, {'LanternGlass': 0.02, 'Gold': 0.006, 'Wax': 0.008, 'Ember': 0.006})
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.35)
    if args.get('--preview'):
        pk.ground_preview(12.0)
        for index, obj in enumerate(objects):
            obj.matrix_world = Matrix.Translation(((index % 4) * 2.2 - 3.3, (index // 4) * 2.4 - 1.2, 0.0))
        pk.preview(objects[:4], args['--preview'], 'hub_a', fill=0.95)
        pk.preview(objects[4:], args['--preview'], 'hub_b', fill=0.95)


if __name__ == '__main__':
    main()
