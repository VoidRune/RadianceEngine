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
import stations

NAME = 'Emberhold'
EMBER = (1.0, 0.42, 0.12)
ARCANE = (0.62, 0.38, 1.0)
SHAFT = [(1.58, 0.36), (1.56, 2.1), (1.64, 2.12), (1.64, 2.28), (1.54, 2.3), (1.5, 4.25), (1.58, 4.27), (1.58, 4.42), (1.48, 4.44), (1.46, 5.5)]


def voussoir(inner, outer, start, end, depth, center, steps=4):
    bm = bmesh.new()
    rows = []
    for i in range(steps + 1):
        angle = start + (end - start) * i / steps
        c, s = math.cos(angle), math.sin(angle)
        row = []
        for radius, y in ((inner, -depth * 0.5), (outer, -depth * 0.5), (outer, depth * 0.5), (inner, depth * 0.5)):
            row.append(bm.verts.new((center[0] + c * radius, y, center[2] + s * radius)))
        rows.append(row)
    for i in range(steps):
        a, b = rows[i], rows[i + 1]
        for j in range(4):
            k = (j + 1) % 4
            bm.faces.new((a[j], a[k], b[k], b[j]))
    bm.faces.new(rows[0])
    bm.faces.new(list(reversed(rows[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def chip(bm, rng, chips, depth):
    for _ in range(chips):
        corner = rng.choice(list(bm.verts)).co.copy()
        middle = Vector()
        for vertex in bm.verts:
            middle += vertex.co
        middle /= len(bm.verts)
        normal = (corner - middle).normalized()
        normal = (normal + Vector((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3)))).normalized()
        kit.cut(bm, corner - normal * depth * rng.uniform(0.7, 1.3), normal)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def block_frame(angle, radius, depth, center):
    tangent = Vector((-math.sin(angle), 0.0, math.cos(angle)))
    radial = Vector((math.cos(angle), 0.0, math.sin(angle)))
    origin = Vector((center[0], -depth * 0.5, center[2])) + radial * radius
    return Matrix((
        (tangent.x, 0.0, radial.x, origin.x),
        (tangent.y, 1.0, radial.y, origin.y),
        (tangent.z, 0.0, radial.z, origin.z),
        (0.0, 0.0, 0.0, 1.0),
    ))


def gate(m, target, rng):
    parts = []
    stone_rng = random.Random(11)
    parts.append(pk.part('Gate_DaisLow', pk.chipped((3.7, 1.04, 0.16), stone_rng, chips=4, depth=0.04, center=(0.0, 0.0, 0.08)), target, m['basalt'], rng=rng, bevel=(0.03, 2, 30.0)))
    parts.append(pk.part('Gate_DaisHigh', pk.chipped((3.15, 0.84, 0.14), stone_rng, chips=4, depth=0.035, center=(0.0, 0.0, 0.23)), target, m['basalt'], rng=rng, bevel=(0.03, 2, 30.0)))
    center = (0.0, 0.0, 1.94)
    inner, outer, depth = 1.22, 1.62, 0.46
    count = 15
    step = 2.0 * math.pi / count
    gap = math.radians(1.2)
    for k in range(count):
        angle = 0.5 * math.pi + k * step
        key = k == 0
        r_out = outer + (0.2 if key else rng.uniform(-0.02, 0.04))
        d = depth + (0.12 if key else rng.uniform(-0.02, 0.03))
        bm = chip(voussoir(inner - (0.04 if key else 0.0), r_out, angle - step * 0.5 + gap, angle + step * 0.5 - gap, d, center), rng, 3, 0.035)
        frame = block_frame(angle, (inner + r_out) * 0.5, d, center)
        parts.append(pk.part(f'Gate_Block{k}', bm, target, m['runes'], frame=frame, bevel=(0.025, 2, 30.0)))
    parts.append(pk.part('Gate_Seam', pk.torus((inner + outer) * 0.5, 0.13, 60, 8), target, m['portal'], matrix=Matrix.Translation(center) @ Matrix.Rotation(0.5 * math.pi, 4, 'X')))
    membrane = pk.cylinder(inner + 0.03, 0.05, 48, z=-0.025)
    parts.append(pk.part('Gate_Membrane', membrane, target, m['portal'], matrix=Matrix.Translation(center) @ Matrix.Rotation(0.5 * math.pi, 4, 'X')))
    gem = stations.crystal_shard(0.075, 0.26, sides=6, tip=0.42, rng=rng)
    gem_obj = pk.part('Gate_Gem', gem, target, m['ember_glass'], rng=rng)
    gem_obj.matrix_world = Matrix.Translation((0.0, -depth * 0.5 - 0.1, center[2] + (inner + outer + 0.2) * 0.5 - 0.06)) @ Matrix.Rotation(-0.5 * math.pi, 4, 'X') @ Matrix.Translation((0.0, 0.0, -0.08))
    parts.append(gem_obj)
    for side in (-1.0, 1.0):
        cradle = pk.chipped((0.42, 0.62, 0.62), stone_rng, chips=3, depth=0.04, center=(side * 1.42, 0.0, 0.55))
        kit.cut(cradle, Vector((side * 1.22, 0.0, 0.86)), Vector((-side * 0.7, 0.0, 0.7)).normalized())
        parts.append(pk.part(f'Gate_Cradle{int(side)}', cradle, target, m['basalt'], rng=rng, bevel=(0.03, 2, 30.0)))
    return kit.join(parts, 'CalderaGate')


def shaft_radius(low, high):
    heights = [low, high] + [z for _, z in SHAFT if low < z < high]
    radius = 0.0
    for z in heights:
        for (r0, z0), (r1, z1) in zip(SHAFT, SHAFT[1:]):
            if z0 <= z <= z1:
                radius = max(radius, r0 + (r1 - r0) * (z - z0) / (z1 - z0))
    return radius


def arch_slab(width, height, depth, segments=12):
    bm = bmesh.new()
    half = width * 0.5
    spring = height - half
    outline = [(-half, 0.0), (half, 0.0)] + [(math.cos(math.pi * i / segments) * half, spring + math.sin(math.pi * i / segments) * half) for i in range(segments + 1)]
    front = [bm.verts.new((x, -depth * 0.5, z)) for x, z in outline]
    back = [bm.verts.new((x, depth * 0.5, z)) for x, z in outline]
    count = len(outline)
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    for i in range(count):
        j = (i + 1) % count
        bm.faces.new((front[i], front[j], back[j], back[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=[face for face in bm.faces if len(face.verts) > 4])
    return bm


def window(name, angle, z, m, target, rng, width=0.56, height=0.98):
    outward = Vector((math.sin(angle), -math.cos(angle), 0.0))
    place = Matrix.Translation(outward * shaft_radius(z - 0.06, z + height) + Vector((0.0, 0.0, z))) @ Matrix.Rotation(angle, 4, 'Z')
    opening, top = width * 0.62, height * 0.8
    parts = [pk.part(name + '_Frame', stations.arch_wall(width, height, 0.18, opening, top, segments=8), target, m['stone'], matrix=place @ Matrix.Translation((0.0, -0.01, 0.0)), rng=rng, bevel=(0.025, 2, 30.0))]
    parts.append(pk.part(name + '_Sill', pk.box((width * 0.82, 0.2, 0.08), (0.0, -0.06, -0.02)), target, m['stone'], matrix=place, rng=rng, bevel=(0.02, 2, 30.0)))
    pane = arch_slab(opening + 0.03, top + 0.015, 0.02, segments=10)
    parts.append(pk.part(name + '_Glass', pane, target, m['arcane_glass'], matrix=place @ Matrix.Translation((0.0, -0.022, 0.0)), rng=rng))
    bar = top - 0.01
    parts.append(pk.part(name + '_Bar', pk.box((0.022, 0.02, bar), (0.0, -0.044, 0.02 + bar * 0.5)), target, m['iron'], matrix=place, rng=rng))
    parts.append(pk.part(name + '_Cross', pk.box((opening + 0.02, 0.02, 0.022), (0.0, -0.048, height * 0.46)), target, m['iron'], matrix=place, rng=rng))
    return parts


def tower(m, target, rng):
    octagon = hubprops.octagon
    parts = [pk.part('Tower_Plinth', pk.lathe([(octagon(1.95), 0.0), (octagon(1.95), 0.22), (octagon(1.84), 0.3), (octagon(1.74), 0.38)], 8, math.pi / 8.0), target, m['stone'], rng=rng, bevel=(0.03, 2, 30.0))]
    parts.append(pk.part('Tower_Shaft', pk.lathe(SHAFT, 32), target, m['masonry'], bevel=(0.03, 2, 40.0)))
    for k in range(4):
        angle = math.pi * 0.25 + k * math.pi * 0.5
        out = Vector((math.sin(angle), -math.cos(angle), 0.0))
        buttress = pk.chipped((0.42, 0.9, 1.9), rng, chips=2, depth=0.04, center=(0.0, 0.0, 0.95))
        kit.cut(buttress, Vector((0.0, -0.45, 0.6)), Vector((0.0, -0.75, 0.66)).normalized())
        parts.append(pk.part(f'Tower_Buttress{k}', buttress, target, m['stone'], matrix=Matrix.Translation(out * 1.62) @ Matrix.Rotation(angle, 4, 'Z'), rng=rng, bevel=(0.03, 2, 30.0)))
    door = Matrix.Translation((0.0, -1.5, 0.38))
    parts.append(pk.part('Tower_DoorArch', stations.arch_wall(1.32, 2.0, 0.34, 0.86, 1.72, segments=10), target, m['stone'], matrix=door @ Matrix.Translation((0.0, -0.06, 0.0)), rng=rng, bevel=(0.03, 2, 30.0)))
    parts.append(pk.part('Tower_Door', arch_slab(0.84, 1.71, 0.08, segments=16), target, m['wood'], matrix=door @ Matrix.Translation((0.0, -0.13, 0.0)), rng=rng, smooth=30.0))
    for z in (0.35, 1.05):
        parts.append(pk.part(f'Tower_Strap{int(z * 10)}', pk.box((0.8, 0.03, 0.07), (0.0, -0.185, z)), target, m['iron'], matrix=door, rng=rng, bevel=(0.008, 1, 30.0)))
    parts.append(pk.part('Tower_Ring', pk.torus(0.06, 0.012, 16, 6), target, m['iron'], matrix=door @ Matrix.Translation((0.22, -0.22, 0.8)) @ Matrix.Rotation(0.5 * math.pi, 4, 'X'), rng=rng, smooth=40.0))
    parts.append(pk.part('Tower_Step', pk.chipped((1.3, 0.42, 0.19), rng, chips=2, depth=0.03, center=(0.0, -2.02, 0.095)), target, m['stone'], rng=rng, bevel=(0.03, 2, 30.0)))
    for index, (angle, z, width, height) in enumerate(((-0.62, 2.55, 0.56, 0.98), (0.72, 3.12, 0.56, 0.98), (-1.25, 4.52, 0.48, 0.74), (0.05, 4.54, 0.48, 0.74))):
        parts += window(f'Tower_Window{index}', angle, z, m, target, rng, width, height)
    lantern = Matrix.Translation((0.62, -1.62, 2.05))
    parts.append(pk.part('Tower_LanternArm', pk.rod((0.62, -1.5, 2.2), (0.62, -1.68, 2.2), 0.014, 6), target, m['iron'], rng=rng, smooth=40.0))
    parts.append(pk.part('Tower_LanternCap', pk.cone_bm((0.0, 0.0, 0.12), (0.0, 0.0, 0.24), 0.09, sides=6), target, m['iron'], matrix=lantern, rng=rng, smooth=30.0))
    parts.append(pk.part('Tower_LanternBase', pk.cylinder(0.075, 0.03, 6, z=-0.12), target, m['iron'], matrix=lantern, rng=rng, smooth=30.0))
    parts.append(pk.part('Tower_LanternGlass', pk.cylinder(0.062, 0.24, 6, z=-0.1), target, m['lantern_glass'], matrix=lantern, rng=rng, smooth=30.0))
    for k in range(18):
        angle = k * 2.0 * math.pi / 18.0
        out = Vector((math.sin(angle), -math.cos(angle), 0.0))
        corbel = pk.chipped((0.16, 0.28, 0.24), rng, chips=1, depth=0.02, center=(0.0, 0.0, 0.0))
        kit.cut(corbel, Vector((0.0, -0.06, -0.06)), Vector((0.0, -0.6, -0.8)).normalized())
        parts.append(pk.part(f'Tower_Corbel{k}', corbel, target, m['stone'], matrix=Matrix.Translation(out * 1.52 + Vector((0.0, 0.0, 5.44))) @ Matrix.Rotation(angle, 4, 'Z'), rng=rng, bevel=(0.02, 1, 30.0)))
    parts.append(pk.part('Tower_Parapet', pk.lathe([(1.44, 5.52), (1.74, 5.6), (1.78, 5.64), (1.78, 5.86), (1.72, 5.9), (1.5, 5.92)], 32), target, m['stone'], rng=rng, bevel=(0.025, 2, 30.0)))
    roof = pk.lathe([(1.98, 5.86), (1.95, 5.96), (1.64, 6.4), (1.16, 7.15), (0.62, 7.95), (0.26, 8.55), (0.0, 8.82)], 32)
    parts.append(pk.part('Tower_Roof', roof, target, m['slate'], smooth=50.0))
    parts.append(pk.part('Tower_Finial', pk.lathe([(0.1, 8.72), (0.06, 8.84), (0.035, 9.0), (0.06, 9.06), (0.03, 9.12), (0.012, 9.32)], 10), target, m['brass'], rng=rng, smooth=40.0))
    crystal = pk.part('Tower_Crystal', stations.crystal_shard(0.11, 0.42, sides=6, tip=0.4, rng=rng), target, m['crystal'], rng=rng)
    crystal.matrix_world = Matrix.Translation((0.0, 0.0, 9.42))
    parts.append(crystal)
    under = pk.part('Tower_CrystalTip', stations.crystal_shard(0.11, 0.16, sides=6, tip=0.99, rng=rng), target, m['crystal'], rng=rng)
    under.matrix_world = Matrix.Translation((0.0, 0.0, 9.42)) @ Matrix.Rotation(math.pi, 4, 'X')
    parts.append(under)
    return kit.join(parts, 'MageTower')


def chain_link(m, target, rng):
    points = [Vector((0.034 + 0.04 * math.cos(-0.5 * math.pi + k * math.pi / 4.0), 0.04 * math.sin(-0.5 * math.pi + k * math.pi / 4.0), 0.0)) for k in range(5)]
    points += [Vector((-0.034 + 0.04 * math.cos(0.5 * math.pi + k * math.pi / 4.0), 0.04 * math.sin(0.5 * math.pi + k * math.pi / 4.0), 0.0)) for k in range(5)]
    return pk.tube(points, 0.019, target, m['iron'], 'ChainLink', resolution=3, sides=6, closed=True, rng=rng)


def pier_post(m, target, rng):
    octagon = hubprops.octagon
    parts = [pk.part('Post_Shaft', pk.lathe([(octagon(0.17), 0.0), (octagon(0.17), 0.07), (octagon(0.145), 0.12), (octagon(0.13), 0.82), (octagon(0.165), 0.87), (octagon(0.175), 0.95), (octagon(0.13), 1.0), (octagon(0.05), 1.07), (0.0, 1.09)], 8, math.pi / 8.0), target, m['stone'], rng=rng, bevel=(0.02, 2, 30.0))]
    parts.append(pk.part('Post_Band', pk.torus(0.137, 0.017, 16, 6), target, m['iron'], matrix=Matrix.Translation((0.0, 0.0, 0.76)), rng=rng, smooth=40.0))
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'Post_Eye{int(side)}', pk.torus(0.035, 0.011, 12, 6), target, m['iron'], matrix=Matrix.Translation((0.0, side * 0.165, 0.76)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Y'), rng=rng, smooth=40.0))
    return kit.join(parts, 'PierPost')


def chain_anchor(m, target, rng):
    parts = [pk.part('Anchor_Block', pk.chipped((0.58, 0.5, 0.34), random.Random(3), chips=3, depth=0.04, center=(0.0, 0.0, 0.17)), target, m['basalt'], rng=rng, bevel=(0.03, 2, 30.0))]
    parts.append(pk.part('Anchor_Plate', pk.box((0.3, 0.04, 0.24), (0.0, -0.265, 0.19)), target, m['iron'], rng=rng, bevel=(0.008, 1, 30.0)))
    for x in (-0.1, 0.1):
        for z in (0.11, 0.27):
            parts.append(pk.part(f'Anchor_Rivet{int(x * 100)}_{int(z * 100)}', pk.ellipsoid_bm((x, -0.287, z), (0.018, 0.012, 0.018), segments=8, rings=4), target, m['iron'], rng=rng, smooth=50.0))
    parts.append(pk.part('Anchor_Ring', pk.torus(0.075, 0.022, 16, 6), target, m['iron'], matrix=Matrix.Translation((0.0, -0.34, 0.19)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Y'), rng=rng, smooth=40.0))
    return kit.join(parts, 'ChainAnchor')


def step_block(key, m, target, rng, chips=3):
    bm = pk.chipped((0.5, 1.0, 0.5), rng, chips=chips, depth=0.035, center=(0.0, 0.0, 0.25))
    seed = Vector((rng.uniform(-40.0, 40.0), rng.uniform(-40.0, 40.0), 0.0))
    for vertex in bm.verts:
        if vertex.co.z > 0.45:
            vertex.co.z += 0.008 * math.sin(vertex.co.x * 7.0 + seed.x) * math.cos(vertex.co.y * 5.0 + seed.y)
    return pk.part(f'StepBlock_{key}', bm, target, m['stone'], rng=rng, bevel=(0.03, 2, 30.0))


def basalt_material(name, glyphs=False, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    x, y, z = s.separate(p)
    island = s.geometry('Random Per Island')
    q = s.offset(p, s.combine(s.mul(island, 37.0), s.mul(island, 53.0), s.mul(island, 71.0)))
    large = s.noise(q, 1.6, detail=3.0)
    mid = s.noise(q, 6.0, detail=5.0, roughness=0.55)
    pits = s.sub(1.0, s.smooth(0.0, 0.1, s.voronoi(q, 55.0, 'F1', 1.0, 'Distance')))
    tone = s.clamp01(s.add(s.mul(large, 0.5), s.add(s.mul(mid, 0.35), s.mul(island, 0.2))))
    base = s.mix(tone, (0.03, 0.027, 0.028), (0.1, 0.088, 0.082))
    base = s.mix(s.mul(pits, 0.45), base, (0.012, 0.011, 0.011))
    edge = pk.wear(s, 0.025)
    base = s.mix(s.mul(edge, 0.45), base, (0.2, 0.18, 0.165))
    dirt = pk.cavity(s, 0.08)
    base = s.mix(s.mul(dirt, 0.6), base, (0.01, 0.008, 0.008))
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.018, s.voronoi(s.stretch(q, (1.0, 1.0, 1.0)), 7.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.55, 0.7, s.noise(q, 2.0, detail=2.0)))
    glow = s.mul(cracks, 0.6)
    if glyphs:
        radial = s.vector('LENGTH', s.combine(x, z, 0.0))
        front = s.sub(1.0, s.smooth(0.004, 0.02, s.math('ABSOLUTE', y)))
        ring = s.sub(1.0, s.smooth(0.007, 0.013, s.math('ABSOLUTE', s.sub(radial, 0.085))))
        span = s.sub(1.0, s.smooth(0.1, 0.11, s.math('ABSOLUTE', z)))
        stem = s.mul(s.sub(1.0, s.smooth(0.006, 0.012, s.math('ABSOLUTE', x))), span)
        slash = s.mul(s.sub(1.0, s.smooth(0.006, 0.012, s.math('ABSOLUTE', s.sub(x, s.mul(z, 0.6))))), s.sub(1.0, s.smooth(0.07, 0.08, s.math('ABSOLUTE', z))))
        chevron = s.mul(s.sub(1.0, s.smooth(0.006, 0.012, s.math('ABSOLUTE', s.sub(s.math('ABSOLUTE', x), s.add(s.mul(z, 0.7), 0.02))))), s.sub(1.0, s.smooth(0.06, 0.07, s.math('ABSOLUTE', x))))
        mark = s.math('MAXIMUM', s.mul(ring, s.smooth(0.25, 0.3, island)), stem)
        mark = s.math('MAXIMUM', mark, s.mul(slash, s.smooth(0.5, 0.55, island)))
        mark = s.math('MAXIMUM', mark, s.mul(chevron, s.sub(1.0, s.smooth(0.45, 0.5, island))))
        rune = s.mul(s.mul(mark, front), s.smooth(0.12, 0.17, island))
        base = s.mix(rune, base, (0.95, 0.42, 0.12))
        glow = s.math('MAXIMUM', glow, rune)
    rough = s.remap(mid, 0.3, 0.7, 0.48, 0.72)
    height = s.sub(s.add(s.mul(mid, 0.7), s.mul(large, 0.3)), s.mul(pits, 0.15))
    return s.finish(base, rough, height=height, bump=0.3, bump_distance=0.01, emission=glow, emission_color=EMBER, emission_strength=3.0, bake_size=bake_size)


def portal_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    x, y, z = s.separate(p)
    radius = s.vector('LENGTH', s.combine(x, y, 0.0))
    angle = s.add(s.math('ARCTAN2', y, x), s.mul(radius, 3.6))
    swirl = s.combine(s.mul(s.math('COSINE', angle), radius), s.mul(s.math('SINE', angle), radius), z)
    filaments = s.smooth(0.45, 0.78, s.noise(swirl, 2.6, detail=5.0, roughness=0.6, distortion=0.8))
    crust = s.mul(s.smooth(0.0, 0.06, s.voronoi(swirl, 3.5, 'DISTANCE_TO_EDGE', 1.0, 'Distance')), s.smooth(0.5, 0.7, s.noise(swirl, 1.4, detail=3.0)))
    core = s.sub(1.0, s.smooth(0.0, 1.25, radius))
    rim = s.smooth(1.0, 1.24, radius)
    glow = s.clamp01(s.add(s.mul(core, 0.55), s.add(s.mul(filaments, 0.6), s.mul(rim, 0.5))))
    glow = s.mul(glow, s.sub(1.0, s.mul(crust, 0.65)))
    base = s.mix(glow, (0.04, 0.012, 0.008), (1.0, 0.55, 0.18))
    return s.finish(base, 0.35, height=filaments, bump=0.1, bump_distance=0.004, emission=s.add(s.mul(glow, 0.9), 0.1), emission_color=EMBER, emission_strength=4.0, bake_size=bake_size)


def slate_material(name, tone=(0.11, 0.115, 0.14), bake_size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    x, y, z = s.separate(p)
    across = s.mul(s.math('ARCTAN2', s.mul(x, -1.0), s.mul(y, -1.0)), s.vector('LENGTH', s.combine(x, y, 0.0)))
    node = s.node('ShaderNodeTexBrick', offset=0.5, offset_frequency=2, squash=1.0, squash_frequency=2)
    s.feed(node.inputs['Vector'], s.combine(across, s.mul(z, 1.25), 0.0))
    s.feed(node.inputs['Color1'], (1.0, 1.0, 1.0))
    s.feed(node.inputs['Color2'], (0.0, 0.0, 0.0))
    s.feed(node.inputs['Mortar'], (0.0, 0.0, 0.0))
    s.feed(node.inputs['Scale'], 1.0)
    s.feed(node.inputs['Mortar Size'], 0.012)
    s.feed(node.inputs['Mortar Smooth'], 0.3)
    s.feed(node.inputs['Bias'], 0.0)
    s.feed(node.inputs['Brick Width'], 0.24)
    s.feed(node.inputs['Row Height'], 0.15)
    joints = node.outputs['Fac']
    tile = s.separate(node.outputs['Color'])[0]
    lap = s.math('FRACT', s.mul(s.mul(z, 1.25), 1.0 / 0.15))
    tone_mix = s.clamp01(s.add(s.mul(tile, 0.55), s.mul(s.noise(p, 5.0, detail=3.0), 0.45)))
    base = s.mix(tone_mix, (tone[0] * 0.55, tone[1] * 0.55, tone[2] * 0.55), (tone[0] * 1.5, tone[1] * 1.45, tone[2] * 1.35))
    shadow = s.sub(1.0, s.smooth(0.0, 0.35, lap))
    base = s.mix(s.mul(shadow, 0.55), base, (0.012, 0.012, 0.016))
    base = s.mix(s.smooth(0.25, 0.6, joints), base, (0.01, 0.01, 0.012))
    lichen = s.mul(s.smooth(0.62, 0.72, s.noise(p, 3.0, detail=4.0)), 0.5)
    base = s.mix(lichen, base, (0.16, 0.17, 0.11))
    rough = s.remap(tile, 0.0, 1.0, 0.5, 0.72)
    height = s.sub(s.mul(lap, 0.8), s.mul(s.sub(1.0, s.smooth(0.25, 0.6, joints)), -0.4))
    return s.finish(base, rough, height=height, bump=0.5, bump_distance=0.012, bake_size=bake_size)


def build(target):
    rng = random.Random(29)
    m = {
        'basalt': basalt_material('Basalt', bake_size=1024),
        'runes': basalt_material('RuneBasalt', glyphs=True, bake_size=2048),
        'portal': portal_material('Portal', bake_size=1024),
        'ember_glass': pk.glow_material('EmberGlass', color=(1.0, 0.45, 0.12), strength=3.5, base=(0.9, 0.4, 0.12), bake_size=128),
        'masonry': pk.masonry_material('TowerMasonry', light=(0.27, 0.25, 0.225), dark=(0.12, 0.11, 0.1), soot=0.5, bake_size=2048, radius=1.52),
        'stone': pk.stone_material('TowerStone', light=(0.3, 0.285, 0.26), dark=(0.13, 0.122, 0.11), moss=0.3, bake_size=1024),
        'slate': slate_material('Slate', bake_size=2048),
        'wood': pk.wood_material('DoorWood', light=(0.2, 0.13, 0.075), dark=(0.08, 0.05, 0.03), weathered=0.35, bake_size=512),
        'iron': pk.iron_material('Iron', bake_size=512),
        'brass': pk.brass_material('Brass', bake_size=256),
        'arcane_glass': pk.glow_material('ArcaneGlass', color=ARCANE, strength=2.6, base=(0.4, 0.26, 0.7), bake_size=256),
        'lantern_glass': pk.glow_material('LanternGlass', color=(1.0, 0.66, 0.3), strength=3.0, base=(0.95, 0.68, 0.36), bake_size=128),
        'crystal': pk.crystal_material('SpireCrystal', core=ARCANE, shell=(0.08, 0.04, 0.16), strength=3.4, bake_size=256),
    }
    return [gate(m, target, rng), tower(m, target, rng), chain_link(m, target, rng), pier_post(m, target, rng), chain_anchor(m, target, rng)] + [step_block(key, m, target, rng, chips) for key, chips in zip('ABCDEF', (3, 3, 3, 2, 4, 3))]


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    pk.unwrap_all(objects, {'EmberGlass': 0.02, 'ArcaneGlass': 0.02, 'LanternGlass': 0.02, 'Brass': 0.01, 'SpireCrystal': 0.01, 'Iron': 0.008})
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.6)
    if args.get('--preview'):
        pk.ground_preview(16.0)
        objects[0].matrix_world = Matrix.Translation((-2.6, 0.0, 0.0))
        objects[1].matrix_world = Matrix.Translation((2.6, 1.0, 0.0))
        for index, obj in enumerate(objects[2:]):
            obj.matrix_world = Matrix.Translation((-1.0 + index * 0.9, -2.6, 0.0))
        pk.preview(objects, args['--preview'], 'emberhold', fill=0.95)


if __name__ == '__main__':
    main()
