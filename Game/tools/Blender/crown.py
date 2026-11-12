import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

NAME = 'Crown'
SPECTRAL = (0.52, 0.4, 1.0)
RING = 0.115
BLADES = [
    (0.0, 0.205, 0.062, 0.0, 0.18),
    (-41.0, 0.15, 0.05, 1.0, 0.24),
    (41.0, 0.15, 0.05, -1.0, 0.24),
    (-88.0, 0.115, 0.043, -1.0, 0.3),
    (88.0, 0.115, 0.043, 1.0, 0.3),
    (-138.0, 0.09, 0.038, 1.0, 0.36),
    (138.0, 0.09, 0.038, -1.0, 0.36),
]
BAND_GAPS = [-20.0, 21.0, 64.0, 113.0, 180.0, 247.0, 296.0]
SHARD_HEIGHT = 0.058
BLADE_DEPTH = 0.4
BAND = (0.008, 0.011, 0.021, 0.013)
VEIN_WIDTH = 0.034
VEIN_BREAKUP = (0.42, 0.56)
EDGE_TONE = (0.74, 0.76, 0.86)
VEIN_STRENGTH = 6.0


def direction(degrees):
    angle = math.radians(degrees)
    return Vector((math.sin(angle), -math.cos(angle), 0.0))


def tangent(degrees):
    angle = math.radians(degrees)
    return Vector((math.cos(angle), math.sin(angle), 0.0))


def crown_metal():
    s = kit.Shader('CrownMetal')
    p = pk.local(s)
    grain = s.noise(s.stretch(p, (1.0, 1.0, 9.0)), 40.0, detail=3.0)
    tone = s.smooth(0.3, 0.75, s.noise(p, 9.0, detail=4.0))
    base = s.mix(tone, (0.03, 0.03, 0.04), (0.075, 0.07, 0.095))
    edge = pk.wear(s, 0.004)
    base = s.mix(edge, base, EDGE_TONE)
    pits = pk.cavity(s, 0.01)
    base = s.mix(s.mul(pits, 0.7), base, (0.008, 0.008, 0.012))
    warped = s.offset(p, s.mul(s.noise_color(p, 14.0, detail=2.0), 0.012))
    fissures = s.voronoi(s.stretch(warped, (1.0, 1.0, 0.4)), 24.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    lines = s.sub(1.0, s.smooth(0.0, VEIN_WIDTH, fissures))
    breakup = s.smooth(VEIN_BREAKUP[0], VEIN_BREAKUP[1], s.noise(p, 16.0, detail=2.0))
    veins = s.mul(lines, breakup)
    base = s.mix(veins, base, (0.3, 0.26, 0.5))
    rough = s.remap(grain, 0.3, 0.7, 0.3, 0.42)
    rough = s.lerp(edge, rough, 0.2)
    metal = s.sub(1.0, s.mul(veins, 0.9))
    height = s.sub(s.mul(grain, 0.3), s.mul(lines, 0.6))
    return s.finish(base, rough, height=height, bump=0.12, bump_distance=0.0006, metallic=metal, emission=veins, emission_color=SPECTRAL, emission_strength=VEIN_STRENGTH, bake_size=1024)


def blade(around, height, width, barb, lean):
    out = direction(around)
    side = tangent(around)
    thick = width * BLADE_DEPTH
    rows = [(0.0, 0.55, 0.55), (0.16, 1.0, 1.0), (0.34, 0.86, 0.86), (0.42, 0.78, 0.78), (0.6, 0.5, 0.5), (0.8, 0.26, 0.26), (0.93, 0.1, 0.1)]
    if barb:
        rows = rows[:3] + [(0.38, 0.84 + (0.95 if barb > 0 else 0.0), 0.84 + (0.95 if barb < 0 else 0.0)), (0.46, 0.66, 0.66)] + rows[4:]
    bm = bmesh.new()

    def point(u, v, t):
        z = t * height
        bend = math.tan(lean) * z + 1.6 * z * z
        return out * (RING + v + bend) + side * u + Vector((0.0, 0.0, z - 0.004))

    rings = []
    for t, left, right in rows:
        hw = width * 0.5
        depth = thick * (1.0 - 0.6 * t)
        rings.append([
            bm.verts.new(point(-hw * left, 0.0, t)),
            bm.verts.new(point(0.0, depth, t)),
            bm.verts.new(point(hw * right, 0.0, t)),
            bm.verts.new(point(0.0, -depth * 0.7, t)),
        ])
    for a, b in zip(rings, rings[1:]):
        for i in range(4):
            j = (i + 1) % 4
            bm.faces.new((a[i], a[j], b[j], b[i]))
    tip = bm.verts.new(point(0.0, 0.0, 1.0))
    for i in range(4):
        bm.faces.new((rings[-1][i], rings[-1][(i + 1) % 4], tip))
    bm.faces.new(list(reversed(rings[0])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def band(start, end, rng):
    steps = max(3, int((end - start) / 6.0))
    bm = bmesh.new()
    rings = []
    for i in range(steps + 1):
        t = i / steps
        around = start + (end - start) * t
        jag = 0.0
        if i == 0 or i == steps:
            jag = rng.uniform(2.0, 5.0) * (1.0 if i == 0 else -1.0)
        out = direction(around)
        out_top = direction(around + jag)
        swell = 0.004 * math.sin(t * math.pi)
        rings.append([
            bm.verts.new(out * (RING - BAND[0]) + Vector((0.0, 0.0, -BAND[2]))),
            bm.verts.new(out * (RING + BAND[1] + swell) + Vector((0.0, 0.0, -BAND[2] + 0.002))),
            bm.verts.new(out_top * (RING + BAND[1] + swell) + Vector((0.0, 0.0, BAND[3]))),
            bm.verts.new(out_top * (RING - BAND[0]) + Vector((0.0, 0.0, BAND[3] - 0.002))),
        ])
    for a, b in zip(rings, rings[1:]):
        for i in range(4):
            j = (i + 1) % 4
            bm.faces.new((a[i], a[j], b[j], b[i]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def soul_shard():
    bm = bmesh.new()
    sides = 6
    radius = 0.013
    middle = [bm.verts.new((math.cos(i * 2.0 * math.pi / sides) * radius, math.sin(i * 2.0 * math.pi / sides) * radius, SHARD_HEIGHT * 0.42)) for i in range(sides)]
    top = bm.verts.new((0.0, 0.0, SHARD_HEIGHT))
    bottom = bm.verts.new((0.0, 0.0, 0.0))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((middle[i], middle[j], top))
        bm.faces.new((middle[j], middle[i], bottom))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def build(target):
    rng = random.Random(4127)
    metal = crown_metal()
    glow = pk.crystal_material('CrownSoul', core=SPECTRAL, shell=(0.05, 0.04, 0.12), strength=4.0, bake_size=128, veins=0.4, cells=30.0, thickness=0.008)
    parts = []
    for index, (around, height, width, barb, lean) in enumerate(BLADES):
        parts.append(pk.part(f'{NAME}_Blade{index}', blade(around, height, width, barb, lean), target, metal, rng=rng, bevel=(0.0012, 2, 30.0)))
    gaps = sorted(BAND_GAPS)
    for index, (a, b) in enumerate(zip(gaps, gaps[1:] + [gaps[0] + 360.0])):
        parts.append(pk.part(f'{NAME}_Band{index}', band(a + 4.0, b - 4.0, rng), target, metal, rng=rng, bevel=(0.001, 2, 30.0)))
    parts.append(pk.part(f'{NAME}_Shard', soul_shard(), target, glow, rng=rng))
    return parts


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    parts = build(target)
    pk.unwrap_all(parts, {'CrownMetal': 0.006, 'CrownSoul': 0.02})
    if args.get('--preview'):
        folder = args['--preview']
        os.makedirs(folder, exist_ok=True)
        scene = bpy.context.scene
        scene.view_settings.view_transform = 'Standard'
        kit.environment('night.exr', strength=0.8, rotation=1.2)
        kit.render(os.path.join(folder, 'crown_front.png'), parts, yaw=0.0, pitch=12.0, fill=1.1, size=(900, 700), samples=64)
        kit.render(os.path.join(folder, 'crown_game.png'), parts, yaw=0.0, pitch=57.0, fill=1.2, size=(900, 700), samples=64)
        kit.render(os.path.join(folder, 'crown_side.png'), parts, yaw=-60.0, pitch=20.0, fill=1.1, size=(900, 700), samples=64)
    if args.get('--out'):
        pk.export(parts, args['--out'], NAME, ao_distance=0.02)


if __name__ == '__main__':
    main()
