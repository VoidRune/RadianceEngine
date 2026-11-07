import bmesh
import bpy
import math
import numpy as np
import os
import random
import sys
from mathutils import Matrix, Vector, noise

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

NAME = 'Abyssal'
BODY = [
    (-5.7, 0.42, 1.34, 1.3), (-4.8, 0.62, 1.28, 1.22), (-4.0, 0.8, 1.2, 1.1), (-3.2, 0.9, 1.15, 1.03), (-2.4, 0.94, 1.14, 0.95),
    (-1.6, 0.9, 1.1, 0.88), (-0.8, 0.82, 1.08, 0.8), (0.0, 0.62, 1.03, 0.73), (0.8, 0.42, 0.94, 0.67), (1.6, 0.27, 0.88, 0.62),
    (2.4, 0.24, 0.85, 0.56), (3.2, 0.3, 0.82, 0.49), (4.0, 0.3, 0.76, 0.43), (4.8, 0.21, 0.66, 0.38), (5.6, 0.13, 0.56, 0.35),
    (6.4, 0.14, 0.5, 0.33), (7.2, 0.2, 0.46, 0.3), (7.8, 0.25, 0.41, 0.26), (8.3, 0.22, 0.34, 0.22), (8.7, 0.03, 0.27, 0.18),
    (8.98, -0.32, 0.19, 0.14), (9.14, -0.72, 0.11, 0.09), (9.16, -1.04, 0.015, 0.015),
]
HINGE = Vector((0.0, -5.75, -0.36))
JAW_REST = 0.36
UPPER = [
    (-9.3, 0.55, 0.1, 0.36), (-9.1, 0.85, 0.02, 0.58), (-8.7, 1.12, -0.06, 0.76), (-8.2, 1.27, -0.12, 0.88), (-7.6, 1.46, -0.2, 1.02),
    (-7.0, 1.6, -0.28, 1.16), (-6.4, 1.68, -0.36, 1.28), (-5.9, 1.76, -0.44, 1.36), (-5.4, 1.86, -0.78, 1.4), (-4.9, 1.9, -0.9, 1.28),
]
LOWER = [
    (-9.2, -0.36, -0.72, 0.4), (-8.85, -0.34, -0.84, 0.66), (-8.2, -0.36, -0.86, 0.86), (-7.5, -0.4, -0.9, 1.0),
    (-6.8, -0.42, -1.0, 1.12), (-6.2, -0.42, -1.06, 1.2), (-5.6, -0.36, -1.02, 1.24), (-5.2, -0.3, -0.88, 1.14),
]
SPINE_COUNT = 20


def smoothstep(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def body_at(y):
    if y <= BODY[0][0]:
        return BODY[0]
    for a, b in zip(BODY, BODY[1:]):
        if y <= b[0]:
            f = (y - a[0]) / (b[0] - a[0])
            f = f * f * (3.0 - 2.0 * f)
            return tuple(a[i] + (b[i] - a[i]) * f for i in range(4))
    return BODY[-1]


def body_surface(y, theta, extra=0.0):
    _, z, hh, hw = body_at(y)
    s, c = math.sin(theta), math.cos(theta)
    p = 2.0 / 2.3
    return Vector((math.copysign(abs(s) ** p, s) * (hw + extra), y, z + math.copysign(abs(c) ** p, c) * (hh + extra)))


def smooth_curve(points, steps=6):
    points = [Vector(p) for p in points]
    out = []
    for i in range(len(points) - 1):
        p0, p1, p2, p3 = points[max(i - 1, 0)], points[i], points[i + 1], points[min(i + 2, len(points) - 1)]
        for k in range(steps):
            t = k / steps
            out.append(0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t * t * t))
    out.append(points[-1])
    return out


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


def tube(points, radii, flat=1.0, sides=10, up=None):
    points = [Vector(p) for p in points]
    rings = []
    side = None
    for i, p in enumerate(points):
        forward = (points[min(i + 1, len(points) - 1)] - points[max(i - 1, 0)]).normalized()
        if side is None:
            helper = Vector(up) if up else (Vector((0.0, 0.0, 1.0)) if abs(forward.z) < 0.9 else Vector((1.0, 0.0, 0.0)))
            side = forward.cross(helper).normalized()
        else:
            side = (side - forward * side.dot(forward)).normalized()
        other = forward.cross(side).normalized()
        r = radii[i] if isinstance(radii, (list, tuple)) else radii
        rings.append([p + side * (math.cos(2.0 * math.pi * k / sides) * r) + other * (math.sin(2.0 * math.pi * k / sides) * r * flat) for k in range(sides)])
    return loft(rings)


def taper(count, start, end, power=1.0):
    return [start + (end - start) * (i / max(count - 1, 1)) ** power for i in range(count)]


def fang(base, tip, radius, curve, sides=7):
    base, tip, curve = Vector(base), Vector(tip), Vector(curve)
    points = [base + (tip - base) * (i / 7.0) + curve * ((i / 7.0) ** 2) for i in range(8)]
    return tube(points, [radius * (1.0 - i / 7.0) ** 0.72 + 0.003 for i in range(8)], flat=0.82, sides=sides)




def body_high_material(name):
    s = kit.Shader(name)
    p = s.coords('Object')
    data = s.node('ShaderNodeAttribute', attribute_name='LeviData', attribute_type='GEOMETRY').outputs['Color']
    crack, tone, edge = s.separate(data)
    fine = s.noise(p, 9.0, detail=5.0, roughness=0.62)
    grit = s.noise(p, 44.0, detail=4.0, roughness=0.6)
    base = s.mix(tone, (0.022, 0.03, 0.038), (0.075, 0.098, 0.12))
    base = s.mix(s.mul(s.smooth(0.36, 0.7, fine), 0.6), base, (0.13, 0.16, 0.19))
    base = s.mix(s.mul(s.smooth(0.56, 0.78, grit), 0.45), base, (0.012, 0.016, 0.02))
    base = s.mix(s.mul(edge, 0.66), base, (0.2, 0.24, 0.275))
    base = s.mix(s.clamp01(crack), base, (0.003, 0.004, 0.006))
    rough = s.remap(s.add(s.mul(fine, 0.6), s.mul(tone, 0.4)), 0.2, 0.8, 0.4, 0.7)
    height = s.add(s.mul(fine, 0.5), s.mul(grit, 0.16))
    return s.finish(base, rough, height=height, bump=0.5, bump_distance=0.012, bake_size=2048)


def low_body_material(name):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    return material


def tendril_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    rings = s.noise(s.stretch(p, (1.2, 2.4, 1.2)), 2.4, detail=3.0)
    grit = s.noise(p, 22.0, detail=4.0)
    base = s.mix(s.smooth(0.34, 0.7, rings), (0.022, 0.03, 0.038), (0.06, 0.078, 0.095))
    base = s.mix(s.mul(pk.cavity(s, 0.05), 0.85), base, (0.006, 0.008, 0.01))
    base = s.mix(s.mul(s.smooth(0.56, 0.8, grit), 0.3), base, (0.12, 0.15, 0.17))
    rough = s.remap(grit, 0.2, 0.8, 0.34, 0.58)
    return s.finish(base, rough, height=s.add(s.mul(rings, 0.4), s.mul(grit, 0.1)), bump=0.3, bump_distance=0.012, bake_size=bake_size)


def fin_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    mottle = s.noise(p, 2.6, detail=5.0, roughness=0.6)
    grit = s.noise(p, 18.0, detail=3.0)
    base = s.mix(s.smooth(0.3, 0.72, mottle), (0.018, 0.026, 0.034), (0.075, 0.095, 0.115))
    base = s.mix(s.mul(pk.cavity(s, 0.05), 0.9), base, (0.004, 0.006, 0.008))
    base = s.mix(s.mul(pk.wear(s, 0.012), 0.6), base, (0.22, 0.26, 0.29))
    rough = s.remap(grit, 0.2, 0.8, 0.36, 0.6)
    return s.finish(base, rough, height=s.add(s.mul(mottle, 0.3), s.mul(grit, 0.2)), bump=0.3, bump_distance=0.012, bake_size=bake_size)


def tooth_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    streak = s.noise(s.stretch(p, (9.0, 9.0, 0.7)), 3.0, detail=3.0)
    base = s.mix(s.smooth(0.3, 0.72, streak), (0.55, 0.52, 0.42), (0.86, 0.84, 0.75))
    rough = s.remap(streak, 0.2, 0.8, 0.2, 0.42)
    return s.finish(base, rough, height=streak, bump=0.1, bump_distance=0.006, bake_size=bake_size)


def gum_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    wet = s.noise(p, 7.0, detail=4.0)
    base = s.mix(s.smooth(0.3, 0.72, wet), (0.016, 0.012, 0.016), (0.075, 0.04, 0.045))
    rough = s.remap(wet, 0.2, 0.8, 0.18, 0.4)
    return s.finish(base, rough, height=wet, bump=0.3, bump_distance=0.012, bake_size=bake_size)




def table_at(table, y):
    if y <= table[0][0]:
        return table[0]
    for a, b in zip(table, table[1:]):
        if y <= b[0]:
            f = (y - a[0]) / (b[0] - a[0])
            return tuple(a[i] + (b[i] - a[i]) * f for i in range(4))
    return table[-1]


def upper_head_rings():
    rings = []
    for y, top, low, wide in UPPER:
        mid = low + (top - low) * 0.22
        ring = []
        for k in range(48):
            a = 2.0 * math.pi * k / 48.0
            s, c = math.sin(a), math.cos(a)
            x = math.copysign(abs(s) ** 0.6, s) * wide
            z = mid + (top - mid) * abs(c) ** 0.85 if c > 0.0 else mid - (mid - low) * abs(c) ** 0.5
            ring.append(Vector((x, y, z)))
        rings.append(ring)
    return loft(rings)


def lower_jaw_rings():
    rings = []
    for y, top, low, wide in LOWER:
        mid = top - (top - low) * 0.16
        ring = []
        for k in range(40):
            a = 2.0 * math.pi * k / 40.0
            s, c = math.sin(a), math.cos(a)
            x = math.copysign(abs(s) ** 0.6, s) * wide
            z = mid + (top - mid) * abs(c) ** 0.5 if c > 0.0 else mid - (mid - low) * abs(c) ** 0.85
            ring.append(Vector((x, y, z)))
        rings.append(ring)
    return loft(rings)


def body_rings():
    rings = []
    y_values = []
    for a, b in zip(BODY, BODY[1:]):
        for k in range(4):
            y_values.append(a[0] + (b[0] - a[0]) * k / 4.0)
    y_values.append(BODY[-1][0])
    centres = [Vector((0.0, y, body_at(y)[1])) for y in y_values]
    for i, y in enumerate(y_values):
        _, z, hh, hw = body_at(y)
        forward = (centres[min(i + 1, len(centres) - 1)] - centres[max(i - 1, 0)]).normalized()
        up = Vector((0.0, -forward.z, forward.y)).normalized()
        ring = []
        for k in range(36):
            a = 2.0 * math.pi * k / 36.0
            s, c = math.sin(a), math.cos(a)
            p = 2.0 / 2.3
            lift = 1.0 + 0.07 * math.exp(-(min(a, 2.0 * math.pi - a) / 0.3) ** 2)
            ring.append(centres[i] + Vector((math.copysign(abs(s) ** p, s) * hw, 0.0, 0.0)) + up * (math.copysign(abs(c) ** p, c) * hh * lift))
        rings.append(ring)
    return loft(rings)


ARMS = {
    'Pec': [(1.1, -5.1, -0.1), (2.05, -4.72, -0.98), (2.72, -3.82, -1.42), (3.28, -2.78, -1.66)],
    'Hind': [(0.56, 0.72, -0.3), (1.18, 1.36, -0.72), (1.56, 2.14, -0.98)],
}


def arm_shapes(chain, start, end):
    shapes = []
    line = smooth_curve(chain, steps=5)
    for i in range(len(line) - 1):
        f = i / (len(line) - 1)
        shapes.append(pk.capsule_bm(line[i], line[i + 1], start + (end - start) * f, start + (end - start) * (f + 1.0 / len(line)), segments=14, rings=3))
    return shapes


def core_shapes():
    shapes = [body_rings(), upper_head_rings()]
    for side in (-1.0, 1.0):
        shapes += [
            pk.capsule_bm((side * 0.98, -6.0, 1.34), (side * 0.86, -7.95, 0.9), 0.3, 0.1, segments=16),
            pk.capsule_bm((side * 0.5, -6.8, 1.5), (side * 0.3, -8.8, 0.8), 0.13, 0.06, segments=12),
            pk.capsule_bm((side * 1.26, -6.5, 0.1), (side * 0.56, -8.95, 0.12), 0.13, 0.05, segments=12),
            pk.ellipsoid_bm((side * 1.28, -5.1, 0.36), (0.36, 1.0, 1.05), segments=20, rings=12),
            pk.capsule_bm((side * 1.42, -5.0, 1.0), (side * 1.3, -4.7, -0.5), 0.18, 0.16, segments=14),
        ]
        for name, chain, start, end in (('Pec', ARMS['Pec'], 0.5, 0.11), ('Hind', ARMS['Hind'], 0.36, 0.08)):
            shapes += arm_shapes([(side * c[0], c[1], c[2]) for c in chain], start, end)
        shapes.append(pk.cone_bm((side * 2.05, -4.72, -0.98), (side * 2.32, -5.45, -1.5), 0.2, sides=8))
        shapes.append(pk.cone_bm((side * 1.18, 1.36, -0.72), (side * 1.34, 0.86, -1.08), 0.13, sides=8))
    return shapes


def jaw_shapes():
    shapes = [lower_jaw_rings()]
    for side in (-1.0, 1.0):
        shapes += [
            pk.capsule_bm((side * 1.14, -5.4, -0.42), (side * 0.42, -8.95, -0.42), 0.1, 0.05, segments=12),
            pk.ellipsoid_bm((side * 1.06, -5.75, -0.76), (0.34, 0.86, 0.38), segments=16, rings=10),
        ]
    return shapes


def carved(shapes, cutters, target, material, name, voxel, smooth):
    obj = pk.sculpt(shapes, target, material, name, voxel, smooth=0)
    for index, cutter in enumerate(cutters):
        tool = kit.mesh_object(f'{name}_Cut{index}', cutter, target)
        boolean = obj.modifiers.new('Carve', 'BOOLEAN')
        boolean.operation = 'DIFFERENCE'
        boolean.solver = 'EXACT'
        boolean.object = tool
        kit.apply_modifiers(obj)
        bpy.data.objects.remove(tool, do_unlink=True)
        print(name, 'cut', index, len(obj.data.polygons))
    pk.remesh(obj, voxel)
    smoother = obj.modifiers.new('Smooth', 'SMOOTH')
    smoother.factor = 0.5
    smoother.iterations = smooth
    kit.apply_modifiers(obj)
    return obj


def hashed(a, b, c=0.0):
    return (math.sin(a * 12.9898 + b * 78.233 + c * 37.719) * 43758.5453) % 1.0


def rock(position, amount=1.0):
    return amount * (0.034 * noise.noise(position * 2.1) + 0.017 * noise.noise(position * 5.7) + 0.007 * noise.noise(position * 14.0))


def cells(position, scale):
    distances, points = noise.voronoi(position * scale, distance_metric='DISTANCE')
    cell = points[0]
    return (distances[1] - distances[0]) * 0.5 / scale, hashed(cell.x, cell.y, cell.z)


def stone_plates(position, scale, stretch):
    bent = position + Vector((noise.noise(position * 0.9), noise.noise(position * 0.9 + Vector((5.2, 1.3, 7.7))), noise.noise(position * 0.9 + Vector((9.1, 3.3, 2.2))))) * 0.12
    d, tone = cells(Vector((bent.x, bent.y * stretch, bent.z)), scale)
    fine, _ = cells(bent, scale * 2.5)
    groove = 1.0 - smoothstep(0.0, 0.03, d)
    split = (1.0 - smoothstep(0.0, 0.015, fine)) * smoothstep(0.02, 0.2, noise.noise(position * 1.1)) * (1.0 - groove)
    ledge = (1.0 - smoothstep(0.03, 0.095, d)) * (1.0 - groove)
    height = 0.05 * smoothstep(0.0, 0.05, d) + 0.055 * tone + rock(position, 1.15) - 0.028 * split
    return height, max(groove, split * 0.85), 0.26 + 0.6 * tone, ledge * 0.85


def tile(along, across, length, width, seed):
    row = math.floor(along)
    shifted = across + 0.5 * (int(row) % 2)
    column = math.floor(shifted)
    fa, fb = along - row, shifted - column
    radius = min(length, width) * 0.34
    qa = abs(fa - 0.5) * length - (length * 0.5 - radius)
    qb = abs(fb - 0.5) * width - (width * 0.5 - radius)
    outside = math.hypot(max(qa, 0.0), max(qb, 0.0)) + min(max(qa, qb), 0.0) - radius
    return -outside, fa, hashed(row + seed, column)


def scale_cell(along, across, length, width):
    a, b = along + across, along - across
    ra, rb = math.floor(a), math.floor(b)
    fa, fb = a - ra, b - rb
    size = length * width / math.hypot(length, width)
    return min(fa, 1.0 - fa, fb, 1.0 - fb) * size, 0.5 * (fa + fb), hashed(ra + 2.0, rb)


def body_tiles(position, y):
    _, cz, hh, hw = body_at(y)
    bent = position + Vector((noise.noise(position * 1.7), noise.noise(position * 1.7 + Vector((5.2, 1.3, 7.7))), noise.noise(position * 1.7 + Vector((9.1, 3.3, 2.2))))) * 0.045
    theta = abs(math.atan2(bent.x / max(hw, 0.05), (bent.z - cz) / max(hh, 0.05))) + 0.05 * noise.noise(position * 1.3)
    radius = 0.5 * (hw + hh)
    late = smoothstep(-3.0, 8.0, y)
    if theta < 0.62:
        length, step, lift, band, seed = 0.72 + (0.34 - 0.72) * late, 0.31, 0.1, 0.62 - theta, 1.0
    elif theta < 1.55:
        length, step, lift, band, seed = 0.5 + (0.3 - 0.5) * late, 0.26, 0.07, min(theta - 0.62, 1.55 - theta), 2.0
    elif theta < 2.2:
        length, step, lift, band, seed = 0.3 + (0.2 - 0.3) * late, 1.45, 0.045, min(theta - 1.55, 2.2 - theta), 3.0
    else:
        length, step, lift, band, seed = 0.6 + (0.34 - 0.6) * late, 2.2, 0.05, theta - 2.2, 4.0
    if seed == 2.0:
        d, along, tone = scale_cell(bent.y / length, theta / step, length, step * radius)
    else:
        d, along, tone = tile(bent.y / length, theta / step, length, step * radius, seed)
    d = min(d, band * radius)
    groove = 1.0 - smoothstep(0.0, 0.026, d)
    ledge = (1.0 - smoothstep(0.026, 0.07, d)) * (1.0 - groove)
    height = 0.05 * smoothstep(0.0, 0.05, d) + lift * (along - 0.3) * smoothstep(0.0, 0.03, d) + 0.03 * tone + rock(position, 0.42)
    return height, groove, 0.2 + 0.62 * tone, ledge * 0.8


def plate_height(position, jaw=False):
    x, y, z = position.x, position.y, position.z
    if jaw:
        _, top, _, wide = table_at(LOWER, y)
        if z > top - 0.05 and abs(x) < wide * 0.93:
            return 0.0, 1.0, 0.0, 0.0
    elif y < -5.5:
        _, _, low, wide = table_at(UPPER, y)
        if z < low + 0.07 and abs(x) < wide * 0.93:
            return 0.0, 1.0, 0.0, 0.0
    _, cz, hh, hw = body_at(y)
    limb = (abs(x) / max(hw, 0.05)) ** 2.3 + (abs(z - cz) / max(hh, 0.05)) ** 2.3 > 1.56
    boundary = -3.95 + 0.55 * noise.noise(position * 0.8)
    if y < boundary:
        return stone_plates(position, 1.08 if y < -5.0 else 1.28, 0.72)
    if limb:
        return stone_plates(position, 3.1, 0.62)
    height, crack, tone, edge = body_tiles(position, y)
    seam = 1.0 - smoothstep(0.0, 0.03, y - boundary)
    return height, max(crack, seam), tone, edge


def displace(obj, jaw=False):
    mesh = obj.data
    count = len(mesh.vertices)
    data = np.zeros(count * 4, dtype=np.float32)
    coordinates = np.zeros(count * 3, dtype=np.float32)
    normals = np.zeros(count * 3, dtype=np.float32)
    mesh.vertices.foreach_get('co', coordinates)
    mesh.vertex_normals.foreach_get('vector', normals)
    for i in range(count):
        height, crack, tone, edge = plate_height(Vector(coordinates[i * 3: i * 3 + 3]), jaw)
        coordinates[i * 3: i * 3 + 3] += normals[i * 3: i * 3 + 3] * height
        data[i * 4: i * 4 + 4] = (crack, tone, edge, 1.0)
    mesh.vertices.foreach_set('co', coordinates)
    mesh.update()
    attribute = mesh.color_attributes.new('LeviData', 'FLOAT_COLOR', 'POINT')
    attribute.data.foreach_set('color', data)


def decimated(obj, name, faces):
    copy = obj.copy()
    copy.data = obj.data.copy()
    copy.name = name
    obj.users_collection[0].objects.link(copy)
    modifier = copy.modifiers.new('Decimate', 'DECIMATE')
    modifier.ratio = min(1.0, faces / max(len(copy.data.polygons), 1))
    kit.apply_modifiers(copy)
    copy.data.shade_smooth()
    if 'LeviData' in copy.data.color_attributes:
        copy.data.color_attributes.remove(copy.data.color_attributes['LeviData'])
    return copy




def solid(obj, thickness):
    modifier = obj.modifiers.new('Solid', 'SOLIDIFY')
    modifier.thickness = thickness
    modifier.offset = 0.0
    kit.apply_modifiers(obj)
    return obj


def leaf_fin(name, root, tip, sweep, width, m, target, rng, rays=7, spans=5):
    root, tip, sweep = Vector(root), Vector(tip), Vector(sweep).normalized()
    span = tip - root
    normal = span.cross(sweep).normalized()
    along = span.normalized()
    per = 5
    columns = rays * per + 1
    lengths = [rng.random() for _ in range(rays)]
    bm = bmesh.new()
    rows = []
    tips = []
    for r in range(spans + 1):
        v = r / spans
        row = []
        for i in range(columns):
            c = i / (columns - 1)
            q = (i % per) / per
            chord = width * (math.sin(math.pi * c ** 0.72) ** 0.8 + 0.012)
            spike = (1.0 - abs(2.0 * q - 1.0)) ** 1.5
            reach = chord * (0.72 + (0.2 + 0.34 * lengths[min(i // per, rays - 1)]) * (1.0 - spike) * v)
            pleat = 0.035 * math.cos(2.0 * math.pi * q) * v
            droop = 0.2 * math.sin(c * 3.0) * v
            point = root + span * c + sweep * (reach * v) + along * (reach * v * 0.42) + normal * (pleat + droop)
            row.append(bm.verts.new(point))
            if r == spans and i % per == 0 and 0 < i < columns - 1:
                tips.append((root + span * c, point))
        rows.append(row)
    for a, b in zip(rows, rows[1:]):
        for j in range(columns - 1):
            bm.faces.new((a[j], a[j + 1], b[j + 1], b[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    blade = pk.part(name, bm, target, m['fin'], rng=rng, smooth=70.0)
    solid(blade, 0.05)
    parts = [blade]
    for k, (start, end) in enumerate(tips):
        far = end + (end - start).normalized() * rng.uniform(0.1, 0.42)
        line = smooth_curve([start, start.lerp(end, 0.55) + normal * 0.05, far], steps=4)
        parts.append(pk.part(f'{name}_Ray{k}', tube(line, taper(len(line), 0.032, 0.005), flat=0.7, sides=6), target, m['tendril'], rng=rng, smooth=60.0))
    return parts


def crest(name, start, end, height, spikes, m, target, rng, lift=1.0, under=False):
    bm = bmesh.new()
    rows = []
    per = 4
    n = spikes * per
    direction = -1.0 if under else 1.0
    rays = []
    for i in range(n + 1):
        f = i / n
        y = start + (end - start) * f
        _, z, hh, _ = body_at(y)
        base = z + direction * hh * 0.86
        envelope = (math.sin(min(f * 1.1, 1.0) * math.pi) ** 0.55) * height
        spike = 1.0 - abs(2.0 * (i % per) / per - 1.0)
        pleat = 0.05 * math.cos(2.0 * math.pi * (i % per) / per)
        reach = envelope * (0.66 + 0.34 * spike ** 2)
        rake = 0.42 * lift * envelope
        row = [Vector((0.0, y, base)), Vector((pleat * 0.6, y + rake * 0.5, base + direction * reach * 0.55)), Vector((pleat, y + rake, base + direction * reach))]
        rows.append([bm.verts.new(p) for p in row])
        if i % per == per // 2:
            rays.append((row[0], row[2] + Vector((0.0, rake * 0.2, direction * envelope * 0.22))))
    for a, b in zip(rows, rows[1:]):
        for j in range(2):
            bm.faces.new((a[j], a[j + 1], b[j + 1], b[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    sheet = pk.part(name, bm, target, m['fin'], rng=rng, smooth=60.0)
    solid(sheet, 0.05)
    parts = [sheet]
    for k, (low, high) in enumerate(rays):
        parts.append(pk.part(f'{name}_Spine{k}', fang(low, high, 0.06, (0.0, 0.06, 0.0), sides=6), target, m['tendril'], rng=rng, smooth=60.0))
    return parts


TENDRILS = {
    'WhipA': [(0.95, -4.9, 1.5), (1.3, -3.7, 2.7), (1.7, -2.3, 3.25), (2.1, -0.9, 3.0), (2.5, 0.4, 3.2), (2.8, 1.8, 3.65), (3.05, 3.0, 3.5)],
    'WhipB': [(0.62, -1.2, 1.85), (1.0, 0.0, 2.4), (1.35, 1.4, 2.2), (1.7, 2.8, 2.35), (2.05, 4.2, 2.75), (2.3, 5.2, 2.7)],
    'RibbonA': [(1.05, -3.4, -0.66), (1.6, -2.6, -1.4), (2.2, -1.5, -1.3), (2.7, -0.4, -1.75), (3.2, 0.6, -1.7)],
    'RibbonB': [(0.62, 3.2, -0.42), (1.0, 4.1, -1.0), (1.4, 5.0, -1.0), (1.75, 5.9, -1.4), (2.0, 6.6, -1.35)],
}
TENDRIL_WIDTH = {'WhipA': 0.21, 'WhipB': 0.17, 'RibbonA': 0.15, 'RibbonB': 0.12}


def lures(m, target, rng):
    parts = []
    spots = []
    for side in (-1.0, 1.0):
        for k in range(11):
            f = k / 10.0
            y = -3.5 + 7.0 * f ** 0.92
            scale = 1.0 - 0.3 * f
            p = body_surface(y, side * 1.28, 0.07)
            spots.append(p)
            r = 0.115 * scale
            parts.append(pk.part(f'Bulb{int(side)}_{k}', pk.ellipsoid_bm(p, (r, r * 1.2, r), segments=12, rings=8), target, m['lure'], rng=rng, smooth=60.0))
            end = body_surface(y - 1.0, side * 0.92, 0.08)
            high = p.lerp(end, 0.5) + Vector((side * 0.22, 0.0, 0.42)) * scale
            arc = smooth_curve([p, p.lerp(high, 0.55) + Vector((side * 0.1, 0.1, 0.06)) * scale, high, end], steps=6)
            parts.append(pk.part(f'Stalk{int(side)}_{k}', tube(arc, taper(len(arc), 0.03 * scale, 0.016 * scale), sides=7), target, m['stalk'], rng=rng, smooth=60.0))
    return parts, spots


def teeth(m, target, rng, opening):
    parts = []
    big_upper = {0, 2, 5, 6, 9, 12, 15}
    big_lower = {0, 2, 4, 7, 10, 13}
    for side in (-1.0, 1.0):
        for k in range(17):
            f = k / 16.0
            y = -9.0 + 3.1 * f
            _, _, low, wide = table_at(UPPER, y)
            x = wide * 0.82
            scale = (1.0 if k in big_upper else 0.34 + 0.3 * rng.random()) * rng.uniform(0.86, 1.1)
            length = (1.08 - 0.5 * f) * scale
            base = Vector((side * x, y, low + 0.14))
            tip = base + Vector((side * rng.uniform(-0.02, 0.05), rng.uniform(-0.04, 0.04), -length - 0.14))
            parts.append(pk.part(f'Upper{int(side)}_{k}', fang(base, tip, 0.032 + 0.05 * scale, (side * -0.03, 0.14 + 0.12 * rng.random(), 0.0)), target, m['tooth'], rng=rng, smooth=45.0))
            if k % 2 == 1 and k < 16:
                inner = Vector((side * x * 0.8, y + 0.08, low + 0.06))
                parts.append(pk.part(f'UpperIn{int(side)}_{k}', fang(inner, inner + Vector((0.0, 0.03, -0.3 - 0.3 * rng.random())), 0.036, (0.0, 0.09, 0.0)), target, m['tooth'], rng=rng, smooth=45.0))
        for offset in (0.17, 0.38):
            _, _, low, wide = table_at(UPPER, -9.16)
            base = Vector((side * offset, -9.14 + offset * 0.2, low + 0.1))
            parts.append(pk.part(f'UpperFront{int(side)}_{offset}', fang(base, base + Vector((0.0, -0.1, -rng.uniform(0.55, 0.8))), 0.075, (0.0, 0.16, 0.0)), target, m['tooth'], rng=rng, smooth=45.0))
        for k in range(17):
            f = k / 16.0
            y = -8.84 + 3.05 * f
            _, top, _, wide = table_at(LOWER, y)
            x = wide * 0.8
            scale = (1.0 if k in big_lower else 0.34 + 0.3 * rng.random()) * rng.uniform(0.86, 1.1)
            length = (0.9 - 0.36 * f) * scale
            base = opening @ Vector((side * x, y, top - 0.1))
            tip = opening @ Vector((side * x * rng.uniform(1.0, 1.05), y + rng.uniform(-0.04, 0.04), top + length))
            parts.append(pk.part(f'Lower{int(side)}_{k}', fang(base, tip, 0.03 + 0.05 * scale, opening.to_3x3() @ Vector((0.0, -0.14, 0.03))), target, m['tooth'], rng=rng, smooth=45.0))
    return parts




def spine_joints():
    joints = []
    for i in range(SPINE_COUNT):
        f = i / (SPINE_COUNT - 1)
        y = -5.4 + f * 14.56
        y = y if y < 8.7 else 8.7 + (y - 8.7) * 0.92
        joints.append(Vector((0.0, y, body_at(y)[1])))
    return joints


def chain_points(points, count):
    line = smooth_curve(points, steps=8)
    return [line[int(i / (count - 1) * (len(line) - 1))] for i in range(count)]


def joint_table():
    table = []
    spine = spine_joints()
    for i, p in enumerate(spine):
        table.append((f'S{i:02d}', f'S{i - 1:02d}' if i else None, p))
    table.append(('Head', 'S00', Vector((0.0, -5.6, 0.45))))
    table.append(('Jaw', 'Head', HINGE.copy()))
    def nearest(y):
        return min(range(len(spine)), key=lambda k: abs(spine[k].y - y))
    for side, tag in ((-1.0, 'L'), (1.0, 'R')):
        pec = [Vector((side * c[0], c[1], c[2])) for c in ARMS['Pec']]
        parent = f'S{nearest(pec[0].y):02d}'
        for i, p in enumerate(pec[:3]):
            table.append((f'Pec{tag}{i}', parent, p))
            parent = f'Pec{tag}{i}'
        hind = [Vector((side * c[0], c[1], c[2])) for c in ARMS['Hind']]
        parent = f'S{nearest(hind[0].y):02d}'
        for i, p in enumerate(hind[:2]):
            table.append((f'Hind{tag}{i}', parent, p))
            parent = f'Hind{tag}{i}'
        for name, joints in (('WhipA', 5), ('WhipB', 4), ('RibbonA', 3), ('RibbonB', 3)):
            points = [(side * c[0], c[1], c[2]) for c in TENDRILS[name]]
            parent = f'S{nearest(points[0][1]):02d}'
            for i, p in enumerate(chain_points(points, joints)):
                table.append((f'{name}{tag}{i}', parent, p))
                parent = f'{name}{tag}{i}'
    return table


def build_rig(target):
    data = bpy.data.armatures.new('Abyssal')
    rig = bpy.data.objects.new('Abyssal', data)
    target.objects.link(rig)
    kit.select_only([rig])
    bpy.ops.object.mode_set(mode='EDIT')
    bones = {}
    for name, parent, position in joint_table():
        bone = data.edit_bones.new(name)
        bone.head = position
        bone.tail = position + Vector((0.0, 0.2, 0.0))
        if parent:
            bone.parent = bones[parent]
        bones[name] = bone
    bpy.ops.object.mode_set(mode='OBJECT')
    return rig


def spine_weights(y):
    spine = spine_joints()
    if y <= spine[0].y:
        return {'S00': 1.0}
    for i in range(len(spine) - 1):
        if y <= spine[i + 1].y:
            f = (y - spine[i].y) / (spine[i + 1].y - spine[i].y)
            f = f * f * (3.0 - 2.0 * f)
            return {f'S{i:02d}': 1.0 - f, f'S{i + 1:02d}': f}
    return {f'S{SPINE_COUNT - 1:02d}': 1.0}


def blend(a, b, f):
    out = {}
    for name, w in a.items():
        out[name] = out.get(name, 0.0) + w * (1.0 - f)
    for name, w in b.items():
        out[name] = out.get(name, 0.0) + w * f
    return out


def body_weight_rule(co):
    weights = spine_weights(co.y)
    if co.y < -4.7:
        weights = blend(weights, {'Head': 1.0}, smoothstep(-4.7, -5.45, co.y))
    side, tag = (-1.0, 'L') if co.x < 0.0 else (1.0, 'R')
    for name, chain, reach, joints in (('Pec', ARMS['Pec'], 0.76, 3), ('Hind', ARMS['Hind'], 0.52, 2)):
        points = [Vector((side * c[0], c[1], c[2])) for c in chain]
        best = 1e9
        place = 0.0
        for i in range(len(points) - 1):
            a, b = points[i], points[i + 1]
            axis = b - a
            t = max(0.0, min(1.0, (co - a).dot(axis) / axis.length_squared))
            d = (co - (a + axis * t)).length
            if d < best:
                best, place = d, i + t
        if best < reach and (abs(co.x) > abs(points[0].x) * 0.9 or place > 0.25):
            arm = {}
            index = min(int(place), joints - 1)
            f = place - int(place)
            arm[f'{name}{tag}{index}'] = 1.0 - f if index + 1 < joints else 1.0
            if index + 1 < joints:
                arm[f'{name}{tag}{index + 1}'] = f
            amount = smoothstep(reach, reach * 0.55, best) * smoothstep(0.0, 0.5, place + 0.15)
            weights = blend(weights, arm, amount)
    return weights


def rule_rigid(name):
    return lambda co: {name: 1.0}


def rule_chain(prefix, side_tag, joints, points, parent_rule=None):
    positions = chain_points(points, joints)
    def rule(co):
        best = 0
        best_d = 1e9
        for i, p in enumerate(positions):
            d = (co - p).length_squared
            if d < best_d:
                best, best_d = i, d
        names = {}
        if best + 1 < joints and (co - positions[best]).dot(positions[best + 1] - positions[best]) > 0.0:
            f = (co - positions[best]).dot(positions[best + 1] - positions[best]) / (positions[best + 1] - positions[best]).length_squared
            names[f'{prefix}{side_tag}{best}'] = 1.0 - min(f, 1.0)
            names[f'{prefix}{side_tag}{best + 1}'] = min(f, 1.0)
        elif best > 0:
            f = (co - positions[best - 1]).dot(positions[best] - positions[best - 1]) / (positions[best] - positions[best - 1]).length_squared
            names[f'{prefix}{side_tag}{best - 1}'] = 1.0 - min(f, 1.0)
            names[f'{prefix}{side_tag}{best}'] = min(f, 1.0)
        else:
            names[f'{prefix}{side_tag}0'] = 1.0
        return names
    return rule


def assign(obj, rule):
    groups = {}
    for vertex in obj.data.vertices:
        total = 0.0
        weights = rule(vertex.co)
        ordered = sorted(weights.items(), key=lambda entry: -entry[1])[:4]
        total = sum(w for _, w in ordered)
        for name, w in ordered:
            if w <= 0.0:
                continue
            group = groups.get(name)
            if group is None:
                group = obj.vertex_groups.new(name=name)
                groups[name] = group
            group.add([vertex.index], w / max(total, 1e-6), 'REPLACE')


def attach(obj, rig):
    obj.parent = rig
    obj.matrix_parent_inverse = Matrix.Identity(4)
    modifier = obj.modifiers.new('Armature', 'ARMATURE')
    modifier.object = rig




def build(target):
    rng = random.Random(4242)
    m = {
        'tendril': tendril_material('LeviTendril'),
        'fin': fin_material('LeviFin'),
        'tooth': tooth_material('LeviTooth'),
        'gum': gum_material('LeviGum'),
        'eye': pk.glow_material('LeviEye', color=(0.55, 0.88, 1.0), strength=0.8, base=(0.6, 0.86, 0.98)),
        'lure': pk.glow_material('LeviLure', color=(0.1, 0.95, 0.9), strength=6.0, base=(0.5, 1.0, 0.98)),
        'stalk': pk.glow_material('LeviStalk', color=(0.02, 0.5, 0.52), strength=0.5, base=(0.02, 0.08, 0.09)),
    }
    high_material = body_high_material('LeviBodyHigh')
    carve = []
    for side in (-1.0, 1.0):
        carve += [
            pk.ellipsoid_bm((side * 1.13, -7.2, 0.76), (0.3, 0.42, 0.25), segments=16, rings=10),
            pk.capsule_bm((side * 0.34, -9.3, 0.5), (side * 0.27, -8.8, 0.68), 0.075, 0.05, segments=8, rings=2),
        ]
    core = carved(core_shapes(), carve, target, high_material, 'CoreHigh', 0.045, 4)
    pk.remesh(core, 0.02)
    jaw = pk.sculpt(jaw_shapes(), target, high_material, 'JawHigh', 0.04, smooth=3, smooth_factor=0.5)
    pk.remesh(jaw, 0.02)
    opening = Matrix.Translation(HINGE) @ Matrix.Rotation(JAW_REST, 4, 'X') @ Matrix.Translation(-HINGE)
    displace(core)
    displace(jaw, True)
    jaw.data.transform(opening)
    jaw.data.update()
    core_low = decimated(core, 'CoreLow', 30000)
    jaw_low = decimated(jaw, 'JawLow', 5000)
    return dict(m=m, rng=rng, core=core, jaw=jaw, core_low=core_low, jaw_low=jaw_low, opening=opening, high_material=high_material)


def mouth_sheet(table, start, end, width, z_of, thick, rows=26, sides=26):
    rings = []
    for r in range(rows + 1):
        y = start + (end - start) * r / rows
        wide = table_at(table, y)[3] * width
        z = z_of(table_at(table, y))
        rings.append([Vector((math.cos(2.0 * math.pi * k / sides) * wide, y, z + math.sin(2.0 * math.pi * k / sides) * thick)) for k in range(sides)])
    return loft(rings)


def dress(state, target, rig):
    m, rng, opening = state['m'], state['rng'], state['opening']
    parts = []
    parts.append(pk.part('Palate', mouth_sheet(UPPER, -9.06, -5.5, 0.93, lambda row: row[2] + 0.03, 0.05), target, m['gum'], rng=rng, smooth=60.0))
    parts.append(pk.part('Throat', pk.ellipsoid_bm((0.0, -5.5, -0.6), (1.16, 0.95, 0.62), segments=20, rings=12), target, m['gum'], rng=rng, smooth=60.0))
    for side in (-1.0, 1.0):
        rim = [Vector((side * table_at(UPPER, y)[3] * 0.84, y, table_at(UPPER, y)[2] + 0.04)) for y in (-9.0 + 3.1 * i / 30.0 for i in range(31))]
        parts.append(pk.part(f'GumUp{int(side)}', tube(rim, taper(len(rim), 0.11, 0.16), flat=0.62, sides=10), target, m['gum'], rng=rng, smooth=60.0))
    for obj in parts:
        assign(obj, rule_rigid('Head'))
    jaw_parts = []
    floor = mouth_sheet(LOWER, -8.95, -5.4, 0.93, lambda row: row[1] - 0.03, 0.05)
    tongue = pk.ellipsoid_bm((0.0, -6.9, -0.4), (0.5, 1.75, 0.12), segments=16, rings=10)
    for mesh in (floor, tongue):
        bmesh.ops.transform(mesh, matrix=opening, verts=mesh.verts)
    jaw_parts.append(pk.part('MouthFloor', floor, target, m['gum'], rng=rng, smooth=60.0))
    jaw_parts.append(pk.part('Tongue', tongue, target, m['gum'], rng=rng, smooth=60.0))
    for side in (-1.0, 1.0):
        rim = [opening @ Vector((side * table_at(LOWER, y)[3] * 0.82, y, table_at(LOWER, y)[1] - 0.03)) for y in (-8.84 + 3.05 * i / 30.0 for i in range(31))]
        jaw_parts.append(pk.part(f'GumLow{int(side)}', tube(rim, taper(len(rim), 0.1, 0.15), flat=0.62, sides=10), target, m['gum'], rng=rng, smooth=60.0))
    for obj in jaw_parts:
        assign(obj, rule_rigid('Jaw'))
    tooth_parts = teeth(m, target, rng, opening)
    for obj in tooth_parts:
        assign(obj, rule_rigid('Jaw' if obj.name.startswith('Lower') else 'Head'))
    eye_parts = [pk.part(f'Eye{int(side)}', pk.ellipsoid_bm((side * 1.0, -7.2, 0.76), (0.17, 0.23, 0.16), segments=16, rings=12), target, m['eye'], rng=rng, smooth=60.0) for side in (-1.0, 1.0)]
    for obj in eye_parts:
        assign(obj, rule_rigid('Head'))
    fins = []
    for side, tag in ((-1.0, 'L'), (1.0, 'R')):
        elbow = Vector((side * ARMS['Pec'][1][0], ARMS['Pec'][1][1], ARMS['Pec'][1][2]))
        for obj in leaf_fin(f'PecFin{tag}', elbow, (side * 4.25, -0.75, -2.1), (-side * 0.62, 0.72, 0.26), 1.45, m, target, rng, rays=9):
            assign(obj, lambda co, t=tag: blend({f'Pec{t}1': 1.0}, {f'Pec{t}2': 1.0}, smoothstep(-4.2, -2.8, co.y)))
            fins.append(obj)
        knee = Vector((side * ARMS['Hind'][1][0], ARMS['Hind'][1][1], ARMS['Hind'][1][2]))
        for obj in leaf_fin(f'HindFin{tag}', knee, (side * 2.35, 4.1, -1.32), (-side * 0.6, 0.74, 0.22), 0.8, m, target, rng, rays=6):
            assign(obj, lambda co, t=tag: {f'Hind{t}1': 1.0})
            fins.append(obj)
    for name, start, end, height, spikes, under in (('MidFin', 0.6, 3.6, 0.62, 10, False), ('Crest', 5.0, 8.72, 1.25, 12, False), ('TailFringe', 4.4, 8.6, 0.6, 15, True)):
        for obj in crest(name, start, end, height, spikes, m, target, rng, under=under):
            assign(obj, lambda co: spine_weights(co.y))
            fins.append(obj)
    whips = []
    for side, tag in ((-1.0, 'L'), (1.0, 'R')):
        for name, joints in (('WhipA', 5), ('WhipB', 4), ('RibbonA', 3), ('RibbonB', 3)):
            points = [(side * c[0], c[1], c[2]) for c in TENDRILS[name]]
            line = smooth_curve(points, steps=9)
            width = TENDRIL_WIDTH[name]
            count = len(line) - 1
            radii = [width * (0.34 + 0.66 * min(i / (0.1 * count), 1.0) ** 0.5) * (1.0 - (i / count) ** 1.7) + 0.006 for i in range(count + 1)]
            obj = pk.part(f'{name}{tag}', tube(line, radii, flat=0.2, sides=12, up=(side, 0.0, 1.0)), target, m['tendril'], rng=rng, smooth=60.0)
            assign(obj, rule_chain(name, tag, joints, points))
            whips.append(obj)
    lure_parts, _ = lures(m, target, rng)
    for obj in lure_parts:
        assign(obj, lambda co: spine_weights(co.y))
    assign(state['core_low'], body_weight_rule)
    assign(state['jaw_low'], rule_rigid('Jaw'))
    return parts + jaw_parts + tooth_parts + eye_parts + fins + whips + lure_parts


def body_bake(state, folder):
    core, jaw, core_low, jaw_low = state['core'], state['jaw'], state['core_low'], state['jaw_low']
    scene = bpy.context.scene
    size = 2048
    high = kit.join([core, jaw], 'BodyHigh')
    low_material = low_body_material('LeviBody')
    for obj in (core_low, jaw_low):
        obj.data.materials.clear()
        obj.data.materials.append(low_material)
    tree = low_material.node_tree
    target = tree.nodes.new('ShaderNodeTexImage')
    tree.nodes.active = target
    high_tree = high.data.materials[0].node_tree
    emission = high_tree.nodes.new('ShaderNodeEmission')
    output = high_tree.nodes['Material Output']
    bsdf = high_tree.nodes['Principled BSDF']
    baked = {}

    def bake(kind, channel, samples, **options):
        image = bpy.data.images.new(f'Levi_{channel}', size, size, alpha=False, float_buffer=True)
        target.image = image
        scene.cycles.samples = samples
        kit.select_only([high, core_low, jaw_low], active=core_low)
        bpy.ops.object.bake(type=kind, use_selected_to_active=True, cage_extrusion=0.16, max_ray_distance=0.34, margin=16, margin_type='EXTEND', use_clear=True, **options)
        baked[channel] = kit.image_pixels(image)
        bpy.data.images.remove(image)

    for channel in ('base', 'rough'):
        high_tree.links.new(high_tree.nodes['OUT_' + channel].outputs[0], emission.inputs['Color'])
        high_tree.links.new(emission.outputs[0], output.inputs['Surface'])
        bake('EMIT', channel, 8)
    high_tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    bake('NORMAL', 'normal', 8, normal_space='TANGENT', normal_r='POS_X', normal_g='POS_Y', normal_b='POS_Z')
    scene.world.light_settings.distance = 0.6
    bake('AO', 'ao', 96)
    baked['normal'] = kit.sanitize_normals(baked['normal'], max_tilt=62.0)
    files = {}
    base_name = f'{NAME}_LeviBody'
    files['albedo'] = os.path.join(folder, base_name + '_albedo.png')
    kit.save_png(files['albedo'], baked['base'], srgb=True)
    files['normal'] = os.path.join(folder, base_name + '_normal.png')
    kit.save_png(files['normal'], baked['normal'])
    orm = np.zeros_like(baked['base'])
    orm[..., 0] = baked['ao'][..., 0]
    orm[..., 1] = baked['rough'][..., 0]
    files['orm'] = os.path.join(folder, base_name + '_orm.png')
    kit.save_png(files['orm'], orm)
    return low_material, files, high


def export_all(state, target, rig, parts, folder):
    os.makedirs(folder, exist_ok=True)
    pk.unwrap_all([state['core_low'], state['jaw_low']] + parts, {'LeviBodyHigh': 0.0025, 'LeviTendril': 0.006, 'LeviFin': 0.006, 'LeviTooth': 0.008, 'LeviGum': 0.008}, angle=62.0)
    low_material, files, high = body_bake(state, folder)
    target.objects.unlink(high)
    bpy.context.view_layer.update()
    trim = kit.join(parts, 'Trim')
    materials = [slot.material for slot in trim.material_slots if slot.material]
    baked = {}
    emissive = {}
    for material in materials:
        made = kit.bake_material(material, [trim], folder, NAME, ao_distance=0.35)
        baked[material.name] = kit.baked_material(material, made)
        if 'emit' in made:
            emissive[baked[material.name].name] = (tuple(material['emit_color']), float(material['emit_strength']))
            emissive[material.name] = emissive[baked[material.name].name]
    for slot in trim.material_slots:
        slot.material = baked[slot.material.name]
    body = kit.baked_material(low_material, files)
    for obj in (state['core_low'], state['jaw_low']):
        obj.data.materials[0] = body
    final = kit.join([trim, state['core_low'], state['jaw_low']], 'Leviathan')
    attach(final, rig)
    kit.export_gltf([final], os.path.join(folder, NAME + '.gltf'), emissive, {}, rig, False, {}, {})
    print('FACES', sum(len(p.vertices) - 2 for p in final.data.polygons), 'SLOTS', [slot.material.name for slot in final.material_slots])


def preview(state, target, parts, folder):
    os.makedirs(folder, exist_ok=True)
    scene = bpy.context.scene
    scene.view_settings.view_transform = 'Standard'
    tree = scene.world.node_tree
    tree.nodes.clear()
    background = tree.nodes.new('ShaderNodeBackground')
    background.inputs['Color'].default_value = (0.05, 0.065, 0.08, 1.0)
    background.inputs['Strength'].default_value = 1.0
    tree.links.new(background.outputs[0], tree.nodes.new('ShaderNodeOutputWorld').inputs['Surface'])
    for energy, rotation, tint in ((4.5, (0.75, 0.0, -0.75), (0.9, 0.95, 1.0)), (0.9, (1.2, 0.0, 2.4), (0.55, 0.75, 1.0))):
        lamp = bpy.data.objects.new('Lamp', bpy.data.lights.new('Lamp', 'SUN'))
        lamp.data.energy = energy
        lamp.data.color = tint
        lamp.data.angle = math.radians(14.0)
        lamp.rotation_euler = rotation
        target.objects.link(lamp)
    state['core_low'].hide_render = True
    state['jaw_low'].hide_render = True
    shown = [state['core'], state['jaw']] + parts
    for name, yaw, pitch, fill, size, look in (('ref', 36.0, 28.0, 0.62, (1400, 760), (0.0, -1.5, 0.0)), ('side', 90.0, 2.0, 0.82, (1500, 620), (0.0, 0.0, 0.0)), ('top', 90.0, 87.0, 0.82, (1500, 620), (0.0, 0.0, 0.0)), ('head', 52.0, 14.0, 0.3, (1100, 760), (0.0, -6.9, 0.2)), ('front', 12.0, 8.0, 0.36, (900, 760), (0.0, -6.0, 0.2)), ('tail', 120.0, 12.0, 0.34, (1100, 700), (0.0, 6.6, 0.0))):
        kit.render(os.path.join(folder, f'{name}.png'), shown, yaw=yaw, pitch=pitch, fill=fill, size=size, samples=40, look_offset=look)


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    state = build(target)
    rig = build_rig(target)
    parts = dress(state, target, rig)
    print('HIGH', len(state['core'].data.polygons), len(state['jaw'].data.polygons), 'LOW', len(state['core_low'].data.polygons), len(state['jaw_low'].data.polygons), 'PARTS', sum(len(p.data.polygons) for p in parts), 'JOINTS', len(joint_table()))
    if args.get('--preview'):
        preview(state, target, parts, args['--preview'])
    if args.get('--out'):
        export_all(state, target, rig, parts, args['--out'])


if __name__ == '__main__':
    main()
