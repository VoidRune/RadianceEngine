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

NAME = 'Chess'
SQUARE = 0.22
TOP = 0.62
BORDER = 0.1
SIDES = 24


def marble_material(name, light, dark, scale=12.0, veins=0.6, roughness=0.3, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    warped = s.offset(p, s.mul(s.noise_color(p, scale * 0.7, detail=3.0), 0.4 / scale))
    lines = s.wave(warped, scale, kind='BANDS', direction='X', distortion=8.0, detail=4.0, detail_scale=1.4)
    vein = s.mul(s.smooth(0.84, 0.97, lines), veins)
    cloud = s.noise(p, scale * 1.6, detail=4.0, roughness=0.55)
    base = s.mix(s.smooth(0.25, 0.75, cloud), dark, light)
    base = s.mix(vein, base, (dark[0] * 0.55, dark[1] * 0.55, dark[2] * 0.6))
    edge = pk.wear(s, 0.006)
    base = s.mix(s.mul(edge, 0.25), base, light)
    rough = s.remap(cloud, 0.3, 0.7, roughness - 0.07, roughness + 0.07)
    return s.finish(base, rough, height=s.sub(s.mul(cloud, 0.6), s.mul(vein, 0.3)), bump=0.03, bump_distance=0.002, bake_size=bake_size)


def onyx_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    warp = s.noise(p, 9.0, detail=3.0, distortion=0.8)
    bands = s.wave(s.stretch(p, (0.6, 0.6, 1.0)), 30.0, kind='BANDS', direction='Z', distortion=6.0, detail=3.0, detail_scale=1.4)
    tone = s.madd(bands, 0.6, s.mul(warp, 0.4))
    base = s.mix(s.smooth(0.3, 0.95, tone), (0.012, 0.012, 0.016), (0.036, 0.033, 0.042))
    sheen = s.mul(s.smooth(0.86, 0.98, bands), 0.5)
    base = s.mix(sheen, base, (0.05, 0.034, 0.022))
    fissures = s.voronoi(s.offset(p, s.mul(s.noise_color(p, 40.0, detail=2.0), 0.01)), 55.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    lines = s.sub(1.0, s.smooth(0.0, 0.01, fissures))
    veins = s.mul(lines, s.smooth(0.5, 0.62, s.noise(p, 26.0, detail=2.0)))
    base = s.mix(veins, base, (0.09, 0.025, 0.008))
    rough = s.remap(s.noise(p, 60.0, detail=3.0), 0.3, 0.7, 0.07, 0.16)
    return s.finish(base, rough, height=s.mul(warp, 0.2), bump=0.02, bump_distance=0.002, emission=veins, emission_color=(1.0, 0.4, 0.1), emission_strength=1.4, specular=0.6, bake_size=bake_size)


def base_profile(radius):
    return [(0.0, 0.0), (radius, 0.0), (radius, 0.011), (radius * 0.92, 0.017), (radius * 0.84, 0.021), (radius * 0.66, 0.027)]


def body(name, profile, target, material, rng):
    return pk.part(name, pk.lathe(profile, SIDES), target, material, rng=rng, smooth=40.0)


def ring(name, major, minor, z, target, material, rng):
    return pk.part(name, pk.torus(major, minor, 28, 6), target, material, matrix=Matrix.Translation((0.0, 0.0, z)), rng=rng, smooth=60.0)


def carve(obj, cutter):
    knife = kit.mesh_object(obj.name + '_Knife', cutter, obj.users_collection[0])
    boolean = obj.modifiers.new('Carve', 'BOOLEAN')
    boolean.operation = 'DIFFERENCE'
    boolean.solver = 'EXACT'
    boolean.object = knife
    kit.apply_modifiers(obj)
    bpy.data.objects.remove(knife, do_unlink=True)
    return obj


def pawn(name, m, side, target, rng):
    profile = base_profile(0.056) + [(0.026, 0.037), (0.02, 0.058), (0.018, 0.068), (0.03, 0.072), (0.031, 0.077), (0.016, 0.082), (0.0, 0.083)]
    head = pk.part(f'{name}_Head', pk.ellipsoid_bm((0.0, 0.0, 0.101), (0.025, 0.025, 0.025), segments=20, rings=12), target, m[side], rng=rng, smooth=60.0)
    return kit.join([body(f'{name}_Body', profile, target, m[side], rng), head], name)


def rook(name, m, side, target, rng):
    profile = base_profile(0.062) + [(0.038, 0.034), (0.034, 0.042), (0.031, 0.098), (0.034, 0.104), (0.046, 0.108), (0.046, 0.132), (0.032, 0.132), (0.032, 0.124), (0.0, 0.124)]
    parts = [body(f'{name}_Body', profile, target, m[side], rng)]
    for k in range(4):
        merlon = pk.box((0.013, 0.03, 0.019), (0.0395, 0.0, 0.1415))
        parts.append(pk.part(f'{name}_Merlon{k}', merlon, target, m[side], matrix=Matrix.Rotation(k * math.pi * 0.5 + math.pi * 0.25, 4, 'Z'), rng=rng, bevel=(0.002, 1, 30.0)))
    return kit.join(parts, name)


def knight(name, m, side, target, rng):
    profile = base_profile(0.062) + [(0.042, 0.032), (0.0, 0.032)]
    head = pk.sculpt([
        pk.capsule_bm((0.0, 0.014, 0.028), (0.0, 0.008, 0.1), 0.031, 0.021),
        pk.ellipsoid_bm((0.0, -0.012, 0.054), (0.025, 0.022, 0.032)),
        pk.capsule_bm((0.0, 0.008, 0.118), (0.0, -0.058, 0.088), 0.021, 0.015),
        pk.ellipsoid_bm((0.0, -0.004, 0.124), (0.019, 0.026, 0.018)),
        pk.ellipsoid_bm((0.0, -0.06, 0.084), (0.0155, 0.014, 0.0135)),
        pk.capsule_bm((0.0, 0.034, 0.04), (0.0, 0.026, 0.138), 0.011, 0.008),
        pk.capsule_bm((0.0, 0.03, 0.14), (0.0, 0.012, 0.146), 0.008, 0.006),
        pk.cone_bm((0.009, 0.012, 0.136), (0.01, 0.017, 0.156), 0.0055, sides=6),
        pk.cone_bm((-0.009, 0.012, 0.136), (-0.01, 0.017, 0.156), 0.0055, sides=6),
    ], target, m[side], f'{name}_Head', voxel=0.0022, smooth=4, smooth_factor=0.5, faces=2000, rng=rng)
    return kit.join([body(f'{name}_Body', profile, target, m[side], rng), head], name)


def bishop(name, m, side, target, rng):
    profile = base_profile(0.06) + [(0.032, 0.036), (0.026, 0.05), (0.02, 0.098), (0.019, 0.112), (0.033, 0.116), (0.033, 0.121), (0.02, 0.126), (0.0, 0.127)]
    mitre = pk.part(f'{name}_Mitre', pk.ellipsoid_bm((0.0, 0.0, 0.153), (0.025, 0.025, 0.034), segments=24, rings=14), target, m[side], rng=rng, smooth=60.0)
    slit = pk.box((0.07, 0.006, 0.03), (0.0, 0.0, 0.0))
    bmesh.ops.transform(slit, matrix=Matrix.Translation((0.0, 0.0, 0.168)) @ Matrix.Rotation(math.radians(-38.0), 4, 'X') @ Matrix.Translation((0.0, 0.012, 0.0)), verts=slit.verts)
    carve(mitre, slit)
    finial = pk.part(f'{name}_Finial', pk.ellipsoid_bm((0.0, 0.0, 0.192), (0.0085, 0.0085, 0.0085), segments=12, rings=8), target, m[side], rng=rng, smooth=60.0)
    return kit.join([body(f'{name}_Body', profile, target, m[side], rng), mitre, finial], name)


def queen(name, m, side, target, rng):
    profile = base_profile(0.066) + [(0.034, 0.04), (0.028, 0.055), (0.021, 0.118), (0.02, 0.134), (0.036, 0.139), (0.036, 0.145), (0.023, 0.15), (0.024, 0.16), (0.037, 0.186), (0.033, 0.19), (0.0, 0.183)]
    parts = [body(f'{name}_Body', profile, target, m[side], rng), ring(f'{name}_Collar', 0.0345, 0.0042, 0.142, target, m['brass'], rng)]
    for k in range(9):
        angle = k * 2.0 * math.pi / 9.0
        parts.append(pk.part(f'{name}_Point{k}', pk.ellipsoid_bm((math.cos(angle) * 0.032, math.sin(angle) * 0.032, 0.192), (0.0065, 0.0065, 0.0075), segments=10, rings=6), target, m[side], rng=rng, smooth=60.0))
    parts.append(pk.part(f'{name}_Gem', pk.ellipsoid_bm((0.0, 0.0, 0.2), (0.011, 0.011, 0.012), segments=14, rings=8), target, m[side + '_gem'], rng=rng, smooth=60.0))
    return kit.join(parts, name)


def king(name, m, side, target, rng):
    profile = base_profile(0.068) + [(0.035, 0.041), (0.029, 0.056), (0.022, 0.126), (0.021, 0.144), (0.038, 0.149), (0.038, 0.155), (0.025, 0.16), (0.027, 0.17), (0.035, 0.193), (0.033, 0.198), (0.012, 0.203), (0.0, 0.204)]
    parts = [body(f'{name}_Body', profile, target, m[side], rng), ring(f'{name}_Collar', 0.0365, 0.0044, 0.152, target, m['brass'], rng)]
    parts.append(pk.part(f'{name}_Post', pk.box((0.011, 0.011, 0.046), (0.0, 0.0, 0.225)), target, m[side + '_gem'], rng=rng, bevel=(0.0015, 1, 30.0)))
    parts.append(pk.part(f'{name}_Bar', pk.box((0.034, 0.011, 0.011), (0.0, 0.0, 0.232)), target, m[side + '_gem'], rng=rng, bevel=(0.0015, 1, 30.0)))
    return kit.join(parts, name)


def table(m, target, rng):
    half = SQUARE * 4.0
    edge = half + BORDER
    oct = hubprops.octagon
    parts = [pk.part('Table_Top', pk.chipped((2.0 * edge, 2.0 * edge, 0.07), rng, chips=3, depth=0.018, center=(0.0, 0.0, TOP - 0.049)), target, m['stone'], rng=rng, bevel=(0.012, 2, 30.0))]
    for rank in range(8):
        for file in range(8):
            light = (file + rank) % 2 == 1
            tile = pk.box((SQUARE - 0.005, SQUARE - 0.005, 0.016), ((rank - 3.5) * SQUARE, (3.5 - file) * SQUARE, TOP - 0.008))
            parts.append(pk.part(f'Table_Tile{rank}{file}', tile, target, m['light'] if light else m['dark'], rng=rng, bevel=(0.0025, 1, 30.0)))
    for k in range(4):
        rotation = Matrix.Rotation(k * math.pi * 0.5, 4, 'Z')
        strip = pk.box((2.0 * half + 0.024, 0.012, 0.012), (0.0, half + 0.008, TOP - 0.008))
        parts.append(pk.part(f'Table_Inlay{k}', strip, target, m['brass'], matrix=rotation, rng=rng, bevel=(0.002, 1, 30.0)))
        for step in range(3):
            stud = pk.cylinder(0.014, 0.008, 8, z=TOP - 0.012)
            bmesh.ops.translate(stud, vec=((step - 1) * half * 0.66, edge - BORDER * 0.5, 0.0), verts=stud.verts)
            parts.append(pk.part(f'Table_Stud{k}{step}', stud, target, m['brass'], matrix=rotation, rng=rng, smooth=40.0))
    pedestal = [(oct(0.42), 0.0), (oct(0.42), 0.06), (oct(0.36), 0.09), (oct(0.25), 0.14), (oct(0.2), 0.2), (oct(0.18), 0.44), (oct(0.24), 0.49), (oct(0.34), TOP - 0.084)]
    parts.append(pk.part('Table_Pedestal', pk.lathe(pedestal, 8, math.pi / 8.0), target, m['stone'], rng=rng, bevel=(0.012, 2, 30.0)))
    stool = [(oct(0.17), 0.0), (oct(0.17), 0.04), (oct(0.11), 0.08), (oct(0.09), 0.3), (oct(0.15), 0.34), (oct(0.18), 0.4), (oct(0.17), 0.42)]
    for direction in (-1.0, 1.0):
        parts.append(pk.part(f'Table_Stool{int(direction)}', pk.lathe(stool, 8, math.pi / 8.0), target, m['stone'], matrix=Matrix.Translation((direction * (edge + 0.42), 0.0, 0.0)), rng=rng, bevel=(0.01, 2, 30.0)))
        parts.append(pk.part(f'Table_Cushion{int(direction)}', pk.lathe([(0.0, 0.42), (0.15, 0.42), (0.16, 0.44), (0.13, 0.47), (0.0, 0.475)], 16), target, m['cloth_white' if direction < 0 else 'cloth_black'], matrix=Matrix.Translation((direction * (edge + 0.42), 0.0, 0.0)), rng=rng, smooth=50.0))
    return kit.join(parts, 'Table')


def build(target):
    rng = random.Random(64)
    m = {
        'white': marble_material('Ivory', (0.86, 0.82, 0.74), (0.7, 0.66, 0.58), scale=26.0, veins=0.45, roughness=0.3),
        'black': onyx_material('Onyx'),
        'white_gem': pk.glow_material('FrostGem', color=(0.35, 0.8, 1.0), strength=3.0, base=(0.5, 0.82, 1.0), bake_size=128),
        'black_gem': pk.glow_material('EmberGem', color=(1.0, 0.45, 0.12), strength=3.5, base=(0.9, 0.4, 0.12), bake_size=128),
        'brass': pk.brass_material('Brass', bake_size=256),
        'stone': pk.stone_material('TableStone', light=(0.4, 0.37, 0.33), dark=(0.22, 0.2, 0.18), bake_size=1024),
        'light': marble_material('BoardLight', (0.62, 0.58, 0.5), (0.5, 0.46, 0.4), scale=3.0, veins=0.5, roughness=0.38, bake_size=1024),
        'dark': marble_material('BoardDark', (0.16, 0.16, 0.19), (0.1, 0.1, 0.12), scale=3.0, veins=0.35, roughness=0.34, bake_size=1024),
        'cloth_white': pk.leather_material('CushionLight', tone=(0.55, 0.5, 0.42), bake_size=128),
        'cloth_black': pk.leather_material('CushionDark', tone=(0.12, 0.05, 0.04), bake_size=128),
    }
    objects = [table(m, target, rng)]
    for side, prefix in (('white', 'White'), ('black', 'Black')):
        for maker, piece in ((pawn, 'Pawn'), (knight, 'Knight'), (bishop, 'Bishop'), (rook, 'Rook'), (queen, 'Queen'), (king, 'King')):
            objects.append(maker(prefix + piece, m, side, target, rng))
    return objects


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    pk.unwrap_all(objects, {'FrostGem': 0.02, 'EmberGem': 0.02, 'Brass': 0.01, 'CushionLight': 0.02, 'CushionDark': 0.02})
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.1)
    if args.get('--preview'):
        pk.ground_preview(6.0)
        setup = {
            'WhiteRook': (0, 0), 'WhiteKnight': (0, 1), 'WhiteBishop': (0, 2), 'WhiteQueen': (0, 3), 'WhiteKing': (0, 4), 'WhitePawn': (1, 4),
            'BlackRook': (7, 0), 'BlackKnight': (7, 1), 'BlackBishop': (7, 2), 'BlackQueen': (7, 3), 'BlackKing': (7, 4), 'BlackPawn': (6, 3),
        }
        for obj in objects[1:]:
            rank, file = setup[obj.name]
            obj.matrix_world = Matrix.Translation(((rank - 3.5) * SQUARE, (3.5 - file) * SQUARE, TOP)) @ Matrix.Rotation(math.pi if obj.name.startswith('Black') else 0.0, 4, 'Z')
        pk.preview(objects, args['--preview'], 'chess_set', fill=0.9)
        for obj in objects[1:]:
            obj.matrix_world = Matrix.Identity(4)
        pieces = objects[1:]
        for index, obj in enumerate(pieces):
            obj.matrix_world = Matrix.Translation(((index % 6) * 0.2 - 0.5, (index // 6) * 0.25 - 0.12, 0.0))
        pk.preview(pieces, args['--preview'], 'chess_pieces', fill=0.95)


if __name__ == '__main__':
    main()
