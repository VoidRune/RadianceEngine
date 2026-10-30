import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Quaternion, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import hubprops
import stations

NAME = 'Arcana'


def rock_base(name, radius, height, m, target, rng):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
    for vertex in bm.verts:
        direction = vertex.co.normalized()
        bump = 1.0 + 0.18 * math.sin(direction.x * 5.1 + rng.uniform(0.0, 6.0)) * math.cos(direction.y * 4.3) + rng.uniform(-0.06, 0.06)
        vertex.co = Vector((direction.x * radius * bump, direction.y * radius * bump * 0.9, max(direction.z, -0.15) * height * bump))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    obj = pk.part(name, bm, target, m['rock'], rng=rng, smooth=35.0)
    return obj


def crystal_cluster(key, spec, m, target, rng):
    parts = [rock_base(f'Crystal{key}_Rock', spec['rock'], 0.16, m, target, rng)]
    for index, (angle, distance, radius, height, lean) in enumerate(spec['shards']):
        shard = pk.part(f'Crystal{key}_Shard{index}', stations.crystal_shard(radius, height, sides=6, tip=rng.uniform(0.25, 0.4), rng=rng), target, m['crystal'], rng=rng)
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        axis = Vector((0.0, 0.0, 1.0)).cross(out).normalized() if distance > 0.01 else Vector((1.0, 0.0, 0.0))
        shard.matrix_world = Matrix.Translation(out * distance + Vector((0.0, 0.0, 0.04))) @ Matrix.Rotation(math.radians(lean), 4, axis) @ Matrix.Rotation(rng.uniform(0.0, 6.28), 4, 'Z')
        parts.append(shard)
    return kit.join(parts, f'Crystal_{key}')


def armillary(m, target, rng):
    parts = [pk.part('Armillary_Base', pk.lathe([(hubprops.octagon(0.42), 0.0), (hubprops.octagon(0.42), 0.1), (hubprops.octagon(0.36), 0.15), (hubprops.octagon(0.22), 0.2), (hubprops.octagon(0.17), 0.3), (hubprops.octagon(0.15), 0.72), (hubprops.octagon(0.2), 0.8), (hubprops.octagon(0.24), 0.86)], 8, math.pi / 8.0), target, m['stone'], rng=rng, bevel=(0.025, 2, 30.0))]
    parts.append(pk.part('Armillary_Cup', pk.lathe([(0.0, 0.85), (0.2, 0.86), (0.24, 0.9), (0.2, 0.93), (0.06, 0.95), (0.05, 1.05)], 16), target, m['brass'], rng=rng, smooth=40.0))
    center = Vector((0.0, 0.0, 1.42))
    for k in range(4):
        angle = k * math.pi * 0.5 + math.pi * 0.25
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        parts.append(pk.tube([out * 0.06 + Vector((0.0, 0.0, 0.95)), out * 0.3 + Vector((0.0, 0.0, 1.0)), out * 0.47 + Vector((0.0, 0.0, 1.32)), out * 0.49 + Vector((0.0, 0.0, 1.42))], 0.012, target, m['brass'], f'Armillary_Arm{k}', resolution=6, sides=6, rng=rng))
    rings = [
        ('Horizon', 0.5, 0.018, Matrix.Identity(4)),
        ('Meridian', 0.47, 0.016, Matrix.Rotation(math.pi * 0.5, 4, 'X')),
        ('Equator', 0.44, 0.013, Matrix.Rotation(math.radians(23.5), 4, 'Y')),
        ('Colure', 0.44, 0.011, Matrix.Rotation(math.radians(23.5), 4, 'Y') @ Matrix.Rotation(math.pi * 0.5, 4, 'Y')),
        ('Tropic', 0.38, 0.009, Matrix.Rotation(math.radians(23.5), 4, 'Y') @ Matrix.Translation((0.0, 0.0, 0.2))),
    ]
    for name, major, minor, orient in rings:
        parts.append(pk.part(f'Armillary_{name}', pk.torus(major, minor, 40, 6), target, m['brass'], matrix=Matrix.Translation(center) @ orient, rng=rng, smooth=50.0))
    band = pk.lathe([(0.42, -0.03), (0.42, 0.03)], 40)
    parts.append(pk.part('Armillary_Ecliptic', band, target, m['brass'], matrix=Matrix.Translation(center) @ Matrix.Rotation(math.radians(23.5), 4, 'Y') @ Matrix.Rotation(math.radians(-23.5), 4, 'X'), rng=rng, smooth=50.0))
    parts.append(pk.part('Armillary_Axis', pk.rod(center + Vector((0.0, 0.0, -0.52)), center + Vector((0.0, 0.0, 0.56)), 0.01, 6), target, m['brass'], matrix=Matrix.Translation(center) @ Matrix.Rotation(math.radians(23.5), 4, 'Y') @ Matrix.Translation(-center), rng=rng, smooth=40.0))
    orb = pk.part('Armillary_Orb', pk.ellipsoid_bm(center, (0.12, 0.12, 0.12), segments=20, rings=12), target, m['orb'], rng=rng, smooth=60.0)
    parts.append(orb)
    return kit.join(parts, 'Armillary')


def telescope(m, target, rng):
    parts = []
    head = Vector((0.0, 0.0, 1.18))
    for k in range(3):
        angle = k * 2.0 * math.pi / 3.0 + math.pi / 2.0
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        parts.append(pk.part(f'Scope_Leg{k}', pk.rod(out * 0.5, head + out * 0.05, 0.032, 8, end_radius=0.024), target, m['wood'], rng=rng, smooth=40.0))
        parts.append(pk.part(f'Scope_Foot{k}', pk.cone_bm(out * 0.48 + Vector((0.0, 0.0, 0.06)), out * 0.48 + Vector((0.0, 0.0, -0.01)), 0.026, sides=8), target, m['brass'], rng=rng, smooth=40.0))
    for k in range(3):
        angle = k * 2.0 * math.pi / 3.0 + math.pi / 2.0
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        parts.append(pk.part(f'Scope_Brace{k}', pk.rod(out * 0.28 + Vector((0.0, 0.0, 0.5)), Vector((0.0, 0.0, 0.62)), 0.008, 6), target, m['brass'], rng=rng, smooth=40.0))
    parts.append(pk.part('Scope_Hub', pk.lathe([(0.07, 1.12), (0.08, 1.16), (0.06, 1.22), (0.035, 1.26)], 12), target, m['brass'], rng=rng, smooth=40.0))
    parts.append(pk.part('Scope_Yoke', pk.box((0.2, 0.04, 0.14), (0.0, 0.0, 1.31)), target, m['brass'], rng=rng, bevel=(0.006, 1, 30.0)))
    pitch = Matrix.Translation((0.0, 0.0, 1.36)) @ Matrix.Rotation(math.radians(55.0), 4, 'X')
    tube = pk.lathe([(0.0, -0.62), (0.038, -0.62), (0.04, -0.58), (0.045, -0.3), (0.05, -0.28), (0.055, 0.1), (0.062, 0.12), (0.065, 0.55), (0.078, 0.58), (0.08, 0.66), (0.07, 0.68), (0.0, 0.67)], 18)
    tube_obj = pk.part('Scope_Tube', tube, target, m['brass'], rng=rng, smooth=35.0)
    tube_obj.matrix_world = pitch
    parts.append(tube_obj)
    for k, z in enumerate((-0.18, 0.32)):
        wrap = pk.part(f'Scope_Wrap{k}', pk.lathe([(0.058 + 0.004 * k, z - 0.07), (0.062 + 0.004 * k, z - 0.065), (0.062 + 0.004 * k, z + 0.065), (0.058 + 0.004 * k, z + 0.07)], 18), target, m['leather'], rng=rng, smooth=35.0)
        wrap.matrix_world = pitch
        parts.append(wrap)
    lens = pk.part('Scope_Lens', pk.cylinder(0.068, 0.006, 18, z=0.66), target, m['lens'], rng=rng, smooth=35.0)
    lens.matrix_world = pitch
    parts.append(lens)
    return kit.join(parts, 'Telescope')


def robe(base, folds=7):
    profile = [(0.0, 0.0), (0.4, 0.0), (0.395, 0.06), (0.36, 0.3), (0.3, 0.6), (0.235, 0.86), (0.205, 0.98), (0.215, 1.1), (0.235, 1.22), (0.24, 1.3), (0.18, 1.38), (0.09, 1.43), (0.0, 1.44)]
    bm = pk.lathe(profile, 40)
    for vertex in bm.verts:
        z = vertex.co.z
        radius = math.hypot(vertex.co.x, vertex.co.y)
        if radius < 1e-5:
            continue
        angle = math.atan2(vertex.co.y, vertex.co.x)
        fold = 1.0 + 0.055 * math.sin(folds * angle + 0.6 * math.sin(3.0 * angle)) * max(0.0, 1.0 - z / 0.95)
        vertex.co.x *= fold
        vertex.co.y *= fold * 0.84
    bmesh.ops.translate(bm, vec=(0.0, 0.0, base), verts=bm.verts)
    return bm


def statue(m, target, rng):
    parts = []
    plinth_rng = random.Random(5)
    parts.append(pk.part('Statue_Plinth', pk.chipped((1.12, 1.12, 0.2), plinth_rng, chips=3, depth=0.03, center=(0.0, 0.0, 0.1)), target, m['stone'], rng=rng, bevel=(0.03, 3, 30.0)))
    parts.append(pk.part('Statue_Die', pk.chipped((0.88, 0.88, 0.52), plinth_rng, chips=2, depth=0.025, center=(0.0, 0.0, 0.46)), target, m['stone'], rng=rng, bevel=(0.03, 3, 30.0)))
    parts.append(pk.part('Statue_Cornice', pk.chipped((1.0, 1.0, 0.12), plinth_rng, chips=3, depth=0.025, center=(0.0, 0.0, 0.78)), target, m['stone'], rng=rng, bevel=(0.03, 3, 30.0)))
    parts.append(pk.part('Statue_Plaque', pk.box((0.46, 0.02, 0.24), (0.0, -0.445, 0.48)), target, m['bronze'], rng=rng, bevel=(0.008, 1, 30.0)))
    base = 0.84
    up = lambda x, y, z: (x, y, base + z)
    figure = pk.sculpt([
        robe(base),
        pk.ellipsoid_bm(up(0.0, 0.02, 1.33), (0.31, 0.21, 0.12)),
        pk.ellipsoid_bm(up(0.0, 0.17, 0.82), (0.29, 0.06, 0.62), Quaternion((1.0, 0.0, 0.0), math.radians(-5.0))),
        pk.ellipsoid_bm(up(0.0, 0.0, 1.55), (0.11, 0.12, 0.13)),
        pk.ellipsoid_bm(up(0.0, 0.035, 1.585), (0.155, 0.17, 0.165)),
        pk.capsule_bm(up(0.0, 0.1, 1.62), up(0.0, 0.2, 1.42), 0.12, 0.08),
        pk.capsule_bm(up(0.24, 0.0, 1.3), up(0.25, -0.13, 1.1), 0.075, 0.07),
        pk.capsule_bm(up(0.25, -0.13, 1.1), up(0.06, -0.27, 1.16), 0.07, 0.085),
        pk.capsule_bm(up(-0.24, 0.0, 1.3), up(-0.25, -0.13, 1.08), 0.075, 0.07),
        pk.capsule_bm(up(-0.25, -0.13, 1.08), up(-0.06, -0.27, 1.06), 0.07, 0.085),
        pk.ellipsoid_bm(up(0.035, -0.31, 1.17), (0.045, 0.045, 0.05)),
        pk.ellipsoid_bm(up(-0.035, -0.31, 1.06), (0.045, 0.045, 0.05)),
    ], target, m['marble'], 'Statue_Figure', voxel=0.011, smooth=5, smooth_factor=0.45, faces=8000, rng=rng, carve=[pk.ellipsoid_bm(up(0.0, -0.15, 1.53), (0.085, 0.07, 0.1))])
    parts.append(figure)
    parts.append(pk.part('Statue_Staff', pk.rod((0.0, -0.33, base), (0.0, -0.33, base + 1.98), 0.03, 8, end_radius=0.026), target, m['marble'], rng=rng, smooth=40.0))
    crown = pk.part('Statue_Crown', stations.crystal_shard(0.06, 0.26, sides=5, tip=0.4, rng=rng), target, m['ember_glass'], rng=rng)
    crown.matrix_world = Matrix.Translation((0.0, -0.33, base + 1.95))
    parts.append(crown)
    for k in range(3):
        angle = k * 2.0 * math.pi / 3.0
        parts.append(pk.part(f'Statue_Prong{k}', pk.rod((0.0, -0.33, base + 1.9), (math.cos(angle) * 0.075, -0.33 + math.sin(angle) * 0.075, base + 2.07), 0.012, 6, end_radius=0.006), target, m['marble'], rng=rng, smooth=40.0))
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'Statue_Eye{int(side)}', pk.ellipsoid_bm((side * 0.034, -0.09, base + 1.55), (0.015, 0.008, 0.008), segments=8, rings=6), target, m['ember_glass'], rng=rng, smooth=60.0))
    return kit.join(parts, 'Statue')


def build(target):
    rng = random.Random(47)
    m = {
        'rock': pk.stone_material('CrystalRock', light=(0.22, 0.2, 0.22), dark=(0.09, 0.085, 0.1), moss=0.3, bake_size=1024),
        'crystal': pk.crystal_material('Crystal', core=(0.3, 0.72, 1.0), shell=(0.03, 0.08, 0.14), strength=3.2, veins=0.12, cells=12.0, thickness=0.2, translucency=0.8),
        'stone': pk.stone_material('Stone', bake_size=2048),
        'marble': pk.stone_material('Marble', light=(0.6, 0.58, 0.54), dark=(0.38, 0.36, 0.33), moss=0.35, bake_size=2048),
        'brass': pk.brass_material('Brass', bake_size=1024),
        'bronze': pk.brass_material('Bronze', tone=(0.42, 0.28, 0.14), bake_size=256),
        'wood': pk.wood_material('Wood', light=(0.2, 0.135, 0.08), dark=(0.085, 0.055, 0.035), weathered=0.3, bake_size=512),
        'leather': pk.leather_material('Leather', bake_size=256),
        'orb': pk.crystal_material('Orb', core=(0.45, 0.6, 1.0), shell=(0.05, 0.08, 0.2), strength=2.0, bake_size=256, translucency=0.6),
        'lens': pk.glow_material('Lens', color=(0.55, 0.75, 1.0), strength=1.2, base=(0.2, 0.3, 0.45), bake_size=128),
        'ember_glass': pk.glow_material('EmberGlass', color=(1.0, 0.45, 0.12), strength=3.5, base=(0.9, 0.4, 0.12), bake_size=128),
    }
    variants = {
        'A': {'rock': 0.34, 'shards': [(0.0, 0.0, 0.11, 1.1, 0.0), (0.4, 0.17, 0.08, 0.72, 22.0), (2.5, 0.18, 0.07, 0.6, 26.0), (4.3, 0.16, 0.06, 0.48, 30.0), (5.4, 0.2, 0.05, 0.36, 38.0)]},
        'B': {'rock': 0.42, 'shards': [(0.3, 0.12, 0.09, 0.7, 14.0), (2.0, 0.15, 0.085, 0.62, 20.0), (3.6, 0.2, 0.07, 0.5, 30.0), (5.0, 0.18, 0.065, 0.42, 34.0), (1.1, 0.27, 0.05, 0.3, 45.0), (4.4, 0.28, 0.045, 0.26, 48.0)]},
        'C': {'rock': 0.3, 'shards': [(0.0, 0.03, 0.13, 1.25, 6.0), (1.6, 0.17, 0.06, 0.42, 28.0), (3.9, 0.15, 0.05, 0.34, 34.0)]},
    }
    objects = [crystal_cluster(key, spec, m, target, rng) for key, spec in variants.items()]
    objects += [armillary(m, target, rng), telescope(m, target, rng), statue(m, target, rng)]
    return objects


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    pk.unwrap_all(objects, {'Crystal': 0.006, 'Orb': 0.02, 'Lens': 0.02, 'EmberGlass': 0.02, 'Bronze': 0.02, 'Leather': 0.01})
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.4)
    if args.get('--preview'):
        pk.ground_preview(12.0)
        for index, obj in enumerate(objects):
            obj.matrix_world = Matrix.Translation(((index % 3) * 2.2 - 2.2, (index // 3) * 2.6 - 1.3, 0.0))
        pk.preview(objects[:3], args['--preview'], 'arcana_a', fill=0.95)
        pk.preview(objects[3:], args['--preview'], 'arcana_b', fill=0.95)


if __name__ == '__main__':
    main()
