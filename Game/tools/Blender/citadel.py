import bmesh
import math
import os
import random
import sys
import time
from mathutils import Matrix, Quaternion, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import emberhold
import nature
import ruins

NAME = 'Citadel'
UP = Vector((0.0, 0.0, 1.0))
BLOCK_BEVEL = (0.03, 2, 30.0)


def block(name, size, center, m, target, rng, yaw=0.0, pitch=0.0, chips=3, depth=0.04):
    bm = pk.chipped(size, rng, chips=chips, depth=depth)
    matrix = Matrix.Translation(center) @ Matrix.Rotation(yaw, 4, 'Z') @ Matrix.Rotation(pitch, 4, 'X')
    bmesh.ops.transform(bm, matrix=matrix, verts=bm.verts)
    return pk.part(name, bm, target, m, frame=matrix, rng=rng, bevel=BLOCK_BEVEL)


def plinth(prefix, m, target, rng):
    parts = [
        block(f'{prefix}_Step', (1.42, 1.42, 0.24), (0.0, 0.0, 0.12), m['stone'], target, rng, chips=5, depth=0.06),
        block(f'{prefix}_Die', (1.06, 1.06, 0.5), (0.0, 0.0, 0.49), m['stone'], target, rng, chips=4, depth=0.05),
        block(f'{prefix}_Cornice', (1.24, 1.24, 0.14), (0.0, 0.0, 0.81), m['marble'], target, rng, chips=5, depth=0.05),
    ]
    for side in (-1.0, 1.0):
        sun = pk.torus(0.17, 0.018, 24, 6)
        bmesh.ops.transform(sun, matrix=Matrix.Translation((side * 0.0, side * -0.54, 0.49)) @ Matrix.Rotation(0.5 * math.pi, 4, 'X'), verts=sun.verts)
        parts.append(pk.part(f'{prefix}_Sun{int(side)}', sun, target, m['marble'], rng=rng, smooth=40.0))
    return parts


def robe_figure(base, name, m, target, rng, hood=True):
    up = lambda x, y, z: (x, y, base + z)
    shapes = [
        pk.capsule_bm(up(0.0, 0.0, 0.1), up(0.0, 0.0, 1.32), 0.4, 0.27),
        pk.ellipsoid_bm(up(0.0, 0.02, 1.33), (0.35, 0.22, 0.2)),
        pk.capsule_bm(up(0.0, 0.2, 0.08), up(0.0, 0.1, 1.3), 0.3, 0.14),
        pk.capsule_bm(up(0.2, -0.13, 0.06), up(0.13, -0.11, 0.95), 0.14, 0.09),
        pk.capsule_bm(up(-0.21, -0.12, 0.06), up(-0.12, -0.12, 0.95), 0.13, 0.09),
        pk.capsule_bm(up(0.0, -0.2, 0.05), up(0.0, -0.16, 0.92), 0.1, 0.07),
        pk.capsule_bm(up(0.34, 0.04, 0.06), up(0.25, 0.02, 1.0), 0.11, 0.08),
        pk.capsule_bm(up(-0.34, 0.04, 0.06), up(-0.25, 0.02, 1.0), 0.11, 0.08),
        pk.capsule_bm(up(0.32, 0.0, 1.38), up(0.3, -0.1, 0.98), 0.1, 0.09),
        pk.capsule_bm(up(0.3, -0.1, 0.98), up(0.08, -0.25, 1.0), 0.09, 0.08),
        pk.capsule_bm(up(-0.32, 0.0, 1.38), up(-0.3, -0.1, 0.98), 0.1, 0.09),
        pk.capsule_bm(up(-0.3, -0.1, 0.98), up(-0.08, -0.25, 1.0), 0.09, 0.08),
        pk.ellipsoid_bm(up(0.0, -0.3, 1.0), (0.1, 0.07, 0.07)),
    ]
    carve = None
    if hood:
        shapes += [
            pk.ellipsoid_bm(up(0.0, 0.06, 1.72), (0.2, 0.23, 0.25)),
            pk.capsule_bm(up(0.0, 0.14, 1.82), up(0.0, 0.26, 1.46), 0.13, 0.1),
            pk.cone_bm(up(0.0, 0.1, 1.86), up(0.0, 0.14, 2.08), 0.13, sides=10),
            pk.ellipsoid_bm(up(0.0, -0.02, 1.47), (0.26, 0.2, 0.09)),
        ]
        carve = [pk.ellipsoid_bm(up(0.0, -0.19, 1.68), (0.11, 0.11, 0.15))]
    else:
        shapes += [pk.ellipsoid_bm(up(0.0, 0.0, 1.7), (0.14, 0.15, 0.17)), pk.capsule_bm(up(0.0, 0.0, 1.42), up(0.0, 0.0, 1.62), 0.1, 0.085)]
    return pk.sculpt(shapes, target, m['marble'], name, voxel=0.016, smooth=4, smooth_factor=0.45, faces=7000, rng=rng, carve=carve)


def statue_robed(m, target, rng):
    parts = plinth('StatueA', m, target, rng)
    parts.append(robe_figure(0.88, 'StatueA_Figure', m, target, rng))
    orb = pk.ellipsoid_bm((0.0, -0.38, 0.88 + 1.08), (0.17, 0.17, 0.17), segments=24, rings=14)
    parts.append(pk.part('StatueA_Orb', orb, target, m['brass'], rng=rng, smooth=50.0))
    band = pk.torus(0.2, 0.014, 32, 6)
    bmesh.ops.transform(band, matrix=Matrix.Translation((0.0, -0.38, 0.88 + 1.08)) @ Matrix.Rotation(0.5, 4, 'Y'), verts=band.verts)
    parts.append(pk.part('StatueA_Band', band, target, m['brass'], rng=rng, smooth=40.0))
    return kit.join(parts, 'Statue_A')


def telescope(m, target, rng):
    parts = []
    stand = pk.lathe([(0.62, 0.0), (0.6, 0.1), (0.42, 0.16), (0.2, 0.3), (0.13, 0.6), (0.11, 1.15), (0.17, 1.26), (0.2, 1.4), (0.0, 1.4)], 32)
    parts.append(pk.part('Telescope_Stand', stand, target, m['brass'], rng=rng, smooth=40.0))
    for k in range(3):
        angle = k * 2.0 * math.pi / 3.0 + 0.5
        leg = pk.rod((math.cos(angle) * 0.14, math.sin(angle) * 0.14, 0.95), (math.cos(angle) * 0.72, math.sin(angle) * 0.72, 0.06), 0.045, 8, end_radius=0.03)
        parts.append(pk.part(f'Telescope_Leg{k}', leg, target, m['bronze'], rng=rng, smooth=40.0))
    for side in (-1.0, 1.0):
        arm = pk.chipped((0.07, 0.3, 0.62), rng, chips=1, depth=0.01, center=(side * 0.4, 0.0, 1.62))
        parts.append(pk.part(f'Telescope_Fork{int(side)}', arm, target, m['bronze'], rng=rng, bevel=(0.018, 1, 30.0)))
    fork = pk.chipped((0.88, 0.2, 0.09), rng, chips=1, depth=0.01, center=(0.0, 0.0, 1.36))
    parts.append(pk.part('Telescope_Yoke', fork, target, m['bronze'], rng=rng, bevel=(0.018, 1, 30.0)))
    profile = [(0.0, -1.5), (0.12, -1.5), (0.13, -1.38), (0.2, -1.3), (0.21, -1.0), (0.235, -0.96), (0.26, -0.92), (0.26, -0.6), (0.3, -0.56), (0.31, -0.5), (0.31, 0.5), (0.35, 0.56), (0.36, 0.62), (0.36, 1.45), (0.42, 1.5), (0.43, 1.74), (0.38, 1.76), (0.34, 1.7), (0.0, 1.7)]
    tube = pk.lathe(profile, 40)
    aim = Matrix.Translation((0.0, 0.0, 1.8)) @ Matrix.Rotation(math.radians(-52.0), 4, 'X') @ Matrix.Rotation(0.0, 4, 'Z')
    bmesh.ops.transform(tube, matrix=aim, verts=tube.verts)
    parts.append(pk.part('Telescope_Tube', tube, target, m['brass'], rng=rng, smooth=35.0))
    pivot = pk.rod((-0.42, 0.0, 1.8), (0.42, 0.0, 1.8), 0.07, 12)
    parts.append(pk.part('Telescope_Pivot', pivot, target, m['bronze'], rng=rng, smooth=40.0))
    for at in (-1.0, -0.2, 0.75):
        ring = pk.torus(0.33 if at > -0.5 else 0.27, 0.03, 40, 6)
        bmesh.ops.transform(ring, matrix=aim @ Matrix.Translation((0.0, 0.0, at)), verts=ring.verts)
        parts.append(pk.part(f'Telescope_Ring{int(at * 10)}', ring, target, m['bronze'], rng=rng, smooth=40.0))
    return kit.join(parts, 'Telescope')


def armillary_ring(name, radius, width, ticks, m, target, rng):
    bm = bmesh.new()
    steps = 96
    profile = ((radius - width, -0.05), (radius, -0.05), (radius, 0.05), (radius - width, 0.05))
    rows = []
    for i in range(steps):
        angle = 2.0 * math.pi * i / steps
        c, s = math.cos(angle), math.sin(angle)
        rows.append([bm.verts.new((r * c, r * s, z)) for r, z in profile])
    for i in range(steps):
        a, b = rows[i], rows[(i + 1) % steps]
        for k in range(4):
            n = (k + 1) % 4
            bm.faces.new((a[k], b[k], b[n], a[n]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    parts = [pk.part(name + '_Band', bm, target, m['brass'], rng=rng, smooth=30.0)]
    for k in range(ticks):
        angle = 2.0 * math.pi * k / ticks
        tick = pk.box((0.045, width * 0.7, 0.12), (0.0, 0.0, 0.0))
        bmesh.ops.transform(tick, matrix=Matrix.Rotation(angle - 0.5 * math.pi, 4, 'Z') @ Matrix.Translation((0.0, radius - width * 0.5, 0.0)), verts=tick.verts)
        parts.append(pk.part(f'{name}_Tick{k}', tick, target, m['bronze'], rng=rng))
    return kit.join(parts, name)


def armillary_core(m, target, rng):
    bm = bmesh.new()
    sides = 6
    points = [(0.0, -0.62), (0.26, -0.2), (0.3, 0.14), (0.0, 0.62)]
    top = None
    rows = []
    for r, z in points[1:3]:
        rows.append([bm.verts.new((r * math.cos(i * 2.0 * math.pi / sides), r * math.sin(i * 2.0 * math.pi / sides), z)) for i in range(sides)])
    low = bm.verts.new((0.0, 0.0, points[0][1]))
    high = bm.verts.new((0.0, 0.0, points[3][1]))
    for i in range(sides):
        n = (i + 1) % sides
        bm.faces.new((low, rows[0][n], rows[0][i]))
        bm.faces.new((rows[0][i], rows[0][n], rows[1][n], rows[1][i]))
        bm.faces.new((rows[1][i], rows[1][n], high))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return pk.part('Armillary_Core', bm, target, m['core'], rng=rng)


def fallen_ring(m, target, rng):
    bm = pk.torus(1.85, 0.1, 96, 8)
    bmesh.ops.scale(bm, vec=Vector((1.0, 1.0, 0.8)), verts=bm.verts)
    bmesh.ops.transform(bm, matrix=Matrix.Translation((0.0, 0.0, 0.1)) @ Matrix.Rotation(0.03, 4, 'X'), verts=bm.verts)
    parts = [pk.part('FallenRing_Band', bm, target, m['brass'], rng=rng, smooth=40.0)]
    for k in range(24):
        angle = 2.0 * math.pi * k / 24.0
        tick = pk.box((0.05, 0.16, 0.05), (0.0, 0.0, 0.0))
        bmesh.ops.transform(tick, matrix=Matrix.Translation((math.cos(angle) * 1.85, math.sin(angle) * 1.85, 0.2)) @ Matrix.Rotation(angle - 0.5 * math.pi, 4, 'Z'), verts=tick.verts)
        parts.append(pk.part(f'FallenRing_Tick{k}', tick, target, m['bronze'], rng=rng))
    return kit.join(parts, 'FallenRing')


def instrument(m, target, rng):
    parts = [
        block('Instrument_Base', (0.86, 0.86, 0.22), (0.0, 0.0, 0.11), m['stone'], target, rng, chips=4, depth=0.05),
        block('Instrument_Die', (0.56, 0.56, 0.5), (0.0, 0.0, 0.47), m['marble'], target, rng, chips=3, depth=0.04),
        block('Instrument_Cap', (0.7, 0.7, 0.1), (0.0, 0.0, 0.77), m['marble'], target, rng, chips=3, depth=0.03),
    ]
    stem = pk.lathe([(0.1, 0.82), (0.06, 0.9), (0.05, 1.0), (0.0, 1.0)], 16)
    parts.append(pk.part('Instrument_Stem', stem, target, m['brass'], rng=rng, smooth=40.0))
    for radius, tilt, spin in ((0.34, 1.2, 0.2), (0.28, 0.4, 1.3), (0.22, 1.9, 2.4)):
        ring = pk.torus(radius, 0.016, 40, 6)
        bmesh.ops.transform(ring, matrix=Matrix.Translation((0.0, 0.0, 1.3)) @ Matrix.Rotation(spin, 4, 'Z') @ Matrix.Rotation(tilt, 4, 'X'), verts=ring.verts)
        parts.append(pk.part(f'Instrument_Ring{int(radius * 100)}', ring, target, m['brass'], rng=rng, smooth=40.0))
    return kit.join(parts, 'Instrument_A')


def fallen_column(m, target, rng):
    parts = []
    length = 3.7
    joints = [-length * 0.5, -0.55, 0.62, length * 0.5 - 0.1]
    lying = Matrix.Translation((0.0, 0.0, 0.39)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Y')
    for index in range(3):
        bm = ruins.fluted_drum(joints[index], joints[index + 1], radius_at=lambda z: 0.385 - 0.012 * (z + 2.0), depth=0.024)
        ruins.chip_rim(bm, rng, joints[index], 0.38, 3, 0.05, -1.0)
        ruins.chip_rim(bm, rng, joints[index + 1], 0.38, 3, 0.05, 1.0)
        twist = Matrix.Rotation(rng.uniform(0.0, 6.28), 4, 'Z')
        bmesh.ops.transform(bm, matrix=lying @ twist, verts=bm.verts)
        parts.append(pk.part(f'FallenColumn_Drum{index}', bm, target, m['marble'], rng=rng, bevel=ruins.DRUM_BEVEL))
    capital = pk.chipped((1.0, 0.98, 0.26), rng, chips=5, depth=0.07)
    bmesh.ops.transform(capital, matrix=Matrix.Translation((length * 0.5 + 0.06, 0.05, 0.44)) @ Matrix.Rotation(0.5 * math.pi + 0.08, 4, 'Y'), verts=capital.verts)
    parts.append(pk.part('FallenColumn_Capital', capital, target, m['marble'], rng=rng, bevel=BLOCK_BEVEL))
    plinth_block = pk.chipped((0.95, 0.95, 0.24), rng, chips=4, depth=0.06)
    bmesh.ops.transform(plinth_block, matrix=Matrix.Translation((-length * 0.5 - 0.16, -0.03, 0.46)) @ Matrix.Rotation(0.5 * math.pi - 0.05, 4, 'Y'), verts=plinth_block.verts)
    parts.append(pk.part('FallenColumn_Plinth', plinth_block, target, m['marble'], rng=rng, bevel=BLOCK_BEVEL))
    return kit.join(parts, 'FallenColumn_A')


def vine(name, length, m, target, rng, sway=0.25, strands=3):
    placements = []
    stems = []
    for strand in range(strands):
        x = rng.uniform(-0.35, 0.35)
        y = rng.uniform(-0.04, 0.05)
        run = length * rng.uniform(0.65, 1.0)
        phase = rng.uniform(0.0, 6.28)
        points = []
        steps = max(4, int(run / 0.45))
        for k in range(steps + 1):
            t = k / steps
            points.append(Vector((x + sway * math.sin(t * 4.1 + phase) * t, y + 0.12 * t * math.sin(phase + t * 2.3), -run * t)))
        stems.append(pk.tube(points, 0.018, target, m['stem'], f'{name}_Stem{strand}', resolution=1, sides=4, radii=[1.1 - 0.75 * k / steps for k in range(steps + 1)], rng=rng))
        for k in range(steps):
            t = k / steps
            taper = 1.0 - 0.72 * t
            for _ in range(5):
                origin = points[k] + Vector((rng.uniform(-0.2, 0.2), rng.uniform(-0.08, 0.05), rng.uniform(-0.2, 0.2)))
                roll = rng.uniform(-0.8, 0.8)
                across = Vector((math.cos(roll), rng.uniform(-0.35, 0.35), math.sin(roll) * 0.4)).normalized()
                along = Vector((math.sin(roll) * -0.6, rng.uniform(-0.5, -0.12), -1.0)).normalized()
                placements.append((origin, across, along, 0.5 * taper + 0.18, 0.62 * taper + 0.2))
    leaves = nature.cards(f'{name}_Leaves', placements, target, m['ivy'], lambda point, facing: (facing * 0.45 + Vector((0.0, -0.7, 0.55))).normalized(), rng)
    return kit.join(stems + [leaves], name)


def ivy_patch(name, width, height, m, target, rng, count=90):
    placements = []
    stems = []
    for strand in range(4):
        x = rng.uniform(-0.4, 0.4) * width
        points = []
        for k in range(9):
            t = k / 8.0
            points.append(Vector((x + 0.3 * width * math.sin(t * 3.0 + strand * 1.7) * t, -0.03, height * t)))
        stems.append(pk.tube(points, 0.02, target, m['stem'], f'{name}_Stem{strand}', resolution=1, sides=4, radii=[1.1 - 0.7 * k / 8.0 for k in range(9)], rng=rng))
    for _ in range(count):
        z = rng.random() ** 0.8
        spread = width * 0.5 * (1.0 - 0.72 * z ** 1.6)
        x = rng.uniform(-spread, spread)
        origin = Vector((x, -rng.uniform(0.04, 0.2), z * height))
        roll = rng.uniform(0.0, 6.28)
        across = Vector((math.cos(roll), rng.uniform(-0.2, 0.2), math.sin(roll))).normalized()
        along = Vector((-math.sin(roll), rng.uniform(-0.3, 0.1), math.cos(roll))).normalized()
        size = rng.uniform(0.42, 0.68)
        placements.append((origin, across, along, size, size * 1.1))
    leaves = nature.cards(f'{name}_Leaves', placements, target, m['ivy'], lambda point, facing: Vector((0.0, -0.75, 0.66)).normalized(), rng)
    return kit.join(stems + [leaves], name)


def ground_ivy(name, radius, m, target, rng, count=46):
    placements = []
    for _ in range(count):
        angle = rng.uniform(0.0, 6.28)
        r = radius * math.sqrt(rng.random())
        origin = Vector((math.cos(angle) * r, math.sin(angle) * r, rng.uniform(0.02, 0.1) + 0.08 * (1.0 - r / radius)))
        roll = rng.uniform(0.0, 6.28)
        across = Vector((math.cos(roll), math.sin(roll), rng.uniform(-0.12, 0.12))).normalized()
        along = Vector((-math.sin(roll), math.cos(roll), rng.uniform(0.05, 0.35))).normalized()
        size = rng.uniform(0.36, 0.62)
        placements.append((origin, across, along, size, size))
    leaves = nature.cards(name, placements, target, m['ivy'], lambda point, facing: UP, rng)
    return leaves


def root_sprawl(name, reach, m, target, rng, branches=4):
    parts = []
    trunk_end = Vector((reach, rng.uniform(-0.4, 0.4), 0.05))
    trunk = []
    steps = 9
    wiggle = rng.uniform(0.0, 6.28)
    for k in range(steps + 1):
        t = k / steps
        trunk.append(Vector((reach * t, 0.35 * math.sin(t * 5.0 + wiggle) * t + trunk_end.y * t, 0.2 * (1.0 - t) ** 2 + 0.05 + 0.05 * math.sin(t * 11.0))))
    parts.append(pk.tube(trunk, 0.16, target, m['bark'], f'{name}_Trunk', resolution=4, sides=8, radii=[1.0 - 0.8 * k / steps for k in range(steps + 1)], rng=rng))
    for b in range(branches):
        at = rng.randint(1, steps - 3)
        start = trunk[at]
        heading = rng.choice((-1.0, 1.0)) * rng.uniform(0.5, 1.0)
        length = reach * rng.uniform(0.3, 0.55) * (1.0 - at / steps * 0.5)
        points = []
        for k in range(6):
            t = k / 5.0
            points.append(start + Vector((math.cos(heading) * length * t, math.sin(heading) * length * t + 0.1 * math.sin(t * 7.0), -0.02 * t + 0.04 * math.sin(t * 9.0))))
        parts.append(pk.tube(points, 0.1 * (1.0 - at / steps * 0.6), target, m['bark'], f'{name}_Branch{b}', resolution=4, sides=6, radii=[1.0 - 0.85 * k / 5.0 for k in range(6)], rng=rng))
    return kit.join(parts, name)


def build(target):
    m = {
        'marble': ruins.marble_material('CitadelMarble'),
        'stone': ruins.stone_material('CitadelStone'),
        'ivy': nature.spray_material('CitadelIvy', light=(0.17, 0.27, 0.05), dark=(0.035, 0.085, 0.02)),
        'stem': nature.stem_material('CitadelVineStem'),
        'bark': nature.bark_material('CitadelRoot', tone=(0.2, 0.145, 0.095)),
        'brass': pk.brass_material('CitadelBrass', tone=(0.72, 0.5, 0.22)),
        'bronze': pk.brass_material('CitadelBronze', tone=(0.3, 0.22, 0.13), bake_size=512),
        'core': pk.glow_material('CitadelCore', color=(0.2, 1.0, 0.8), strength=3.5, base=(0.3, 0.9, 0.75)),
    }
    objects = [
        statue_robed(m, target, random.Random(201)),
        fallen_column(m, target, random.Random(203)),
        telescope(m, target, random.Random(204)),
        armillary_ring('Armillary_A', 2.5, 0.16, 24, m, target, random.Random(215)),
        armillary_ring('Armillary_B', 2.15, 0.13, 16, m, target, random.Random(216)),
        armillary_ring('Armillary_C', 1.8, 0.11, 12, m, target, random.Random(217)),
        armillary_core(m, target, random.Random(218)),
        fallen_ring(m, target, random.Random(219)),
        instrument(m, target, random.Random(220)),
        vine('Vine_A', 3.2, m, target, random.Random(206)),
        vine('Vine_B', 5.0, m, target, random.Random(207), strands=4),
        vine('Vine_C', 7.0, m, target, random.Random(208), strands=3, sway=0.35),
        ivy_patch('Ivy_A', 2.4, 3.0, m, target, random.Random(209)),
        ivy_patch('Ivy_B', 1.6, 2.2, m, target, random.Random(210), count=56),
        ground_ivy('GroundIvy_A', 0.9, m, target, random.Random(211)),
        ground_ivy('GroundIvy_B', 0.55, m, target, random.Random(212), count=22),
        root_sprawl('Roots_A', 3.8, m, target, random.Random(213)),
        root_sprawl('Roots_B', 2.6, m, target, random.Random(214), branches=3),
    ]
    return objects


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    ruins.bounds(objects)
    pk.unwrap_all(objects, {'CitadelMarble': 0.004, 'CitadelStone': 0.004})
    if args.get('--preview'):
        pk.ground_preview(30.0)
        spots = {obj.name: ((i % 6) * 4.0 - 10.0, (i // 6) * 5.0 - 5.0) for i, obj in enumerate(objects)}
        for obj in objects:
            x, y = spots[obj.name]
            lift = 7.5 if obj.name.startswith('Vine') else 0.0
            obj.matrix_world = Matrix.Translation((x, y, lift))
        pk.preview(objects, args['--preview'], 'citadel_all', close=True, fill=0.95)
        for obj in objects:
            obj.matrix_world = Matrix.Identity(4)
    if args.get('--out'):
        started = time.perf_counter()
        pk.export(objects, args['--out'], NAME, ao_distance=0.5)
        print(f'EXPORT {time.perf_counter() - started:.1f} s')


if __name__ == '__main__':
    main()
