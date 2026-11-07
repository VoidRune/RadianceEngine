import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

NAME = 'Harbor'


def frost(s, p, base, rough, amount=1.0):
    up = s.smooth(0.5, 0.85, s.separate(s.geometry('Normal'))[2])
    drift = s.smooth(0.35, 0.6, s.noise(p, 1.6, detail=4.0, roughness=0.6))
    snow = s.clamp01(s.mul(s.mul(up, s.add(0.35, s.mul(drift, 0.9))), amount))
    rime = s.mul(s.smooth(0.55, 0.8, s.noise(p, 7.0, detail=3.0)), 0.35 * amount)
    base = s.mix(rime, base, (0.5, 0.58, 0.64))
    base = s.mix(snow, base, (0.8, 0.85, 0.92))
    rough = s.lerp(snow, rough, 0.62)
    return base, rough


def harbor_wood(name, light=(0.2, 0.155, 0.115), dark=(0.065, 0.05, 0.04), wet=False, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    island = s.geometry('Random Per Island')
    fibre = s.noise(s.stretch(p, (14.0, 14.0, 0.7)), 2.2, detail=6.0, roughness=0.6)
    across = s.noise(s.stretch(p, (0.7, 14.0, 14.0)), 2.2, detail=6.0, roughness=0.6)
    grain = s.lerp(s.smooth(0.4, 0.6, s.noise(p, 0.6, detail=1.0)), fibre, across)
    blotch = s.noise(p, 1.8, detail=3.0)
    tone = s.clamp01(s.add(s.mul(grain, 0.6), s.mul(island, 0.4)))
    base = s.mix(tone, dark, light)
    base = s.mix(s.mul(s.smooth(0.45, 0.75, s.noise(p, 3.5, detail=4.0)), 0.6), base, (0.17, 0.165, 0.155))
    base = s.mix(s.mul(s.smooth(0.55, 0.8, blotch), 0.4), base, (0.04, 0.035, 0.03))
    edge = pk.wear(s, 0.015)
    base = s.mix(s.mul(edge, 0.4), base, (0.3, 0.27, 0.23))
    base = s.mix(s.mul(pk.cavity(s, 0.06), 0.7), base, (0.02, 0.018, 0.016))
    rough = s.remap(grain, 0.3, 0.7, 0.72, 0.9)
    if wet:
        algae = s.smooth(0.4, 0.7, s.noise(p, 1.3, detail=5.0, roughness=0.6))
        base = s.mix(s.mul(algae, 0.7), base, (0.035, 0.06, 0.045))
        rough = s.remap(grain, 0.3, 0.7, 0.42, 0.62)
    else:
        base, rough = frost(s, p, base, rough)
    return s.finish(base, rough, height=s.add(s.mul(grain, 0.7), s.mul(blotch, 0.2)), bump=0.3, bump_distance=0.008, bake_size=bake_size)


def harbor_stone(name, bake_size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    x, y, z = s.separate(p)
    node = s.node('ShaderNodeTexBrick', offset=0.5, offset_frequency=2, squash=1.0, squash_frequency=2)
    across = s.mul(s.math('ARCTAN2', s.mul(x, -1.0), s.mul(y, -1.0)), 1.9)
    s.feed(node.inputs['Vector'], s.combine(across, z, 0.0))
    s.feed(node.inputs['Color1'], (1.0, 1.0, 1.0))
    s.feed(node.inputs['Color2'], (0.0, 0.0, 0.0))
    s.feed(node.inputs['Mortar'], (0.0, 0.0, 0.0))
    s.feed(node.inputs['Scale'], 1.0)
    s.feed(node.inputs['Mortar Size'], 0.02)
    s.feed(node.inputs['Mortar Smooth'], 0.4)
    s.feed(node.inputs['Bias'], 0.0)
    s.feed(node.inputs['Brick Width'], 0.62)
    s.feed(node.inputs['Row Height'], 0.34)
    joints = node.outputs['Fac']
    large = s.noise(p, 1.2, detail=3.0)
    mid = s.noise(p, 5.0, detail=5.0, roughness=0.55)
    tone = s.clamp01(s.add(s.mul(s.separate(node.outputs['Color'])[0], 0.45), s.add(s.mul(large, 0.35), s.mul(mid, 0.3))))
    base = s.mix(tone, (0.11, 0.115, 0.12), (0.3, 0.3, 0.29))
    base = s.mix(s.smooth(0.2, 0.6, joints), base, (0.05, 0.05, 0.05))
    streaks = s.smooth(0.5, 0.75, s.noise(s.stretch(p, (12.0, 12.0, 0.6)), 1.0, detail=3.0))
    base = s.mix(s.mul(streaks, 0.45), base, (0.06, 0.07, 0.07))
    base = s.mix(s.mul(pk.cavity(s, 0.1), 0.7), base, (0.03, 0.03, 0.03))
    edge = pk.wear(s, 0.025)
    base = s.mix(s.mul(edge, 0.3), base, (0.4, 0.4, 0.39))
    rough = s.remap(mid, 0.3, 0.7, 0.78, 0.92)
    base, rough = frost(s, p, base, rough)
    height = s.add(s.mul(s.sub(1.0, s.smooth(0.2, 0.6, joints)), -0.6), s.mul(mid, 0.4))
    return s.finish(base, rough, height=height, bump=0.35, bump_distance=0.014, bake_size=bake_size)


def harbor_rock(name, bake_size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    large = s.noise(p, 0.45, detail=4.0, roughness=0.6)
    mid = s.noise(p, 2.2, detail=5.0, roughness=0.58)
    strata = s.noise(s.stretch(p, (0.25, 0.25, 2.4)), 1.0, detail=4.0, roughness=0.6)
    tone = s.clamp01(s.add(s.mul(large, 0.45), s.add(s.mul(mid, 0.3), s.mul(strata, 0.25))))
    base = s.mix(tone, (0.035, 0.04, 0.05), (0.15, 0.16, 0.17))
    base = s.mix(s.mul(s.smooth(0.55, 0.78, strata), 0.4), base, (0.2, 0.2, 0.19))
    base = s.mix(s.mul(pk.cavity(s, 0.25), 0.8), base, (0.012, 0.014, 0.018))
    edge = pk.wear(s, 0.04)
    base = s.mix(s.mul(edge, 0.3), base, (0.3, 0.31, 0.32))
    rough = s.remap(mid, 0.3, 0.7, 0.62, 0.86)
    base, rough = frost(s, p, base, rough)
    height = s.add(s.mul(mid, 0.7), s.mul(strata, 0.3))
    return s.finish(base, rough, height=height, bump=0.5, bump_distance=0.03, bake_size=bake_size)


def roughen(obj, scale, amount):
    from mathutils import noise
    for vertex in obj.data.vertices:
        offset = noise.noise(vertex.co * scale)
        vertex.co += vertex.normal * offset * amount


def cave_arch(m, target, rng):
    shapes = [
        pk.ellipsoid_bm((0.0, 3.0, 4.0), (9.5, 5.2, 7.4), segments=32, rings=20),
        pk.ellipsoid_bm((-6.8, 0.6, 2.4), (3.2, 3.4, 5.2), segments=20, rings=14),
        pk.ellipsoid_bm((6.6, 0.8, 2.0), (3.4, 3.2, 4.8), segments=20, rings=14),
        pk.capsule_bm((-5.5, -0.4, 6.4), (5.2, -0.2, 6.9), 2.2, 1.9, segments=20),
        pk.ellipsoid_bm((2.0, 4.5, 8.5), (5.0, 3.6, 3.4), segments=20, rings=12),
    ]
    carve = [pk.capsule_bm((0.0, -4.0, 1.0), (0.4, 12.0, 0.2), 3.6, 2.6, segments=24, rings=6)]
    arch = pk.sculpt(shapes, target, m['rock'], 'CaveArch_A', 0.12, smooth=3, smooth_factor=0.5, rng=rng, carve=carve)
    roughen(arch, 0.35, 0.35)
    arch.data.update()
    decimate = arch.modifiers.new('Decimate', 'DECIMATE')
    decimate.ratio = min(1.0, 14000 / max(len(arch.data.polygons), 1))
    kit.apply_modifiers(arch)
    arch.data.shade_smooth()
    pk.tag(arch, rng=rng)
    return arch


def loft(rings, caps=True):
    bm = bmesh.new()
    vertices = [[bm.verts.new(co) for co in ring] for ring in rings]
    count = len(rings[0])
    for a, b in zip(vertices, vertices[1:]):
        for i in range(count):
            j = (i + 1) % count
            bm.faces.new((a[i], a[j], b[j], b[i]))
    if caps:
        bm.faces.new(list(reversed(vertices[0])))
        bm.faces.new(vertices[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def tube(points, radii, sides=10, flat=1.0):
    points = [Vector(p) for p in points]
    rings = []
    side = None
    for i, p in enumerate(points):
        forward = (points[min(i + 1, len(points) - 1)] - points[max(i - 1, 0)]).normalized()
        if side is None:
            helper = Vector((0.0, 0.0, 1.0)) if abs(forward.z) < 0.9 else Vector((1.0, 0.0, 0.0))
            side = forward.cross(helper).normalized()
        else:
            side = (side - forward * side.dot(forward)).normalized()
        other = forward.cross(side).normalized()
        r = radii[i] if isinstance(radii, (list, tuple)) else radii
        rings.append([p + side * (math.cos(2.0 * math.pi * k / sides) * r) + other * (math.sin(2.0 * math.pi * k / sides) * r * flat) for k in range(sides)])
    return loft(rings)


def solid(obj, thickness):
    modifier = obj.modifiers.new('Solid', 'SOLIDIFY')
    modifier.thickness = thickness
    modifier.offset = 0.0
    kit.apply_modifiers(obj)
    return obj


def plank(name, length, width, thick, at, yaw, m, target, rng, tilt=0.0, roll=0.0, chips=3):
    bm = pk.chipped((length, width, thick), rng, chips=chips, depth=0.025)
    obj = pk.part(name, bm, target, m['wood'], rng=rng, bevel=(0.006, 1, 40.0))
    obj.matrix_world = Matrix.Translation(at) @ Matrix.Rotation(yaw, 4, 'Z') @ Matrix.Rotation(tilt, 4, 'Y') @ Matrix.Rotation(roll, 4, 'X')
    return obj


def bake_transform(obj):
    obj.data.transform(obj.matrix_world)
    obj.matrix_world = Matrix.Identity(4)
    return obj


def piling(m, target, rng):
    rings = []
    height = 6.0
    for k in range(16):
        f = k / 15.0
        z = height * f
        r = 0.17 - 0.02 * f
        ring = []
        for i in range(12):
            a = 2.0 * math.pi * i / 12.0
            top = z if k < 15 else height - 0.12 * (0.5 + 0.5 * math.sin(a * 3.0 + 0.7)) - 0.05 * rng.random()
            wob = 1.0 + 0.04 * math.sin(a * 2.0 + f * 9.0)
            ring.append((math.cos(a) * r * wob, math.sin(a) * r * wob, top))
        rings.append(ring)
    post = pk.part('Post', loft(rings), target, m['wood'], rng=rng, smooth=50.0)
    band = pk.part('Band', pk.lathe([(0.0, 5.15), (0.19, 5.15), (0.2, 5.2), (0.2, 5.36), (0.19, 5.4), (0.0, 5.4)], 16), target, m['iron'], rng=rng, smooth=40.0)
    rope = []
    for k in range(3):
        z = 4.6 - 0.07 * k
        rope.append(pk.part(f'Rope{k}', tube([(math.cos(a) * 0.18, math.sin(a) * 0.18, z + 0.02 * math.sin(a * 2.0)) for a in [2.0 * math.pi * i / 16.0 for i in range(17)]], 0.03, sides=6), target, m['wood'], rng=rng, smooth=50.0))
    return kit.join([post, band] + rope, 'Piling_A')


def deck(name, m, target, rng, broken):
    parts = []
    for side in (-1.0, 1.0):
        length = 1.9 if not broken or side < 0 else 1.15
        beam = plank(f'Beam{int(side)}', length, 0.2, 0.24, (-(1.9 - length) * 0.5, side * 0.78, -0.18), 0.0, m, target, rng, tilt=(0.18 if broken and side > 0 else 0.0))
        parts.append(beam)
    x = -0.82
    k = 0
    while x < 0.9:
        width = rng.uniform(0.17, 0.22)
        missing = broken and (x > 0.35 and rng.random() < 0.55 or rng.random() < 0.18)
        if not missing:
            length = 1.86 if not broken else rng.uniform(0.9, 1.86)
            shift = 0.0 if not broken else (1.86 - length) * (0.5 if rng.random() < 0.5 else -0.5)
            tilt = rng.uniform(-0.03, 0.03) + (rng.uniform(-0.25, 0.25) if broken and rng.random() < 0.3 else 0.0)
            parts.append(plank(f'Plank{k}', width, length, 0.06, (x + width * 0.5, shift, rng.uniform(-0.01, 0.01)), rng.uniform(-0.02, 0.02), m, target, rng, roll=tilt))
        x += width + rng.uniform(0.02, 0.045)
        k += 1
    for obj in parts:
        bake_transform(obj)
    return kit.join(parts, name)


def hull(m, target, rng):
    length = 10.0
    parts = []

    def section(t, s):
        width = 1.75 * math.sin(math.pi * min(max(t, 0.0), 1.0)) ** 0.55 + 0.05
        depth = 1.7 - 0.25 * abs(2.0 * t - 1.0)
        a = s * math.pi * 0.5
        return Vector((math.sin(a) * width, (t - 0.5) * length, 1.85 - math.cos(a) * depth - (0.25 * abs(2.0 * t - 1.0) ** 3)))

    keel = pk.part('Keel', tube([section(t, 0.0) + Vector((0.0, 0.0, -0.08)) for t in [i / 20.0 for i in range(21)]], 0.12, sides=6, flat=1.6), target, m['wood'], rng=rng, smooth=40.0)
    parts.append(keel)
    for k in range(14):
        t = 0.06 + 0.88 * k / 13.0
        reach_left = -1.0 if rng.random() > 0.2 else rng.uniform(-0.7, -0.3)
        reach_right = 1.0 if rng.random() > 0.35 else rng.uniform(0.2, 0.7)
        points = [section(t, reach_left + (reach_right - reach_left) * i / 16.0) for i in range(17)]
        parts.append(pk.part(f'Rib{k}', tube(points, 0.075, sides=6, flat=1.4), target, m['wood'], rng=rng, smooth=40.0))
    for strip in range(9):
        s0 = -0.95 + strip * 0.21
        s1 = s0 + 0.19
        start = rng.uniform(0.04, 0.25)
        end = rng.uniform(0.6, 0.96) if s0 < 0.3 else rng.uniform(0.3, 0.6)
        if strip >= 7 and rng.random() < 0.5:
            continue
        rows = []
        for i in range(25):
            t = start + (end - start) * i / 24.0
            rows.append([section(t, s0) * 1.01, section(t, s1) * 1.01])
        bm = bmesh.new()
        verts = [[bm.verts.new(p) for p in row] for row in rows]
        for a, b in zip(verts, verts[1:]):
            bm.faces.new((a[0], a[1], b[1], b[0]))
        obj = pk.part(f'Strake{strip}', bm, target, m['wood'], rng=rng, smooth=40.0)
        solid(obj, 0.06)
        parts.append(obj)
    mast = pk.part('Mast', pk.lathe([(0.0, 0.0), (0.2, 0.0), (0.19, 1.6), (0.12, 1.9), (0.0, 1.75)], 10), target, m['wood'], rng=rng, smooth=45.0)
    mast.matrix_world = Matrix.Translation((0.0, -0.8, 0.3)) @ Matrix.Rotation(0.18, 4, 'X')
    bake_transform(mast)
    parts.append(mast)
    for k in range(3):
        y = -3.2 + 2.6 * k
        width = section((y / length) + 0.5, 1.0).x
        parts.append(bake_transform(plank(f'Thwart{k}', 0.25, width * 1.7, 0.08, (0.0, y, 1.45), math.pi * 0.5, m, target, rng)))
    return kit.join(parts, 'Hull_A')


def lighthouse(m, target, rng):
    outer = []
    inner = []
    sides = 28
    levels = 18
    tops = [6.2 + 1.3 * (0.5 + 0.5 * math.sin(2.0 * math.pi * i / sides * 2.0 + 0.6)) + rng.uniform(-0.35, 0.35) for i in range(sides)]
    for k in range(levels + 1):
        f = k / levels
        ring_o = []
        ring_i = []
        for i in range(sides):
            a = 2.0 * math.pi * i / sides
            z = tops[i] * f
            r = 2.05 - 0.4 * f
            ring_o.append((math.cos(a) * r, math.sin(a) * r, z))
            ring_i.append((math.cos(a) * (r - 0.42), math.sin(a) * (r - 0.42), z))
        outer.append(ring_o)
        inner.append(ring_i)
    bm = bmesh.new()
    vo = [[bm.verts.new(p) for p in ring] for ring in outer]
    vi = [[bm.verts.new(p) for p in ring] for ring in inner]
    for k in range(levels):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((vo[k][i], vo[k][j], vo[k + 1][j], vo[k + 1][i]))
            bm.faces.new((vi[k][j], vi[k][i], vi[k + 1][i], vi[k + 1][j]))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((vo[levels][i], vo[levels][j], vi[levels][j], vi[levels][i]))
        bm.faces.new((vo[0][j], vo[0][i], vi[0][i], vi[0][j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    tower = pk.part('Tower', bm, target, m['stone'], rng=rng, smooth=35.0)
    cutters = [pk.box((0.9, 1.4, 1.9), (0.0, -1.9, 0.95)), pk.box((1.4, 0.4, 0.75), (1.75, 0.0, 3.6)), pk.box((0.4, 1.4, 0.75), (-0.3, 1.7, 4.9))]
    for index, cutter in enumerate(cutters):
        tool = kit.mesh_object(f'Cut{index}', cutter, target)
        boolean = tower.modifiers.new('Cut', 'BOOLEAN')
        boolean.operation = 'DIFFERENCE'
        boolean.solver = 'EXACT'
        boolean.object = tool
        kit.apply_modifiers(tower)
        bpy_remove(tool)
    pk.tag(tower, rng=rng)
    plinth = pk.part('Plinth', pk.lathe([(0.0, -0.4), (2.55, -0.4), (2.6, 0.2), (2.45, 0.42), (0.0, 0.42)], 28), target, m['stone'], rng=rng, bevel=(0.03, 2, 40.0))
    parts = [tower, plinth]
    for k in range(9):
        a = rng.uniform(0.0, 2.0 * math.pi)
        r = rng.uniform(2.7, 4.2)
        size = (rng.uniform(0.45, 0.7), rng.uniform(0.3, 0.42), rng.uniform(0.28, 0.36))
        block = pk.part(f'Block{k}', pk.chipped(size, rng, chips=3, depth=0.05), target, m['stone'], rng=rng, bevel=(0.02, 2, 40.0))
        block.matrix_world = Matrix.Translation((math.cos(a) * r, math.sin(a) * r, size[2] * 0.4)) @ Matrix.Rotation(rng.uniform(0, 6.28), 4, 'Z') @ Matrix.Rotation(rng.uniform(-0.3, 0.3), 4, 'X')
        parts.append(bake_transform(block))
    return kit.join(parts, 'Lighthouse_A')


def bpy_remove(obj):
    import bpy
    bpy.data.objects.remove(obj, do_unlink=True)


def anchor(m, target, rng):
    parts = [pk.part('Shank', pk.capsule_bm((0.0, 0.0, 0.1), (0.0, 0.0, 1.9), 0.075, 0.06, segments=10, rings=2), target, m['iron'], rng=rng, smooth=40.0)]
    arm = [(math.sin(a) * 0.85, 0.0, 0.55 - math.cos(a) * 0.5) for a in [(-1.0 + 2.0 * i / 16.0) * 1.2 for i in range(17)]]
    parts.append(pk.part('Arms', tube(arm, [0.07 - 0.02 * abs(-1.0 + 2.0 * i / 16.0) for i in range(17)], sides=8), target, m['iron'], rng=rng, smooth=40.0))
    for side in (-1.0, 1.0):
        tip = Vector(arm[0 if side < 0 else -1])
        parts.append(pk.part(f'Fluke{int(side)}', pk.cone_bm(tip - Vector((side * 0.12, 0.0, 0.2)), tip + Vector((side * 0.1, 0.0, 0.3)), 0.2, sides=4, flatten=0.25), target, m['iron'], rng=rng, smooth=30.0))
    parts.append(pk.part('Stock', pk.capsule_bm((0.0, -0.75, 1.7), (0.0, 0.75, 1.7), 0.05, 0.05, segments=8, rings=2), target, m['iron'], rng=rng, smooth=40.0))
    ring = [(0.0, math.cos(a) * 0.18, 2.1 + math.sin(a) * 0.18) for a in [2.0 * math.pi * i / 18.0 for i in range(19)]]
    parts.append(pk.part('Ring', tube(ring, 0.035, sides=6), target, m['iron'], rng=rng, smooth=40.0))
    return kit.join(parts, 'Anchor_A')


def rowboat(m, target, rng):
    rows = []
    for i in range(17):
        t = i / 16.0
        width = 0.72 * math.sin(math.pi * t) ** 0.7 + 0.03
        depth = 0.5 + 0.1 * abs(2.0 * t - 1.0) ** 2
        rise = 0.18 * abs(2.0 * t - 1.0) ** 3
        ring = []
        for k in range(11):
            a = (-1.0 + 2.0 * k / 10.0) * math.pi * 0.5
            ring.append(Vector((math.sin(a) * width, (t - 0.5) * 3.6, depth - math.cos(a) * depth + rise)))
        rows.append(ring)
    bm = bmesh.new()
    verts = [[bm.verts.new(p) for p in row] for row in rows]
    for a, b in zip(verts, verts[1:]):
        for k in range(10):
            bm.faces.new((a[k], a[k + 1], b[k + 1], b[k]))
    shell = pk.part('Shell', bm, target, m['wood'], rng=rng, smooth=40.0)
    solid(shell, 0.05)
    parts = [shell]
    for k, y in enumerate((-0.6, 0.55)):
        parts.append(bake_transform(plank(f'Seat{k}', 0.22, 1.25, 0.05, (0.0, y, 0.38), math.pi * 0.5, m, target, rng)))
    oar = pk.part('Oar', pk.capsule_bm((-0.2, -1.1, 0.5), (0.4, 1.3, 0.62), 0.025, 0.025, segments=6, rings=2), target, m['wood'], rng=rng, smooth=40.0)
    parts.append(oar)
    return kit.join(parts, 'Rowboat_A')


def bollard(m, target, rng):
    body = pk.part('Bollard_A', pk.lathe([(0.0, 0.0), (0.24, 0.0), (0.24, 0.1), (0.15, 0.16), (0.13, 0.5), (0.15, 0.56), (0.21, 0.62), (0.2, 0.7), (0.0, 0.72)], 18), target, m['iron'], rng=rng, smooth=40.0)
    return body


def build(target):
    m = {
        'wood': harbor_wood('HarborWood'),
        'iron': pk.iron_material('HarborIron', rust=0.9),
        'stone': harbor_stone('HarborStone'),
        'wet': harbor_wood('HarborWet', light=(0.13, 0.11, 0.085), dark=(0.04, 0.035, 0.03), wet=True),
        'rock': harbor_rock('HarborRock'),
    }
    return [
        piling(m, target, random.Random(11)),
        deck('PierDeck_A', m, target, random.Random(12), False),
        deck('PierDeck_B', m, target, random.Random(13), True),
        hull(dict(m, wood=m['wet']), target, random.Random(14)),
        lighthouse(m, target, random.Random(15)),
        anchor(m, target, random.Random(16)),
        rowboat(m, target, random.Random(17)),
        bollard(m, target, random.Random(18)),
        cave_arch(m, target, random.Random(19)),
    ]


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    for obj in objects:
        print(f'{obj.name}: {len(obj.data.polygons)} faces')
    pk.unwrap_all(objects, {'HarborWood': 0.004, 'HarborWet': 0.004, 'HarborIron': 0.006, 'HarborStone': 0.004, 'HarborRock': 0.003}, angle=55.0)
    x = 0.0
    for obj in objects:
        obj.matrix_world = Matrix.Translation((x, 0.0, 0.0))
        x += 14.0
    if args.get('--preview'):
        pk.ground_preview(80.0)
        for name, group in (('hull', ['Hull_A', 'Anchor_A', 'Rowboat_A']), ('pier', ['Piling_A', 'PierDeck_A', 'PierDeck_B', 'Bollard_A']), ('lighthouse', ['Lighthouse_A']), ('cave', ['CaveArch_A'])):
            shown = [obj for obj in objects if obj.name in group]
            for obj in objects:
                obj.hide_render = obj not in shown
            for k, obj in enumerate(shown):
                obj.matrix_world = Matrix.Translation((k * 7.0 - 0.5 * (len(shown) - 1) * 7.0, 0.0, 0.0))
            pk.preview(shown, args['--preview'], name, fill=1.1)
        x = 0.0
        for obj in objects:
            obj.hide_render = False
            obj.matrix_world = Matrix.Translation((x, 0.0, 0.0))
            x += 14.0
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.4)


if __name__ == '__main__':
    main()
