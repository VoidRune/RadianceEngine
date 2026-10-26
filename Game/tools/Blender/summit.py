import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector, noise

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import stations

NAME = 'Summit'

RUNES = [
    [((0.0, 0.0), (0.0, 1.0)), ((0.0, 0.62), (0.42, 0.88)), ((0.0, 0.34), (0.42, 0.6))],
    [((-0.3, 0.0), (-0.3, 1.0)), ((-0.3, 1.0), (0.3, 0.68)), ((0.3, 0.68), (0.3, 0.0))],
    [((0.0, 0.0), (0.0, 1.0)), ((0.0, 0.76), (0.36, 0.5)), ((0.36, 0.5), (0.0, 0.24))],
    [((0.0, 0.0), (0.0, 1.0)), ((0.0, 0.56), (-0.4, 0.96)), ((0.0, 0.56), (0.4, 0.96))],
    [((0.0, 0.0), (0.0, 1.0)), ((0.0, 1.0), (-0.4, 0.66)), ((0.0, 1.0), (0.4, 0.66))],
    [((-0.3, 1.0), (0.22, 0.66)), ((0.22, 0.66), (-0.22, 0.34)), ((-0.22, 0.34), (0.3, 0.0))],
    [((-0.32, 0.0), (-0.32, 1.0)), ((0.32, 0.0), (0.32, 1.0)), ((-0.32, 1.0), (0.32, 0.5)), ((0.32, 1.0), (-0.32, 0.5))],
    [((-0.34, 0.12), (0.34, 0.88)), ((0.34, 0.12), (-0.34, 0.88)), ((0.0, 0.0), (0.0, 1.0))],
    [((0.0, 0.0), (0.0, 1.0)), ((-0.34, 0.5), (0.34, 0.5)), ((-0.24, 0.2), (0.24, 0.8))],
    [((-0.28, 0.0), (0.0, 0.5)), ((0.0, 0.5), (-0.28, 1.0)), ((0.28, 0.0), (0.0, 0.5)), ((0.0, 0.5), (0.28, 1.0))],
]


def granite_material(name, light=(0.25, 0.25, 0.245), dark=(0.085, 0.09, 0.098), bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(s.coords('Object'))
    large = s.noise(p, 1.2, detail=3.0)
    mid = s.noise(p, 4.2, detail=5.0, roughness=0.6)
    fine = s.noise(p, 40.0, detail=2.0)
    specks = s.sub(1.0, s.smooth(0.0, 0.16, s.voronoi(p, 70.0, 'F1', 1.0, 'Distance')))
    flecks = s.sub(1.0, s.smooth(0.0, 0.1, s.voronoi(p, 55.0, 'F1', 1.0, 'Distance', w=None)))
    island = s.geometry('Random Per Island')
    tone = s.madd(s.smooth(0.3, 0.7, large), 0.45, s.add(s.mul(mid, 0.3), s.mul(island, 0.25)))
    base = s.mix(tone, dark, light)
    base = s.mix(s.mul(specks, 0.45), base, (0.05, 0.05, 0.055))
    base = s.mix(s.mul(flecks, 0.25), base, (0.5, 0.5, 0.48))
    dirt = pk.cavity(s, 0.1)
    base = s.mix(s.mul(dirt, 0.65), base, (0.04, 0.04, 0.042))
    streaks = s.smooth(0.52, 0.74, s.noise(s.stretch(p, (12.0, 12.0, 0.7)), 1.0, detail=3.0, roughness=0.6))
    base = s.mix(s.mul(streaks, 0.35), base, (0.06, 0.062, 0.068))
    grime = s.mul(s.sub(1.0, s.smooth(0.03, 0.45, z)), s.smooth(0.3, 0.6, s.noise(p, 3.5, detail=4.0)))
    base = s.mix(s.mul(grime, 0.55), base, (0.05, 0.048, 0.04))
    up = s.smooth(0.45, 0.85, s.separate(s.geometry('Normal'))[2])
    lichen = s.smooth(0.62, 0.72, s.noise(p, 3.0, detail=5.0, roughness=0.65))
    base = s.mix(s.mul(lichen, s.add(0.35, s.mul(up, 0.4))), base, (0.27, 0.28, 0.16))
    orange = s.mul(s.smooth(0.7, 0.78, s.noise(p, 5.5, detail=4.0, roughness=0.6)), 0.6)
    base = s.mix(orange, base, (0.36, 0.2, 0.08))
    edge = pk.wear(s, 0.03)
    base = s.mix(s.mul(edge, 0.3), base, (light[0] * 1.05, light[1] * 1.05, light[2] * 1.05))
    rough = s.remap(fine, 0.3, 0.7, 0.76, 0.92)
    pits = s.smooth(0.02, 0.18, s.voronoi(p, 26.0, 'F1', 1.0, 'Distance'))
    height = s.add(s.mul(mid, 0.8), s.add(s.mul(pits, 0.08), s.mul(fine, 0.04)))
    return s.finish(base, rough, height=height, bump=0.25, bump_distance=0.012, bake_size=bake_size)


def bronze_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    noise_value = s.noise(p, 11.0, detail=3.0)
    base = s.mix(noise_value, (0.3, 0.19, 0.1), (0.44, 0.29, 0.15))
    pits = pk.cavity(s, 0.04)
    up = s.smooth(0.3, 0.85, s.separate(s.geometry('Normal'))[2])
    patina = s.clamp01(s.add(s.mul(pits, 0.8), s.mul(s.mul(up, s.smooth(0.45, 0.7, s.noise(p, 6.0, detail=4.0))), 0.9)))
    base = s.mix(patina, base, (0.17, 0.36, 0.3))
    edge = pk.wear(s, 0.012)
    base = s.mix(s.mul(edge, 0.6), base, (0.62, 0.43, 0.22))
    rough = s.lerp(patina, s.remap(noise_value, 0.3, 0.7, 0.3, 0.45), 0.85)
    metal = s.lerp(patina, 1.0, 0.1)
    return s.finish(base, rough, height=noise_value, bump=0.06, bump_distance=0.002, metallic=metal, bake_size=bake_size)


def banner_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    island = s.geometry('Random Per Island')
    weave = s.noise(s.stretch(p, (60.0, 60.0, 60.0)), 1.0, detail=2.0)
    fade = s.smooth(0.3, 0.8, s.noise(p, 2.0, detail=3.0))
    indigo = s.mix(fade, (0.06, 0.09, 0.24), (0.2, 0.24, 0.38))
    crimson = s.mix(fade, (0.28, 0.05, 0.05), (0.42, 0.2, 0.16))
    ochre = s.mix(fade, (0.42, 0.3, 0.1), (0.55, 0.47, 0.3))
    color = s.mix(s.smooth(0.33, 0.34, island), indigo, crimson)
    color = s.mix(s.smooth(0.66, 0.67, island), color, ochre)
    base = s.mix(s.mul(weave, 0.25), color, (0.08, 0.07, 0.06))
    dirt = pk.cavity(s, 0.05)
    base = s.mix(s.mul(dirt, 0.5), base, (0.05, 0.045, 0.04))
    return s.finish(base, 0.92, height=weave, bump=0.1, bump_distance=0.002, bake_size=bake_size)


def weathered(co, rng_offset, amount, scale=2.2):
    shifted = co * scale + rng_offset
    return amount * (noise.noise(shifted) + 0.35 * noise.noise(shifted * 2.7))


def monolith(width, depth, height, rng, taper=0.8, lean=0.05, slant=0.12, cuts=10, rough=0.035, panel=True, chips=4):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts, use_grid_fill=True)
    offset = Vector((rng.uniform(-50.0, 50.0), rng.uniform(-50.0, 50.0), rng.uniform(-50.0, 50.0)))
    tilt = rng.uniform(-1.0, 1.0)
    bend = (rng.uniform(-1.0, 1.0) * 0.05 * width, rng.uniform(-1.0, 1.0) * 0.04 * depth, rng.uniform(0.0, 6.28))
    front = rng.uniform(0.85, 1.0)
    for vertex in bm.verts:
        u, v, w = vertex.co.x, vertex.co.y, vertex.co.z + 0.5
        shrink = 1.0 + (taper - 1.0) * w
        bulge = 1.0 + 0.06 * math.sin(math.pi * w)
        x = u * width * shrink * bulge
        y = v * depth * shrink * bulge * (front if v < 0.0 else 1.0)
        z = w * height
        if w > 0.99:
            z -= height * slant * (0.5 + tilt * u)
        outward = Vector((x, y * 1.4, 0.0))
        outward = outward.normalized() if outward.length > 1e-5 else Vector((0.0, 0.0, 1.0))
        calm = 1.0
        if panel and v < -0.49 and abs(u) < 0.32 and 0.3 < w < 0.9:
            calm = 0.3
        point = Vector((x, y, z))
        bump = weathered(point, offset, rough * 2.0 * calm, 0.9) + weathered(point, offset + Vector((3.0, 9.0, 5.0)), rough * calm, 2.4) + weathered(point, offset + Vector((11.0, 2.0, 7.0)), rough * 0.35, 7.0)
        sway = Vector((bend[0] * math.sin(w * 2.6 + bend[2]), bend[1] * math.sin(w * 2.1 + bend[2] * 1.7), 0.0))
        vertex.co = Vector((x + lean * z, y, z)) + sway + outward * bump
        if w > 0.99:
            vertex.co.z += weathered(point, offset + Vector((7.0, 3.0, 1.0)), rough * 3.0, 3.0)
    for _ in range(chips):
        side = Vector((rng.choice((-1.0, 1.0)) * width * 0.5, rng.choice((-1.0, 1.0)) * depth * 0.5, rng.uniform(0.55, 1.0) * height))
        normal = Vector((math.copysign(rng.uniform(0.5, 1.0), side.x), math.copysign(rng.uniform(0.3, 0.8), side.y), rng.uniform(0.1, 0.9))).normalized()
        kit.cut(bm, side * Vector((taper, taper, 1.0)) - normal * rng.uniform(0.04, 0.1), normal)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def face_y(depth, taper, height, z):
    return -0.5 * depth * (1.0 + (taper - 1.0) * z / height)


def rune_strokes(glyph, origin, size, target, material, name, rng, plane='XZ', facing=-1.0, depth=0.0, lean=0.0):
    parts = []
    for index, ((ax, az), (bx, bz)) in enumerate(RUNES[glyph % len(RUNES)]):
        a = Vector((origin[0] + ax * size, origin[1], origin[2] + az * size))
        b = Vector((origin[0] + bx * size, origin[1], origin[2] + bz * size))
        a.x += lean * a.z
        b.x += lean * b.z
        bm = pk.rod(a, b, size * 0.07, steps=4)
        obj = pk.part(f'{name}_Rune{index}', bm, target, material, rng=rng)
        parts.append(obj)
    return parts


def standing_stone(key, size, m, target, rng):
    width, depth, height = size
    taper, lean = 0.8, rng.uniform(-0.04, 0.04)
    body = pk.part(f'StandingStone{key}_Body', monolith(width, depth, height, rng, taper=taper, lean=lean), target, m['granite'], rng=rng, smooth=35.0)
    parts = [body]
    glyphs = rng.sample(range(len(RUNES)), 3)
    for row, glyph in enumerate(glyphs):
        z = height * (0.42 + 0.17 * row)
        y = face_y(depth, taper, height, z) - 0.006
        parts += rune_strokes(glyph, (0.0, y, z), 0.24, target, m['rune'], f'StandingStone{key}_{row}', rng, lean=lean)
    return kit.join(parts, f'StandingStone_{key}')


def glyph_stone(key, size, m, target, rng):
    width, depth, height = size
    bm = monolith(width, depth, height, rng, taper=0.86, lean=0.0, slant=0.0, cuts=8, rough=0.03)
    for vertex in bm.verts:
        if vertex.co.z > height * 0.7:
            arch = 1.0 - (vertex.co.x / (width * 0.5)) ** 2
            vertex.co.z = height * 0.7 + (vertex.co.z - height * 0.7) * max(arch, 0.15)
    parts = [pk.part(f'GlyphStone{key}_Body', bm, target, m['granite'], rng=rng, smooth=35.0)]
    glyphs = rng.sample(range(len(RUNES)), 2)
    for row, glyph in enumerate(glyphs):
        z = height * (0.2 + 0.32 * row)
        parts += rune_strokes(glyph, (0.0, face_y(depth, 0.86, height, z) - 0.006, z), 0.22, target, m['rune'], f'GlyphStone{key}_{row}', rng)
    return kit.join(parts, f'GlyphStone_{key}')


def storm_spire(m, target, rng):
    parts = [pk.part('Spire_Plinth', pk.chipped((1.5, 1.5, 0.32), rng, chips=4, depth=0.07, center=(0.0, 0.0, 0.16)), target, m['granite'], rng=rng, bevel=(0.03, 2, 30.0))]
    parts.append(pk.part('Spire_Step', pk.chipped((1.08, 1.08, 0.28), rng, chips=3, depth=0.05, center=(0.0, 0.0, 0.46)), target, m['granite'], rng=rng, bevel=(0.025, 2, 30.0)))
    shaft = monolith(0.62, 0.62, 3.2, rng, taper=0.64, lean=0.0, slant=0.0, cuts=12, rough=0.02, panel=False, chips=0)
    bmesh.ops.translate(shaft, verts=shaft.verts, vec=(0.0, 0.0, 0.6))
    parts.append(pk.part('Spire_Shaft', shaft, target, m['granite'], rng=rng, smooth=30.0))
    top = 3.8
    half = 0.62 * 0.64 * 0.5
    cap = bmesh.new()
    corners = [cap.verts.new((sx * half, sy * half, top - 0.02)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    apex = cap.verts.new((0.0, 0.0, top + 0.34))
    cap.faces.new(list(reversed(corners)))
    for i in range(4):
        cap.faces.new((corners[i], corners[(i + 1) % 4], apex))
    bmesh.ops.recalc_face_normals(cap, faces=cap.faces[:])
    parts.append(pk.part('Spire_Cap', cap, target, m['bronze'], rng=rng))
    parts.append(pk.part('Spire_Band', pk.torus(half * 1.48, 0.035, steps=4, sides=6), target, m['bronze'], matrix=Matrix.Translation((0.0, 0.0, top - 0.12)) @ Matrix.Rotation(math.pi / 4.0, 4, 'Z'), rng=rng))
    for i in range(4):
        angle = math.pi / 4.0 + i * math.pi / 2.0
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        base = out * half * 1.05 + Vector((0.0, 0.0, top - 0.1))
        bend = out * 0.32 + Vector((0.0, 0.0, top + 0.55))
        tip = out * 0.12 + Vector((0.0, 0.0, top + 1.25))
        parts.append(pk.part(f'Spire_Prong{i}a', pk.rod(base, bend, 0.04, steps=6, end_radius=0.032), target, m['bronze'], rng=rng, smooth=40.0))
        parts.append(pk.part(f'Spire_Prong{i}b', pk.rod(bend, tip, 0.032, steps=6, end_radius=0.012), target, m['bronze'], rng=rng, smooth=40.0))
    crystal = stations.crystal_shard(0.13, 0.85, sides=6, tip=0.3, rng=rng)
    parts.append(pk.part('Spire_Crystal', crystal, target, m['crystal'], matrix=Matrix.Translation((0.0, 0.0, top + 0.42)), rng=rng, bevel=(0.01, 1, 30.0)))
    for face in range(4):
        angle = face * math.pi / 2.0
        rotation = Matrix.Rotation(angle, 4, 'Z')
        for row in range(2):
            z = 1.4 + row * 0.95 + face * 0.22
            half_at = 0.5 * 0.62 * (1.0 + (0.64 - 1.0) * (z - 0.6) / 3.2)
            strokes = rune_strokes(rng.randrange(len(RUNES)), (0.0, -half_at - 0.006, z), 0.26, target, m['rune'], f'Spire_{face}_{row}', rng)
            for stroke in strokes:
                stroke.matrix_world = rotation @ stroke.matrix_world
            parts += strokes
    return kit.join(parts, 'StormSpire_A')


def stone_gate(m, target, rng):
    parts = []
    for side, key in ((-1.0, 'L'), (1.0, 'R')):
        pillar = monolith(0.6, 0.58, 2.45, rng, taper=0.9, lean=-side * 0.015, slant=0.0, cuts=10, rough=0.03, panel=False, chips=0)
        bmesh.ops.translate(pillar, verts=pillar.verts, vec=(side * 1.15, 0.0, 0.0))
        parts.append(pk.part(f'Gate_Pillar{key}', pillar, target, m['granite'], rng=rng, smooth=35.0))
    lintel = monolith(0.48, 0.68, 2.95, rng, taper=0.95, lean=0.0, slant=0.0, cuts=10, rough=0.03, panel=False, chips=1)
    for vertex in lintel.verts:
        x, y, z = vertex.co
        sag = 0.04 * math.sin(math.pi * z / 2.95)
        vertex.co = Vector((z - 1.475, y, 2.45 + x + 0.24 - sag))
    bmesh.ops.recalc_face_normals(lintel, faces=lintel.faces[:])
    parts.append(pk.part('Gate_Lintel', lintel, target, m['granite'], rng=rng, smooth=35.0))
    for index, glyph in enumerate(rng.sample(range(len(RUNES)), 4)):
        x = -0.75 + index * 0.5
        parts += rune_strokes(glyph, (x, -0.35, 2.52), 0.17, target, m['rune'], f'Gate_{index}', rng)
    return kit.join(parts, 'StoneGate_A')


def flat_stone(radius, height, rng):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
    waves = [(Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(-0.3, 0.3))).normalized(), rng.uniform(2.0, 4.0), rng.uniform(0.0, 6.28), rng.uniform(0.04, 0.1)) for _ in range(4)]
    stretch = rng.uniform(0.75, 0.95)
    for vertex in bm.verts:
        d = vertex.co.normalized()
        bump = 1.0 + sum(amount * math.sin(d.dot(axis) * frequency + phase) for axis, frequency, phase, amount in waves)
        vertex.co = Vector((d.x * radius * bump, d.y * radius * bump * stretch, d.z * height * bump))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def cairn(key, layers, m, target, rng):
    parts = []
    z = 0.0
    radius = layers[0]
    for index, radius in enumerate(layers):
        height = radius * rng.uniform(0.32, 0.42)
        stone = pk.part(f'Cairn{key}_{index}', flat_stone(radius, height, rng), target, m['granite'], rng=rng, smooth=30.0)
        stone.matrix_world = Matrix.Translation((rng.uniform(-0.04, 0.04), rng.uniform(-0.04, 0.04), z + height * 0.75)) @ Matrix.Rotation(rng.uniform(0.0, 6.28), 4, 'Z') @ Matrix.Rotation(rng.uniform(-0.12, 0.12), 4, 'X')
        parts.append(stone)
        z += height * 1.45
    return kit.join(parts, f'Cairn_{key}')


def banner(width, length, rng):
    bm = bmesh.new()
    columns, rows = 4, 6
    grid = []
    for r in range(rows + 1):
        row = []
        t = r / rows
        for c in range(columns + 1):
            s = c / columns
            ragged = 0.0
            if r == rows:
                ragged = rng.uniform(0.0, 0.12) * length
            x = (s - 0.5) * width
            z = -t * length + ragged
            y = 0.05 * math.sin(t * 2.4 + s * 1.3) * t + 0.03 * math.sin(s * 6.0) * t
            row.append(bm.verts.new((x, y, z)))
        grid.append(row)
    for r in range(rows):
        for c in range(columns):
            bm.faces.new((grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]))
    bmesh.ops.solidify(bm, geom=bm.faces[:], thickness=0.012)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def prayer_pole(m, target, rng):
    parts = [pk.part('Pole_Shaft', pk.rod((0.0, 0.0, -0.1), (0.04, 0.02, 2.85), 0.06, steps=8, end_radius=0.045), target, m['wood'], rng=rng, smooth=40.0)]
    parts.append(pk.part('Pole_Bar', pk.rod((-0.5, 0.0, 2.45), (0.5, 0.0, 2.48), 0.035, steps=6), target, m['wood'], rng=rng, smooth=40.0))
    for index, x in enumerate((-0.36, 0.0, 0.36)):
        cloth = pk.part(f'Pole_Banner{index}', banner(0.24, rng.uniform(0.6, 0.85), rng), target, m['banner'], rng=rng, smooth=50.0)
        cloth.matrix_world = Matrix.Translation((x, 0.02, 2.44))
        parts.append(cloth)
    for index in range(4):
        angle = index * math.pi / 2.0 + rng.uniform(-0.3, 0.3)
        stone = pk.part(f'Pole_Stone{index}', flat_stone(0.16, 0.08, rng), target, m['granite'], rng=rng, smooth=30.0)
        stone.matrix_world = Matrix.Translation((math.cos(angle) * 0.13, math.sin(angle) * 0.13, 0.05))
        parts.append(stone)
    return kit.join(parts, 'PrayerPole_A')


def boulder(name, size, m, target, rng, facets=9, sink=0.32, detail=2):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=detail, radius=1.0)
    offset = Vector((rng.uniform(-50.0, 50.0), rng.uniform(-50.0, 50.0), rng.uniform(-50.0, 50.0)))
    for vertex in bm.verts:
        d = vertex.co.normalized()
        point = Vector((d.x * size[0], d.y * size[1], d.z * size[2]))
        vertex.co = point + d * (weathered(point, offset, 0.07, 1.4) + weathered(point, offset + Vector((5.0, 1.0, 3.0)), 0.03, 3.6))
    for _ in range(facets):
        normal = Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(-0.2, 1.0))).normalized()
        reach = math.sqrt((normal.x * size[0]) ** 2 + (normal.y * size[1]) ** 2 + (normal.z * size[2]) ** 2)
        kit.cut(bm, normal * reach * rng.uniform(0.7, 0.86), normal)
    kit.cut(bm, Vector((0.0, 0.0, -size[2] * (1.0 - sink))), Vector((0.0, 0.0, -1.0)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return pk.part(name, bm, target, m['boulder'], rng=rng, smooth=35.0)


def build(target):
    rng = random.Random(83)
    m = {
        'granite': granite_material('SummitGranite'),
        'rune': pk.glow_material('SummitRune', color=(0.38, 0.7, 1.0), strength=4.0, base=(0.5, 0.75, 1.0)),
        'bronze': bronze_material('SummitBronze'),
        'crystal': pk.crystal_material('SummitCrystal', core=(0.45, 0.78, 1.0), shell=(0.05, 0.1, 0.18), strength=3.5),
        'wood': pk.wood_material('SummitWood', light=(0.27, 0.21, 0.15), dark=(0.11, 0.085, 0.06), weathered=0.6),
        'banner': banner_material('SummitBanner'),
        'boulder': granite_material('SummitBoulder', light=(0.24, 0.24, 0.24), dark=(0.08, 0.085, 0.092)),
    }
    objects = [
        standing_stone('A', (0.86, 0.5, 2.45), m, target, rng),
        standing_stone('B', (0.78, 0.46, 2.7), m, target, rng),
        standing_stone('C', (0.92, 0.54, 2.2), m, target, rng),
        glyph_stone('A', (0.62, 0.38, 1.1), m, target, rng),
        glyph_stone('B', (0.56, 0.34, 0.95), m, target, rng),
        storm_spire(m, target, rng),
        stone_gate(m, target, rng),
        cairn('A', (0.42, 0.34, 0.27, 0.2, 0.14), m, target, rng),
        cairn('B', (0.38, 0.3, 0.24, 0.17), m, target, rng),
        prayer_pole(m, target, rng),
        boulder('Rock_A', (0.5, 0.42, 0.38), m, target, rng),
        boulder('Rock_B', (0.46, 0.36, 0.3), m, target, rng),
        boulder('Rock_C', (0.55, 0.3, 0.27), m, target, rng),
        boulder('Rock_D', (0.4, 0.38, 0.44), m, target, rng),
        boulder('Slab_A', (0.6, 0.45, 0.17), m, target, rng, facets=6, sink=0.45),
        boulder('Slab_B', (0.5, 0.4, 0.14), m, target, rng, facets=6, sink=0.45),
        boulder('Boulder_A', (0.85, 0.7, 0.62), m, target, rng, facets=12, detail=3),
        boulder('Boulder_B', (0.75, 0.66, 0.72), m, target, rng, facets=12, detail=3),
    ]
    return objects


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    pk.unwrap_all(objects, {'SummitRune': 0.01, 'SummitBanner': 0.01})
    for index, obj in enumerate(objects):
        obj.matrix_world = Matrix.Translation((index * 4.0, 0.0, 0.0)) @ obj.matrix_world
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.4)
    if args.get('--preview'):
        pk.ground_preview(14.0)
        for index, obj in enumerate(objects):
            obj.matrix_world = Matrix.Translation(((index % 5) * 2.6 - 5.2, (index // 5 % 2) * 3.4 - 1.7, 0.0)) @ Matrix.Translation((-index * 4.0, 0.0, 0.0)) @ obj.matrix_world
        pk.preview(objects[:5], args['--preview'], 'summit_a', fill=0.95)
        pk.preview(objects[5:10], args['--preview'], 'summit_b', fill=0.95)
        pk.preview(objects[10:], args['--preview'], 'summit_c', fill=0.95)


if __name__ == '__main__':
    main()
