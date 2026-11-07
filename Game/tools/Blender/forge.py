import bpy
import bmesh
import math
import os
import random
import re
import sys
import time
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

NAME = 'Forge'
EMBER = (1.0, 0.42, 0.12)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
SQRT2 = math.sqrt(2.0)
GAP = 0.06
JOINT_RISE = 0.022
SLAB_BOTTOM = -0.3
CURB_GLYPH_PERIOD = 0.6
FASCIA_GLYPHS = 40
FASCIA_RUNE_Z = -0.2
CLAMPS_PER_SIDE = 8
GATE_THICKNESS = 0.12
GATE_HEIGHT = 0.85
GATE_BEAM = 1.75
GATE_STEM_X = 0.7
GATE_STEM_TOP = 2.45


def read_forge():
    with open(os.path.join(ROOT, 'GameShared', 'src', 'GameShared', 'Content', 'Forge.h'), 'r', encoding='utf-8') as file:
        text = file.read()
    number = r'(-?\d+(?:\.\d+)?)f'
    values = {m.group(1): float(m.group(2)) for m in re.finditer(r'inline constexpr float (\w+) = ' + number + ';', text)}
    for m in re.finditer(r'inline constexpr float (\w+)\[\] = \{([^}]*)\};', text):
        values[m.group(1)] = [float(v) for v in re.findall(r'-?\d+(?:\.\d+)?', m.group(2))]
    for m in re.finditer(r'inline constexpr Vec2 (\w+)\{ ' + number + ', ' + number + r' \};', text):
        values[m.group(1)] = (float(m.group(2)), float(m.group(3)))
    for m in re.finditer(r'inline constexpr Vec2 (\w+)\[\] = \{(.*?)\};', text):
        values[m.group(1)] = [(float(a), float(b)) for a, b in re.findall(r'\{ ' + number + ', ' + number + r' \}', m.group(2))]
    return values


F = read_forge()


def blender(x, y, z=0.0):
    return Vector((x, -y, z))


def polygon_distance(p, points):
    best = (p[0] - points[0][0]) ** 2 + (p[1] - points[0][1]) ** 2
    sign = 1.0
    j = len(points) - 1
    for i in range(len(points)):
        ex, ey = points[j][0] - points[i][0], points[j][1] - points[i][1]
        wx, wy = p[0] - points[i][0], p[1] - points[i][1]
        t = max(0.0, min(1.0, (wx * ex + wy * ey) / (ex * ex + ey * ey)))
        bx, by = wx - ex * t, wy - ey * t
        best = min(best, bx * bx + by * by)
        c0 = p[1] >= points[i][1]
        c1 = p[1] < points[j][1]
        c2 = ex * wy > ey * wx
        if (c0 and c1 and c2) or (not c0 and not c1 and not c2):
            sign = -sign
        j = i
    return sign * math.sqrt(best)


def trench_distance(p):
    cx, cy = F['PommelCenter']
    pommel = math.hypot(p[0] - cx, p[1] - cy) - F['PommelRadius']
    gy = 0.5 * (F['GripLow'] + F['GripHigh'])
    hx, hy = F['GripHalf'], 0.5 * (F['GripHigh'] - F['GripLow'])
    dx, dy = abs(p[0]) - hx, abs(p[1] - gy) - hy
    grip = math.hypot(max(dx, 0.0), max(dy, 0.0)) + min(max(dx, dy), 0.0)
    return min(pommel, grip, polygon_distance(p, F['GuardOutline']), polygon_distance(p, F['BladeOutline']))


def trench_normal(p):
    h = 0.02
    gx = trench_distance((p[0] + h, p[1])) - trench_distance((p[0] - h, p[1]))
    gy = trench_distance((p[0], p[1] + h)) - trench_distance((p[0], p[1] - h))
    length = math.hypot(gx, gy) or 1.0
    return gx / length, gy / length


def trace_outline():
    def project(p):
        for _ in range(4):
            d = trench_distance(p)
            n = trench_normal(p)
            p = (p[0] - n[0] * d, p[1] - n[1] * d)
        return p

    start = project((1.4, F['BridgeYs'][0]))
    loop = [start]
    p = start
    travelled = 0.0
    for _ in range(40000):
        n = trench_normal(p)
        q = project((p[0] - n[1] * 0.04, p[1] + n[0] * 0.04))
        travelled += math.hypot(q[0] - p[0], q[1] - p[1])
        p = q
        if travelled > 1.0 and math.hypot(p[0] - start[0], p[1] - start[1]) < 0.03:
            break
        loop.append(p)
    lengths = [0.0]
    for a, b in zip(loop, loop[1:]):
        lengths.append(lengths[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    return loop, lengths


def offset_polygon(points, distance, mitre=2.2):
    area = sum(points[i][0] * points[(i + 1) % len(points)][1] - points[(i + 1) % len(points)][0] * points[i][1] for i in range(len(points)))
    orient = 1.0 if area > 0.0 else -1.0
    result = []
    count = len(points)
    for i in range(count):
        a, b, c = points[i - 1], points[i], points[(i + 1) % count]
        e1 = (b[0] - a[0], b[1] - a[1])
        e2 = (c[0] - b[0], c[1] - b[1])
        l1, l2 = math.hypot(*e1), math.hypot(*e2)
        n1 = (orient * e1[1] / l1, -orient * e1[0] / l1)
        n2 = (orient * e2[1] / l2, -orient * e2[0] / l2)
        bis = (n1[0] + n2[0], n1[1] + n2[1])
        bl = math.hypot(*bis)
        if bl < 1e-6:
            result.append((b[0] + n1[0] * distance, b[1] + n1[1] * distance))
            continue
        cosine = (n1[0] * bis[0] + n1[1] * bis[1]) / bl
        scale = distance / max(cosine, 1e-3)
        if scale > mitre * distance:
            result.append((b[0] + n1[0] * distance, b[1] + n1[1] * distance))
            result.append((b[0] + n2[0] * distance, b[1] + n2[1] * distance))
        else:
            result.append((b[0] + bis[0] / bl * scale, b[1] + bis[1] / bl * scale))
    return result


def prism(points, z0, z1):
    bm = bmesh.new()
    bottom = [bm.verts.new(blender(x, y, z0)) for x, y in points]
    top = [bm.verts.new(blender(x, y, z1)) for x, y in points]
    bm.faces.new(bottom)
    bm.faces.new(list(reversed(top)))
    count = len(points)
    for i in range(count):
        j = (i + 1) % count
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def circle(center, radius, steps=72):
    return [(center[0] + radius * math.cos(2.0 * math.pi * k / steps), center[1] + radius * math.sin(2.0 * math.pi * k / steps)) for k in range(steps)]


def box_points(cx, cy, hx, hy):
    return [(cx - hx, cy - hy), (cx + hx, cy - hy), (cx + hx, cy + hy), (cx - hx, cy + hy)]


def boolean(obj, cutter, operation):
    modifier = obj.modifiers.new('Bool', 'BOOLEAN')
    modifier.operation = operation
    modifier.object = cutter
    modifier.solver = 'EXACT'
    kit.apply_modifiers(obj)


def remove(obj):
    bpy.data.objects.remove(obj, do_unlink=True)
    bpy.context.view_layer.update()


def sword_solid(name, grow, z0, z1, target):
    cx, cy = F['PommelCenter']
    gy = 0.5 * (F['GripLow'] + F['GripHigh'])
    shapes = [
        circle((cx, cy), F['PommelRadius'] + grow),
        box_points(0.0, gy, F['GripHalf'] + grow, 0.5 * (F['GripHigh'] - F['GripLow']) + grow),
        offset_polygon(F['GuardOutline'], grow) if grow > 0.0 else F['GuardOutline'],
        offset_polygon(F['BladeOutline'], grow) if grow > 0.0 else F['BladeOutline'],
    ]
    objects = [kit.mesh_object(f'{name}{i}', prism(points, z0, z1), target) for i, points in enumerate(shapes)]
    base = objects[0]
    for other in objects[1:]:
        boolean(base, other, 'UNION')
        remove(other)
    base.name = name
    return base


def set_local(obj, function):
    mesh = obj.data
    attribute = mesh.attributes.get('Local') or mesh.attributes.new('Local', 'FLOAT_VECTOR', 'POINT')
    values = []
    for vertex in mesh.vertices:
        values.extend(function(vertex.co))
    attribute.data.foreach_set('vector', values)


def rune_strokes(s, u, v, cell, half, width):
    def hash_of(salt):
        return s.math('FRACT', s.mul(s.math('SINE', s.add(s.mul(cell, 12.9898), salt)), 43758.547))

    def line(distance):
        return s.sub(1.0, s.smooth(width * 0.55, width, s.math('ABSOLUTE', distance)))

    def within(value, limit):
        return s.sub(1.0, s.smooth(limit, limit + width * 0.5, s.math('ABSOLUTE', value)))

    def pick(salt, low=0.45):
        return s.smooth(low, low + 0.05, hash_of(salt))

    radius = s.vector('LENGTH', s.combine(u, v, 0.0))
    stem = s.mul(line(u), within(v, half))
    bar = s.mul(s.mul(line(s.sub(v, half * 0.42)), within(u, half * 0.62)), pick(3.1))
    low_bar = s.mul(s.mul(line(s.add(v, half * 0.5)), within(u, half * 0.5)), pick(9.7, 0.6))
    slash = s.mul(s.mul(line(s.sub(u, s.mul(v, 0.62))), within(v, half * 0.85)), pick(5.3))
    back = s.mul(s.mul(line(s.add(u, s.mul(v, 0.62))), within(v, half * 0.6)), pick(2.3, 0.62))
    chevron = s.mul(s.mul(line(s.sub(s.math('ABSOLUTE', u), s.add(s.mul(v, 0.72), half * 0.08))), within(u, half * 0.7)), pick(7.9, 0.55))
    ring = s.mul(line(s.sub(radius, half * 0.58)), pick(1.7, 0.7))
    glyph = s.math('MAXIMUM', stem, bar)
    for stroke in (low_bar, slash, back, chevron, ring):
        glyph = s.math('MAXIMUM', glyph, stroke)
    return s.mul(glyph, within(u, half * 1.05))


def stone_base(s, p, dark, light, scale=1.0):
    island = s.geometry('Random Per Island')
    q = s.offset(p, s.combine(s.mul(island, 37.0), s.mul(island, 53.0), s.mul(island, 71.0)))
    large = s.noise(q, 0.6 * scale, detail=3.0)
    mid = s.noise(q, 3.0 * scale, detail=5.0, roughness=0.55)
    fine = s.noise(q, 14.0 * scale, detail=3.0)
    tone = s.clamp01(s.add(s.mul(large, 0.45), s.add(s.mul(mid, 0.3), s.mul(island, 0.35))))
    base = s.mix(tone, dark, light)
    base = s.mix(s.mul(fine, 0.25), base, (dark[0] * 0.6, dark[1] * 0.6, dark[2] * 0.6))
    return base, island, q, large, mid, fine


def slab_material(name, bake_size):
    s = kit.Shader(name)
    p = pk.local(s)
    base, island, q, large, mid, fine = stone_base(s, p, (0.014, 0.013, 0.016), (0.085, 0.077, 0.08))
    scorch = s.smooth(0.55, 0.8, s.noise(p, 0.18, detail=2.0))
    base = s.mix(s.mul(scorch, 0.35), base, (0.19, 0.075, 0.045))
    warped = s.offset(q, s.mul(s.noise_color(q, 1.3, detail=2.0), 0.3))
    crack_mask = s.smooth(0.42, 0.62, s.noise(q, 0.55, detail=3.0))
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.016, s.voronoi(warped, 1.4, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), crack_mask)
    hair_mask = s.smooth(0.5, 0.7, s.noise(q, 1.1, detail=2.0))
    hairline = s.mul(s.sub(1.0, s.smooth(0.0, 0.006, s.voronoi(warped, 4.2, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), hair_mask)
    pits = s.mul(s.sub(1.0, s.smooth(0.0, 0.2, s.voronoi(q, 34.0, 'F1', 1.0, 'Distance'))), s.smooth(0.45, 0.7, s.noise(q, 2.4, detail=2.0)))
    dents = s.sub(1.0, s.smooth(0.0, 0.12, s.voronoi(q, 7.0, 'F1', 1.0, 'Distance')))
    polish = s.smooth(0.52, 0.78, s.noise(q, 0.32, detail=2.0))
    edge = pk.wear(s, 0.035)
    chipped = s.mul(edge, s.smooth(0.42, 0.6, s.noise(q, 7.0, detail=3.0)))
    flecks = s.sub(1.0, s.smooth(0.0, 0.08, s.voronoi(q, 60.0, "F1", 1.0, "Distance")))
    vein_warp = s.offset(q, s.mul(s.noise_color(q, 0.7, detail=3.0), 0.9))
    vein = s.sub(1.0, s.math("ABSOLUTE", s.sub(s.mul(s.noise(vein_warp, 0.45, detail=4.0, roughness=0.6), 2.0), 1.0)))
    vein = s.mul(s.smooth(0.93, 0.985, vein), s.smooth(0.4, 0.62, s.noise(q, 0.25, detail=2.0)))
    glass = s.clamp01(s.add(s.mul(polish, 0.7), s.smooth(0.42, 0.62, s.noise(q, 0.6, detail=2.0))))
    base = s.mix(s.mul(polish, 0.5), base, s.mul(base, 0.72))
    base = s.mix(s.mul(glass, 0.6), base, (0.008, 0.008, 0.011))
    base = s.mix(s.mul(vein, 0.45), base, (0.2, 0.19, 0.2))
    base = s.mix(s.mul(dents, 0.3), base, (0.018, 0.016, 0.015))
    base = s.mix(s.mul(pits, 0.75), base, (0.008, 0.007, 0.007))
    base = s.mix(s.mul(flecks, 0.25), base, (0.16, 0.15, 0.15))
    base = s.mix(s.mul(chipped, 0.75), base, (0.17, 0.155, 0.15))
    base = s.mix(s.mul(s.add(cracks, s.mul(hairline, 0.6)), 0.9), base, (0.006, 0.005, 0.005))
    dirt = pk.cavity(s, 0.08)
    base = s.mix(s.mul(dirt, 0.7), base, (0.01, 0.008, 0.007))
    rough = s.remap(mid, 0.3, 0.7, 0.28, 0.5)
    rough = s.lerp(polish, rough, 0.12)
    rough = s.lerp(glass, rough, 0.06)
    rough = s.lerp(s.mul(vein, 0.6), rough, 0.2)
    rough = s.lerp(s.mul(edge, 0.5), rough, 0.42)
    rough = s.lerp(chipped, rough, 0.82)
    rough = s.lerp(s.clamp01(s.add(s.add(cracks, hairline), pits)), rough, 0.95)
    height = s.add(s.mul(large, 0.35), s.add(s.mul(mid, 0.4), s.mul(fine, 0.1)))
    height = s.sub(height, s.add(s.mul(cracks, 0.8), s.mul(hairline, 0.3)))
    height = s.sub(height, s.add(s.mul(pits, 0.3), s.add(s.mul(dents, 0.2), s.mul(chipped, 0.45))))
    return s.finish(base, rough, height=height, bump=1.0, bump_distance=0.05, bake_size=bake_size)


def iron_plate_material(name, bake_size):
    s = kit.Shader(name)
    p = pk.local(s)
    noise = s.noise(p, 2.5, detail=4.0)
    fine = s.noise(p, 18.0, detail=3.0)
    base = s.mix(noise, (0.035, 0.033, 0.032), (0.085, 0.078, 0.072))
    rust = s.mul(s.smooth(0.58, 0.78, s.noise(p, 1.2, detail=4.0)), 0.5)
    base = s.mix(rust, base, (0.12, 0.05, 0.02))
    edge = pk.wear(s, 0.02)
    base = s.mix(s.mul(edge, 0.7), base, (0.3, 0.27, 0.24))
    dirt = pk.cavity(s, 0.05)
    base = s.mix(s.mul(dirt, 0.5), base, (0.01, 0.009, 0.008))
    metal = s.lerp(rust, 0.85, 0.3)
    rough = s.remap(fine, 0.3, 0.7, 0.42, 0.62)
    rough = s.lerp(s.mul(edge, 0.6), rough, 0.28)
    return s.finish(base, rough, height=s.add(s.mul(noise, 0.5), s.mul(fine, 0.5)), bump=0.15, bump_distance=0.004, metallic=metal, bake_size=bake_size)


def rune_material(name, mode, bake_size):
    s = kit.Shader(name)
    p = pk.local(s)
    x, y, z = s.separate(p)
    large = s.noise(p, 1.2, detail=3.0)
    mid = s.noise(p, 5.0, detail=5.0, roughness=0.55)
    tone = s.clamp01(s.add(s.mul(large, 0.55), s.mul(mid, 0.45)))
    if mode == 'fascia':
        base = s.mix(tone, (0.05, 0.04, 0.032), (0.13, 0.1, 0.075))
        period = 22.627417 / FASCIA_GLYPHS
        cell = s.math('FLOOR', s.mul(x, 1.0 / period))
        u = s.sub(s.sub(x, s.mul(cell, period)), period * 0.5)
        v = s.sub(z, FASCIA_RUNE_Z)
        facing = s.smooth(0.038, 0.047, y)
        glyph = s.mul(rune_strokes(s, u, v, cell, 0.2, 0.034), facing)
        metal = 0.55
    else:
        base = s.mix(tone, (0.04, 0.036, 0.034), (0.12, 0.108, 0.098))
        cell = s.math('FLOOR', s.mul(x, 1.0 / CURB_GLYPH_PERIOD))
        u = s.sub(s.sub(x, s.mul(cell, CURB_GLYPH_PERIOD)), CURB_GLYPH_PERIOD * 0.5)
        v = s.sub(y, 0.16)
        facing = s.smooth(F['CurbTop'] - 0.012, F['CurbTop'] - 0.003, z)
        glyph = s.mul(rune_strokes(s, u, v, cell, 0.1, 0.022), facing)
        metal = 0.0
    edge = pk.wear(s, 0.025)
    base = s.mix(s.mul(edge, 0.45), base, (0.25, 0.21, 0.17))
    dirt = pk.cavity(s, 0.05)
    base = s.mix(s.mul(dirt, 0.6), base, (0.012, 0.01, 0.009))
    base = s.mix(glyph, base, (0.95, 0.4, 0.1))
    rough = s.remap(mid, 0.3, 0.7, 0.45, 0.72)
    height = s.sub(s.add(s.mul(mid, 0.7), s.mul(large, 0.3)), s.mul(glyph, 0.7))
    return s.finish(base, rough, height=height, bump=0.35, bump_distance=0.01, metallic=s.mul(s.sub(1.0, glyph), metal), emission=glyph, emission_color=EMBER, emission_strength=3.2, bake_size=bake_size)


def block_material(name, bake_size):
    s = kit.Shader(name)
    p = pk.local(s)
    base, island, q, large, mid, fine = stone_base(s, p, (0.03, 0.026, 0.024), (0.1, 0.086, 0.076), scale=1.6)
    soot = s.smooth(0.45, 0.75, s.noise(p, 0.8, detail=3.0))
    base = s.mix(s.mul(soot, 0.6), base, (0.012, 0.01, 0.009))
    edge = pk.wear(s, 0.025)
    base = s.mix(s.mul(edge, 0.45), base, (0.2, 0.17, 0.15))
    dirt = pk.cavity(s, 0.06)
    base = s.mix(s.mul(dirt, 0.6), base, (0.01, 0.008, 0.007))
    rough = s.remap(mid, 0.3, 0.7, 0.6, 0.82)
    return s.finish(base, rough, height=s.add(s.mul(mid, 0.6), s.mul(large, 0.4)), bump=0.3, bump_distance=0.01, bake_size=bake_size)


def slab_rects(inner, outer, rng, along=(1.1, 3.4), across=(0.9, 1.9)):
    a, b = inner / SQRT2, outer / SQRT2
    rects = []
    strips = [((-b, a), (a, b), True), ((a, b), (-a, b), False), ((-a, b), (-b, -a), True), ((-b, -a), (-b, a), False)]
    for (u0, u1), (v0, v1), horizontal in strips:
        length = (u1 - u0) if horizontal else (v1 - v0)
        width = (v1 - v0) if horizontal else (u1 - u0)
        rows = max(1, int(round(width / rng.uniform(*across))))
        for row in range(rows):
            w0 = row / rows
            w1 = (row + 1) / rows
            t = 0.0
            while t < length - 1e-3:
                step = min(rng.uniform(*along), length - t)
                if length - t - step < along[0] * 0.6:
                    step = length - t
                if horizontal:
                    rects.append((u0 + t, u0 + t + step, v0 + w0 * (v1 - v0), v0 + w1 * (v1 - v0)))
                else:
                    rects.append((u0 + w0 * (u1 - u0), u0 + w1 * (u1 - u0), v0 + t, v0 + t + step))
                t += step
    return rects


def bsp_rects(u0, u1, v0, v1, rng, largest=2.9, smallest=1.0):
    width, height = u1 - u0, v1 - v0
    if max(width, height) <= largest or (width < 2.0 * smallest and height < 2.0 * smallest):
        return [(u0, u1, v0, v1)]
    if width >= height:
        cut = u0 + width * rng.uniform(0.35, 0.65)
        return bsp_rects(u0, cut, v0, v1, rng, largest, smallest) + bsp_rects(cut, u1, v0, v1, rng, largest, smallest)
    cut = v0 + height * rng.uniform(0.35, 0.65)
    return bsp_rects(u0, u1, v0, cut, rng, largest, smallest) + bsp_rects(u0, u1, cut, v1, rng, largest, smallest)


def clip_half(poly, normal, offset):
    out = []
    for i, a in enumerate(poly):
        b = poly[(i + 1) % len(poly)]
        da = normal[0] * a[0] + normal[1] * a[1] - offset
        db = normal[0] * b[0] + normal[1] * b[1] - offset
        if da >= 0.0:
            out.append(a)
        if (da >= 0.0) != (db >= 0.0):
            t = da / (da - db)
            out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
    return out


def chip_corners(poly, rng):
    def toward(c, other):
        d = math.hypot(other[0] - c[0], other[1] - c[1])
        cut = min(rng.uniform(0.04, 0.13), 0.3 * d)
        return (c[0] + (other[0] - c[0]) * cut / d, c[1] + (other[1] - c[1]) * cut / d)

    out = []
    for i, c in enumerate(poly):
        if rng.random() < 0.3:
            out += [toward(c, poly[i - 1]), toward(c, poly[(i + 1) % len(poly)])]
        else:
            out.append(c)
    return out


def slab_pieces(rect, rng):
    u0, u1, v0, v1 = rect
    poly = [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]
    pieces = [poly]
    if rng.random() < 0.3 and max(u1 - u0, v1 - v0) > 0.9:
        along_u = (u1 - u0) >= (v1 - v0)
        t = rng.uniform(0.3, 0.7)
        skew = rng.uniform(-0.35, 0.35)
        if along_u:
            a = (u0 + (u1 - u0) * t + skew * (v1 - v0) * 0.5, v0)
            b = (u0 + (u1 - u0) * t - skew * (v1 - v0) * 0.5, v1)
        else:
            a = (u0, v0 + (v1 - v0) * t + skew * (u1 - u0) * 0.5)
            b = (u1, v0 + (v1 - v0) * t - skew * (u1 - u0) * 0.5)
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        normal = ((b[1] - a[1]) / length, -(b[0] - a[0]) / length)
        offset = normal[0] * a[0] + normal[1] * a[1]
        gap = rng.uniform(0.008, 0.02)
        pieces = [clip_half(poly, normal, offset + gap), clip_half(poly, (-normal[0], -normal[1]), -offset + gap)]
    return [chip_corners(piece, rng) for piece in pieces if len(piece) >= 3]


def slab_mesh(rects, height, rng, inset=GAP * 0.5):
    bm = bmesh.new()
    for u0, u1, v0, v1 in rects:
        u0, u1, v0, v1 = u0 + inset, u1 - inset, v0 + inset, v1 - inset
        if u1 - u0 < 0.05 or v1 - v0 < 0.05:
            continue
        sunk = -0.012 if rng.random() < 0.08 else 0.0
        for piece in slab_pieces((u0, u1, v0, v1), rng):
            top = height + sunk + rng.uniform(-0.012, 0.01)
            tilt = rng.uniform(-0.006, 0.006), rng.uniform(-0.006, 0.006)
            cu = sum(c[0] for c in piece) / len(piece)
            cv = sum(c[1] for c in piece) / len(piece)
            points = [((u - v) / SQRT2, (u + v) / SQRT2) for u, v in piece]
            low = [bm.verts.new(blender(x, y, SLAB_BOTTOM)) for x, y in points]
            high = [bm.verts.new(blender(x, y, top + tilt[0] * (u - cu) + tilt[1] * (v - cv))) for (x, y), (u, v) in zip(points, piece)]
            bm.faces.new(low)
            bm.faces.new(list(reversed(high)))
            for i in range(len(piece)):
                j = (i + 1) % len(piece)
                bm.faces.new((low[i], low[j], high[j], high[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def quadrant_of(rect):
    u, v = 0.5 * (rect[0] + rect[1]), 0.5 * (rect[2] + rect[3])
    if abs(v) >= abs(u):
        return 0 if v > 0.0 else 2
    return 1 if u > 0.0 else 3


def band_of(outer):
    bands = F['ForgeBandEdges']
    return max(b for b in range(len(bands) - 1) if bands[b] >= outer - 1e-4)


def band_strips(inner, outer):
    if inner <= 1e-3:
        h = outer / SQRT2
        return [(0.0, h, 0.0, h), (0.0, h, -h, 0.0), (-h, 0.0, -h, 0.0), (-h, 0.0, 0.0, h)]
    a, b = inner / SQRT2, outer / SQRT2
    return [(-b, a, a, b), (a, b, -a, b), (-a, b, -b, -a), (-b, -a, -b, a)]


def uv_points(rect):
    u0, u1, v0, v1 = rect
    return [((u - v) / SQRT2, (u + v) / SQRT2) for u, v in ((u0, v0), (u1, v0), (u1, v1), (u0, v1))]


def build_floor(m, target, rng):
    edges, heights = F['ForgeRingEdges'], F['ForgeRingHeights']
    bands = F['ForgeBandEdges']
    groove = min(heights)
    cutter = sword_solid('FloorCutter', F['CurbWidth'] + 0.02, -2.0, 2.0, target)
    cx, cy = F['PommelCenter']
    ring_cut = kit.mesh_object('RingCut', prism(circle((cx, cy), F['PommelRingOuter'] + 0.02), -2.0, 2.0), target)
    boolean(cutter, ring_cut, 'UNION')
    remove(ring_cut)
    groups = {}
    for ring in range(len(heights)):
        if heights[ring] <= groove + 1e-4:
            continue
        outer, inner = edges[ring], edges[ring + 1]
        if ring == 0:
            rects = slab_rects(inner, outer - 0.01, rng, along=(2.6, 3.0), across=(1.0, 1.0))
            prefix, material = 'Rim', m['rim']
        elif inner <= 1e-3:
            half = outer / SQRT2
            rects = bsp_rects(-half, half, -half, half, rng)
            prefix, material = f'Slabs{band_of(outer)}', m['slab']
        else:
            rects = slab_rects(inner, outer, rng)
            prefix, material = f'Slabs{band_of(outer)}', m['slab']
        for rect in rects:
            groups.setdefault((prefix, quadrant_of(rect)), (material, heights[ring], []))[2].append(rect)
    floor = []
    for (prefix, quadrant), (material, height, rects) in groups.items():
        obj = kit.mesh_object(f'{prefix}{quadrant}', slab_mesh(rects, height, rng), target, material)
        pk.tag(obj)
        kit.bevel(obj, 0.028, segments=1, angle=40.0)
        boolean(obj, cutter, 'DIFFERENCE')
        pk.tag(obj)
        floor.append(obj)
    remove(cutter)
    trench = sword_solid('BedCutter', F['CurbWidth'] * 0.5, -2.0, 2.0, target)
    for band in range(len(bands) - 1):
        outer = min(bands[band], F['ForgeReach'] - 0.05)
        for quadrant, rect in enumerate(band_strips(bands[band + 1], outer)):
            bed = kit.mesh_object(f'Bed{band}{quadrant}', prism(uv_points(rect), -0.6, groove + JOINT_RISE), target, m["iron"])
            boolean(bed, trench, 'DIFFERENCE')
            if not bed.data.polygons:
                remove(bed)
                continue
            pk.tag(bed)
            floor.append(bed)
    remove(trench)
    return floor


def build_curb(m, target):
    loop, lengths = trace_outline()
    curb = sword_solid('Curb', F['CurbWidth'], F['TrenchFloor'] - 0.02, F['CurbTop'], target)
    inner = sword_solid('CurbInner', 0.0, -2.0, 2.0, target)
    boolean(curb, inner, 'DIFFERENCE')
    remove(inner)
    edge = sword_solid('CurbEdge', 0.075, 0.0, F['CurbTop'] + 0.03, target)
    hole = sword_solid('CurbEdgeInner', 0.012, -2.0, 2.0, target)
    boolean(edge, hole, 'DIFFERENCE')
    remove(hole)
    kit.bevel(curb, 0.02, segments=2, angle=40.0)
    curb.data.materials.append(m['curb'])
    edge.data.materials.append(m['bronze'])

    def arc(co):
        x, y = co.x, -co.y
        best, at = 1e9, 0.0
        for i in range(len(loop) - 1):
            ax, ay = loop[i]
            bx, by = loop[i + 1]
            ex, ey = bx - ax, by - ay
            span = ex * ex + ey * ey
            t = max(0.0, min(1.0, ((x - ax) * ex + (y - ay) * ey) / span)) if span > 1e-12 else 0.0
            d = (x - ax - ex * t) ** 2 + (y - ay - ey * t) ** 2
            if d < best:
                best, at = d, lengths[i] + t * math.sqrt(span)
        return (at, max(trench_distance((x, y)), 0.0), co.z)

    total = lengths[-1]
    mesh = curb.data
    if mesh.attributes.get('Local'):
        mesh.attributes.remove(mesh.attributes['Local'])
    attribute = mesh.attributes.new('Local', 'FLOAT_VECTOR', 'CORNER')
    per_vertex = [arc(vertex.co) for vertex in mesh.vertices]
    values = [0.0] * (len(mesh.loops) * 3)
    for polygon in mesh.polygons:
        corners = [per_vertex[mesh.loops[i].vertex_index] for i in polygon.loop_indices]
        low = min(c[0] for c in corners)
        wrap = max(c[0] for c in corners) - low > 0.5 * total
        for i, c in zip(polygon.loop_indices, corners):
            values[i * 3:i * 3 + 3] = (c[0] + total if wrap and c[0] < 0.5 * total else c[0], c[1], c[2])
    attribute.data.foreach_set('vector', values)
    pk.tag(edge)
    return [curb, edge]


def build_trench(m, target, rng):
    floor = sword_solid('TrenchFloor', 0.08, F['TrenchFloor'] - 0.3, F['TrenchFloor'], target)
    floor.data.materials.append(m['block'])
    pk.tag(floor)
    parts = [floor]
    spine = kit.mesh_object('Fuller', prism(box_points(0.0, 0.5 * (F['FullerHigh'] + F['FullerLow']), F['FullerHalf'], 0.5 * (F['FullerHigh'] - F['FullerLow'])), F['TrenchFloor'] - 0.05, F['FullerTop']), target, m['bronze'])
    pk.tag(spine)
    kit.bevel(spine, 0.03, segments=2, angle=40.0)
    parts.append(spine)
    cx, cy = F['PommelCenter']
    step = kit.mesh_object('PommelStep', prism(circle((cx, cy), F['PommelRadius'] * 0.63), F['TrenchFloor'] - 0.05, F['FullerTop'] - 0.03), target, m['block'])
    hole = kit.mesh_object('PommelStepHole', prism(circle((cx, cy), F['PommelRadius'] * 0.36), -2.0, 2.0), target)
    boolean(step, hole, 'DIFFERENCE')
    remove(hole)
    pk.tag(step)
    kit.bevel(step, 0.02, segments=2, angle=40.0)
    parts.append(step)
    ring = kit.mesh_object('PommelRing', prism(circle((cx, cy), F['PommelRingOuter']), -0.3, F['PommelRingTop']), target, m['block'])
    hole = kit.mesh_object('PommelRingHole', prism(circle((cx, cy), F['PommelRadius'] + F['CurbWidth'] - 0.01), -2.0, 2.0), target)
    boolean(ring, hole, 'DIFFERENCE')
    remove(hole)
    pk.tag(ring)
    kit.bevel(ring, 0.02, segments=2, angle=40.0)
    parts.append(ring)
    band = kit.mesh_object('PommelBand', prism(circle((cx, cy), F['PommelRingOuter'] + 0.07), -0.3, F['PommelRingTop'] + 0.03), target, m['bronze'])
    hole = kit.mesh_object('PommelBandHole', prism(circle((cx, cy), F['PommelRingOuter'] - 0.08), -2.0, 2.0), target)
    boolean(band, hole, 'DIFFERENCE')
    remove(hole)
    pk.tag(band)
    kit.bevel(band, 0.012, segments=2, angle=40.0)
    parts.append(band)
    return parts


def edge_frame(side):
    reach = F['ForgeReach']
    corners = [(0.0, reach), (reach, 0.0), (0.0, -reach), (-reach, 0.0)]
    a, b = corners[side], corners[(side + 1) % 4]
    length = math.hypot(b[0] - a[0], b[1] - a[1])
    along = ((b[0] - a[0]) / length, (b[1] - a[1]) / length)
    mid = ((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5)
    norm = math.hypot(*mid)
    outward = (mid[0] / norm, mid[1] / norm)
    x_axis = blender(along[0], along[1]).normalized()
    y_axis = blender(outward[0], outward[1]).normalized()
    origin = blender(a[0], a[1])
    frame = Matrix((
        (x_axis.x, y_axis.x, 0.0, origin.x),
        (x_axis.y, y_axis.y, 0.0, origin.y),
        (0.0, 0.0, 1.0, 0.0),
        (0.0, 0.0, 0.0, 1.0),
    ))
    if frame.to_3x3().determinant() < 0.0:
        frame = Matrix((
            (x_axis.x, -y_axis.x, 0.0, origin.x),
            (x_axis.y, -y_axis.y, 0.0, origin.y),
            (0.0, 0.0, 1.0, 0.0),
            (0.0, 0.0, 0.0, 1.0),
        ))
        return frame, length, -1.0
    return frame, length, 1.0


def build_fascia(m, target, rng):
    sides = []
    bottom = F['ForgePlateBottom']
    rim = F['ForgeRingHeights'][0]
    for side in range(4):
        parts = []
        sides.append(parts)
        frame, length, flip = edge_frame(side)

        def piece(name, size, center, material, bevel=0.02, local_offset=None):
            obj = pk.part(f'{name}{side}', pk.box(size, (center[0], center[1] * flip, center[2])), target, material, bevel=(bevel, 2, 40.0) if bevel else None)
            obj.matrix_world = frame
            if local_offset is not None:
                set_local(obj, lambda co: (co.x + local_offset, co.y * flip, co.z))
            parts.append(obj)
            return obj

        piece('Wall', (length + 1.0, 0.6, rim - 0.02 - bottom), (length * 0.5, -0.3, 0.5 * (rim - 0.02 + bottom)), m['block'], bevel=0.0)
        piece('Cap', (length + 0.4, 0.14, 0.22), (length * 0.5, 0.0, rim - 0.09), m['bronze'], bevel=0.025)
        piece('RuneBand', (length + 0.2, 0.05, 0.62), (length * 0.5, 0.025, FASCIA_RUNE_Z), m['fascia'], bevel=0.012, local_offset=side * length)
        for label, low, high in (('BandA', -0.66, -0.52), ('BandB', -1.72, -1.56), ('BandC', -2.6, -2.44)):
            piece(label, (length + 0.4, 0.08, high - low), (length * 0.5, 0.04, 0.5 * (low + high)), m['bronze'], bevel=0.015)
            for k in range(int(length / 0.62)):
                x = (k + 0.5) * length / int(length / 0.62)
                rivet = pk.part(f'Rivet{label}{side}_{k}', pk.cylinder(0.026, 0.03, 8), target, m['iron'], matrix=frame @ Matrix.Translation((x, 0.08 * flip, 0.5 * (low + high))) @ Matrix.Rotation(-0.5 * math.pi * flip, 4, 'X'), smooth=40.0)
                parts.append(rivet)
        for course, (low, high) in enumerate(((-1.52, -0.7), (-2.4, -1.76))):
            x = -0.2 + rng.uniform(0.0, 0.8)
            while x < length + 0.2:
                width = rng.uniform(1.1, 1.9)
                piece(f'Course{course}_{int(x * 100)}_', (width - 0.02, 0.04, high - low - 0.02), (x + width * 0.5, 0.02, 0.5 * (low + high)), m['block'], bevel=0.015)
                x += width
        for j in range(CLAMPS_PER_SIDE):
            x = (j + 0.5) * length / CLAMPS_PER_SIDE
            piece(f'Clamp{j}_', (0.42, 0.12, rim + 0.95), (x, 0.11, 0.5 * (rim + 0.06 - 0.95)), m['bronze'], bevel=0.03)
            piece(f'Flange{j}_', (0.42, 0.62, 0.08), (x, -0.25, rim + 0.05), m['bronze'], bevel=0.025)
            for bolt in (-0.12, 0.12):
                for z in (rim - 0.25, -0.45):
                    rivet = pk.part(f'Bolt{side}_{j}_{bolt}_{z}', pk.cylinder(0.04, 0.04, 8), target, m['iron'], matrix=frame @ Matrix.Translation((x + bolt, 0.17 * flip, z)) @ Matrix.Rotation(-0.5 * math.pi * flip, 4, 'X'), smooth=40.0)
                    parts.append(rivet)
    return sides


def build_anchors(m, target, rng):
    parts = []
    reach = F['ForgeReach']
    top = F['AnchorTop']
    for index, (cx, cy) in enumerate([(0.0, reach), (reach, 0.0), (0.0, -reach), (-reach, 0.0)]):
        out = (cx / reach, cy / reach)
        center = (cx - out[0] * 0.45, cy - out[1] * 0.45)
        angle = math.atan2(-out[1], out[0])
        frame = Matrix.Translation(blender(center[0], center[1])) @ Matrix.Rotation(angle + 0.25 * math.pi, 4, 'Z')
        block = pk.part(f'Anchor{index}', pk.chipped((1.6, 1.6, top + 1.6), rng, chips=3, depth=0.04, center=(0.0, 0.0, 0.5 * (top - 1.6))), target, m['bronze'], bevel=(0.04, 2, 40.0))
        block.matrix_world = frame
        parts.append(block)
        cap = pk.part(f'AnchorCap{index}', pk.chipped((1.2, 1.2, 0.18), rng, chips=2, depth=0.02, center=(0.0, 0.0, top + 0.09)), target, m['iron'], bevel=(0.03, 2, 40.0))
        cap.matrix_world = frame
        parts.append(cap)
        for z in (top - 0.55, top - 1.35):
            band = pk.part(f'AnchorBand{index}_{int(z * 100)}', pk.box((1.72, 1.72, 0.2), (0.0, 0.0, z)), target, m['iron'], bevel=(0.03, 2, 40.0))
            band.matrix_world = frame
            parts.append(band)
        for corner in range(4):
            a = 0.5 * math.pi * corner + 0.25 * math.pi
            bolt = pk.part(f'AnchorBolt{index}_{corner}', pk.cylinder(0.06, 0.05, 8, z=top + 0.16), target, m['iron'], smooth=40.0)
            bolt.matrix_world = frame @ Matrix.Translation((0.62 * math.cos(a), 0.62 * math.sin(a), 0.02))
            parts.append(bolt)
        shackle = pk.part(f'Shackle{index}', pk.torus(0.42, 0.13, 28, 10), target, m['chain'], smooth=40.0)
        shackle.matrix_world = Matrix.Translation(blender(center[0], center[1], top + 0.42)) @ Matrix.Rotation(angle, 4, 'Z') @ Matrix.Rotation(0.5 * math.pi, 4, 'X')
        parts.append(shackle)
    return parts


def build_bridges(m, target, rng):
    parts = []
    hx, hy, top = F['BridgeHalfX'], F['BridgeHalfY'], F['BridgeTop']
    for index, y in enumerate(F['BridgeYs']):
        deck = pk.part(f'Deck{index}', pk.chipped((2.0 * hx - 0.3, 2.0 * hy, top + 0.12), rng, chips=3, depth=0.03, center=(0.0, 0.0, 0.5 * (top - 0.12))), target, m['block'], bevel=(0.035, 2, 40.0))
        deck.matrix_world = Matrix.Translation(blender(0.0, y))
        parts.append(deck)
        for side in (-1.0, 1.0):
            cap = pk.part(f'BridgeCap{index}{int(side)}', pk.box((0.28, 2.0 * hy + 0.12, top + 0.2), (side * (hx - 0.14), 0.0, 0.5 * (top + 0.06) - 0.04)), target, m['bronze'], bevel=(0.03, 2, 40.0))
            cap.matrix_world = Matrix.Translation(blender(0.0, y))
            parts.append(cap)
        for strap in (-0.45, 0.45):
            band = pk.part(f'Strap{index}{strap}', pk.box((2.0 * hx - 0.5, 0.14, 0.03), (0.0, strap, top + 0.012)), target, m['iron'], bevel=(0.01, 2, 40.0))
            band.matrix_world = Matrix.Translation(blender(0.0, y))
            parts.append(band)
            for k in range(6):
                rivet = pk.part(f'StrapRivet{index}{strap}{k}', pk.cylinder(0.024, 0.018, 8, z=top + 0.025), target, m['iron'], smooth=40.0)
                rivet.matrix_world = Matrix.Translation(blender(-hx + 0.45 + k * (2.0 * hx - 0.9) / 5.0, y + strap * -1.0))
                parts.append(rivet)
    return parts


def chain_material(name, bake_size):
    s = kit.Shader(name)
    p = pk.local(s)
    hammer = s.voronoi(p, 16.0, 'F1', 1.0, 'Distance')
    noise = s.noise(p, 6.0, detail=4.0)
    base = s.mix(noise, (0.032, 0.03, 0.028), (0.08, 0.072, 0.065))
    crevice = pk.cavity(s, 0.05)
    rust = s.clamp01(s.add(s.mul(crevice, 0.8), s.mul(s.smooth(0.55, 0.75, s.noise(p, 2.5, detail=4.0)), 0.6)))
    base = s.mix(rust, base, (0.15, 0.065, 0.028))
    flakes = s.mul(s.smooth(0.58, 0.8, s.noise(p, 15.0, detail=3.0)), rust)
    base = s.mix(s.mul(flakes, 0.7), base, (0.26, 0.12, 0.045))
    edge = pk.wear(s, 0.025)
    base = s.mix(s.mul(edge, 0.85), base, (0.45, 0.43, 0.41))
    metal = s.lerp(rust, 0.85, 0.1)
    metal = s.lerp(edge, metal, 1.0)
    rough = s.remap(noise, 0.3, 0.7, 0.42, 0.66)
    rough = s.lerp(s.mul(edge, 0.9), rough, 0.22)
    rough = s.lerp(rust, rough, 0.9)
    height = s.sub(s.add(s.mul(hammer, 0.5), s.mul(noise, 0.3)), s.add(s.mul(rust, 0.25), s.mul(flakes, 0.2)))
    return s.finish(base, rough, height=height, bump=0.55, bump_distance=0.012, metallic=metal, bake_size=bake_size)


def chain_link(m, target, rng):
    radius, straight, wire = 0.27, 0.26, 0.12
    points = [Vector((straight + radius * math.cos(-0.5 * math.pi + k * math.pi / 4.0), radius * math.sin(-0.5 * math.pi + k * math.pi / 4.0), 0.0)) for k in range(5)]
    points += [Vector((-straight + radius * math.cos(0.5 * math.pi + k * math.pi / 4.0), radius * math.sin(0.5 * math.pi + k * math.pi / 4.0), 0.0)) for k in range(5)]
    return pk.tube(points, wire, target, m['chain'], 'ChainLink', resolution=3, sides=8, closed=True, rng=rng)


def blade_half_width(y):
    outline = F['BladeOutline']
    (x0, y0), (x1, y1) = outline[1], outline[2]
    return x0 + (x1 - x0) * (y - y0) / (y1 - y0)


def build_sluice(m, target, rng):
    line = F['ForgeHiltLine']
    half = blade_half_width(line - GATE_THICKNESS * 0.5)
    post_x = half + F['CurbWidth'] * 0.5
    y = line - GATE_THICKNESS * 0.5
    curb_top = F['CurbTop']
    frame = []
    for side in (-1.0, 1.0):
        frame.append(pk.part(f'SluicePost{int(side)}', pk.box((0.24, 0.34, GATE_BEAM - curb_top), (side * post_x, -y, 0.5 * (GATE_BEAM + curb_top))), target, m['bronze'], bevel=(0.03, 2, 40.0)))
        frame.append(pk.part(f'SluiceFoot{int(side)}', pk.box((0.34, 0.5, 0.1), (side * post_x, -y, curb_top + 0.05)), target, m['iron'], bevel=(0.02, 2, 40.0)))
        frame.append(pk.part(f'SluiceNut{int(side)}', pk.box((0.26, 0.3, 0.22), (side * GATE_STEM_X, -y, GATE_BEAM + 0.2)), target, m['bronze'], bevel=(0.03, 2, 40.0)))
        for z in (curb_top + 0.35, GATE_BEAM - 0.25):
            frame.append(pk.part(f'SluiceBolt{int(side)}{int(z * 10)}', pk.cylinder(0.04, 0.05, 8, z=0.0), target, m['iron'], matrix=Matrix.Translation((side * post_x, -y + 0.17, z)) @ Matrix.Rotation(-0.5 * math.pi, 4, 'X'), smooth=40.0))
    frame.append(pk.part('SluiceBeam', pk.box((2.0 * post_x + 0.36, 0.38, 0.24), (0.0, -y, GATE_BEAM + 0.12)), target, m['bronze'], bevel=(0.03, 2, 40.0)))
    frame.append(pk.part('SluiceDrum', pk.rod((-0.45, -y - 0.28, GATE_BEAM + 0.18), (0.45, -y - 0.28, GATE_BEAM + 0.18), 0.16, 16), target, m['iron'], smooth=40.0))
    width = 2.0 * (half + 0.14)
    gate = [pk.part('SluicePlate', pk.chipped((width, GATE_THICKNESS, GATE_HEIGHT), rng, chips=2, depth=0.02, center=(0.0, 0.0, 0.5 * GATE_HEIGHT)), target, m['iron'], bevel=(0.02, 2, 40.0))]
    for z in (0.15, GATE_HEIGHT - 0.15):
        gate.append(pk.part(f'SluiceBand{int(z * 100)}', pk.box((width + 0.02, GATE_THICKNESS + 0.04, 0.1), (0.0, 0.0, z)), target, m['bronze'], bevel=(0.015, 2, 40.0)))
        for k in range(7):
            x = -0.5 * width + (k + 0.5) * width / 7.0
            for face in (-1.0, 1.0):
                gate.append(pk.part(f'SluiceRivet{int(z * 100)}_{k}{int(face)}', pk.ellipsoid_bm((x, face * (GATE_THICKNESS * 0.5 + 0.025), z), (0.03, 0.02, 0.03), segments=8, rings=4), target, m['iron'], smooth=60.0))
    for side in (-1.0, 1.0):
        gate.append(pk.part(f'SluiceStem{int(side)}', pk.cylinder(0.055, GATE_STEM_TOP - GATE_HEIGHT + 0.05, 10, z=GATE_HEIGHT - 0.05), target, m['iron'], matrix=Matrix.Translation((side * GATE_STEM_X, 0.0, 0.0)), smooth=40.0))
        gate.append(pk.part(f'SluiceLug{int(side)}', pk.box((0.18, GATE_THICKNESS + 0.08, 0.16), (side * GATE_STEM_X, 0.0, GATE_HEIGHT + 0.03)), target, m['bronze'], bevel=(0.02, 2, 40.0)))
    return [kit.join(frame, 'SluiceFrame'), kit.join(gate, 'SluiceGate')]


def curb_bolts(m, target):
    loop, lengths = trace_outline()
    parts = []
    spacing = 1.25
    next_at = 0.4
    for p, at in zip(loop, lengths):
        if at < next_at:
            continue
        next_at += spacing
        n = trench_normal(p)
        x, y = p[0] + n[0] * F['CurbWidth'] * 0.5, p[1] + n[1] * F['CurbWidth'] * 0.5
        if any(abs(y - by) < F['BridgeHalfY'] + 0.2 and abs(x) < F['BridgeHalfX'] + 0.2 for by in F['BridgeYs']):
            continue
        bolt = pk.part(f'CurbBolt{len(parts)}', pk.cylinder(0.055, 0.035, 6, z=F['CurbTop'] - 0.005), target, m['iron'], smooth=30.0)
        bolt.matrix_world = Matrix.Translation(blender(x, y))
        parts.append(bolt)
    return parts


def build_steps(m, target, rng):
    edges, heights = F['ForgeRingEdges'], F['ForgeRingHeights']
    flights = [(3, heights[2], heights[4]), (1, heights[0], heights[2])]
    parts = []
    for axis in range(4):
        for groove, upper, lower in flights:
            outer = edges[groove] / SQRT2
            tread_depth = 0.36
            treads = 2
            for k in range(treads):
                top = upper - (upper - lower) * (k + 1) / (treads + 1)
                v0 = outer - tread_depth * (k + 1)
                v1 = outer - tread_depth * k + (0.05 if k == 0 else 0.0)
                half = 1.35 - 0.05 * k
                u0, u1 = 4.5 - half, 4.5 + half
                corners = [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]
                rotated = []
                for u, v in corners:
                    if axis == 1:
                        u, v = v, -u
                    elif axis == 2:
                        u, v = -u, -v
                    elif axis == 3:
                        u, v = -v, u
                    rotated.append(((u - v) / SQRT2, (u + v) / SQRT2))
                tread = kit.mesh_object(f'Step{axis}_{groove}_{k}', prism(rotated, SLAB_BOTTOM, top), target, m['block'])
                pk.tag(tread)
                kit.bevel(tread, 0.02, segments=2, angle=40.0)
                parts.append(tread)
    return parts


def build(target):
    rng = random.Random(53)
    m = {
        'slab': slab_material('BasaltSlab', 2048),
        'rim': iron_plate_material('RimIron', 2048),

        'fascia': rune_material('FasciaRunes', 'fascia', 2048),
        'block': block_material('ForgeBlock', 2048),
        'bronze': pk.brass_material('ForgeBronze', tone=(0.5, 0.33, 0.15), bake_size=2048),
        'iron': pk.iron_material('ForgeIron', tone=(0.06, 0.056, 0.052), rust=0.5, bake_size=1024),
        'chain': chain_material('ChainIron', 1024),
    }
    m['curb'] = m['block']
    objects = build_floor(m, target, rng)
    objects.append(kit.join(build_steps(m, target, rng), 'Steps'))
    objects += build_curb(m, target)
    objects.append(kit.join(curb_bolts(m, target), 'CurbBolts'))
    objects += build_trench(m, target, rng)
    fascia = build_fascia(m, target, rng)
    objects += [kit.join(parts, f'Fascia{side}') for side, parts in enumerate(fascia)]
    objects.append(kit.join(build_anchors(m, target, rng), 'Anchors'))
    objects.append(kit.join(build_bridges(m, target, rng), 'Bridges'))
    objects += build_sluice(m, target, rng)
    objects.append(chain_link(m, target, rng))
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
    pk.unwrap_all(objects, {'ForgeBronze': 0.004, 'ForgeIron': 0.004, 'BasaltSlab': 0.002, 'CurbRunes': 0.002, 'FasciaRunes': 0.002, 'RimIron': 0.002, 'ForgeBlock': 0.002})
    for obj in objects:
        tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
        print('TRIS', obj.name, tris)
    if args.get('--preview'):
        pk.ground_preview(60.0)
        objects[-1].matrix_world = Matrix.Translation((20.0, 0.0, 1.0))
        kit.render(os.path.join(args['--preview'], 'forge_top.png'), objects[:-1], yaw=0.0, pitch=57.0, fill=1.05, size=(1280, 760), samples=48)
        kit.render(os.path.join(args['--preview'], 'forge_close.png'), objects[:-1], yaw=-20.0, pitch=35.0, fill=0.35, size=(1280, 760), samples=48, look_offset=(2.0, -6.0, 0.0))
        objects[-1].matrix_world = Matrix.Identity(4)
    if args.get('--out'):
        started = time.perf_counter()
        pk.export(objects, args['--out'], NAME, ao_distance=0.4)
        print(f'EXPORT {time.perf_counter() - started:.1f} s')


if __name__ == '__main__':
    main()
