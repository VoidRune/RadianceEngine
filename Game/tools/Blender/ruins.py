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
import emberhold

NAME = 'Ruins'
FLUTES = 16
AROUND = 96
SHAFT_BOTTOM = 0.335
SHAFT_TOP = 2.86
SHAFT_RADIUS = 0.32
TOP_RADIUS = 0.282
BLOCK_BEVEL = (0.032, 2, 30.0)
DRUM_BEVEL = (0.03, 2, 40.0)
SKY = (0.25, 0.42, 0.56)


def shaft_radius(z):
    t = min(max((z - SHAFT_BOTTOM) / (SHAFT_TOP - SHAFT_BOTTOM), 0.0), 1.0)
    return SHAFT_RADIUS + (TOP_RADIUS - SHAFT_RADIUS) * t + 0.008 * math.sin(math.pi * t)


def weathering(s, p, base, rough, height, bright, stain=0.35, moss=1.0, ochre=0.75, grey=0.6):
    ground = s.separate(s.coords('Object'))[2]
    normal_z = s.separate(s.geometry('Normal'))[2]
    up = s.smooth(0.35, 0.85, normal_z)
    dirt = pk.cavity(s, 0.12)
    blotch = s.smooth(0.45, 0.75, s.noise(s.offset(p, (7.0, 3.0, 1.0)), 0.8, detail=3.0))
    base = s.mix(s.mul(blotch, stain), base, (0.3, 0.23, 0.14))
    streak = s.mul(s.smooth(0.5, 0.75, s.noise(s.stretch(p, (10.0, 10.0, 0.6)), 1.0, detail=3.0, roughness=0.6)), s.sub(1.0, up))
    base = s.mix(s.mul(streak, 0.35), base, (0.16, 0.15, 0.13))
    grime = s.mul(s.sub(1.0, s.smooth(0.0, 0.4, ground)), s.smooth(0.3, 0.6, s.noise(p, 3.0, detail=3.0)))
    base = s.mix(s.mul(grime, 0.5), base, (0.12, 0.1, 0.07))
    base = s.mix(s.mul(dirt, 0.75), base, (0.07, 0.06, 0.045))
    colony = s.smooth(0.56, 0.68, s.noise(s.offset(p, (3.0, 11.0, 5.0)), 1.4, detail=3.0))
    gold = s.mul(s.sub(1.0, s.smooth(0.25, 0.5, s.voronoi(p, 4.5, 'F1', 1.0, 'Distance'))), colony)
    base = s.mix(s.mul(gold, ochre), base, (0.42, 0.29, 0.07))
    grey_colony = s.smooth(0.52, 0.66, s.noise(s.offset(p, (13.0, 2.0, 7.0)), 1.2, detail=3.0))
    crust = s.mul(s.sub(1.0, s.smooth(0.25, 0.5, s.voronoi(s.offset(p, (13.1, 7.3, 2.9)), 4.0, 'F1', 1.0, 'Distance'))), grey_colony)
    base = s.mix(s.mul(crust, grey), base, (0.3, 0.34, 0.37))
    patches = s.smooth(0.55, 0.65, s.add(s.noise(s.offset(p, (2.0, 9.0, 4.0)), 2.0, detail=4.0, roughness=0.6), s.mul(s.noise(p, 14.0, detail=2.0), 0.08)))
    moss_mask = s.clamp01(s.mul(s.add(s.mul(dirt, 1.3), s.mul(up, 0.55)), patches))
    base = s.mix(s.mul(moss_mask, 0.92 * moss), base, (0.06, 0.12, 0.025))
    edge = pk.wear(s, 0.03)
    base = s.mix(s.mul(edge, 0.5), base, bright)
    rough = s.lerp(s.mul(edge, 0.5), rough, 0.66)
    rough = s.lerp(s.mul(moss_mask, moss), rough, 0.95)
    height = s.add(height, s.mul(s.mul(moss_mask, moss), 0.25))
    return base, rough, height


def marble_material(name, bake_size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    island = s.geometry('Random Per Island')
    q = s.offset(p, s.combine(s.mul(island, 31.0), s.mul(island, 47.0), s.mul(island, 59.0)))
    large = s.noise(q, 0.8, detail=3.0)
    mid = s.noise(q, 3.0, detail=4.0, roughness=0.55)
    fine = s.noise(q, 30.0, detail=2.0)
    tone = s.clamp01(s.add(s.mul(s.smooth(0.25, 0.75, large), 0.45), s.add(s.mul(mid, 0.15), s.mul(island, 0.4))))
    base = s.mix(tone, (0.36, 0.33, 0.27), (0.56, 0.54, 0.49))
    base = s.mix(s.mul(s.smooth(0.6, 0.9, island), 0.3), base, (0.46, 0.39, 0.27))
    veins_field = s.noise(q, 1.2, detail=5.0, roughness=0.6, distortion=1.4)
    veins = s.mul(s.sub(1.0, s.smooth(0.0, 0.06, s.math('ABSOLUTE', s.sub(veins_field, 0.5)))), s.smooth(0.25, 0.6, island))
    base = s.mix(s.mul(veins, 0.35), base, (0.25, 0.25, 0.25))
    pits = s.mul(s.sub(1.0, s.smooth(0.05, 0.2, s.voronoi(q, 9.0, 'F1', 1.0, 'Distance'))), s.smooth(0.5, 0.7, s.noise(q, 2.0, detail=2.0)))
    base = s.mix(s.mul(pits, 0.4), base, (0.18, 0.16, 0.12))
    normal_z = s.separate(s.geometry('Normal'))[2]
    level = s.node('ShaderNodeAttribute', attribute_name='Paint', attribute_type='GEOMETRY').outputs['Fac']
    band = s.mul(s.smooth(0.32, 0.36, level), s.sub(1.0, s.smooth(0.64, 0.68, level)))
    side = s.sub(1.0, s.smooth(0.4, 0.7, s.math('ABSOLUTE', normal_z)))
    fade = s.smooth(0.3, 0.52, s.noise(q, 4.0, detail=5.0, roughness=0.65))
    base = s.mix(s.mul(s.mul(s.mul(band, side), fade), 0.75), base, SKY)
    rough = s.remap(fine, 0.3, 0.7, 0.64, 0.8)
    height = s.add(s.mul(mid, 0.6), s.mul(large, 0.3))
    base, rough, height = weathering(s, q, base, rough, height, (0.66, 0.64, 0.58), stain=0.18, moss=0.6, ochre=0.45, grey=0.7)
    return s.finish(base, rough, height=height, bump=0.16, bump_distance=0.01, bake_size=bake_size)


def stone_material(name, bake_size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    island = s.geometry('Random Per Island')
    q = s.offset(p, s.combine(s.mul(island, 23.0), s.mul(island, 41.0), s.mul(island, 67.0)))
    large = s.noise(q, 1.0, detail=3.0)
    mid = s.noise(q, 3.6, detail=5.0, roughness=0.55)
    fine = s.noise(q, 34.0, detail=2.0)
    tone = s.clamp01(s.add(s.mul(s.smooth(0.25, 0.75, large), 0.35), s.add(s.mul(mid, 0.15), s.mul(island, 0.5))))
    base = s.mix(tone, (0.22, 0.165, 0.1), (0.44, 0.36, 0.24))
    base = s.mix(s.mul(s.smooth(0.72, 0.95, island), 0.5), base, (0.3, 0.29, 0.26))
    strata = s.wave(s.stretch(q, (0.25, 0.25, 1.0)), 3.0, kind='BANDS', direction='Z', distortion=2.0, detail=1.0)
    base = s.mix(s.mul(strata, 0.12), base, (0.5, 0.42, 0.29))
    pits = s.mul(s.sub(1.0, s.smooth(0.05, 0.22, s.voronoi(q, 8.0, 'F1', 1.0, 'Distance'))), s.smooth(0.45, 0.65, s.noise(q, 1.8, detail=2.0)))
    base = s.mix(s.mul(pits, 0.45), base, (0.13, 0.1, 0.07))
    rough = s.remap(fine, 0.3, 0.7, 0.8, 0.93)
    height = s.sub(s.add(s.mul(mid, 0.6), s.mul(large, 0.3)), s.mul(pits, 0.08))
    base, rough, height = weathering(s, q, base, rough, height, (0.55, 0.48, 0.36))
    return s.finish(base, rough, height=height, bump=0.2, bump_distance=0.01, bake_size=bake_size)


def smoothstep(low, high, x):
    t = min(max((x - low) / (high - low), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def fracture(rng, height, radius, slope=0.15, step=0.12, rough=0.012):
    angle = rng.uniform(0.0, 2.0 * math.pi)
    tilt = (math.cos(angle) * slope, math.sin(angle) * slope)
    cut = rng.uniform(0.0, 2.0 * math.pi)
    across = (math.cos(cut), math.sin(cut))
    offset = rng.uniform(-0.25, 0.25) * radius
    phases = [rng.uniform(0.0, 2.0 * math.pi) for _ in range(3)]
    waves = []
    for _ in range(3):
        a = rng.uniform(0.0, 2.0 * math.pi)
        k = 2.0 * math.pi / rng.uniform(0.16, 0.32)
        waves.append((k * math.cos(a), k * math.sin(a), rng.uniform(0.0, 2.0 * math.pi), rough * rng.uniform(0.6, 1.2)))
    scoop_angle = rng.uniform(0.0, 2.0 * math.pi)
    scoop = (math.cos(scoop_angle) * radius * 0.85, math.sin(scoop_angle) * radius * 0.85)

    def surface(x, y):
        z = height + tilt[0] * x + tilt[1] * y
        along = -across[1] * x + across[0] * y
        d = across[0] * x + across[1] * y - offset + 0.03 * math.sin(along * 23.0 + phases[0]) + 0.02 * math.sin(along * 41.0 + phases[1])
        z += step * smoothstep(-0.025, 0.025, d)
        for kx, ky, phase, amp in waves:
            z += amp * math.sin(kx * x + ky * y + phase)
        rim = math.hypot(x, y) / radius
        z -= 0.03 * smoothstep(0.7, 1.0, rim) * (0.5 + 0.5 * math.sin(math.atan2(y, x) * 5.0 + phases[2]))
        z -= 0.45 * step * math.exp(-((x - scoop[0]) ** 2 + (y - scoop[1]) ** 2) / (0.09 * radius) ** 2 * 0.25)
        return z
    return surface


def fan(bm, ring, center):
    for i in range(len(ring)):
        bm.faces.new((ring[i], ring[(i + 1) % len(ring)], center))


def bridge(bm, a, b):
    for i in range(len(a)):
        k = (i + 1) % len(a)
        bm.faces.new((a[i], a[k], b[k], b[i]))


def fluted_drum(z0, z1, depth=0.02, chamfer=0.03, radius_at=shaft_radius, top=None):
    bm = bmesh.new()
    count = max(2, int(math.ceil((z1 - z0 - 2.0 * chamfer) / 0.16)))
    columns = []
    for i in range(AROUND):
        angle = 2.0 * math.pi * i / AROUND
        c, s = math.cos(angle), math.sin(angle)
        groove = depth * math.sin(0.5 * FLUTES * angle) ** 2
        end = z1 if top is None else top(c * radius_at(z1), s * radius_at(z1))
        upper = end - chamfer if top is None else end
        rings = [(z0, chamfer), (z0 + chamfer, 0.0)] + [(z0 + chamfer + (upper - z0 - chamfer) * k / count, 0.0) for k in range(1, count + 1)]
        if top is None:
            rings.append((end, chamfer))
        column = []
        for z, inset in rings:
            r = radius_at(z) - inset - groove * (0.6 if inset > 0.0 else 1.0)
            column.append(bm.verts.new((r * c, r * s, z)))
        columns.append(column)
    for j in range(len(columns[0]) - 1):
        bridge(bm, [column[j] for column in columns], [column[j + 1] for column in columns])
    fan(bm, [column[0] for column in columns], bm.verts.new((0.0, 0.0, z0)))
    rim = [column[-1] for column in columns]
    if top is None:
        fan(bm, rim, bm.verts.new((0.0, 0.0, z1)))
    else:
        previous = rim
        for scale in (0.9, 0.78, 0.66, 0.54, 0.42, 0.3, 0.18, 0.08):
            ring = [bm.verts.new((v.co.x * scale, v.co.y * scale, top(v.co.x * scale, v.co.y * scale))) for v in rim]
            bridge(bm, previous, ring)
            previous = ring
        fan(bm, previous, bm.verts.new((0.0, 0.0, top(0.0, 0.0))))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def chip_rim(bm, rng, z, radius, chips, depth, up):
    for _ in range(chips):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        radial = Vector((math.cos(angle), math.sin(angle), 0.0))
        tilt = rng.uniform(0.55, 1.0)
        normal = (radial * math.cos(tilt) + Vector((0.0, 0.0, up)) * math.sin(tilt)).normalized()
        point = Vector((radial.x * radius, radial.y * radius, z)) - normal * depth * rng.uniform(0.7, 1.3)
        kit.cut(bm, point, normal)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def band(bm):
    layer = bm.verts.layers.float.new('Paint')
    low = min(vertex.co.z for vertex in bm.verts)
    high = max(vertex.co.z for vertex in bm.verts)
    for vertex in bm.verts:
        vertex[layer] = (vertex.co.z - low) / (high - low)
    return bm


def settle(bm, floor=0.0):
    lowest = min(vertex.co.z for vertex in bm.verts)
    bmesh.ops.translate(bm, vec=Vector((0.0, 0.0, floor - lowest)), verts=bm.verts)
    return bm


def recenter(obj):
    vertices = obj.data.vertices
    low = [min(vertex.co[i] for vertex in vertices) for i in range(3)]
    high = [max(vertex.co[i] for vertex in vertices) for i in range(3)]
    shift = (-0.5 * (low[0] + high[0]), -0.5 * (low[1] + high[1]), -low[2])
    obj.data.transform(Matrix.Translation(shift))
    print(f'RECENTER {obj.name} shift ({shift[0]:.4f}, {shift[1]:.4f}, {shift[2]:.4f})')
    return obj


def fragment(size, rng, breaks=1, chips=4, depth=0.05):
    bm = pk.box(size)
    half = Vector(size) * 0.5
    for _ in range(breaks):
        normal = Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(-0.6, 0.6))).normalized()
        reach = abs(normal.x) * half.x + abs(normal.y) * half.y + abs(normal.z) * half.z
        kit.cut(bm, normal * reach * rng.uniform(0.35, 0.65), normal)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return emberhold.chip(bm, rng, chips, depth)


def column_base(prefix, m, target, rng):
    plinth = pk.part(prefix + '_Plinth', pk.chipped((0.95, 0.95, 0.22), rng, chips=3, depth=0.05, center=(0.0, 0.0, 0.11)), target, m['marble'], rng=rng, bevel=BLOCK_BEVEL)
    torus = pk.lathe([(0.445, 0.215), (0.452, 0.245), (0.438, 0.282), (0.405, 0.306), (0.37, 0.322), (0.342, 0.331), (0.33, SHAFT_BOTTOM), (0.0, SHAFT_BOTTOM)], 64)
    return [plinth, pk.part(prefix + '_Base', torus, target, m['marble'], rng=rng, smooth=35.0)]


def drum_part(name, z0, z1, m, target, rng, chips=(2, 2), top=None):
    bm = fluted_drum(z0, z1, top=top)
    if chips[0]:
        chip_rim(bm, rng, z0, shaft_radius(z0), chips[0], 0.045, -1.0)
    if chips[1] and top is None:
        chip_rim(bm, rng, z1, shaft_radius(z1), chips[1], 0.05, 1.0)
    return pk.part(name, bm, target, m['marble'], rng=rng, bevel=DRUM_BEVEL)


def echinus_profile(z, flip=False):
    r0, r1 = TOP_RADIUS + 0.004, 0.44
    points = [(r0, 0.0), (r0 + (r1 - r0) * 0.1, 0.03), (r0 + (r1 - r0) * 0.32, 0.07), (r0 + (r1 - r0) * 0.6, 0.11), (r0 + (r1 - r0) * 0.86, 0.15), (r1, 0.175)]
    if flip:
        points = [(r, 0.175 - h) for r, h in reversed(points)]
    return [(r, z + h) for r, h in points] + [(0.0, z + (0.0 if flip else 0.175))]


def capital_parts(prefix, z, m, target, rng):
    neck = pk.lathe([(TOP_RADIUS + 0.004, z - 0.006), (TOP_RADIUS + 0.006, z + 0.1), (0.0, z + 0.1)], 64)
    parts = [pk.part(prefix + '_Neck', neck, target, m['marble'], rng=rng, smooth=35.0)]
    echinus = chip_rim(pk.lathe(echinus_profile(z + 0.095), 64), rng, z + 0.27, 0.44, 2, 0.05, 1.0)
    parts.append(pk.part(prefix + '_Echinus', echinus, target, m['marble'], rng=rng, bevel=DRUM_BEVEL))
    abacus = band(pk.chipped((0.95, 0.95, 0.17), rng, chips=4, depth=0.07, center=(0.0, 0.0, z + 0.355)))
    parts.append(pk.part(prefix + '_Abacus', abacus, target, m['marble'], rng=rng, bevel=BLOCK_BEVEL))
    return parts


def column_full(m, target, rng):
    parts = column_base('ColumnA', m, target, rng)
    joints = [SHAFT_BOTTOM - 0.012, 1.17 + rng.uniform(-0.05, 0.05), 2.0 + rng.uniform(-0.05, 0.05), SHAFT_TOP]
    for index in range(3):
        parts.append(drum_part(f'ColumnA_Drum{index}', joints[index], joints[index + 1], m, target, rng, chips=(2, 2 if index < 2 else 1)))
    parts += capital_parts('ColumnA', SHAFT_TOP, m, target, rng)
    return recenter(kit.join(parts, 'Column_A'))


def column_broken(m, target, rng):
    parts = column_base('ColumnB', m, target, rng)
    joint = 1.15 + rng.uniform(-0.04, 0.04)
    parts.append(drum_part('ColumnB_Drum0', SHAFT_BOTTOM - 0.012, joint, m, target, rng, chips=(2, 3)))
    top = fracture(rng, 1.66, SHAFT_RADIUS, slope=0.15, step=0.14)
    parts.append(drum_part('ColumnB_Drum1', joint, 1.66, m, target, rng, chips=(2, 0), top=top))
    return recenter(kit.join(parts, 'Column_B'))


def column_stump(m, target, rng):
    parts = column_base('ColumnC', m, target, rng)
    top = fracture(rng, 0.8, SHAFT_RADIUS, slope=0.12, step=0.1)
    parts.append(drum_part('ColumnC_Drum0', SHAFT_BOTTOM - 0.012, 0.8, m, target, rng, chips=(2, 0), top=top))
    return recenter(kit.join(parts, 'Column_C'))


def fallen_drum(m, target, rng):
    bm = fluted_drum(-0.4, 0.4, radius_at=lambda z: 0.34)
    chip_rim(bm, rng, -0.4, 0.34, 3, 0.06, -1.0)
    chip_rim(bm, rng, 0.4, 0.34, 4, 0.07, 1.0)
    bmesh.ops.transform(bm, matrix=Matrix.Rotation(0.5 * math.pi, 4, 'Y') @ Matrix.Rotation(rng.uniform(0.0, 2.0 * math.pi), 4, 'Z'), verts=bm.verts)
    settle(bm)
    return recenter(pk.part('Drum_A', bm, target, m['marble'], rng=rng, bevel=DRUM_BEVEL))


def fallen_capital(m, target, rng):
    abacus = band(pk.chipped((0.9, 0.9, 0.17), rng, chips=5, depth=0.08, center=(0.0, 0.0, 0.085)))
    echinus = chip_rim(pk.lathe(echinus_profile(0.165, flip=True), 64), rng, 0.165, 0.44, 2, 0.05, -1.0)
    neck = fluted_drum(0.33, 0.43, depth=0.016, radius_at=lambda z: TOP_RADIUS + 0.004, top=fracture(rng, 0.43, TOP_RADIUS, slope=0.06, step=0.035, rough=0.006))
    pose = Matrix.Rotation(0.15, 4, 'Z') @ Matrix.Rotation(math.radians(5.0), 4, 'X')
    pieces = (abacus, echinus, neck)
    for piece in pieces:
        bmesh.ops.transform(piece, matrix=pose, verts=piece.verts)
    lowest = min(min(vertex.co.z for vertex in piece.verts) for piece in pieces)
    for piece in pieces:
        bmesh.ops.translate(piece, vec=Vector((0.0, 0.0, -lowest)), verts=piece.verts)
    raised = pose @ Vector((0.0, 0.43, 0.0))
    wedge = fragment((0.22, 0.17, 0.11), rng, breaks=1, chips=3, depth=0.03)
    bmesh.ops.transform(wedge, matrix=Matrix.Rotation(0.4, 4, 'Z') @ Matrix.Rotation(0.15, 4, 'Y'), verts=wedge.verts)
    settle(wedge)
    bmesh.ops.translate(wedge, vec=Vector((raised.x, raised.y, 0.0)), verts=wedge.verts)
    parts = [
        pk.part('CapitalA_Abacus', abacus, target, m['marble'], rng=rng, bevel=BLOCK_BEVEL),
        pk.part('CapitalA_Echinus', echinus, target, m['marble'], rng=rng, bevel=DRUM_BEVEL),
        pk.part('CapitalA_Neck', neck, target, m['marble'], rng=rng, bevel=DRUM_BEVEL),
        pk.part('CapitalA_Wedge', wedge, target, m['stone'], rng=rng, bevel=(0.03, 2, 30.0)),
    ]
    return recenter(kit.join(parts, 'Capital_A'))


def ashlar(name, size, center, m, target, rng, yaw=0.0, chips=None, depth=0.035, broken=False):
    bm = pk.chipped(size, rng, chips=rng.randint(1, 3) if chips is None else chips, depth=depth)
    if broken:
        normal = Vector((rng.uniform(-0.5, 0.5), rng.uniform(-0.3, 0.3), 1.0)).normalized()
        kit.cut(bm, Vector((rng.uniform(-0.25, 0.25) * size[0], 0.0, 0.5 * size[2] - rng.uniform(0.05, 0.1))), normal)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    matrix = Matrix.Translation(center) @ Matrix.Rotation(yaw, 4, 'Z')
    bmesh.ops.transform(bm, matrix=matrix, verts=bm.verts)
    return pk.part(name, bm, target, m, frame=matrix, rng=rng, bevel=BLOCK_BEVEL)


def arch(m, target, rng):
    parts = []
    stone = m['stone']
    inner, outer, depth = 0.95, 1.4, 0.56
    spring = 3.3 - outer
    for side in (-1.0, 1.0):
        x = side * 1.25
        tag = 'L' if side < 0.0 else 'R'
        parts.append(ashlar(f'ArchPier{tag}_Base', (0.74, 0.72, 0.28), (x, 0.0, 0.14), stone, target, rng, yaw=rng.uniform(-0.02, 0.02), chips=3, depth=0.05))
        z = 0.28
        for course, height in enumerate((0.46, 0.44, 0.46)):
            size = (0.6 - rng.uniform(0.0, 0.03), 0.6 - rng.uniform(0.0, 0.03), height - 0.012)
            parts.append(ashlar(f'ArchPier{tag}_{course}', size, (x + rng.uniform(-0.015, 0.015), rng.uniform(-0.015, 0.015), z + height * 0.5), stone, target, rng, yaw=rng.uniform(-0.04, 0.04), chips=rng.randint(2, 4), depth=0.045))
            z += height
        parts.append(ashlar(f'ArchPier{tag}_Impost', (0.74, 0.68, spring - z - 0.006), (x - side * 0.04, 0.0, (z + spring) * 0.5), stone, target, rng, chips=3, depth=0.05))
    count = 11
    step = math.pi / count
    gap = math.radians(0.9)
    center = (0.0, 0.0, spring)
    missing = 2
    for k in range(count):
        if k == missing:
            continue
        start = k * step + gap
        end = (k + 1) * step - gap
        middle = 0.5 * (start + end)
        r_out = outer + (0.07 if k == count // 2 else rng.uniform(-0.03, 0.02))
        block = emberhold.chip(emberhold.voussoir(inner, r_out, start, end, depth + rng.uniform(-0.03, 0.02), center), rng, rng.randint(2, 4), 0.045)
        frame = emberhold.block_frame(middle, (inner + r_out) * 0.5, depth, center)
        if k == missing + 1:
            pivot = Vector((math.cos(middle) * inner, 0.0, spring + math.sin(middle) * inner))
            slip = Matrix.Translation(pivot + Vector((0.02, 0.0, -0.05))) @ Matrix.Rotation(math.radians(-6.0), 4, 'Y') @ Matrix.Translation(-pivot)
            bmesh.ops.transform(block, matrix=slip, verts=block.verts)
        elif k == count // 2:
            bmesh.ops.translate(block, vec=Vector((0.0, 0.0, -0.035)), verts=block.verts)
        parts.append(pk.part(f'ArchVoussoir{k}', block, target, stone, frame=frame, rng=rng, bevel=(0.028, 2, 30.0)))
    return recenter(kit.join(parts, 'Arch_A'))


def wall_run(prefix, length, depth, courses, profile, m, target, rng, place=Matrix.Identity(4), keep=(1.0, -1.0), gap=0.012):
    parts, tops = [], []
    yaw = math.atan2(place[1][0], place[0][0])
    z = 0.0
    for row, height in enumerate(courses):
        cursor = -0.5 * length
        index = 0
        while cursor < 0.5 * length - 0.04:
            span = rng.uniform(0.4, 0.9)
            if index == 0 and row % 2 == 1:
                span *= 0.55
            end = min(cursor + span, 0.5 * length)
            if 0.5 * length - end < 0.24:
                end = 0.5 * length
            middle = 0.5 * (cursor + end)
            top = z + height
            limit = profile(middle)
            exposed = top > limit - 0.5 * height
            kept = keep[0] <= middle <= keep[1]
            if top <= limit + 1e-4 and not (exposed and not kept and rng.random() < 0.18):
                broken = exposed and not kept and rng.random() < 0.35
                size = (end - cursor - gap, depth - rng.uniform(0.0, 0.04), height - gap)
                center = place @ Vector((middle, rng.uniform(-0.022, 0.022), z + 0.5 * height))
                chips = rng.randint(3, 5) if exposed else rng.randint(1, 3)
                parts.append(ashlar(f'{prefix}_{row}_{index}', size, center, m, target, rng, yaw=yaw + rng.uniform(-0.035, 0.035), chips=chips, depth=0.06 if exposed else 0.035, broken=broken))
                tops.append((cursor, end, top, broken))
            cursor = end
            index += 1
        z += height
    return parts, tops


def surface(tops, x):
    return max([(top, broken) for start, end, top, broken in tops if start - 1e-4 <= x <= end + 1e-4] + [(0.0, False)])


def plateaus(tops, low, high, half):
    found = []
    x = low
    while x <= high:
        samples = [surface(tops, x + d) for d in (-half, -0.5 * half, 0.0, 0.5 * half, half)]
        heights = [z for z, _ in samples]
        if not any(broken for _, broken in samples) and max(heights) - min(heights) < 1e-4:
            found.append((x, heights[0]))
        x += 0.02
    return found


def loose_block(name, size, at, m, target, rng, tilt=0.0, yaw=0.0, roll=0.0):
    bm = pk.chipped(size, rng, chips=rng.randint(3, 5), depth=0.05)
    rotation = Matrix.Rotation(yaw, 4, 'Z') @ Matrix.Rotation(tilt, 4, 'X') @ Matrix.Rotation(roll, 4, 'Y')
    bmesh.ops.transform(bm, matrix=rotation, verts=bm.verts)
    lowest = min(vertex.co.z for vertex in bm.verts)
    bmesh.ops.translate(bm, vec=Vector((at[0], at[1], at[2] - lowest)), verts=bm.verts)
    return pk.part(name, bm, target, m, rng=rng, bevel=BLOCK_BEVEL)


def stepped(steps):
    def profile(x):
        for limit, height in steps:
            if x <= limit:
                return height
        return steps[-1][1]
    return profile


def wall_long(m, target, rng):
    courses = (0.34, 0.3, 0.3, 0.28, 0.26, 0.26)
    profile = stepped(((-0.55, 1.74), (0.05, 1.48), (0.55, 1.22), (1.4, 0.94)))
    parts, tops = wall_run('WallA', 2.6, 0.5, courses, profile, m['stone'], target, rng, keep=(-0.55, 0.05))
    spots = [spot for spot in plateaus(tops, -1.1, 1.1, 0.17) if 0.9 < spot[1] < 1.7]
    spot = max(spots, key=lambda spot: (round(spot[1], 3), -abs(spot[0] + 0.2))) if spots else (-0.2, -0.48, 0.0)
    parts.append(loose_block('WallA_LooseTop', (0.3, 0.26, 0.18), (spot[0], 0.03 if len(spot) == 2 else spot[1], spot[-1] - 0.012), m['stone'], target, rng, tilt=0.08, yaw=0.3))
    print(f'LOOSE WallA_LooseTop at {spot}')
    parts.append(loose_block('WallA_Fallen0', (0.56, 0.42, 0.26), (0.62, -0.52, 0.0), m['stone'], target, rng, tilt=0.18, yaw=0.5, roll=0.1))
    parts.append(loose_block('WallA_Fallen1', (0.34, 0.3, 0.22), (-0.75, 0.5, 0.0), m['stone'], target, rng, tilt=-0.3, yaw=1.2))
    return recenter(kit.join(parts, 'Wall_A'))


def wall_corner(m, target, rng):
    courses = (0.34, 0.32, 0.32, 0.32)
    main = stepped(((-0.35, 1.3), (0.2, 0.98), (0.9, 0.66)))
    parts, _ = wall_run('WallB_Main', 1.6, 0.5, courses, main, m['stone'], target, rng)
    turn = Matrix.Translation((-0.55, -0.6, 0.0)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Z')
    arm, _ = wall_run('WallB_Arm', 0.7, 0.5, courses, stepped(((-0.12, 0.66), (0.4, 1.02))), m['stone'], target, rng, place=turn)
    parts += arm
    parts.append(loose_block('WallB_Fallen', (0.4, 0.34, 0.24), (0.42, -0.55, 0.0), m['stone'], target, rng, tilt=0.3, yaw=0.8))
    return recenter(kit.join(parts, 'Wall_B'))


def pedestal(m, target, rng):
    parts = []
    for index, x in enumerate((-0.3, 0.3)):
        parts.append(ashlar(f'Pedestal_Step{index}', (0.588, 1.2, 0.27), (x, 0.0, 0.135), m['marble'], target, rng, yaw=rng.uniform(-0.02, 0.02), chips=3, depth=0.055))
    parts.append(ashlar('Pedestal_Mid', (0.98, 0.98, 0.24), (0.0, 0.0, 0.39), m['marble'], target, rng, yaw=rng.uniform(-0.03, 0.03), chips=3, depth=0.05))
    parts.append(ashlar('Pedestal_Die', (0.76, 0.76, 0.27), (0.0, 0.0, 0.645), m['marble'], target, rng, yaw=rng.uniform(-0.05, 0.05), chips=4, depth=0.06))
    return recenter(kit.join(parts, 'Pedestal_A'))


def heap(key, m, rng, fragments):
    pieces = []
    for index, (x, y, size, breaks, material) in enumerate(fragments):
        bm = fragment(size, rng, breaks=breaks, chips=rng.randint(3, 5), depth=0.025 + 0.12 * min(size))
        lean = 0.3 if max(size) < 0.2 else 0.16
        rotation = Matrix.Rotation(rng.uniform(0.0, 2.0 * math.pi), 4, 'Z') @ Matrix.Rotation(rng.uniform(-lean, lean), 4, 'X') @ Matrix.Rotation(rng.uniform(-lean, lean), 4, 'Y')
        bmesh.ops.transform(bm, matrix=rotation, verts=bm.verts)
        settle(bm, -0.02)
        bmesh.ops.translate(bm, vec=Vector((x, y, 0.0)), verts=bm.verts)
        pieces.append((f'Rubble{key}_{index}', bm, m[material]))
    return pieces


def rest_on(pieces, bm, x, y, reach):
    floor = max([vertex.co.z for _, other, _ in pieces for vertex in other.verts if (vertex.co.x - x) ** 2 + (vertex.co.y - y) ** 2 < reach * reach] + [0.0])
    settle(bm, floor - 0.05)
    bmesh.ops.translate(bm, vec=Vector((x, y, 0.0)), verts=bm.verts)
    return bm


def rubble_a(m, target, rng):
    pieces = heap('A', m, rng, [
        (-0.18, 0.02, (0.5, 0.38, 0.26), 1, 'stone'),
        (0.26, 0.08, (0.42, 0.34, 0.22), 1, 'stone'),
        (0.05, -0.26, (0.3, 0.26, 0.18), 1, 'stone'),
        (-0.5, -0.2, (0.26, 0.22, 0.16), 2, 'stone'),
        (0.5, -0.18, (0.24, 0.2, 0.15), 1, 'stone'),
        (-0.02, 0.32, (0.28, 0.22, 0.16), 2, 'stone'),
        (-0.62, 0.18, (0.13, 0.11, 0.08), 1, 'stone'),
        (0.62, 0.22, (0.11, 0.1, 0.07), 1, 'stone'),
        (0.3, -0.42, (0.12, 0.1, 0.07), 1, 'stone'),
        (-0.26, -0.42, (0.1, 0.09, 0.06), 1, 'stone'),
        (0.42, 0.36, (0.09, 0.08, 0.06), 1, 'stone'),
    ])
    top = fragment((0.24, 0.18, 0.11), rng, breaks=1, chips=3, depth=0.035)
    bmesh.ops.transform(top, matrix=Matrix.Rotation(0.7, 4, 'Z') @ Matrix.Rotation(0.22, 4, 'X'), verts=top.verts)
    pieces.append(('RubbleA_Top', rest_on(pieces, top, 0.04, 0.05, 0.1), m['stone']))
    return recenter(kit.join([pk.part(name, bm, target, material, rng=rng, bevel=(0.03, 2, 30.0)) for name, bm, material in pieces], 'Rubble_A'))


def fluted_shard(rng):
    shard = fluted_drum(0.0, 0.36, radius_at=lambda z: 0.33)
    kit.cut(shard, Vector((0.12, 0.0, 0.0)), Vector((-1.0, 0.0, 0.0)))
    kit.cut(shard, Vector((0.22, 0.0, 0.3)), Vector((0.35, 0.45, 1.0)).normalized())
    chip_rim(shard, rng, 0.0, 0.33, 2, 0.05, -1.0)
    bmesh.ops.transform(shard, matrix=Matrix.Rotation(rng.uniform(-0.5, 0.5), 4, 'Z') @ Matrix.Rotation(-0.5 * math.pi, 4, 'Y'), verts=shard.verts)
    return settle(shard, -0.02)


def rubble_b(m, target, rng):
    pieces = heap('B', m, rng, [
        (0.4, 0.1, (0.44, 0.36, 0.24), 1, 'stone'),
        (-0.5, -0.15, (0.32, 0.28, 0.2), 1, 'stone'),
        (0.15, -0.3, (0.28, 0.22, 0.16), 2, 'stone'),
        (-0.45, 0.3, (0.2, 0.16, 0.12), 1, 'marble'),
        (0.62, -0.25, (0.12, 0.1, 0.08), 1, 'stone'),
        (-0.68, 0.02, (0.1, 0.09, 0.07), 1, 'stone'),
        (0.02, 0.42, (0.1, 0.09, 0.06), 1, 'marble'),
        (0.35, 0.4, (0.09, 0.08, 0.06), 1, 'stone'),
        (-0.25, -0.4, (0.1, 0.08, 0.06), 1, 'stone'),
    ])
    shard = fluted_shard(rng)
    bmesh.ops.translate(shard, vec=Vector((-0.1, 0.05, 0.0)), verts=shard.verts)
    pieces.append(('RubbleB_Fluted', shard, m['marble']))
    return recenter(kit.join([pk.part(name, bm, target, material, rng=rng, bevel=(0.03, 2, 30.0) if material is m['stone'] else DRUM_BEVEL) for name, bm, material in pieces], 'Rubble_B'))


def build(target):
    m = {
        'marble': marble_material('RuinMarble'),
        'stone': stone_material('RuinStone'),
    }
    builders = [
        (column_full, 101), (column_broken, 102), (column_stump, 103), (fallen_drum, 104), (fallen_capital, 105),
        (arch, 106), (wall_long, 107), (wall_corner, 108), (pedestal, 109), (rubble_a, 110), (rubble_b, 111),
    ]
    return [builder(m, target, random.Random(seed)) for builder, seed in builders]


def bounds(objects):
    for obj in objects:
        low = [min(vertex.co[i] for vertex in obj.data.vertices) for i in range(3)]
        high = [max(vertex.co[i] for vertex in obj.data.vertices) for i in range(3)]
        materials = ', '.join(slot.material.name for slot in obj.material_slots)
        print(f'BOUNDS {obj.name} min ({low[0]:.3f}, {low[1]:.3f}, {low[2]:.3f}) max ({high[0]:.3f}, {high[1]:.3f}, {high[2]:.3f}) verts {len(obj.data.vertices)} faces {len(obj.data.polygons)} materials {materials}')


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    bounds(objects)
    pk.unwrap_all(objects, {'RuinMarble': 0.004, 'RuinStone': 0.004})
    if args.get('--out'):
        for index, obj in enumerate(objects):
            obj.matrix_world = Matrix.Translation(((index % 4) * 6.0, (index // 4) * 6.0, 0.0))
        started = time.perf_counter()
        pk.export(objects, args['--out'], NAME, ao_distance=0.5)
        print(f'EXPORT {time.perf_counter() - started:.1f} s')
    if args.get('--preview'):
        pk.ground_preview(30.0)
        layout = {
            'Wall_A': (-2.6, 2.8), 'Arch_A': (1.0, 2.8), 'Wall_B': (4.2, 2.8),
            'Column_A': (-3.2, 0.7), 'Column_B': (-2.0, 0.7), 'Column_C': (-0.8, 0.7), 'Pedestal_A': (0.6, 0.7),
            'Drum_A': (-3.0, -1.1), 'Capital_A': (-1.6, -1.1), 'Rubble_A': (0.0, -1.1), 'Rubble_B': (1.8, -1.1),
        }
        for obj in objects:
            x, y = layout[obj.name]
            obj.matrix_world = Matrix.Translation((x, y, 0.0))
        pk.preview(objects, args['--preview'], 'ruins_all', close=False, fill=0.95)
        for group, names in (('ruins_columns', ('Column_A', 'Column_B', 'Column_C', 'Pedestal_A')), ('ruins_arch', ('Arch_A', 'Wall_A', 'Wall_B')), ('ruins_small', ('Drum_A', 'Capital_A', 'Rubble_A', 'Rubble_B'))):
            pk.preview([obj for obj in objects if obj.name in names], args['--preview'], group, fill=1.0)


if __name__ == '__main__':
    main()
