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
from crown import RING, direction, tangent

NAME = 'FrostCrown'
ICE = (0.5, 0.82, 1.0)
SPIKES = [
    (0.0, 0.235, 0.024, 9.0),
    (-34.0, 0.165, 0.02, 15.0),
    (34.0, 0.165, 0.02, 15.0),
    (-70.0, 0.13, 0.018, 20.0),
    (70.0, 0.13, 0.018, 20.0),
    (-110.0, 0.105, 0.017, 24.0),
    (110.0, 0.105, 0.017, 24.0),
    (-148.0, 0.088, 0.016, 27.0),
    (148.0, 0.088, 0.016, 27.0),
    (180.0, 0.072, 0.015, 30.0),
]
BAND = (0.01, 0.013, 0.024, 0.012)
BAND_JAG = 0.006
CORE = (0.016, 0.075)


def shard(bm, matrix, radius, height, sides, tip, rng):
    ring = []
    for i in range(sides):
        angle = 2.0 * math.pi * i / sides + rng.uniform(-0.1, 0.1)
        r = radius * rng.uniform(0.85, 1.1)
        ring.append((math.cos(angle) * r, math.sin(angle) * r))
    bottom = [bm.verts.new(matrix @ Vector((x, y, -0.02))) for x, y in ring]
    shoulder = height * (1.0 - tip)
    top = [bm.verts.new(matrix @ Vector((x * 0.82, y * 0.82, shoulder))) for x, y in ring]
    apex = bm.verts.new(matrix @ Vector((radius * rng.uniform(-0.15, 0.15), radius * rng.uniform(-0.15, 0.15), height)))
    bm.faces.new(list(reversed(bottom)))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
        bm.faces.new((top[i], top[j], apex))


def lean(around, outward, sideways=0.0):
    out = direction(around)
    side = tangent(around)
    return Matrix.Rotation(math.radians(sideways), 4, out) @ Matrix.Rotation(math.radians(outward), 4, side)


def spike(around, height, radius, tilt, rng):
    out = direction(around)
    side = tangent(around)
    bm = bmesh.new()
    base = Matrix.Translation(out * (RING + 0.002) + Vector((0.0, 0.0, -0.006)))
    shard(bm, base @ lean(around, tilt), radius, height, 6, rng.uniform(0.28, 0.36), rng)
    for sign in (-1.0, 1.0):
        scale = rng.uniform(0.42, 0.6)
        offset = Matrix.Translation(out * (RING + 0.004) + side * (sign * radius * 1.35) + Vector((0.0, 0.0, -0.01)))
        shard(bm, offset @ lean(around, tilt + rng.uniform(8.0, 16.0), sign * rng.uniform(14.0, 24.0)), radius * 0.68, height * scale, 5, 0.38, rng)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def band(start, end, rng):
    steps = max(3, int((end - start) / 5.0))
    bm = bmesh.new()
    rings = []
    for i in range(steps + 1):
        t = i / steps
        out = direction(start + (end - start) * t)
        jag = rng.uniform(-BAND_JAG, BAND_JAG) if 0 < i < steps else 0.0
        bulge = rng.uniform(0.0, 0.004)
        rings.append([
            bm.verts.new(out * (RING - BAND[0]) + Vector((0.0, 0.0, -BAND[2]))),
            bm.verts.new(out * (RING + BAND[1] + bulge) + Vector((0.0, 0.0, -BAND[2] + 0.003))),
            bm.verts.new(out * (RING + BAND[1] + bulge) + Vector((0.0, 0.0, BAND[3] + jag))),
            bm.verts.new(out * (RING - BAND[0]) + Vector((0.0, 0.0, BAND[3] + jag * 0.5 - 0.003))),
        ])
    for a, b in zip(rings, rings[1:]):
        for i in range(4):
            j = (i + 1) % 4
            bm.faces.new((a[i], a[j], b[j], b[i]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def core():
    bm = bmesh.new()
    sides = 6
    radius, height = CORE
    middle = [bm.verts.new((math.cos(i * 2.0 * math.pi / sides) * radius, math.sin(i * 2.0 * math.pi / sides) * radius, height * 0.5)) for i in range(sides)]
    top = bm.verts.new((0.0, 0.0, height))
    bottom = bm.verts.new((0.0, 0.0, 0.0))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((middle[i], middle[j], top))
        bm.faces.new((middle[j], middle[i], bottom))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def build(target):
    rng = random.Random(7351)
    ice = pk.crystal_material('FrostCrownIce', core=ICE, shell=(0.6, 0.78, 0.9), strength=2.2, bake_size=512, veins=0.3, cells=14.0, thickness=0.014)
    heart = pk.crystal_material('FrostCrownCore', core=(0.62, 0.9, 1.0), shell=(0.18, 0.36, 0.55), strength=4.0, bake_size=128, veins=0.4, cells=30.0, thickness=0.008)
    parts = []
    for index, (around, height, radius, tilt) in enumerate(SPIKES):
        parts.append(pk.part(f'{NAME}_Spike{index}', spike(around, height, radius, tilt, rng), target, ice, rng=rng, bevel=(0.0012, 1, 30.0)))
    angles = sorted(around % 360.0 for around, *_ in SPIKES)
    for index, (a, b) in enumerate(zip(angles, angles[1:] + [angles[0] + 360.0])):
        parts.append(pk.part(f'{NAME}_Band{index}', band(a + 6.0, b - 6.0, rng), target, ice, rng=rng, bevel=(0.0012, 1, 30.0)))
    parts.append(pk.part(f'{NAME}_Core', core(), target, heart, rng=rng))
    return parts


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    parts = build(target)
    pk.unwrap_all(parts, {'FrostCrownIce': 0.006, 'FrostCrownCore': 0.02})
    if args.get('--preview'):
        folder = args['--preview']
        os.makedirs(folder, exist_ok=True)
        bpy.context.scene.view_settings.view_transform = 'Standard'
        kit.environment('night.exr', strength=0.8, rotation=1.2)
        kit.render(os.path.join(folder, 'frostcrown_front.png'), parts, yaw=0.0, pitch=12.0, fill=1.1, size=(900, 700), samples=64)
        kit.render(os.path.join(folder, 'frostcrown_game.png'), parts, yaw=0.0, pitch=57.0, fill=1.2, size=(900, 700), samples=64)
        kit.render(os.path.join(folder, 'frostcrown_side.png'), parts, yaw=-60.0, pitch=20.0, fill=1.1, size=(900, 700), samples=64)
    if args.get('--out'):
        pk.export(parts, args['--out'], NAME, ao_distance=0.02)


if __name__ == '__main__':
    main()
