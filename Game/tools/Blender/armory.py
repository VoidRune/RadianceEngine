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
        'gold': pk.brass_material('ArmoryGold', tone=(0.78, 0.55, 0.2), bake_size=512),
        'leather': pk.leather_material('ArmoryLeather', tone=(0.045, 0.028, 0.019), bake_size=256),
        'ember': pk.crystal_material('ArmoryEmber', core=EMBER, shell=(0.16, 0.04, 0.012), strength=4.5, bake_size=128, veins=0.4, cells=40.0, thickness=0.01),
        'ember_large': pk.crystal_material('ArmoryEmberLarge', core=EMBER, shell=(0.16, 0.04, 0.012), strength=4.5, bake_size=256, veins=0.3, cells=11.0, thickness=0.02),
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


def twisted(rows, steps, rng, strands=3, turns=1.5, depth=0.32, gnarl=0.05):
    bm = pk.lathe(rows, steps, phase=rng.uniform(0.0, math.pi))
    seed = Vector((rng.uniform(-50.0, 50.0), rng.uniform(-50.0, 50.0), rng.uniform(-50.0, 50.0)))
    for vertex in bm.verts:
        radial = Vector((vertex.co.x, vertex.co.y, 0.0))
        if radial.length < 1e-7:
            continue
        z = vertex.co.z
        angle = math.atan2(radial.y, radial.x)
        rope = abs(math.cos(0.5 * strands * (angle + z * turns * 2.0 * math.pi)))
        swell = depth * (rope ** 0.5 - 0.6) + gnarl * noise.noise(seed + Vector((math.cos(angle) * 1.5, math.sin(angle) * 1.5, z * 7.0)))
        vertex.co = Vector((0.0, 0.0, z)) + radial * (1.0 + swell)
    return bm


def helix(z0, z1, radius, turns, phase=0.0, count=120):
    points = []
    for k in range(count + 1):
        t = k / count
        z = z0 + (z1 - z0) * t
        a = phase + turns * 2.0 * math.pi * t
        points.append((math.cos(a) * radius(z), math.sin(a) * radius(z), z))
    return points


def staff(m, target, rng):
    parts = []
    gold, wood = m['gold'], m['wood']
    ferrule = pk.lathe([(0.0, -0.99), (0.006, -0.96), (0.016, -0.9), (0.0235, -0.875), (0.026, -0.862), (0.0225, -0.85), (0.019, -0.84)], 16)
    parts.append(pk.part('Staff_Ferrule', ferrule, target, gold, rng=rng, smooth=40.0))
    profile = [(0.0175, -0.85), (0.0195, -0.6), (0.0205, -0.3), (0.0198, -0.16), (0.0165, -0.13), (0.0165, 0.15), (0.0198, 0.18), (0.021, 0.42), (0.0215, 0.66), (0.024, 0.74), (0.02, 0.775)]
    rows = []
    for (r0, z0), (r1, z1) in zip(profile, profile[1:]):
        count = max(2, int((z1 - z0) / 0.012))
        for k in range(count):
            t = k / count
            rows.append((r0 + (r1 - r0) * t, z0 + (z1 - z0) * t))
    rows.append(profile[-1])
    parts.append(pk.part('Staff_Shaft', twisted(rows, 24, rng), target, wood, rng=rng, smooth=60.0))
    parts.append(pk.part('Staff_Grip', wrap(-0.12, 0.14, 0.0172, 52.0, steps=20, depth=0.0013), target, m['leather'], rng=rng, smooth=50.0))

    def girth(z):
        return 0.0228 if z > 0.0 else 0.0222

    for z0, z1, phase in ((-0.82, -0.16, 0.4), (0.17, 0.72, 2.1)):
        band = helix(z0, z1, girth, turns=(z1 - z0) * 3.4, phase=phase)
        parts.append(pk.tube(band, 0.0032, target, gold, f'Staff_Spiral{len(parts)}', resolution=2, sides=8, rng=rng))
        for k in range(1, 4):
            a, b = Vector(band[k * len(band) // 4]), Vector(band[k * len(band) // 4 - 2])
            out = Vector((a.x, a.y, 0.0)).normalized()
            spike = pk.cone_bm(a - out * 0.004, a + out * 0.03 + (a - b).normalized() * 0.012, 0.0055, sides=6)
            parts.append(pk.part('Staff_Thorn', spike, target, gold, rng=rng, bevel=(0.0008, 1, 30.0)))
    for z in (-0.135, 0.155):
        ring = pk.lathe([(0.02, z - 0.008), (0.0255, z - 0.005), (0.0268, z), (0.0255, z + 0.005), (0.02, z + 0.008)], 20)
        parts.append(pk.part('Staff_Ring', ring, target, gold, rng=rng, smooth=40.0))
    collar = pk.lathe([(0.021, 0.74), (0.03, 0.75), (0.034, 0.765), (0.031, 0.78), (0.022, 0.79)], 20)
    parts.append(pk.part('Staff_Collar', collar, target, gold, rng=rng, smooth=40.0))

    center = Vector((0.0, 0.0, 0.905))
    hoop_major, hoop_minor = 0.088, 0.0105
    hoop = pk.torus(hoop_major, hoop_minor, steps=40, sides=12)
    bmesh.ops.transform(hoop, verts=hoop.verts, matrix=Matrix.Translation(center) @ Matrix.Rotation(math.pi * 0.5, 4, 'X'))
    obj = pk.part('Staff_Hoop', hoop, target, wood, rng=rng, smooth=60.0)
    parts.append(gnarl_object(obj, rng, 0.0018))
    for k in range(8):
        a = math.pi * 0.5 + 2.0 * math.pi * k / 8.0
        direction = Vector((math.cos(a), 0.0, math.sin(a)))
        base = center + direction * (hoop_major + hoop_minor * 0.4)
        length = 0.085 if k == 0 else 0.05 if k % 2 == 0 else 0.035
        if k == 4:
            continue
        spike = pk.cone_bm(base - direction * 0.006, base + direction * length, 0.011 if k % 2 == 0 else 0.008, sides=4, flatten=0.45)
        parts.append(pk.part('Staff_Spike', spike, target, gold, rng=rng, bevel=(0.0012, 1, 30.0)))
        if k % 2 == 1:
            band = pk.torus(hoop_minor + 0.0022, 0.0028, steps=14, sides=6)
            tangent = Vector((-math.sin(a), 0.0, math.cos(a)))
            frame = Matrix.Translation(center + direction * hoop_major) @ tangent.to_track_quat('Z', 'Y').to_matrix().to_4x4()
            bmesh.ops.transform(band, verts=band.verts, matrix=frame)
            parts.append(pk.part('Staff_HoopBand', band, target, gold, rng=rng, smooth=50.0))
    for side in (-1.0, 1.0):
        points = [(side * 0.012, 0.0, 0.78), (side * 0.032, 0.0, 0.8), (side * 0.03, 0.0, 0.85), (side * 0.018, 0.0, 0.875)]
        parts.append(pk.tube(points, 0.0035, target, gold, 'Staff_Prong', resolution=6, sides=8, radii=[1.2, 1.0, 0.85, 0.6], rng=rng))
    for side in (-1.0, 1.0):
        points = [(0.0, side * 0.01, 0.78), (0.0, side * 0.024, 0.82), (0.0, side * 0.02, 0.86), (0.0, side * 0.012, 0.88)]
        parts.append(pk.tube(points, 0.003, target, gold, 'Staff_Prong', resolution=6, sides=8, radii=[1.2, 1.0, 0.85, 0.6], rng=rng))
    parts.append(pk.part('Staff_Crystal', crystal((0.0, 0.0, 0.89), 0.036, 0.1, sides=6, squash=0.95, twist=0.2), target, m['ember_large'], rng=rng, bevel=(0.0015, 1, 30.0)))
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
