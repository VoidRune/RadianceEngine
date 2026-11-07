import bpy
import bmesh
import math
import os
import random
import sys
import time
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import forge as fg

NAME = 'ForgeBackdrop'
F = fg.F
PIVOT = (0.0, 14.0, 3.6)
CRUCIBLE_SCALE = 1.4
BEARING = 2.0
HANDLE = (0.0, 16.7, 3.4)
TITAN_SHIFT = 3.0


def W(x, y, z=0.0):
    return Vector((-x, -y, z))


def T(x, y, z=0.0):
    return W(x, y - TITAN_SHIFT, z)


def placed(x, y, z=0.0, yaw=0.0):
    return Matrix.Translation(W(x, y, z)) @ Matrix.Rotation(yaw, 4, 'Z')


def facing_center(x, y):
    return math.atan2(-x, y)


def oriented(bm, start, end):
    a, b = Vector(start), Vector(end)
    rotation = (b - a).normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation(a) @ rotation, verts=bm.verts)
    return bm


def sleeve(start, end, radius, steps=20, lip=0.12):
    length = (Vector(end) - Vector(start)).length
    bm = pk.lathe([(radius, 0.0), (radius + lip, 0.06), (radius + lip, length - 0.06), (radius, length)], steps)
    return oriented(bm, start, end)


def ring(center, axis, major, minor, steps=28, sides=8):
    bm = pk.torus(major, minor, steps, sides)
    c = Vector(center)
    return oriented(bm, c, c + Vector(axis).normalized())


def prism_bm(points, z0, z1):
    bm = bmesh.new()
    bottom = [bm.verts.new((x, y, z0)) for x, y in points]
    top = [bm.verts.new((x, y, z1)) for x, y in points]
    bm.faces.new(bottom)
    bm.faces.new(list(reversed(top)))
    for i in range(len(points)):
        j = (i + 1) % len(points)
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def rect_mask(s, u, v, cx, cz, hx, hz, edge=0.012):
    du = s.sub(hx, s.math('ABSOLUTE', s.sub(u, cx)))
    dv = s.sub(hz, s.math('ABSOLUTE', s.sub(v, cz)))
    return s.mul(s.smooth(-edge, edge, du), s.smooth(-edge, edge, dv))


BANNER_W = 3.0
BANNER_H = 7.5


def banner_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    x, _, z = s.separate(p)
    u = s.mul(x, 1.0 / BANNER_W)
    v = s.mul(z, -1.0 / BANNER_H)
    weave = s.noise(s.stretch(p, (55.0, 55.0, 55.0)), 1.0, detail=2.0)
    fade = s.smooth(0.3, 0.8, s.noise(p, 1.4, detail=3.0))
    cloth = s.mix(fade, (0.2, 0.025, 0.02), (0.36, 0.06, 0.04))
    sigil = rect_mask(s, u, v, 0.0, 0.36, 0.27, 0.04)
    sigil = s.math('MAXIMUM', sigil, rect_mask(s, u, v, -0.33, 0.35, 0.09, 0.022))
    sigil = s.math('MAXIMUM', sigil, rect_mask(s, u, v, 0.0, 0.43, 0.08, 0.05))
    sigil = s.math('MAXIMUM', sigil, rect_mask(s, u, v, 0.0, 0.5, 0.19, 0.025))
    sigil = s.math('MAXIMUM', sigil, rect_mask(s, u, v, 0.13, 0.2, 0.022, 0.1))
    sigil = s.math('MAXIMUM', sigil, rect_mask(s, u, v, 0.13, 0.11, 0.09, 0.035))
    hem = s.math('MAXIMUM', rect_mask(s, u, v, 0.0, 0.035, 0.6, 0.02), s.sub(1.0, rect_mask(s, u, v, 0.0, 0.5, 0.43, 0.6)))
    gold = s.mix(weave, (0.42, 0.28, 0.08), (0.62, 0.44, 0.14))
    base = s.mix(s.math('MAXIMUM', sigil, s.mul(hem, 0.85)), cloth, gold)
    base = s.mix(s.mul(weave, 0.2), base, (0.05, 0.03, 0.02))
    burn = s.smooth(0.82, 0.98, v)
    base = s.mix(s.mul(burn, 0.8), base, (0.03, 0.02, 0.015))
    rough = s.lerp(sigil, 0.92, 0.55)
    return s.finish(base, rough, height=weave, bump=0.1, bump_distance=0.003, metallic=s.mul(sigil, 0.6), bake_size=bake_size)


def molten_material(name, bake_size=512, strength=5.0):
    s = kit.Shader(name)
    p = pk.local(s)
    warped = s.offset(p, s.mul(s.noise_color(p, 1.5, detail=2.0), 0.3))
    cells = s.voronoi(warped, 2.2, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    crust = s.smooth(0.05, 0.16, cells)
    heat = s.smooth(0.3, 0.75, s.noise(p, 1.2, detail=3.0))
    glow = s.clamp01(s.sub(1.0, s.mul(crust, s.sub(0.85, s.mul(heat, 0.6)))))
    base = s.mix(glow, (0.03, 0.025, 0.022), (0.9, 0.45, 0.12))
    return s.finish(base, s.lerp(glow, 0.85, 0.3), height=crust, bump=0.2, bump_distance=0.01, emission=glow, emission_color=(1.0, 0.45, 0.1), emission_strength=strength, bake_size=bake_size)


def furnace_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(p)
    flame = s.noise(s.stretch(p, (2.0, 2.0, 0.6)), 1.6, detail=4.0, distortion=1.5)
    low = s.sub(1.0, s.smooth(0.0, 3.5, z))
    glow = s.clamp01(s.add(s.mul(low, 0.7), s.mul(s.smooth(0.45, 0.75, flame), 0.6)))
    base = s.mix(glow, (0.04, 0.03, 0.025), (1.0, 0.55, 0.2))
    return s.finish(base, 0.8, height=flame, bump=0.1, bump_distance=0.01, emission=glow, emission_color=(1.0, 0.5, 0.14), emission_strength=4.0, bake_size=bake_size)


def materials():
    return {
        'stone': pk.stone_material('TitanStone', light=(0.25, 0.205, 0.17), dark=(0.085, 0.07, 0.06), bake_size=2048),
        'bronze': pk.brass_material('TitanBronze', tone=(0.34, 0.21, 0.1), bake_size=2048),
        'masonry': pk.masonry_material('CavernMasonry', light=(0.2, 0.17, 0.15), dark=(0.08, 0.068, 0.06), brick=(1.6, 0.8), mortar=(0.04, 0.034, 0.03), soot=0.7, bake_size=2048),
        'basalt': fg.block_material('CavernBasalt', 1024),
        'iron': pk.iron_material('BackIron', tone=(0.055, 0.05, 0.047), rust=0.45, bake_size=1024),
        'steel': pk.iron_material('BladeSteel', tone=(0.2, 0.2, 0.21), rust=0.2, bake_size=1024),
        'wood': pk.wood_material('HaftWood', light=(0.2, 0.12, 0.07), dark=(0.08, 0.05, 0.03), bake_size=512),
        'banner': banner_material('AnvilBanner'),
        'molten': molten_material('MoltenTop'),
        'furnace': furnace_material('FurnaceGlow'),
        "leather": pk.leather_material("BellowsLeather", tone=(0.16, 0.07, 0.04), bake_size=1024),
        "chain": fg.chain_material("BackChain", 1024),
        "obsidian": obsidian_material("ScatterObsidian"),
        "gold": pk.brass_material("GoldIngot", tone=(0.92, 0.64, 0.22), bake_size=512),
        'eyes': pk.glow_material('EmberEyes', color=(1.0, 0.5, 0.12), strength=8.0, base=(1.0, 0.55, 0.2)),
    }


def titan(m, target, rng):
    objects = []
    body = []
    for side in (-1.0, 1.0):
        body.append(pk.capsule_bm(T(side * 3.3, 28.0, 9.8), T(side * 7.0, 21.2, 8.5), 2.5, 2.2, segments=20, rings=6))
        body.append(pk.ellipsoid_bm(T(side * 7.2, 21.0, 8.6), (2.3, 2.3, 2.3), segments=20, rings=12))
        body.append(pk.capsule_bm(T(side * 7.2, 20.8, 8.2), T(side * 6.8, 19.6, -1.0), 2.0, 1.6, segments=18, rings=5))
        body.append(pk.ellipsoid_bm(T(side * 6.9, 18.0, -2.2), (1.8, 2.9, 1.3), segments=18, rings=10))
        body.append(pk.ellipsoid_bm(T(side * 6.7, 29.0, 19.0), (2.6, 2.7, 2.5), segments=20, rings=12))
        body.append(pk.capsule_bm(T(side * 7.0, 28.6, 18.4), T(side * 6.4, 24.4, 11.9), 2.0, 1.8, segments=18, rings=5))
    body.append(pk.ellipsoid_bm(T(0.0, 28.6, 10.0), (5.6, 3.7, 2.7), segments=24, rings=12))
    body.append(pk.ellipsoid_bm(T(0.0, 29.2, 15.6), (6.3, 4.0, 5.6), segments=24, rings=14))
    body.append(pk.ellipsoid_bm(T(0.0, 28.2, 11.8), (5.0, 3.4, 2.6), segments=24, rings=12))
    body.append(pk.capsule_bm(T(0.0, 29.3, 19.5), T(0.0, 28.4, 22.2), 2.0, 1.8, segments=16, rings=4))
    body.append(pk.ellipsoid_bm(T(0.0, 21.5, 7.4), (3.2, 2.4, 1.0), segments=16, rings=8))
    objects.append(pk.sculpt(body, target, m['stone'], 'TitanBody', 0.13, smooth=6, faces=8000, rng=rng))

    arms = []
    bar_y, bar_z = HANDLE[1], HANDLE[2]
    for side in (-1.0, 1.0):
        arms.append(pk.capsule_bm(T(side * 6.4, 24.4, 11.9), W(side * 2.6, 18.4, 4.3), 1.7, 1.3, segments=18, rings=5))
        arms.append(pk.ellipsoid_bm(W(side * 2.25, 17.8, 3.9), (1.25, 1.2, 1.05), segments=16, rings=10))
        arms.append(pk.ellipsoid_bm(W(side * 1.6, bar_y + 0.2, bar_z + 0.05), (1.05, 0.95, 1.0), segments=16, rings=10))
        for index in range(4):
            x = side * (0.85 + 0.36 * index)
            top = W(x, bar_y + 0.15, bar_z + 0.72)
            front = W(x, bar_y - 0.58, bar_z + 0.15)
            under = W(x, bar_y - 0.2, bar_z - 0.5)
            arms.append(pk.capsule_bm(top, front, 0.3, 0.28, segments=10, rings=3))
            arms.append(pk.capsule_bm(front, under, 0.28, 0.25, segments=10, rings=3))
        arms.append(pk.capsule_bm(W(side * 1.0, bar_y + 0.5, bar_z - 0.55), W(side * 0.62, bar_y - 0.3, bar_z - 0.5), 0.36, 0.3, segments=10, rings=3))
    objects.append(pk.sculpt(arms, target, m['stone'], 'TitanArms', 0.07, smooth=5, faces=8000, rng=rng))

    head = [
        pk.ellipsoid_bm(T(0.0, 27.6, 23.6), (2.2, 2.5, 2.9), segments=24, rings=14),
        pk.ellipsoid_bm(T(0.0, 25.4, 24.4), (1.7, 0.55, 0.45), segments=16, rings=8),
        pk.cone_bm(T(0.0, 25.3, 24.2), T(0.0, 24.6, 23.0), 0.42, sides=8),
        pk.ellipsoid_bm(T(0.0, 25.9, 21.6), (1.9, 1.3, 2.0), segments=18, rings=10),
        pk.cone_bm(T(0.0, 25.8, 21.0), T(0.0, 25.4, 18.4), 1.4, sides=10, flatten=0.7),
        pk.ellipsoid_bm(T(-0.75, 25.0, 22.7), (0.8, 0.35, 0.3), segments=12, rings=6),
        pk.ellipsoid_bm(T(0.75, 25.0, 22.7), (0.8, 0.35, 0.3), segments=12, rings=6),
    ]
    objects.append(pk.sculpt(head, target, m['stone'], 'TitanHead', 0.07, smooth=4, faces=4000, rng=rng))

    hood = [pk.ellipsoid_bm(T(0.0, 28.1, 24.4), (3.3, 3.6, 3.9), segments=28, rings=16)]
    for side in (-1.0, 1.0):
        hood.append(pk.capsule_bm(T(side * 2.6, 28.4, 23.0), T(side * 5.2, 29.0, 19.6), 1.3, 1.6, segments=14, rings=4))
    carve = [pk.ellipsoid_bm(T(0.0, 26.4, 23.9), (2.7, 3.6, 3.35), segments=24, rings=14)]
    objects.append(pk.sculpt(hood, target, m['stone'], 'TitanHood', 0.09, smooth=4, faces=3000, rng=rng, carve=carve))

    eyes = [pk.part(f'Eye{int(side)}', pk.ellipsoid_bm(T(side * 0.78, 25.15, 23.75), (0.36, 0.14, 0.17), segments=10, rings=6), target, m['eyes'], smooth=60.0) for side in (-1.0, 1.0)]
    objects.append(kit.join(eyes, 'TitanEyes'))

    bronze = []
    for side in (-1.0, 1.0):
        elbow, wrist = Vector(T(side * 6.4, 24.4, 11.9)), Vector(W(side * 2.6, 18.4, 4.3))
        for t0, t1, radius in ((0.55, 0.9, 1.4), (0.12, 0.3, 1.64)):
            bronze.append(sleeve(elbow.lerp(wrist, t0), elbow.lerp(wrist, t1), radius))
        shoulder, elbow2 = Vector(T(side * 7.0, 28.6, 18.4)), Vector(T(side * 6.4, 24.4, 11.9))
        bronze.append(sleeve(shoulder.lerp(elbow2, 0.45), shoulder.lerp(elbow2, 0.7), 1.92))
        for k in range(3):
            tilt = Vector((0.0, 0.0, 1.0)).rotation_difference(Vector((side * (0.4 + 0.12 * k), 0.0, 1.0)).normalized())
            bronze.append(pk.ellipsoid_bm(T(side * (7.1 + 0.35 * k), 28.8 - 0.15 * k, 20.3 - 0.85 * k), (2.9 - 0.15 * k, 3.0 - 0.15 * k, 1.05), rotation=tilt, segments=22, rings=8))
        bronze.append(pk.ellipsoid_bm(T(side * 7.25, 19.1, 8.9), (1.7, 0.75, 1.7), segments=16, rings=8))
        bronze.append(sleeve(T(side * 7.15, 20.5, 5.8), T(side * 6.95, 20.0, 1.6), 1.82))
    bronze.append(ring(T(0.0, 28.4, 12.0), (0.0, 0.0, 1.0), 5.2, 0.42, steps=40, sides=8))
    bronze.append(pk.ellipsoid_bm(T(0.0, 26.0, 15.8), (4.3, 1.0, 3.6), segments=22, rings=10))
    bronze.append(pk.box((1.6, 0.6, 1.3), T(0.0, 24.8, 12.0)))
    obj = kit.mesh_object('TitanArmour', pk.merge_bm(bronze), target, m['bronze'])
    pk.tag(obj, rng=rng)
    kit.smooth_by_angle(obj, 40.0)
    objects.append(obj)

    throne = [pk.part('ThroneBlock', pk.chipped((15.5, 7.5, 13.0), rng, chips=4, depth=0.4, center=tuple(T(0.0, 31.6, 2.0))), target, m['masonry'], bevel=(0.08, 2, 40.0))]
    for side in (-1.0, 1.0):
        throne.append(pk.part(f'ThroneArm{int(side)}', pk.chipped((2.4, 8.5, 4.0), rng, chips=3, depth=0.3, center=tuple(T(side * 9.4, 29.0, 9.5))), target, m['masonry'], bevel=(0.08, 2, 40.0)))
        throne.append(pk.part(f'ThroneStep{int(side)}', pk.chipped((5.0, 4.0, 3.0), rng, chips=3, depth=0.3, center=tuple(T(side * 5.8, 18.6, -3.0))), target, m['masonry'], bevel=(0.08, 2, 40.0)))
    objects.append(kit.join(throne, 'Throne'))
    return objects


def cradle(m, target, rng):
    px, py, pz = PIVOT
    hx, hy, hz = HANDLE
    parts = []
    for side in (-1.0, 1.0):
        bearing = ring(W(side * BEARING, py, pz), (1.0, 0.0, 0.0), 0.5, 0.2, steps=20, sides=8)
        parts.append(pk.part(f'CradleBearing{int(side)}', bearing, target, m['bronze'], smooth=40.0))
        parts.append(pk.part(f'CradleCheek{int(side)}', pk.box((0.34, 1.3, 1.3), W(side * BEARING, py, pz)), target, m['bronze'], bevel=(0.06, 2, 40.0)))
        parts.append(pk.part(f'CradleShank{int(side)}', pk.rod(W(side * BEARING, py + 0.4, pz - 0.1), W(side * 1.25, hy, hz), 0.3, 8), target, m['bronze'], smooth=40.0))
        parts.append(pk.part(f'CradleKnuckle{int(side)}', pk.ellipsoid_bm(W(side * 1.25, hy, hz), (0.48, 0.48, 0.48), segments=12, rings=8), target, m['bronze'], smooth=50.0))
        parts.append(pk.part(f'CradleKnob{int(side)}', pk.ellipsoid_bm(W(side * 2.55, hy, hz), (0.42, 0.42, 0.42), segments=12, rings=8), target, m['bronze'], smooth=50.0))
        for k in range(3):
            parts.append(pk.part(f'CradleRivet{int(side)}{k}', pk.ellipsoid_bm(W(side * (BEARING + 0.19), py - 0.4 + 0.4 * k, pz + 0.45), (0.07, 0.07, 0.07), segments=6, rings=4), target, m['iron'], smooth=60.0))
    parts.append(pk.part('CradleHandle', pk.rod(W(-2.5, hy, hz), W(2.5, hy, hz), 0.36, 10), target, m['iron'], smooth=40.0))
    return kit.join(parts, 'Cradle')


def crucible(m, target, rng):
    parts = []
    profile = [(0.0, -0.86), (0.78, -0.86), (0.92, -0.74), (1.02, -0.2), (1.07, 0.5), (1.15, 0.7), (1.17, 0.8), (1.08, 0.84), (0.98, 0.76), (0.93, 0.1), (0.86, -0.45), (0.74, -0.64), (0.0, -0.66)]
    parts.append(pk.part('CrucibleBody', pk.lathe(profile, 32), target, m['iron'], smooth=35.0))
    for z, radius in ((-0.58, 0.985), (0.0, 1.04), (0.52, 1.085)):
        parts.append(pk.part(f'CrucibleHoop{int(z * 10)}', ring((0.0, 0.0, z), (0.0, 0.0, 1.0), radius + 0.02, 0.06, steps=40, sides=6), target, m['bronze'], smooth=40.0))
        for k in range(14):
            a = 2.0 * math.pi * (k + 0.5) / 14.0
            parts.append(pk.part(f'CrucibleRivet{int(z * 10)}_{k}', pk.ellipsoid_bm((math.cos(a) * (radius + 0.08), math.sin(a) * (radius + 0.08), z), (0.04, 0.04, 0.04), segments=6, rings=4), target, m['iron'], smooth=60.0))
    spout = [(-0.34, 0.85), (0.34, 0.85), (0.16, 1.42), (-0.16, 1.42)]
    lip = prism_bm(spout, 0.62, 0.8)
    parts.append(pk.part('CrucibleSpout', lip, target, m['iron'], bevel=(0.025, 2, 40.0)))
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'Trunnion{int(side)}', oriented(pk.lathe([(0.2, 0.0), (0.2, 0.4), (0.26, 0.4), (0.26, 0.52), (0.0, 0.52)], 16), (side * 0.98, 0.0, 0.0), (side * 2.0, 0.0, 0.0)), target, m['bronze'], smooth=40.0))
        parts.append(pk.part(f'TrunnionBoss{int(side)}', pk.box((0.14, 0.62, 0.62), (side * 1.05, 0.0, 0.0)), target, m['bronze'], bevel=(0.04, 2, 40.0)))
    body = kit.join(parts, 'Crucible')
    body.matrix_world = Matrix.Scale(CRUCIBLE_SCALE, 4) @ body.matrix_world
    return [body]


def anvil(m, target, rng, x, y, z, yaw):
    frame = placed(x, y, z, yaw)
    parts = []
    for size, h in (((7.4, 8.4, 0.8), 0.4), ((6.0, 7.6, 0.6), 1.1)):
        parts.append(pk.part(f"AnvilPlinth{int(h * 10)}", pk.chipped(size, rng, chips=3, depth=0.25, center=(0.0, 0.0, h)), target, m['masonry'], matrix=frame, bevel=(0.06, 2, 40.0)))
    iron = []
    iron.append(pk.part('AnvilFeet', pk.box((4.6, 6.0, 1.0), (0.0, 0.0, 1.9)), target, m['iron'], matrix=frame, bevel=(0.08, 2, 40.0)))
    waist = prism_bm([(-1.2, -1.6), (1.2, -1.6), (1.2, 1.6), (-1.2, 1.6)], 2.4, 4.0)
    iron.append(pk.part('AnvilWaist', waist, target, m['iron'], matrix=frame, bevel=(0.08, 2, 40.0)))
    iron.append(pk.part('AnvilFace', pk.box((3.2, 7.4, 1.5), (0.0, -0.4, 4.7)), target, m['iron'], matrix=frame, bevel=(0.1, 2, 40.0)))
    iron.append(pk.part('AnvilHorn', pk.cone_bm((0.0, 3.2, 4.85), (0.0, 7.0, 5.15), 1.0, sides=12, flatten=1.0), target, m['iron'], matrix=frame, smooth=35.0))
    iron.append(pk.part('AnvilHeel', pk.box((2.4, 1.6, 1.2), (0.0, -4.6, 4.85)), target, m['iron'], matrix=frame, bevel=(0.08, 2, 40.0)))
    iron.append(pk.part('AnvilHardy', pk.box((0.5, 0.5, 0.1), (0.0, -3.6, 5.46)), target, m['steel'], matrix=frame))
    iron.append(pk.part('HammerHead', pk.box((1.6, 2.8, 1.6), (-3.0, 1.2, 2.2)), target, m['iron'], matrix=frame, bevel=(0.1, 2, 40.0)))
    iron.append(pk.part('HammerBand', sleeve((-3.0, 0.3, 2.2), (-3.0, 0.5, 2.2), 0.95, steps=4, lip=0.05), target, m['bronze'], matrix=frame, smooth=30.0))
    haft = pk.part('HammerHaft', pk.rod((-3.0, 1.2, 3.0), (-1.7, 1.0, 9.6), 0.32, 10), target, m['wood'], matrix=frame, smooth=35.0)
    iron.append(haft)
    iron.append(pk.part('HammerGrip', pk.rod((-1.85, 1.02, 8.8), (-1.68, 1.0, 9.7), 0.36, 10), target, m['bronze'], matrix=frame, smooth=35.0))
    stone = kit.join(parts, 'AnvilPlinth')
    metal = kit.join(iron, 'Anvil')
    return [stone, metal]


def sword(name, m, target, rng, frame, length):
    parts = []
    blade = prism_bm([(-0.12, -0.45), (0.12, -0.45), (0.12, 0.45), (-0.12, 0.45)], 0.0, 1.0)
    for v in blade.verts:
        z = v.co.z
        taper = 1.0 - 0.75 * z
        v.co = Vector((v.co.x * (1.0 - 0.5 * z), v.co.y * taper, 1.6 + z * length))
    tip = [v for v in blade.verts if v.co.z > 1.6 + length - 1e-3]
    for v in tip:
        v.co.y *= 0.05
        v.co.x *= 0.3
    parts.append(pk.part(f'{name}Blade', blade, target, m['steel'], matrix=frame, bevel=(0.03, 1, 30.0)))
    parts.append(pk.part(f'{name}Guard', pk.box((0.34, 2.0, 0.3), (0.0, 0.0, 1.5)), target, m['iron'], matrix=frame, bevel=(0.05, 2, 40.0)))
    parts.append(pk.part(f'{name}Grip', pk.cylinder(0.16, 1.1, 10, z=0.35), target, m['wood'], matrix=frame, smooth=35.0))
    parts.append(pk.part(f'{name}Pommel', pk.ellipsoid_bm((0.0, 0.0, 0.25), (0.3, 0.3, 0.26), segments=10, rings=6), target, m['iron'], matrix=frame, smooth=50.0))
    return parts


def axe(name, m, target, rng, frame, height):
    parts = [pk.part(f'{name}Haft', pk.cylinder(0.17, height, 10, z=0.2), target, m['wood'], matrix=frame, smooth=35.0)]
    for side in (-1.0, 1.0):
        points = [(side * 0.18, -0.35), (side * 0.18, 0.35), (side * 1.25, 0.95), (side * 1.55, 0.2), (side * 1.55, -0.2), (side * 1.25, -0.95)]
        head = prism_bm(points, -0.1, 0.1)
        bmesh.ops.transform(head, matrix=Matrix.Translation((0.0, 0.0, height - 0.6)) @ Matrix.Rotation(0.5 * math.pi, 4, 'X'), verts=head.verts)
        parts.append(pk.part(f'{name}Head{int(side)}', head, target, m['steel'], matrix=frame, bevel=(0.03, 1, 30.0)))
    parts.append(pk.part(f'{name}Collar', pk.cylinder(0.26, 0.9, 10, z=height - 1.05), target, m['iron'], matrix=frame, smooth=35.0))
    return parts


def rack(m, target, rng, x, y, z, yaw):
    frame = placed(x, y, z, yaw)
    stone = []
    for size, h in (((6.6, 9.8, 0.7), 0.35),):
        stone.append(pk.part(f"RackPlinth{int(h * 10)}", pk.chipped(size, rng, chips=3, depth=0.25, center=(0.0, 0.0, h)), target, m['masonry'], matrix=frame, bevel=(0.06, 2, 40.0)))
    metal = []
    for k, py in enumerate((-4.6, 4.6)):
        metal.append(pk.part(f'RackPost{k}', pk.box((0.5, 0.5, 7.2), (1.4, py, 4.3)), target, m['iron'], matrix=frame, bevel=(0.05, 2, 40.0)))
    metal.append(pk.part('RackBeam', pk.box((0.45, 9.8, 0.45), (1.4, 0.0, 7.4)), target, m['iron'], matrix=frame, bevel=(0.05, 2, 40.0)))
    metal.append(pk.part('RackRail', pk.box((0.4, 9.6, 0.35), (1.4, 0.0, 1.6)), target, m['iron'], matrix=frame, bevel=(0.04, 2, 40.0)))
    weapons = []
    for k, py in enumerate((-3.4, -1.4, 0.6, 2.6)):
        lean = Matrix.Translation((0.6, py, 0.7)) @ Matrix.Rotation(-0.1, 4, 'Y') @ Matrix.Rotation(0.5 * math.pi, 4, 'Z')
        weapons += sword(f'Sword{k}', m, target, rng, frame @ lean, rng.uniform(5.0, 5.8))
    for k, (px, py) in enumerate(((-1.4, -3.0), (-1.4, 0.0), (-1.4, 3.0))):
        lean = Matrix.Translation((px, py, 0.7)) @ Matrix.Rotation(0.12, 4, 'Y')
        weapons += axe(f'Axe{k}', m, target, rng, frame @ lean, rng.uniform(5.2, 6.0))
    return [kit.join(stone, 'RackPlinth'), kit.join(metal + weapons, 'Rack')]


def vat(index, m, target, rng):
    side = -1.0 if index == 0 else 1.0
    x, y = side * 13.2, 17.0
    frame = placed(x, y, 0.0, 0.0)
    stone = [pk.part(f'VatPlinth{index}', pk.chipped((8.6, 8.6, 5.0), rng, chips=4, depth=0.3, center=(0.0, 0.0, -1.5)), target, m['masonry'], matrix=frame, bevel=(0.06, 2, 40.0))]
    profile = [(0.0, 1.0), (2.6, 1.0), (3.2, 2.0), (3.5, 4.0), (3.35, 6.2), (3.05, 7.1), (3.25, 7.3), (3.25, 7.55), (2.95, 7.55), (2.85, 7.0), (0.0, 7.0)]
    metal = [pk.part(f'VatBody{index}', pk.lathe(profile, 36), target, m['iron'], matrix=frame, smooth=30.0)]
    for z in (1.9, 3.9, 6.0):
        r = 3.25 if z < 2.5 else (3.55 if z < 5.0 else 3.42)
        metal.append(pk.part(f'VatHoop{index}{int(z * 10)}', ring((0.0, 0.0, z), (0.0, 0.0, 1.0), r, 0.13, steps=48, sides=6), target, m['bronze'], matrix=frame, smooth=40.0))
        for k in range(16):
            a = 2.0 * math.pi * k / 16.0
            metal.append(pk.part(f'VatRivet{index}{int(z * 10)}_{k}', pk.ellipsoid_bm((math.cos(a) * (r + 0.14), math.sin(a) * (r + 0.14), z), (0.09, 0.09, 0.09), segments=6, rings=4), target, m['iron'], matrix=frame, smooth=60.0))
    for k in range(4):
        a = 0.5 * math.pi * k + 0.25 * math.pi
        metal.append(pk.part(f'VatLeg{index}{k}', pk.box((0.7, 0.7, 2.4), (math.cos(a) * 2.6, math.sin(a) * 2.6, 0.2)), target, m['iron'], matrix=frame, bevel=(0.05, 2, 40.0)))
    melt = pk.part(f'VatMelt{index}', pk.cylinder(2.88, 0.05, 32, z=7.05), target, m['molten'], matrix=frame, smooth=30.0)
    return [kit.join(stone, f'VatPlinth{index}'), kit.join(metal, f'Vat{index}'), kit.join([melt], f'VatMelt{index}')]


def banner_cloth(rng, columns=8, rows=18):
    bm = bmesh.new()
    grid = []
    for r in range(rows + 1):
        v = r / rows
        row = []
        for c in range(columns + 1):
            u = c / columns - 0.5
            z = -v * BANNER_H
            if r == rows:
                z += BANNER_H * 0.12 * (1.0 - abs(u) * 2.0)
            wave = 0.12 * math.sin(u * 7.0 + v * 2.0) * v + 0.06 * math.sin(v * 9.0)
            row.append(bm.verts.new((u * BANNER_W, wave, z)))
        grid.append(row)
    for r in range(rows):
        for c in range(columns):
            bm.faces.new((grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]))
    return bm


def banner(name, m, target, rng, frame):
    parts = [pk.part(f'{name}Cloth', banner_cloth(rng), target, m['banner'], matrix=frame)]
    parts.append(pk.part(f'{name}Bar', pk.rod((-BANNER_W * 0.6, -0.08, 0.1), (BANNER_W * 0.6, -0.08, 0.1), 0.12, 8), target, m['iron'], matrix=frame, smooth=40.0))
    return parts


def pillars(m, target, rng):
    objects = []
    spots = [(-12.5, -8.5), (12.5, -8.5), (-7.0, -18.0), (7.0, -18.0), (0.0, -19.0)]
    for index, (x, y) in enumerate(spots):
        parts = []
        for k in range(rng.randint(3, 5)):
            radius = rng.uniform(0.7, 1.3)
            top = rng.uniform(-1.6, 0.6) if y < 0.0 else rng.uniform(-0.5, 2.5)
            column = pk.cylinder(radius, top + 5.0, 6, z=-5.0)
            angle = rng.uniform(0.0, 2.0 * math.pi)
            frame = placed(x + math.cos(angle) * rng.uniform(0.0, 1.8), y + math.sin(angle) * rng.uniform(0.0, 1.8), 0.0, rng.uniform(0.0, math.pi))
            parts.append(pk.part(f'Pillar{index}_{k}', column, target, m['basalt'], matrix=frame, bevel=(0.05, 2, 40.0)))
        objects.append(kit.join(parts, f'Pillars{index}'))
    return objects


def channel(index, angle_degrees, m, target, rng, inner=18.6, outer=26.0):
    angle = math.radians(angle_degrees)
    length = outer - inner
    middle = 0.5 * (inner + outer)
    x, y = math.cos(angle) * middle, math.sin(angle) * middle
    frame = placed(x, y, 0.0, facing_center(x, y))
    stone = []
    stone.append(pk.part(f'ChannelBed{index}', pk.chipped((1.9, length, 0.5), rng, chips=2, depth=0.08, center=(0.0, 0.0, 0.15)), target, m['masonry'], matrix=frame, bevel=(0.04, 2, 40.0)))
    for side in (-1.0, 1.0):
        stone.append(pk.part(f'ChannelWall{index}{int(side)}', pk.box((0.35, length, 0.75), (side * 0.78, 0.0, 0.55)), target, m['masonry'], matrix=frame, bevel=(0.04, 2, 40.0)))
    for k in range(3):
        v = -0.5 * length + (k + 0.5) * length / 3.0
        stone.append(pk.part(f'ChannelPier{index}{k}', pk.chipped((1.6, 1.4, 4.2), rng, chips=3, depth=0.15, center=(0.0, v, -2.0)), target, m['basalt'], matrix=frame, bevel=(0.05, 2, 40.0)))
    metal = []
    for k in range(int(length / 1.6)):
        v = -0.5 * length + (k + 0.5) * length / int(length / 1.6)
        metal.append(pk.part(f'ChannelStrap{index}{k}', pk.box((2.0, 0.16, 0.1), (0.0, v, 0.95)), target, m['iron'], matrix=frame, bevel=(0.02, 1, 30.0)))
    melt = [pk.part(f'ChannelMeltTop{index}', pk.box((1.2, length + 0.1, 0.06), (0.0, 0.0, 0.48)), target, m['molten'], matrix=frame)]
    return [kit.join(stone, f'Channel{index}'), kit.join(metal, f'ChannelStraps{index}'), kit.join(melt, f'ChannelMelt{index}')]


def bellows(index, frame, m, target, rng):
    parts = []
    leather = []
    for k, (z, width) in enumerate(((1.0, 2.6), (2.0, 2.9), (3.0, 2.6))):
        leather.append(pk.part(f'BellowsFold{index}{k}', pk.chipped((width, 3.6 - 0.4 * k, 0.9), rng, chips=2, depth=0.06, center=(0.0, -0.3 * k, z)), target, m['leather'], matrix=frame, bevel=(0.12, 3, 40.0)))
    parts.append(pk.part(f"BellowsBoard{index}", pk.box((3.0, 4.2, 0.22), (0.0, 0.0, 0.45)), target, m["wood"], matrix=frame, bevel=(0.04, 2, 40.0)))
    leather.append(pk.part(f"BellowsLid{index}", pk.box((3.0, 4.2, 0.22), (0.0, 0.0, 3.6)), target, m["wood"], matrix=frame, bevel=(0.04, 2, 40.0)))
    parts.append(pk.part(f'BellowsNozzle{index}', pk.rod((0.0, 2.1, 2.0), (0.0, 3.4, 1.6), 0.32, 10, end_radius=0.18), target, m['bronze'], matrix=frame, smooth=40.0))
    parts.append(pk.part(f'BellowsLever{index}', pk.rod((0.0, -2.0, 3.7), (0.0, -3.6, 6.4), 0.18, 8), target, m['iron'], matrix=frame, smooth=40.0))
    parts.append(pk.part(f'BellowsPlinth{index}', pk.chipped((3.6, 5.0, 3.6), rng, chips=3, depth=0.2, center=(0.0, 0.0, -1.5)), target, m['masonry'], matrix=frame, bevel=(0.05, 2, 40.0)))
    return [kit.join(parts, f'Bellows{index}'), kit.join(leather, f"BellowsPump{index}")]


def gear(m, target, rng, matrix):
    teeth = 18
    radius, width = 3.2, 0.7
    profile = [(0.0, -width * 0.5), (radius, -width * 0.5), (radius, width * 0.5), (0.0, width * 0.5)]
    parts = [pk.part('GearDisc', pk.lathe(profile, 36), target, m['iron'], smooth=30.0)]
    for k in range(teeth):
        a = 2.0 * math.pi * k / teeth
        if k in (3, 4, 11):
            continue
        tooth = pk.box((0.6, 0.55, width * 0.9))
        bmesh.ops.transform(tooth, matrix=Matrix.Translation((math.cos(a) * (radius + 0.25), math.sin(a) * (radius + 0.25), 0.0)) @ Matrix.Rotation(a, 4, "Z"), verts=tooth.verts)
        parts.append(pk.part(f'GearTooth{k}', tooth, target, m['iron'], bevel=(0.04, 2, 40.0)))
    parts.append(pk.part('GearHub', pk.cylinder(0.9, width + 0.4, 16, z=-0.5 * width - 0.2), target, m['bronze'], smooth=40.0))
    obj = kit.join(parts, 'Gear')
    obj.matrix_world = matrix
    return obj


def sunken_blade(m, target, rng):
    frame = placed(10.5, -14.0, -3.2, 2.2) @ Matrix.Rotation(-0.55, 4, 'Y')
    parts = sword('Sunken', m, target, rng, frame @ Matrix.Rotation(0.5 * math.pi, 4, 'Z'), 4.2)
    obj = kit.join(parts, 'SunkenBlade')
    return obj


LAVA = -3.0
SUNK = -4.6


def xz_prism(points, y0, y1):
    bm = bmesh.new()
    front = [bm.verts.new((x, y1, z)) for x, z in points]
    back = [bm.verts.new((x, y0, z)) for x, z in points]
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    for i in range(len(points)):
        j = (i + 1) % len(points)
        bm.faces.new((back[i], back[j], front[j], front[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def arch_points(half, straight, steps=10, floor=0.0):
    points = [(-half, floor), (-half, straight)]
    points += [(-math.cos(math.pi * k / steps) * half, straight + math.sin(math.pi * k / steps) * half) for k in range(1, steps)]
    points += [(half, straight), (half, floor)]
    return points


def tunnel_bm(points, depth):
    bm = bmesh.new()
    front = [bm.verts.new((x, 0.0, z)) for x, z in points]
    back = [bm.verts.new((x, -depth, z)) for x, z in points]
    bm.faces.new(list(reversed(back)))
    for i in range(len(points)):
        j = (i + 1) % len(points)
        bm.faces.new((back[i], back[j], front[j], front[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    return bm


def column_bm(radius, bottom, top, rng, sides=6):
    pieces = []
    z = bottom
    drums = max(1, int((top - bottom) / rng.uniform(1.6, 2.6)))
    for k in range(drums):
        z1 = top if k == drums - 1 else z + (top - bottom) / drums * rng.uniform(0.8, 1.2)
        r = radius * rng.uniform(0.94, 1.04)
        sides_k = sides if rng.random() < 0.7 else sides - 1
        drum = pk.cylinder(r, max(z1 - z - 0.04, 0.1), sides_k, z=z, top=r * rng.uniform(0.95, 1.0))
        bmesh.ops.rotate(drum, verts=drum.verts, cent=(0.0, 0.0, 0.0), matrix=Matrix.Rotation(rng.uniform(-0.12, 0.12), 3, 'Z'))
        bmesh.ops.translate(drum, verts=drum.verts, vec=(rng.uniform(-0.04, 0.04) * radius, rng.uniform(-0.04, 0.04) * radius, 0.0))
        if rng.random() < 0.35:
            corner = rng.uniform(0.0, 2.0 * math.pi)
            normal = Vector((math.cos(corner), math.sin(corner), rng.uniform(0.3, 1.2))).normalized()
            kit.cut(drum, Vector((math.cos(corner) * r * 0.8, math.sin(corner) * r * 0.8, z1 - 0.05)), normal)
        pieces.append(drum)
        z = z1
    bm = pk.merge_bm(pieces)
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0.0, 0.0, 0.0), matrix=Matrix.Rotation(rng.uniform(0.0, math.pi), 3, 'Z'))
    if rng.random() < 0.5:
        normal = Vector((rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), 1.0)).normalized()
        kit.cut(bm, Vector((0.0, 0.0, top - rng.uniform(0.1, 0.4))), normal)
    return bm


def obsidian_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(p)
    warped = s.offset(p, s.mul(s.noise_color(p, 1.8, detail=2.0), 0.25))
    vein = s.sub(1.0, s.smooth(0.0, 0.025, s.voronoi(warped, 1.6, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
    vein = s.mul(vein, s.smooth(0.45, 0.65, s.noise(p, 1.3, detail=2.0)))
    low = s.sub(1.0, s.smooth(0.2, 2.2, z))
    glow = s.mul(vein, low)
    sheen = s.noise(p, 5.0, detail=3.0)
    base = s.mix(sheen, (0.01, 0.009, 0.012), (0.045, 0.04, 0.05))
    base = s.mix(s.mul(vein, 0.6), base, (0.14, 0.05, 0.03))
    rough = s.remap(sheen, 0.3, 0.7, 0.04, 0.2)
    rough = s.lerp(vein, rough, 0.7)
    return s.finish(base, rough, height=s.mul(vein, -1.0), bump=0.2, bump_distance=0.008, emission=glow, emission_color=(1.0, 0.33, 0.07), emission_strength=3.0, bake_size=bake_size)


def jitter(bm, rng, amount, keep_below=None):
    for v in bm.verts:
        if keep_below is not None and v.co.z < keep_below:
            continue
        v.co += Vector((rng.uniform(-amount, amount), rng.uniform(-amount, amount), rng.uniform(-amount, amount) * 0.5))


def spike_bm(base, tip, radius, rng, sides=7):
    bm = bmesh.new()
    a, b = Vector(base), Vector(tip)
    axis = b - a
    rings = 5
    rotation = axis.normalized().to_track_quat('Z', 'Y').to_matrix()
    twist = rng.uniform(-0.6, 0.6)
    loops = []
    for r in range(rings):
        t = r / rings
        width = radius * (1.0 - t) ** 1.15 * rng.uniform(0.85, 1.1)
        loop = []
        for k in range(sides):
            angle = 2.0 * math.pi * k / sides + twist * t + rng.uniform(-0.12, 0.12)
            local = Vector((math.cos(angle) * width, math.sin(angle) * width * rng.uniform(0.75, 1.0), 0.0))
            bend = Vector((math.sin(t * 2.0) * radius * 0.25, 0.0, 0.0))
            loop.append(bm.verts.new(a + rotation @ (local + bend) + axis * t))
        loops.append(loop)
    apex = bm.verts.new(b + rotation @ Vector((rng.uniform(-0.05, 0.05), rng.uniform(-0.05, 0.05), 0.0)))
    bm.faces.new(list(reversed(loops[0])))
    for r in range(rings - 1):
        for k in range(sides):
            j = (k + 1) % sides
            bm.faces.new((loops[r][k], loops[r][j], loops[r + 1][j], loops[r + 1][k]))
    for k in range(sides):
        bm.faces.new((loops[-1][k], loops[-1][(k + 1) % sides], apex))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def scatter_spike(index, m, target, rng):
    spikes = []
    height = rng.uniform(3.5, 5.5)
    spikes.append(spike_bm((0.0, 0.0, -1.0), (rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), height), rng.uniform(0.7, 0.95), rng))
    for k in range(rng.randint(3, 6)):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        dist = rng.uniform(0.5, 1.3)
        lean = rng.uniform(0.25, 0.9)
        h = height * rng.uniform(0.3, 0.7)
        base = (math.cos(angle) * dist, math.sin(angle) * dist, -1.0)
        tip = (base[0] + math.cos(angle) * lean * h * 0.5, base[1] + math.sin(angle) * lean * h * 0.5, h)
        spikes.append(spike_bm(base, tip, rng.uniform(0.3, 0.55), rng, sides=rng.choice((5, 6, 7))))
    obj = kit.mesh_object(f'ScatterSpike{index}', pk.merge_bm(spikes), target, m['obsidian'])
    pk.tag(obj)
    kit.smooth_by_angle(obj, 25.0)
    return obj


def scatter_rock(index, m, target, rng):
    shapes = []
    for k in range(rng.randint(3, 6)):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        dist = rng.uniform(0.0, 1.4)
        size = (rng.uniform(0.9, 1.8), rng.uniform(0.8, 1.6), rng.uniform(0.6, 1.5))
        shapes.append(pk.ellipsoid_bm((math.cos(angle) * dist, math.sin(angle) * dist, rng.uniform(-0.6, 0.3)), size, rotation=Vector((0.0, 0.0, 1.0)).rotation_difference(Vector((rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), 1.0)).normalized()), segments=14, rings=8))
    obj = pk.sculpt(shapes, target, m['basalt'], f'ScatterRock{index}', 0.12, smooth=2, faces=700, rng=rng)
    from mathutils import noise
    seed = Vector((rng.uniform(0, 50), rng.uniform(0, 50), rng.uniform(0, 50)))
    for v in obj.data.vertices:
        n = noise.noise(v.co * 1.4 + seed) * 0.28 + noise.noise(v.co * 3.7 + seed) * 0.08
        v.co += v.normal * n
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for k in range(3):
        normal = Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(0.2, 1.0))).normalized()
        kit.cut(bm, normal * rng.uniform(0.7, 1.2), normal)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.shade_flat()
    return obj


def basalt_island(index, m, target, rng):
    cx, cy = F['ForgeIslandCenters'][index]
    radius = F['ForgeIslandRadii'][index]
    top = F['ForgeIslandTops'][index]
    spacing = 1.08
    columns = []
    rows = int(radius / (spacing * 0.866)) + 1
    for j in range(-rows, rows + 1):
        for i in range(-rows - 1, rows + 2):
            x = (i + 0.5 * (j % 2)) * spacing + rng.uniform(-0.12, 0.12)
            y = j * spacing * 0.866 + rng.uniform(-0.12, 0.12)
            edge = math.hypot(x, y) / radius
            if edge > 1.0 + rng.uniform(-0.15, 0.05):
                continue
            height = top - edge * edge * rng.uniform(0.8, 2.0) + rng.uniform(-0.18, 0.12)
            if edge > 0.8 and rng.random() < 0.3:
                height = LAVA + rng.uniform(0.15, 0.7)
            bm = column_bm(rng.uniform(0.55, 0.64), SUNK, height, rng)
            bmesh.ops.translate(bm, verts=bm.verts, vec=W(cx + x, cy + y))
            columns.append(bm)
    obj = kit.mesh_object(f'Island{index}', pk.merge_bm(columns), target, m['basalt'])
    pk.tag(obj, rng=rng)
    kit.bevel(obj, 0.035, segments=1, angle=40.0)
    return obj


def furnace(name, x, y, base, size, opening, chimney, m, target, rng, banners=True):
    w, d, h = size
    half, straight = opening
    frame = placed(x, y, base, facing_center(x, y))
    stone, metal, glow = [], [], []
    body = pk.part(f'KilnBody{name}', pk.chipped((w, d, h), rng, chips=4, depth=0.35, center=(0.0, -0.5 * d, 0.5 * h)), target, m['masonry'], bevel=(0.08, 2, 40.0))
    cutter = kit.mesh_object(f'KilnCut{name}', xz_prism(arch_points(half, straight, floor=-0.5), -d * 0.6, 1.0), target)
    fg.boolean(body, cutter, 'DIFFERENCE')
    fg.remove(cutter)
    body.matrix_world = frame
    stone.append(body)
    stone.append(pk.part(f'KilnFoot{name}', pk.chipped((w + 2.4, d + 2.4, base - SUNK), rng, chips=4, depth=0.4, center=(0.0, -0.5 * d, 0.5 * (SUNK - base) - 0.02)), target, m['basalt'], matrix=frame, bevel=(0.06, 2, 40.0)))
    stone.append(pk.part(f'KilnLedge{name}', pk.chipped((2.0 * half + 4.0, 3.2, 0.7), rng, chips=2, depth=0.08, center=(0.0, 1.5, -0.35)), target, m['masonry'], matrix=frame, bevel=(0.05, 2, 40.0)))
    stone.append(pk.part(f'KilnCrown{name}', pk.box((w + 0.8, d + 0.8, 0.9), (0.0, -0.5 * d, h + 0.2)), target, m['masonry'], matrix=frame, bevel=(0.06, 2, 40.0)))
    inner = arch_points(half - 0.06, straight)
    liner = pk.part(f'Furnace{name}', tunnel_bm(inner, d * 0.55), target, m['furnace'])
    liner.matrix_world = frame @ Matrix.Translation((0.0, -0.02, 0.0))
    glow.append(liner)
    outer = arch_points(half + 0.65, straight, floor=-0.05)
    ring = list(reversed(arch_points(half - 0.02, straight, floor=-0.05)))
    metal.append(pk.part(f'KilnArch{name}', xz_prism(outer + ring, -0.1, 0.4), target, m['bronze'], matrix=frame, bevel=(0.03, 2, 40.0)))
    for side in (-1.0, 1.0):
        metal.append(pk.part(f'KilnPost{name}{int(side)}', pk.box((0.8, 0.8, h + 0.5), (side * (0.5 * w - 0.1), 0.25, 0.5 * (h + 0.5) - 0.3)), target, m['iron'], matrix=frame, bevel=(0.04, 2, 40.0)))
    for z in (0.45 * h, 0.82 * h):
        if z < straight + half + 0.8:
            for side in (-1.0, 1.0):
                span = 0.5 * w - half - 0.75
                metal.append(pk.part(f'KilnBand{name}{int(z * 10)}{int(side)}', pk.box((span, 0.3, 0.45), (side * (half + 0.75 + 0.5 * span), 0.12, z)), target, m['iron'], matrix=frame, bevel=(0.03, 2, 40.0)))
        else:
            metal.append(pk.part(f'KilnBand{name}{int(z * 10)}', pk.box((w, 0.3, 0.45), (0.0, 0.12, z)), target, m['iron'], matrix=frame, bevel=(0.03, 2, 40.0)))
    for k in range(5):
        bx = -half * 0.75 + k * half * 1.5 / 4.0
        metal.append(pk.part(f'KilnGrate{name}{k}', pk.box((0.16, 0.16, straight + 0.4), (bx, 0.45, 0.5 * (straight + 0.4))), target, m['iron'], matrix=frame, bevel=(0.02, 1, 30.0)))
    for k in range(4):
        px = -half - 1.6 + k * (2.0 * half + 3.2) / 3.0
        metal.append(pk.part(f'KilnRail{name}{k}', pk.box((0.14, 0.14, 1.2), (px, 2.9, 0.6)), target, m['iron'], matrix=frame))
    metal.append(pk.part(f'KilnRailBar{name}', pk.box((2.0 * half + 3.4, 0.12, 0.12), (0.0, 2.9, 1.2)), target, m['iron'], matrix=frame))
    if chimney > 0.0:
        cx, cy = 0.25 * w, -0.65 * d
        stone.append(pk.part(f'KilnStack{name}', pk.cylinder(1.7, chimney, 8, z=h + 0.6, top=1.25), target, m['masonry'], matrix=frame @ Matrix.Translation((cx, cy, 0.0)), smooth=20.0))
        for k in range(3):
            z = h + 0.6 + chimney * (0.25 + 0.3 * k)
            r = 1.7 - 0.45 * (z - h - 0.6) / chimney + 0.12
            metal.append(pk.part(f'KilnStackBand{name}{k}', pk.cylinder(r, 0.5, 8, z=z), target, m['iron'], matrix=frame @ Matrix.Translation((cx, cy, 0.0)), smooth=20.0))
    if banners:
        cloth = []
        for k, offset in enumerate((-1.0, 1.0)):
            cloth += banner(f'KilnBanner{name}{k}', m, target, rng, frame @ Matrix.Translation((offset * (half + 2.4), 0.45, min(h - 0.8, straight + half + 5.0))))
        stone.append(kit.join(cloth, f'KilnBanners{name}'))
    return [kit.join(stone, f'Kiln{name}'), kit.join(metal, f'KilnIron{name}'), kit.join(glow, f'Furnace{name}')], frame


def bridge(name, start, end, deck, width, m, target, rng):
    sx, sy = start
    ex, ey = end
    length = math.hypot(ex - sx, ey - sy)
    delta = W(ex, ey) - W(sx, sy)
    yaw = math.atan2(delta.y, delta.x)
    frame = Matrix.Translation(W(0.5 * (sx + ex), 0.5 * (sy + ey), 0.0)) @ Matrix.Rotation(yaw, 4, 'Z')
    wall = pk.part(f'BridgeWall{name}', pk.box((length, width, deck - SUNK), (0.0, 0.0, 0.5 * (deck + SUNK))), target, m['masonry'], bevel=(0.05, 2, 40.0))
    spans = max(1, int(round(length / 5.5)))
    pier = 1.2
    clear = (length - pier * (spans + 1)) / spans
    for k in range(spans):
        cx = -0.5 * length + pier + k * (clear + pier) + 0.5 * clear
        points = [(px + cx, pz) for px, pz in arch_points(0.5 * clear, deck - 1.1 - 0.5 * clear, floor=SUNK - 1.0)]
        cutter_bm = xz_prism(points, -width, width)
        cutter = kit.mesh_object(f'BridgeCut{name}{k}', cutter_bm, target)
        fg.boolean(wall, cutter, 'DIFFERENCE')
        fg.remove(cutter)
    wall.matrix_world = frame
    stone = [wall]
    stone.append(pk.part(f'BridgeDeck{name}', pk.chipped((length + 0.6, width + 0.5, 0.35), rng, chips=3, depth=0.06, center=(0.0, 0.0, deck + 0.17)), target, m["basalt"], matrix=frame, bevel=(0.04, 2, 40.0)))
    metal = []
    for side in (-1.0, 1.0):
        stone.append(pk.part(f'BridgeParapet{name}{int(side)}', pk.box((length + 0.6, 0.35, 0.9), (0.0, side * (0.5 * width + 0.07), deck + 0.75)), target, m['masonry'], matrix=frame, bevel=(0.04, 2, 40.0)))
        metal.append(pk.part(f'BridgeCap{name}{int(side)}', pk.box((length + 0.7, 0.45, 0.12), (0.0, side * (0.5 * width + 0.07), deck + 1.26)), target, m['bronze'], matrix=frame, bevel=(0.02, 2, 40.0)))
        for k in range(spans + 1):
            px = -0.5 * length + 0.5 * pier + k * (clear + pier)
            metal.append(pk.part(f'BridgePlate{name}{int(side)}{k}', pk.box((pier * 0.9, 0.12, 1.6), (px, side * (0.5 * width + 0.02), deck - 0.7)), target, m['iron'], matrix=frame, bevel=(0.02, 1, 30.0)))
    return [kit.join(stone, f'Bridge{name}'), kit.join(metal, f'BridgeIron{name}')]


def trough(name, start, end, height, m, target, rng):
    sx, sy = start
    ex, ey = end
    length = math.hypot(ex - sx, ey - sy)
    delta = W(ex, ey) - W(sx, sy)
    yaw = math.atan2(delta.y, delta.x)
    frame = Matrix.Translation(W(0.5 * (sx + ex), 0.5 * (sy + ey), height)) @ Matrix.Rotation(yaw, 4, 'Z')
    stone = [pk.part(f'TroughBed{name}', pk.chipped((length, 2.2, 0.6), rng, chips=2, depth=0.08, center=(0.0, 0.0, -0.3)), target, m['masonry'], matrix=frame, bevel=(0.04, 2, 40.0))]
    metal = []
    for side in (-1.0, 1.0):
        stone.append(pk.part(f'TroughWall{name}{int(side)}', pk.box((length, 0.4, 0.8), (0.0, side * 0.9, 0.4)), target, m['masonry'], matrix=frame, bevel=(0.04, 2, 40.0)))
        metal.append(pk.part(f'TroughLip{name}{int(side)}', pk.box((length, 0.5, 0.1), (0.0, side * 0.9, 0.84)), target, m['bronze'], matrix=frame, bevel=(0.02, 1, 30.0)))
    for k in range(max(2, int(length / 4.5))):
        px = -0.5 * length + 1.2 + k * (length - 2.4) / max(1, int(length / 4.5) - 1)
        stone.append(pk.part(f'TroughPier{name}{k}', pk.chipped((1.6, 1.8, height - SUNK), rng, chips=3, depth=0.15, center=(px, 0.0, 0.5 * (SUNK - height) - 0.3)), target, m['basalt'], matrix=frame, bevel=(0.05, 2, 40.0)))
    melt = [pk.part(f'ChannelMeltTop{name}', pk.box((length + 0.05, 1.4, 0.06), (0.0, 0.0, 0.42)), target, m['molten'], matrix=frame)]
    return [kit.join(stone, f'Trough{name}'), kit.join(metal, f'TroughIron{name}'), kit.join(melt, f'ChannelMelt{name}')]


def link_bm(scale=1.0):
    bm = pk.torus(0.3 * scale, 0.09 * scale, 8, 4)
    bmesh.ops.scale(bm, vec=(1.55, 1.0, 1.0), verts=bm.verts)
    return bm


def hanging_chain(name, top, bottom, m, target, scale=1.0):
    a, b = Vector(W(*top)), Vector(W(*bottom))
    axis = b - a
    length = axis.length
    pitch = 0.62 * scale
    count = max(1, int(length / pitch))
    direction = axis.normalized()
    links = []
    for k in range(count):
        bm = link_bm(scale)
        rotation = direction.to_track_quat('X', 'Z').to_matrix().to_4x4() @ Matrix.Rotation(0.5 * math.pi * (k % 2), 4, 'X')
        bmesh.ops.transform(bm, matrix=Matrix.Translation(a + direction * (pitch * (k + 0.5))) @ rotation, verts=bm.verts)
        links.append(bm)
    obj = kit.mesh_object(name, pk.merge_bm(links), target, m['chain'])
    pk.tag(obj)
    kit.smooth_by_angle(obj, 50.0)
    return obj


def gantry(m, target, rng):
    parts = []
    for side in (-1.0, 1.0):
        x = side * 11.5
        parts.append(pk.part(f'GantryLeg{int(side)}', pk.box((1.3, 1.3, 36.0), W(x, 21.0, 13.0)), target, m['iron'], bevel=(0.05, 2, 40.0)))
        parts.append(pk.part(f'GantryFoot{int(side)}', pk.chipped((3.2, 3.2, 3.0), rng, chips=3, depth=0.2, center=tuple(W(x, 21.0, -3.1))), target, m['basalt'], bevel=(0.05, 2, 40.0)))
        for z in (4.0, 12.0, 20.0):
            parts.append(pk.part(f'GantryCollar{int(side)}{int(z)}', pk.box((1.6, 1.6, 0.5), W(x, 21.0, z)), target, m['bronze'], bevel=(0.03, 2, 40.0)))
        parts.append(pk.part(f'GantryBrace{int(side)}', pk.rod(W(x, 21.0, 22.0), W(side * 4.0, 21.0, 30.5), 0.35, 8), target, m['iron'], smooth=40.0))
    parts.append(pk.part('GantryBeam', pk.box((25.0, 1.6, 1.6), W(0.0, 21.0, 31.0)), target, m['iron'], bevel=(0.05, 2, 40.0)))
    objects = [kit.join(parts, 'Gantry')]
    for side in (-1.0, 1.0):
        objects.append(hanging_chain(f'GantryChain{int(side)}', (side * 3.0, 21.0, 30.2), (side * BEARING, PIVOT[1], PIVOT[2] + 0.55), m, target, 1.25))
    return objects


def hanging_crucible(name, x, y, z, scale, m, target, rng):
    frame = Matrix.Translation(W(x, y, z)) @ Matrix.Scale(scale, 4)
    profile = [(0.0, -0.9), (0.7, -0.9), (0.95, -0.6), (1.1, 0.2), (1.2, 0.85), (1.05, 0.95), (0.98, 0.5), (0.0, 0.5)]
    parts = [pk.part(f'SmallCrucibleBody{name}', pk.lathe(profile, 20), target, m['iron'], matrix=frame, smooth=35.0)]
    parts.append(pk.part(f'SmallCrucibleHoop{name}', ring((0.0, 0.0, 0.3), (0.0, 0.0, 1.0), 1.15, 0.08, steps=24, sides=6), target, m['bronze'], matrix=frame, smooth=40.0))
    melt = pk.part(f'SmallCrucibleMelt{name}', pk.cylinder(0.97, 0.05, 20, z=0.62), target, m['molten'], matrix=frame, smooth=30.0)
    objects = [kit.join(parts, f'SmallCrucible{name}'), kit.join([melt], f'VatMeltSmall{name}')]
    top = (x, y, 17.0)
    for k in range(3):
        a = 2.0 * math.pi * k / 3.0
        objects.append(hanging_chain(f'CrucibleChain{name}{k}', top, (x + math.cos(a) * 1.1 * scale, y + math.sin(a) * 1.1 * scale, z + 0.9 * scale), m, target, 0.8))
    return objects


def crane(m, target, rng):
    mast = (-24.0, -18.0)
    tip = (-20.0, -14.6)
    base = SUNK
    parts = [pk.part('CraneMast', pk.box((1.0, 1.0, 9.0 - base), W(mast[0], mast[1], 0.5 * (9.0 + base))), target, m['iron'], bevel=(0.04, 2, 40.0))]
    parts.append(pk.part("CraneFoot", pk.chipped((3.2, 3.2, LAVA + 1.0 - SUNK), rng, chips=3, depth=0.2, center=tuple(W(mast[0], mast[1], 0.5 * (LAVA + 1.0 + SUNK)))), target, m["basalt"], bevel=(0.05, 2, 40.0)))
    parts.append(pk.part('CraneBoom', pk.rod(W(mast[0], mast[1], 8.4), W(tip[0], tip[1], 7.8), 0.38, 8), target, m['iron'], smooth=40.0))
    parts.append(pk.part('CraneStay', pk.rod(W(mast[0], mast[1], 4.0), W(0.5 * (mast[0] + tip[0]), 0.5 * (mast[1] + tip[1]), 8.1), 0.22, 8), target, m['iron'], smooth=40.0))
    parts.append(pk.part('CraneSheave', ring(W(tip[0], tip[1], 7.5), (1.0, 0.0, 0.0), 0.5, 0.12, steps=16, sides=6), target, m['bronze'], smooth=40.0))
    parts.append(pk.part('CraneDrum', pk.rod(W(mast[0] - 0.9, mast[1], LAVA + 1.9), W(mast[0] + 0.9, mast[1], LAVA + 1.9), 0.55, 12), target, m['bronze'], smooth=40.0))
    parts.append(pk.part('CraneWeight', pk.chipped((1.6, 1.6, 1.8), rng, chips=2, depth=0.06, center=tuple(W(tip[0], tip[1], 2.2))), target, m['iron'], bevel=(0.04, 2, 40.0)))
    objects = [kit.join(parts, 'Crane')]
    objects.append(hanging_chain("CraneChain", (tip[0], tip[1], 7.2), (tip[0], tip[1], 3.1), m, target, 0.9))
    objects.append(gear(m, target, rng, Matrix.Translation(W(mast[0] + 1.4, mast[1] - 2.2, LAVA + 3.4)) @ Matrix.Rotation(0.5 * math.pi, 4, 'X') @ Matrix.Scale(0.8, 4)))
    return objects


def ingots(m, target, rng, x, y, z):
    frame = placed(x, y, z, rng.uniform(0.0, math.pi))
    bars = []
    for layer in range(3):
        for k in range(4 - layer):
            px = (k - 0.5 * (3 - layer)) * 0.85
            bm = pk.chipped((0.7, 1.9, 0.38), rng, chips=1, depth=0.03, center=(px, 0.0, 0.19 + layer * 0.38))
            bars.append(pk.part(f'Ingot{x:.0f}{layer}{k}', bm, target, m['gold'], matrix=frame @ Matrix.Rotation(0.5 * math.pi * layer, 4, 'Z'), bevel=(0.03, 1, 40.0)))
    return kit.join(bars, f'Ingots{abs(int(x))}')


def cliffs(m, target, rng):
    sectors = {}
    angle = 0.0
    while angle < 360.0:
        a = angle % 360.0
        skip = abs(a - 32.0) < 13.0 or abs(a - 148.0) < 13.0 or 172.0 < a < 210.0 or 72.0 < a < 108.0
        if not skip:
            south = 200.0 < a < 340.0
            r = rng.uniform(31.0, 35.0)
            cx, cy = math.cos(math.radians(a)) * r, math.sin(math.radians(a)) * r
            for k in range(rng.randint(3, 5)):
                ox, oy = rng.uniform(-2.0, 2.0), rng.uniform(-2.0, 2.0)
                height = rng.uniform(5.0, 12.0) if south else rng.uniform(12.0, 30.0)
                bm = column_bm(rng.uniform(1.4, 2.4), SUNK, height, rng)
                bmesh.ops.translate(bm, verts=bm.verts, vec=W(cx + ox, cy + oy))
                sectors.setdefault(int(a // 45.0), []).append(bm)
            if not south and rng.random() < 0.5:
                bm = pk.chipped((rng.uniform(5.0, 8.0), rng.uniform(4.0, 6.0), rng.uniform(16.0, 26.0)), rng, chips=4, depth=0.6, center=(0.0, 0.0, 6.0))
                bmesh.ops.transform(bm, matrix=Matrix.Translation(W(cx * 1.12, cy * 1.12)) @ Matrix.Rotation(rng.uniform(0.0, math.pi), 4, 'Z'), verts=bm.verts)
                sectors.setdefault(int(a // 45.0), []).append(bm)
        angle += rng.uniform(6.0, 9.0)
    objects = []
    for sector, shapes in sectors.items():
        obj = kit.mesh_object(f'Cliff{sector}', pk.merge_bm(shapes), target, m['basalt'])
        pk.tag(obj, rng=rng)
        kit.bevel(obj, 0.05, segments=1, angle=40.0)
        objects.append(obj)
    return objects


def pipes(m, target, rng):
    objects = []
    runs = [
        [W(-27.5, 4.0, 9.0), W(-24.5, 6.0, 9.5), W(-21.5, 10.5, 7.0), W(-19.5, 12.0, 4.5)],
        [W(26.5, 9.5, 11.0), W(29.0, 4.0, 10.0), W(31.0, -2.0, 6.0), W(30.5, -6.0, 1.0)],
        [W(-30.0, -12.0, 7.0), W(-25.5, -15.5, 5.5), W(-22.5, -16.0, 3.0)],
    ]
    for k, points in enumerate(runs):
        objects.append(pk.tube(points, 0.42, target, m['bronze'], f'Pipe{k}', resolution=6, sides=10, rng=rng))
    return objects


def polar(degrees, radius):
    return math.cos(math.radians(degrees)) * radius, math.sin(math.radians(degrees)) * radius


def build(target):
    rng = random.Random(97)
    m = materials()
    tops = F['ForgeIslandTops']
    objects = titan(m, target, rng)
    objects.append(cradle(m, target, rng))
    objects += crucible(m, target, rng)
    objects += vat(0, m, target, rng) + vat(1, m, target, rng)
    objects += gantry(m, target, rng)
    for index in range(len(F['ForgeIslandCenters'])):
        objects.append(basalt_island(index, m, target, rng))
    parts, frame = furnace('NE', *polar(32.0, 30.0), 0.5, (11.0, 9.0, 13.0), (2.2, 3.2), 16.0, m, target, rng)
    objects += parts + bellows(0, frame @ Matrix.Translation((8.4, -3.2, -0.2)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Z'), m, target, rng)
    parts, frame = furnace('NW', *polar(148.0, 30.0), 0.5, (10.0, 9.0, 11.5), (2.0, 3.0), 20.0, m, target, rng)
    objects += parts + bellows(1, frame @ Matrix.Translation((-8.0, -3.2, -0.2)) @ Matrix.Rotation(-0.5 * math.pi, 4, 'Z'), m, target, rng)
    objects += furnace('W1', *polar(183.0, 30.5), 0.0, (9.5, 9.0, 10.5), (1.9, 2.8), 14.0, m, target, rng)[0]
    objects += furnace('W2', *polar(201.0, 30.5), 0.0, (9.0, 8.5, 12.5), (1.7, 3.4), 18.0, m, target, rng)[0]
    objects.append(pk.part('KilnLink', pk.chipped((7.0, 7.0, 13.0), rng, chips=3, depth=0.4, center=tuple(W(*polar(192.0, 33.5), 1.9))), target, m['masonry'], bevel=(0.06, 2, 40.0)))
    objects += furnace('SE', 21.0, -19.0, tops[0], (5.5, 4.5, 5.0), (1.2, 1.6), 6.0, m, target, rng, banners=False)[0]
    objects += furnace('SW', -21.5, -12.0, tops[2], (5.0, 4.5, 4.8), (1.1, 1.5), 7.0, m, target, rng, banners=False)[0]
    objects += rack(m, target, rng, 15.5, -14.0, tops[0], 0.0)
    objects.append(ingots(m, target, rng, 19.5, -12.0, tops[0]))
    objects.append(ingots(m, target, rng, -16.0, -12.5, tops[2]))
    objects += anvil(m, target, rng, 26.0, -1.5, tops[1], 0.5 * math.pi)
    objects += bridge('ESE', (24.3, -4.5), (19.6, -11.8), -0.9, 2.6, m, target, rng)
    objects += bridge('East', (29.5, -2.5), (44.0, -5.0), -0.6, 3.0, m, target, rng)
    objects += bridge('West', (-19.8, -10.0), (-23.8, 5.6), -0.8, 2.8, m, target, rng)
    objects += channel(0, 32.0, m, target, rng, 18.6, 25.6) + channel(1, 148.0, m, target, rng, 18.6, 25.6)
    objects += trough('S', (4.0, -33.0), (4.0, -21.0), 0.6, m, target, rng)
    objects += hanging_crucible('A', -18.5, 10.0, 3.5, 1.5, m, target, rng)
    objects += hanging_crucible('B', -23.0, 4.5, 6.0, 1.9, m, target, rng)
    objects += crane(m, target, rng)
    objects.append(sunken_blade(m, target, rng))
    objects += pillars(m, target, rng)
    objects += cliffs(m, target, rng)
    objects += pipes(m, target, rng)
    for index in range(3):
        objects.append(scatter_spike(index, m, target, rng))
        objects.append(scatter_rock(index, m, target, rng))
    for obj in objects:
        kit.select_only([obj])
        with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
            bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return objects


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    started = time.perf_counter()
    objects = build(target)
    print(f'BUILD {time.perf_counter() - started:.1f} s')
    pk.unwrap_all(objects, {'TitanStone': 0.002, 'CavernMasonry': 0.002, 'TitanBronze': 0.003})
    for obj in objects:
        print('TRIS', obj.name, sum(len(p.vertices) - 2 for p in obj.data.polygons))
    if args.get('--preview'):
        folder = args['--preview']
        kit.environment('forest.exr', strength=0.25)
        moving = [obj for obj in objects if obj.name.startswith('Crucible')]
        for obj in moving:
            obj.matrix_world = Matrix.Translation(W(*PIVOT))
        titan_parts = [obj for obj in objects if obj.name.startswith(('Titan', 'Crucible', 'Cradle'))]
        kit.render(os.path.join(folder, 'backdrop_titan.png'), titan_parts, yaw=200.0, pitch=35.0, fov=40.0, fill=0.8, size=(1280, 720), samples=32)
        kit.render(os.path.join(folder, 'backdrop_hands.png'), [obj for obj in objects if obj.name.startswith(('TitanArms', 'Crucible', 'Cradle'))], yaw=180.0, pitch=57.0, fov=40.0, fill=0.9, size=(1280, 720), samples=32)
        kit.render(os.path.join(folder, 'backdrop_top.png'), objects, yaw=180.0, pitch=70.0, fill=1.0, size=(1280, 720), samples=32)
        for obj in moving:
            obj.matrix_world = Matrix.Identity(4)
    if args.get('--out'):
        started = time.perf_counter()
        pk.export(objects, args['--out'], NAME, ao_distance=1.2)
        print(f'EXPORT {time.perf_counter() - started:.1f} s')


if __name__ == '__main__':
    main()
