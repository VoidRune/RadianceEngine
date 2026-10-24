import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector, noise

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

NAME = 'Armory'
EMBER = (1.0, 0.42, 0.12)
SPIRIT = (0.5, 0.86, 1.0)


def smoothstep(low, high, value):
    t = min(max((value - low) / (high - low), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def blackwood_material(name, runes=None, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    fibre = s.noise(s.stretch(p, (26.0, 26.0, 0.45)), 4.0, detail=8.0, roughness=0.65)
    streak = s.noise(s.stretch(p, (7.0, 7.0, 0.18)), 2.5, detail=3.0)
    tone = s.smooth(0.32, 0.72, s.add(s.mul(fibre, 0.65), s.mul(streak, 0.35)))
    base = s.mix(tone, (0.011, 0.007, 0.006), (0.07, 0.034, 0.022))
    edge = pk.wear(s, 0.005)
    base = s.mix(s.mul(edge, 0.4), base, (0.15, 0.08, 0.05))
    dirt = pk.cavity(s, 0.015)
    base = s.mix(s.mul(dirt, 0.6), base, (0.006, 0.004, 0.004))
    rough = s.remap(fibre, 0.3, 0.7, 0.38, 0.6)
    rough = s.lerp(s.mul(edge, 0.6), rough, 0.28)
    height = s.add(s.mul(fibre, 0.7), s.mul(streak, 0.3))
    glow = None
    if runes:
        turns, low, high = runes
        x, y, z = s.separate(pk.local(s, 'Axis'))
        phase = s.add(s.math('ARCTAN2', y, x), s.mul(z, 2.0 * math.pi * turns))
        line = s.sub(1.0, s.smooth(0.05, 0.16, s.math('ABSOLUTE', s.math('SINE', s.mul(phase, 0.5)))))
        dashes = s.smooth(0.15, 0.45, s.math('ABSOLUTE', s.math('SINE', s.mul(z, 230.0))))
        glyph = s.smooth(0.38, 0.55, s.noise(s.combine(s.mul(z, 60.0), 0.0, 0.0), 4.0, detail=1.0))
        span = s.mul(s.smooth(low, low + 0.012, z), s.sub(1.0, s.smooth(high - 0.012, high, z)))
        groove = s.mul(line, span)
        mark = s.mul(groove, s.math('MAXIMUM', s.mul(dashes, 0.35), glyph))
        base = s.mix(groove, base, (0.004, 0.003, 0.003))
        base = s.mix(mark, base, (0.85, 0.42, 0.16))
        rough = s.lerp(groove, rough, 0.7)
        height = s.sub(height, s.mul(groove, 1.4))
        glow = s.mul(mark, 0.95)
    return s.finish(base, rough, height=height, bump=0.22, bump_distance=0.0012, emission=glow, emission_color=EMBER, emission_strength=3.2, bake_size=bake_size)


def axis_tag(obj):
    attribute = obj.data.attributes.get('Axis') or obj.data.attributes.new('Axis', 'FLOAT_VECTOR', 'POINT')
    values = []
    for vertex in obj.data.vertices:
        values.extend(vertex.co)
    attribute.data.foreach_set('vector', values)
    return obj


def gnarl_object(obj, rng, amount):
    seed = Vector((rng.uniform(-50.0, 50.0), rng.uniform(-50.0, 50.0), rng.uniform(-50.0, 50.0)))
    for vertex in obj.data.vertices:
        offset = noise.noise_vector(seed + vertex.co * 60.0)
        vertex.co += offset * amount
    return obj


def materials():
    return {
        'wood': blackwood_material('ArmoryWood'),
        'runewood': blackwood_material('ArmoryRunewood', runes=(12.0, 0.11, 0.34)),
        'brass': pk.brass_material('ArmoryBrass', tone=(0.55, 0.38, 0.17), bake_size=256),
        'leather': pk.leather_material('ArmoryLeather', tone=(0.045, 0.028, 0.019), bake_size=256),
        'ember': pk.crystal_material('ArmoryEmber', core=EMBER, shell=(0.16, 0.04, 0.012), strength=4.5, bake_size=128, veins=0.4, cells=40.0, thickness=0.01),
        'spirit': pk.crystal_material('ArmorySpirit', core=SPIRIT, shell=(0.02, 0.07, 0.12), strength=3.6, bake_size=256, veins=0.4, cells=24.0, thickness=0.025),
    }


def shaft(profile, steps, rng, gnarl=0.0, bend=0.0, knots=()):
    bm = pk.lathe(profile, steps, phase=rng.uniform(0.0, math.pi))
    seed = Vector((rng.uniform(-50.0, 50.0), rng.uniform(-50.0, 50.0), rng.uniform(-50.0, 50.0)))
    low, high = profile[0][1], profile[-1][1]
    for vertex in bm.verts:
        z = vertex.co.z
        radial = Vector((vertex.co.x, vertex.co.y, 0.0))
        if radial.length < 1e-7:
            continue
        direction = radial.normalized()
        angle = math.atan2(direction.y, direction.x)
        swell = gnarl * noise.noise(seed + Vector((math.cos(angle) * 1.5, math.sin(angle) * 1.5, z * 9.0)))
        for center, height, size in knots:
            facing = 0.5 + 0.5 * math.cos(angle - center[0])
            swell += size * math.exp(-((z - center[1]) / height) ** 2) * facing ** 3
        radial = radial * (1.0 + swell)
        t = (z - low) / max(high - low, 1e-6)
        sway = bend * math.sin(t * math.pi * 1.3 + seed.x) * smoothstep(0.0, 0.3, t)
        vertex.co = Vector((radial.x + sway, radial.y + 0.4 * sway, z))
    return bm


def wrap(z0, z1, radius, turns_per_metre, steps=18, rows=None, depth=0.0011):
    rows = rows or max(8, int((z1 - z0) / 0.0025))
    profile = [(radius, z0 + (z1 - z0) * i / rows) for i in range(rows + 1)]
    bm = pk.lathe(profile, steps)
    for vertex in bm.verts:
        radial = Vector((vertex.co.x, vertex.co.y, 0.0))
        if radial.length < 1e-7:
            continue
        angle = math.atan2(radial.y, radial.x)
        phase = angle + vertex.co.z * turns_per_metre * 2.0 * math.pi
        band = 0.5 + 0.5 * math.cos(phase)
        edge = smoothstep(z0, z0 + 0.004, vertex.co.z) * (1.0 - smoothstep(z1 - 0.004, z1, vertex.co.z))
        vertex.co = Vector((0.0, 0.0, vertex.co.z)) + radial.normalized() * (radius + depth * (band ** 0.6) * (0.4 + 0.6 * edge))
    return bm


def crystal(center, radius, height, sides=6, squash=0.62, twist=0.0):
    z = center[2]
    profile = [(0.0, z - height * squash), (radius * 0.86, z - height * 0.12), (radius, z + height * 0.06), (radius * 0.74, z + height * 0.3), (0.0, z + height)]
    bm = pk.lathe(profile, sides, phase=twist)
    bmesh.ops.translate(bm, verts=bm.verts, vec=(center[0], center[1], 0.0))
    return bm


def prongs(m, target, rng, name, base, radius, top, count=3, phase=0.0, thickness=0.0028):
    parts = []
    for i in range(count):
        angle = phase + 2.0 * math.pi * i / count
        c, s = math.cos(angle), math.sin(angle)
        points = []
        for (r, z, turn) in ((radius * 0.55, base, 0.0), (radius * 1.15, base + (top - base) * 0.35, 0.18), (radius * 1.05, base + (top - base) * 0.72, 0.3), (radius * 0.45, top, 0.42)):
            a = angle + turn
            points.append((math.cos(a) * r, math.sin(a) * r, z))
        parts.append(pk.tube(points, thickness, target, m['brass'], f'{name}{i}', resolution=6, sides=8, radii=[1.1, 1.0, 0.85, 0.55], rng=rng))
    return parts


def wand(m, target, rng):
    parts = []
    pommel = pk.lathe([(0.0, -0.106), (0.006, -0.105), (0.012, -0.1), (0.0145, -0.093), (0.0125, -0.086), (0.0095, -0.081), (0.0112, -0.077), (0.0122, -0.073), (0.0108, -0.069)], 20)
    parts.append(pk.part('Wand_Pommel', pommel, target, m['brass'], rng=rng, smooth=40.0))
    parts.append(pk.part('Wand_PommelGem', crystal((0.0, 0.0, -0.1055), 0.0045, 0.006, sides=6, squash=0.35), target, m['ember'], rng=rng, bevel=(0.0006, 1, 30.0)))
    parts.append(pk.part('Wand_Grip', wrap(-0.07, 0.052, 0.0112, 76.0), target, m['leather'], rng=rng, smooth=50.0))
    collar = pk.lathe([(0.0108, 0.05), (0.0152, 0.052), (0.0166, 0.058), (0.0158, 0.064), (0.0142, 0.067), (0.0146, 0.071), (0.0122, 0.075)], 20)
    parts.append(pk.part('Wand_Collar', collar, target, m['brass'], rng=rng, smooth=40.0))
    profile = [(0.0124, 0.073), (0.0119, 0.1), (0.0109, 0.16), (0.0098, 0.22), (0.0087, 0.28), (0.0077, 0.33), (0.0071, 0.362)]
    rows = []
    for (r0, z0), (r1, z1) in zip(profile, profile[1:]):
        for k in range(8):
            t = k / 8.0
            rows.append((r0 + (r1 - r0) * t, z0 + (z1 - z0) * t))
    rows.append(profile[-1])
    body = shaft(rows, 20, rng, gnarl=0.06, bend=0.0012, knots=(((rng.uniform(0.0, 6.28), 0.09), 0.006, 0.12),))
    parts.append(axis_tag(pk.part('Wand_Shaft', body, target, m['runewood'], rng=rng, smooth=60.0)))
    cup = pk.lathe([(0.0066, 0.358), (0.0098, 0.363), (0.0112, 0.37), (0.009, 0.375), (0.0, 0.376)], 20)
    parts.append(pk.part('Wand_Cup', cup, target, m['brass'], rng=rng, smooth=40.0))
    parts += prongs(m, target, rng, 'Wand_Prong', 0.369, 0.0118, 0.432, phase=0.3, thickness=0.0026)
    parts.append(pk.part('Wand_Crystal', crystal((0.0, 0.0, 0.398), 0.011, 0.047, twist=0.3), target, m['ember'], rng=rng, bevel=(0.0008, 1, 30.0)))
    return kit.join(parts, 'Wand')


def staff(m, target, rng):
    parts = []
    ferrule = pk.lathe([(0.0, -1.05), (0.012, -1.048), (0.0175, -1.04), (0.0205, -1.02), (0.0215, -0.99), (0.0235, -0.984), (0.0225, -0.976), (0.0205, -0.972)], 18)
    parts.append(pk.part('Staff_Ferrule', ferrule, target, m['brass'], rng=rng, smooth=40.0))
    profile = [(0.0196, -0.975), (0.0205, -0.8), (0.0212, -0.55), (0.0218, -0.3), (0.0205, -0.16), (0.016, -0.125), (0.016, 0.145), (0.0205, 0.18), (0.0216, 0.4), (0.0206, 0.62), (0.0196, 0.72), (0.015, 0.75)]
    rows = []
    for (r0, z0), (r1, z1) in zip(profile, profile[1:]):
        count = max(2, int((z1 - z0) / 0.03))
        for k in range(count):
            t = k / count
            rows.append((r0 + (r1 - r0) * t, z0 + (z1 - z0) * t))
    rows.append(profile[-1])
    knots = tuple(((rng.uniform(0.0, 6.28), z), rng.uniform(0.012, 0.02), rng.uniform(0.15, 0.25)) for z in (-0.72, -0.41, 0.33, 0.52))
    parts.append(pk.part('Staff_Shaft', shaft(rows, 16, rng, gnarl=0.09, bend=0.006, knots=knots), target, m['wood'], rng=rng, smooth=60.0))
    parts.append(pk.part('Staff_Grip', wrap(-0.11, 0.13, 0.0168, 52.0, steps=20, depth=0.0013), target, m['leather'], rng=rng, smooth=50.0))
    for z in (-0.125, 0.145, 0.6):
        ring = pk.lathe([(0.021, z - 0.009), (0.0258, z - 0.006), (0.027, z), (0.0258, z + 0.006), (0.021, z + 0.009)], 20)
        parts.append(pk.part('Staff_Ring', ring, target, m['brass'], rng=rng, smooth=40.0))
    phase = rng.uniform(0.0, 6.28)
    for i in range(3):
        angle = phase + 2.0 * math.pi * i / 3.0
        points, radii = [], []
        for (r, z, turn, width) in ((0.006, 0.7, 0.0, 1.0), (0.03, 0.79, 0.35, 0.9), (0.055, 0.87, 0.75, 0.75), (0.052, 0.95, 1.1, 0.6), (0.026, 1.01, 1.45, 0.45), (0.004, 1.035, 1.7, 0.3)):
            a = angle + turn
            points.append((math.cos(a) * r, math.sin(a) * r, z))
            radii.append(width)
        tine = pk.tube(points, 0.0095, target, m['wood'], f'Staff_Tine{i}', resolution=12, sides=12, radii=radii, rng=rng)
        parts.append(gnarl_object(tine, rng, 0.0016))
    collar = pk.lathe([(0.0198, 0.69), (0.0255, 0.695), (0.027, 0.705), (0.0245, 0.716), (0.017, 0.722)], 20)
    parts.append(pk.part('Staff_Collar', collar, target, m['brass'], rng=rng, smooth=40.0))
    for i in range(3):
        angle = phase + 2.0 * math.pi * i / 3.0 + 0.93
        band = pk.torus(0.0098, 0.0024, steps=16, sides=8)
        bmesh.ops.transform(band, verts=band.verts, matrix=Matrix.Translation((math.cos(angle) * 0.054, math.sin(angle) * 0.054, 0.91)))
        parts.append(pk.part('Staff_Band', band, target, m['brass'], rng=rng, smooth=50.0))
    parts.append(pk.part('Staff_Crystal', crystal((0.0, 0.0, 0.888), 0.026, 0.11, sides=7, twist=phase), target, m['spirit'], rng=rng, bevel=(0.0015, 1, 30.0)))
    return kit.join(parts, 'Staff')


def build(target):
    rng = random.Random(23)
    m = materials()
    weapons = [wand(m, target, rng), staff(m, target, rng)]
    for index, obj in enumerate(weapons):
        obj.location = (0.4 * index, 0.0, 0.0)
    return weapons


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    weapons = build(target)
    if args.get('--preview'):
        folder = args['--preview']
        os.makedirs(folder, exist_ok=True)
        scene = bpy.context.scene
        scene.view_settings.view_transform = 'Standard'
        kit.environment('forest.exr', strength=1.0, rotation=1.2)
        kit.render(os.path.join(folder, 'armory_wand.png'), weapons[:1], yaw=-20.0, pitch=10.0, fill=0.95, size=(500, 1200), samples=64)
        kit.render(os.path.join(folder, 'armory_staff.png'), weapons[1:], yaw=-20.0, pitch=10.0, fill=0.95, size=(500, 1200), samples=64)
        kit.render(os.path.join(folder, 'armory_tip.png'), weapons[:1], yaw=-25.0, pitch=15.0, fill=0.3, size=(800, 800), samples=64, look_offset=(0.0, 0.0, 0.2))
        kit.render(os.path.join(folder, 'armory_head.png'), weapons[1:], yaw=-25.0, pitch=15.0, fill=0.16, size=(800, 800), samples=64, look_offset=(0.0, 0.0, 0.86))
    if args.get('--out'):
        pk.unwrap_all(weapons, {'ArmoryEmber': 0.02, 'ArmorySpirit': 0.01, 'ArmoryBrass': 0.008})
        pk.export(weapons, args['--out'], NAME, ao_distance=0.04)


if __name__ == '__main__':
    main()
