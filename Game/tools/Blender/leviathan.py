import bmesh
import bpy
import math
import os
import random
import sys
from mathutils import Matrix, Vector, noise
from mathutils.bvhtree import BVHTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

NAME = 'Leviathan'


def tooth_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(p)
    streak = s.noise(s.stretch(p, (9.0, 9.0, 0.8)), 3.0, detail=3.0)
    base = s.mix(s.smooth(0.3, 0.72, streak), (0.44, 0.41, 0.31), (0.74, 0.72, 0.62))
    base = s.mix(s.mul(pk.cavity(s, 0.05), 0.75), base, (0.14, 0.1, 0.07))
    rough = s.remap(streak, 0.2, 0.8, 0.22, 0.44)
    return s.finish(base, rough, height=streak, bump=0.12, bump_distance=0.008, bake_size=bake_size)


def bone_material(name, bake_size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    up = s.smooth(0.35, 0.85, s.separate(s.geometry('Normal'))[2])
    large = s.noise(p, 0.9, detail=5.0, roughness=0.56)
    mid = s.noise(p, 4.6, detail=5.0, roughness=0.6)
    pits = s.noise(p, 26.0, detail=4.0, roughness=0.62)
    fibres = s.noise(s.stretch(p, (5.0, 0.7, 5.0)), 6.0, detail=3.0)
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.006, s.voronoi(p, 3.2, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.44, 0.6, s.noise(p, 1.3, detail=2.0)))
    base = s.mix(s.smooth(0.3, 0.72, large), (0.21, 0.195, 0.15), (0.47, 0.44, 0.355))
    base = s.mix(s.mul(s.smooth(0.42, 0.72, fibres), 0.34), base, (0.26, 0.23, 0.17))
    base = s.mix(s.mul(pk.cavity(s, 0.22), 0.9), base, (0.035, 0.032, 0.03))
    base = s.mix(s.mul(s.smooth(0.56, 0.78, pits), 0.5), base, (0.1, 0.09, 0.07))
    base = s.mix(s.mul(cracks, 0.7), base, (0.05, 0.045, 0.04))
    base = s.mix(s.mul(pk.wear(s, 0.03), 0.55), base, (0.68, 0.65, 0.55))
    drift = s.smooth(0.42, 0.66, s.noise(p, 2.2, detail=5.0, roughness=0.6))
    rime = s.smooth(0.5, 0.86, s.mul(up, s.add(0.02, s.mul(drift, 1.0))))
    base = s.mix(rime, base, (0.8, 0.86, 0.92))
    rough = s.remap(rime, 0.0, 1.0, 0.6, 0.84)
    height = s.add(s.sub(s.add(s.mul(mid, 0.45), s.mul(fibres, 0.14)), s.add(s.mul(cracks, 0.5), s.mul(pits, 0.1))), s.mul(rime, 0.26))
    return s.finish(base, rough, height=height, bump=0.5, bump_distance=0.03, bake_size=bake_size)


def icicle_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    tone = s.noise(p, 3.0, detail=3.0)
    streak = s.noise(s.stretch(p, (5.0, 5.0, 0.6)), 3.0, detail=2.0)
    base = s.mix(s.smooth(0.3, 0.75, s.add(s.mul(tone, 0.5), s.mul(streak, 0.5))), (0.05, 0.26, 0.36), (0.36, 0.68, 0.78))
    rough = s.remap(streak, 0.0, 1.0, 0.08, 0.24)
    return s.finish(base, rough, height=streak, bump=0.1, bump_distance=0.012, emission=s.mul(streak, 0.4), emission_color=(0.08, 0.42, 0.5), emission_strength=0.35, bake_size=bake_size)


def clear_ice_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    up = s.smooth(0.55, 0.92, s.separate(s.geometry('Normal'))[2])
    planes = s.mul(s.sub(1.0, s.smooth(0.0, 0.014, s.voronoi(p, 1.4, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.5, 0.64, s.noise(p, 1.2, detail=2.0)))
    bubbles = s.sub(1.0, s.smooth(0.0, 0.1, s.voronoi(p, 34.0, 'F1', 1.0, 'Distance')))
    edge = pk.wear(s, 0.035)
    tone = s.noise(p, 1.8, detail=3.0)
    base = s.mix(s.smooth(0.3, 0.8, tone), (0.07, 0.19, 0.27), (0.12, 0.29, 0.38))
    base = s.mix(s.mul(edge, 0.7), base, (0.42, 0.66, 0.78))
    base = s.mix(s.mul(bubbles, 0.25), base, (0.36, 0.56, 0.68))
    base = s.mix(s.mul(planes, 0.8), base, (0.55, 0.78, 0.88))
    frost = s.mul(up, s.smooth(0.58, 0.78, s.noise(p, 2.2, detail=3.0)))
    base = s.mix(frost, base, (0.74, 0.8, 0.88))
    rough = s.remap(frost, 0.0, 1.0, 0.04, 0.62)
    glow = s.mul(s.clamp01(s.add(s.mul(planes, 0.5), s.mul(edge, 0.15))), s.sub(1.0, frost))
    height = s.add(s.mul(planes, 0.4), s.mul(frost, 0.3))
    return s.finish(base, rough, height=height, bump=0.08, bump_distance=0.003, emission=glow, emission_color=(0.3, 0.7, 1.0), emission_strength=0.5, specular=0.8, bake_size=bake_size, translucency=0.5, thickness=0.6, thin=s.sub(1.0, s.mul(frost, 0.6)), transmission=1.0, ior=1.31, attenuation=((0.66, 0.86, 0.96), 0.9))


def loft(rings, cap_start=True, cap_end=True):
    bm = bmesh.new()
    vertices = [[bm.verts.new(co) for co in ring] for ring in rings]
    count = len(rings[0])
    for a, b in zip(vertices, vertices[1:]):
        for i in range(count):
            j = (i + 1) % count
            bm.faces.new((a[i], a[j], b[j], b[i]))
    if cap_start:
        bm.faces.new(list(reversed(vertices[0])))
    if cap_end:
        bm.faces.new(vertices[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def fang(base, tip, radius, curve, sides=7):
    base = Vector(base)
    tip = Vector(tip)
    curve = Vector(curve)
    axis = (tip - base)
    rings = []
    forward = axis.normalized()
    helper = Vector((1.0, 0.0, 0.0)) if abs(forward.x) < 0.9 else Vector((0.0, 1.0, 0.0))
    a = forward.cross(helper).normalized()
    b = forward.cross(a).normalized()
    for i in range(7):
        f = i / 6.0
        centre = base + axis * f + curve * (f * f)
        r = radius * (1.0 - f) ** 0.8 + 0.003
        rings.append([centre + a * (math.cos(2.0 * math.pi * k / sides) * r) + b * (math.sin(2.0 * math.pi * k / sides) * r * 0.82) for k in range(sides)])
    return loft(rings)


def roughen(obj, scale, amount):
    for vertex in obj.data.vertices:
        offset = noise.noise(vertex.co * scale) + 0.5 * noise.noise(vertex.co * scale * 2.3)
        vertex.co += vertex.normal * (offset * amount)


def path_loft(points, radii, flat=1.0, sides=10, bind=None):
    points = [Vector(p) for p in points]
    rings = []
    previous = None
    for i, p in enumerate(points):
        forward = (points[min(i + 1, len(points) - 1)] - points[max(i - 1, 0)]).normalized()
        if previous is None:
            helper = Vector((0.0, 1.0, 0.0)) if abs(forward.y) < 0.9 else Vector((1.0, 0.0, 0.0))
            side = forward.cross(helper).normalized()
        else:
            side = (previous - forward * previous.dot(forward)).normalized()
        previous = side
        other = forward.cross(side).normalized()
        r = radii[i]
        rings.append([p + side * (math.cos(2.0 * math.pi * k / sides) * r) + other * (math.sin(2.0 * math.pi * k / sides) * r * flat) for k in range(sides)])
    return loft(rings)


def smooth_curve(points, steps=4):
    points = [Vector(p) for p in points]
    out = []
    for i in range(len(points) - 1):
        p0 = points[max(i - 1, 0)]
        p1 = points[i]
        p2 = points[i + 1]
        p3 = points[min(i + 2, len(points) - 1)]
        for k in range(steps):
            t = k / steps
            out.append(0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t * t * t))
    out.append(points[-1])
    return out


def vertebra_shapes(y, z, scale, spine_height, rng, lean=0.35):
    shapes = []
    r = 0.25 * scale
    shapes.append(pk.lathe([(0.0, -0.17 * scale), (r * 1.08, -0.17 * scale), (r * 0.8, -0.08 * scale), (r * 0.74, 0.0), (r * 0.8, 0.08 * scale), (r * 1.08, 0.17 * scale), (0.0, 0.17 * scale)], 14))
    bmesh.ops.transform(shapes[-1], matrix=Matrix.Translation((0.0, y, z)) @ Matrix.Rotation(math.pi * 0.5, 4, 'X'), verts=shapes[-1].verts)
    top = z + r * 0.8
    shapes.append(pk.capsule_bm((0.15 * scale, y, top), (0.0, y + 0.03 * scale, top + 0.24 * scale), 0.07 * scale, 0.07 * scale, segments=8, rings=2))
    shapes.append(pk.capsule_bm((-0.15 * scale, y, top), (0.0, y + 0.03 * scale, top + 0.24 * scale), 0.07 * scale, 0.07 * scale, segments=8, rings=2))
    if spine_height > 0.0:
        spine = pk.capsule_bm((0.0, 0.0, 0.0), (0.0, lean * spine_height, spine_height), 0.2 * scale, 0.11 * scale, segments=10, rings=3)
        bmesh.ops.transform(spine, matrix=Matrix.Translation((0.0, y, top + 0.12 * scale)) @ Matrix.Diagonal((0.38, 1.0, 1.0, 1.0)), verts=spine.verts)
        shapes.append(spine)
    shapes.append(pk.capsule_bm((0.0, y, z + 0.02), (0.42 * scale, y + 0.05 * scale, z + 0.12 * scale), 0.07 * scale, 0.035 * scale, segments=8, rings=2))
    shapes.append(pk.capsule_bm((0.0, y, z + 0.02), (-0.42 * scale, y + 0.05 * scale, z + 0.12 * scale), 0.07 * scale, 0.035 * scale, segments=8, rings=2))
    return shapes


def spine_path():
    points = []
    for i in range(7):
        t = i / 6.0
        y = -7.4 + 1.9 * t
        points.append(Vector((0.25 * math.sin(t * 1.4), y, 1.75 + 0.85 * t)))
    for i in range(1, 15):
        t = i / 14.0
        y = -5.5 + 11.0 * t
        hump = math.sin(min(t * 1.12, 1.0) * math.pi)
        points.append(Vector((0.4 * math.sin(y * 0.33), y, 2.2 + 2.1 * hump)))
    for i in range(1, 19):
        t = i / 18.0
        points.append(Vector((2.3 * math.sin(t * math.pi * 0.95) + 0.4 * math.sin(5.5 * 0.33), 5.5 + 9.2 * t, 2.2 * (1.0 - t) ** 1.6 + 0.25 - 0.55 * t * t)))
    return points


def heading(points, i):
    a = points[max(i - 1, 0)]
    b = points[min(i + 1, len(points) - 1)]
    d = b - a
    return math.atan2(-d.x, d.y)


def placed(shapes, position, yaw, pitch=0.0):
    matrix = Matrix.Translation((position.x, position.y, 0.0)) @ Matrix.Rotation(yaw, 4, 'Z') @ Matrix.Rotation(pitch, 4, 'X')
    for bm in shapes:
        bmesh.ops.transform(bm, matrix=matrix, verts=bm.verts)
    return shapes


def flipper(m, target, rng, prefix, shoulder, side, scale, sweep, parts):
    s = scale
    shoulder = Vector(shoulder)
    elbow = shoulder + Vector((side * 0.75 * s, 0.95 * s, -0.75 * s))
    wrist = elbow + Vector((side * 0.38 * s, 1.25 * s, -0.42 * s))
    wrist.z = max(wrist.z, 0.12)
    humerus = [pk.capsule_bm(shoulder, elbow, 0.2 * s, 0.15 * s, segments=12, rings=4),
               pk.ellipsoid_bm(shoulder, (0.27 * s, 0.24 * s, 0.24 * s), segments=12, rings=8),
               pk.ellipsoid_bm(elbow, (0.2 * s, 0.19 * s, 0.17 * s), segments=12, rings=8)]
    parts.append(pk.sculpt(humerus, target, m['bone'], f'{prefix}Humerus', 0.03, smooth=3, smooth_factor=0.5, faces=2400, rng=rng))
    across = Vector((-(wrist - elbow).y, (wrist - elbow).x, 0.0)).normalized()
    for k, offset in enumerate((-0.11, 0.11)):
        a = elbow + across * (offset * s)
        b = wrist + across * (offset * 1.25 * s)
        parts.append(pk.part(f'{prefix}Fore{k}', pk.capsule_bm(a, b, (0.1 - 0.02 * k) * s, (0.085 - 0.015 * k) * s, segments=10, rings=3), target, m['bone'], rng=rng, smooth=50.0))
    forward = (wrist - elbow)
    forward.z = 0.0
    forward.normalize()
    base_angle = math.atan2(forward.y, forward.x)
    for d in range(5):
        spread = (d - 2.0) * 0.17 * sweep - 0.12 + rng.uniform(-0.05, 0.05)
        angle = base_angle + side * spread
        direction = Vector((math.cos(angle), math.sin(angle), 0.0))
        root = wrist + across * ((d - 2.0) * 0.1 * s)
        lengths = [0.82, 0.72, 0.62, 0.5] if d not in (0, 4) else [0.66, 0.56, 0.46]
        bend = rng.uniform(-0.12, 0.12)
        cursor = root.copy()
        radius = 0.1 * s
        for j, length in enumerate(lengths):
            direction = Matrix.Rotation(side * bend * (j + 1) * 0.5, 3, 'Z') @ direction
            tip = cursor + direction * (length * s)
            tip.z = max(0.07, cursor.z - 0.06 * s)
            bone = [pk.capsule_bm(cursor, tip, radius, radius * 0.82, segments=8, rings=3),
                    pk.ellipsoid_bm(cursor, (radius * 1.35, radius * 1.35, radius * 1.15), segments=8, rings=6)]
            parts.append(pk.part(f'{prefix}Digit{d}_{j}', pk.merge_bm(bone), target, m['bone'], rng=rng, smooth=50.0))
            cursor = tip + direction * (0.045 * s)
            radius *= 0.84
    return wrist


def ice_chunk(name, centre, radii, rng, target, material, points=40, flat_top=None, tilt=0.0):
    bm = bmesh.new()
    centre = Vector(centre)
    for _ in range(points):
        while True:
            v = Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0)))
            if v.length <= 1.0:
                break
        v = v.normalized() * (0.7 + 0.3 * rng.random())
        p = Vector((v.x * radii[0], v.y * radii[1], v.z * radii[2]))
        if flat_top is not None:
            p.z = min(p.z, flat_top * radii[2] + rng.uniform(-0.04, 0.04))
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=bm.verts[:])
    for vert in [v for v in bm.verts if not v.link_faces]:
        bm.verts.remove(vert)
    bmesh.ops.transform(bm, matrix=Matrix.Translation(centre) @ Matrix.Rotation(tilt, 4, 'Z'), verts=bm.verts)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    obj = kit.mesh_object(name, bm, target, material)
    kit.bevel(obj, 0.035, segments=2, angle=30.0)
    return obj


def ribcage(m, target, rng):
    parts = []
    sources = []
    path = spine_path()
    column = []
    body_start = 6
    body_end = 6 + 14
    for i, p in enumerate(path):
        yaw = heading(path, i)
        if i < body_start:
            t = i / body_start
            scale = 0.95 + 0.25 * t
            column += placed(vertebra_shapes(0.0, p.z, scale, 0.25 + 0.35 * t, rng), p, yaw)
        elif i <= body_end:
            t = (i - body_start) / (body_end - body_start)
            hump = math.sin(min(t * 1.12, 1.0) * math.pi)
            scale = 1.32 - 0.34 * t
            column += placed(vertebra_shapes(0.0, p.z, scale, 0.95 + 0.85 * hump, rng, lean=0.42), p, yaw)
            if 0 < i - body_start < 14:
                reach = 1.05 + 1.45 * hump
                root = -2.2 + 0.4 * t
                for side in (-1.0, 1.0):
                    if rng.random() < 0.07:
                        continue
                    arc = []
                    broken = rng.random() < 0.12
                    count = 8 if not broken else 5
                    for k in range(count):
                        f = k / 7.0
                        a = f * 2.72
                        r = reach * (1.0 + 0.05 * math.sin(f * 3.0))
                        local = Vector((side * (0.3 * scale + r * math.sin(a)), (0.42 + 0.22 * t) * f * f, p.z - (p.z - root) * 0.5 + (p.z - root) * 0.5 * math.cos(a)))
                        arc.append(Matrix.Translation((p.x, p.y, 0.0)) @ Matrix.Rotation(yaw, 4, 'Z') @ local)
                    line = smooth_curve(arc, steps=4)
                    radii = [0.15 * scale * (1.0 - 0.62 * (index / (len(line) - 1)) ** 0.9) + 0.014 for index in range(len(line))]
                    rib = path_loft(line, radii, flat=0.56, sides=10)
                    parts.append(pk.part(f'Rib{i}_{int(side)}', rib, target, m['bone'], rng=rng, smooth=50.0))
                    sources.extend([arc[2], arc[3]])
        else:
            t = (i - body_end) / (len(path) - 1 - body_end)
            scale = 0.98 - 0.74 * t
            column += placed(vertebra_shapes(0.0, p.z, scale, 0.75 * (1.0 - t) + 0.08, rng, lean=0.6), p, yaw, pitch=-0.12 * t)
    spine = pk.sculpt(column, target, m['bone'], 'Spine', 0.028, smooth=3, smooth_factor=0.45, faces=46000, rng=rng)
    roughen(spine, 7.0, 0.006)
    parts.append(spine)
    front = path[body_start + 2]
    front_yaw = heading(path, body_start + 2)
    rear = path[body_end - 2]
    rear_yaw = heading(path, body_end - 2)
    frame = lambda p, yaw, v: Matrix.Translation((p.x, p.y, 0.0)) @ Matrix.Rotation(yaw, 4, 'Z') @ Vector(v)
    for side in (-1.0, 1.0):
        blade = pk.ellipsoid_bm(frame(front, front_yaw, (side * 2.15, 0.2, 1.9)), (0.1, 0.62, 1.0), rotation=Vector((side * 0.16, 0.06, 1.0)).to_track_quat('Z', 'Y'), segments=16, rings=10)
        parts.append(pk.part(f'Scapula{int(side)}', blade, target, m['bone'], rng=rng, smooth=45.0))
        flipper(m, target, rng, f'Front{int(side)}', frame(front, front_yaw, (side * 2.15, 0.1, 1.05)), side, 1.28 if side > 0 else 1.2, 1.0, parts)
        hip = frame(rear, rear_yaw, (side * 1.0, 0.0, 1.35))
        parts.append(pk.part(f'Pelvis{int(side)}', pk.ellipsoid_bm(hip, (0.22, 0.55, 0.32), rotation=Vector((side * 0.4, 0.2, 1.0)).to_track_quat('Z', 'Y'), segments=12, rings=8), target, m['bone'], rng=rng, smooth=45.0))
        flipper(m, target, rng, f'Hind{int(side)}', hip + Vector((side * 0.4, 0.0, -0.25)), side, 0.72, 0.8, parts)
    for k in range(44):
        base = Vector(sources[rng.randrange(len(sources))]) + Vector((rng.uniform(-0.06, 0.06), rng.uniform(-0.06, 0.06), -0.04))
        length_ = rng.uniform(0.2, 0.7)
        parts.append(pk.part(f'Icicle{k}', pk.cone_bm(base, base + Vector((rng.uniform(-0.02, 0.02), rng.uniform(-0.02, 0.02), -length_)), rng.uniform(0.025, 0.065), sides=6), target, m['icicle'], rng=rng, smooth=45.0))
    return kit.join(parts, 'Ribcage')


def rib_ice(m, target, rng):
    path = spine_path()
    chunks = []
    tail = path[-8]
    chunks.append(ice_chunk('RibIceTail', (tail.x, tail.y, 0.55), (1.9, 2.6, 1.0), rng, target, m['ice'], points=48, flat_top=0.62, tilt=0.3))
    hip = path[18]
    chunks.append(ice_chunk('RibIceHip', (hip.x + 1.6, hip.y + 0.6, 0.45), (1.5, 1.7, 0.9), rng, target, m['ice'], points=40, flat_top=0.7, tilt=-0.4))
    front = path[8]
    for side, size in ((-1.0, 1.0), (1.0, 0.7)):
        centre = (front.x + side * 3.1, front.y + 3.0, 0.12)
        chunks.append(ice_chunk(f'RibIceSheet{int(side)}', centre, (1.2 * size + 0.4, 2.2 * size + 0.5, 0.34), rng, target, m['ice'], points=44, flat_top=0.55, tilt=rng.uniform(-0.6, 0.6)))
    for k in range(5):
        i = 8 + 2 * k
        p = path[i]
        side = -1.0 if k % 2 == 0 else 1.0
        chunks.append(ice_chunk(f'RibIceFoot{k}', (p.x + side * 2.2, p.y + 0.2, 0.25), (0.75, 0.9, 0.55), rng, target, m['ice'], points=28, flat_top=0.6, tilt=rng.uniform(0.0, 3.0)))
    return kit.join(chunks, 'RibIce')


def skull_ice(m, target, rng):
    chunks = [ice_chunk('SkullIceJaw', (0.0, -2.2, -1.35), (1.25, 1.1, 0.75), rng, target, m['ice'], points=40, flat_top=0.55, tilt=0.2),
              ice_chunk('SkullIceSide', (1.55, 0.4, -1.1), (0.8, 1.2, 0.7), rng, target, m['ice'], points=32, flat_top=0.6, tilt=-0.3)]
    return kit.join(chunks, 'SkullIce')



def bone_skull(m, target, rng):
    shapes = [
        pk.ellipsoid_bm((0.0, 0.85, 0.62), (1.34, 1.62, 1.02), segments=32, rings=20),
        pk.capsule_bm((0.0, 0.0, 0.44), (0.0, -2.75, 0.0), 0.9, 0.42, segments=24),
        pk.capsule_bm((0.92, 0.42, 1.14), (0.42, -1.45, 0.62), 0.3, 0.1),
        pk.capsule_bm((-0.92, 0.42, 1.14), (-0.42, -1.45, 0.62), 0.3, 0.1),
        pk.capsule_bm((1.32, 0.75, 0.04), (0.74, -1.65, -0.26), 0.25, 0.14),
        pk.capsule_bm((-1.32, 0.75, 0.04), (-0.74, -1.65, -0.26), 0.25, 0.14),
        pk.capsule_bm((0.0, 1.45, 1.42), (0.0, 2.45, 1.48), 0.44, 0.2),
        pk.capsule_bm((1.24, 1.1, -0.1), (1.2, 1.42, -0.95), 0.24, 0.2),
        pk.capsule_bm((-1.24, 1.1, -0.1), (-1.2, 1.42, -0.95), 0.24, 0.2),
        pk.capsule_bm((0.0, -2.5, 0.54), (0.0, -0.4, 1.16), 0.17, 0.16),
    ]
    carve = [
        pk.ellipsoid_bm((0.0, 0.7, 0.45), (1.02, 1.32, 0.72), segments=24, rings=14),
        pk.capsule_bm((0.0, 0.0, 0.3), (0.0, -2.35, -0.02), 0.62, 0.22, segments=18),
        pk.ellipsoid_bm((1.3, -0.12, 0.74), (0.72, 0.68, 0.56), segments=20, rings=12),
        pk.ellipsoid_bm((-1.3, -0.12, 0.74), (0.72, 0.68, 0.56), segments=20, rings=12),
        pk.ellipsoid_bm((1.34, 1.28, 0.22), (0.72, 0.56, 0.58), segments=20, rings=12),
        pk.ellipsoid_bm((-1.34, 1.28, 0.22), (0.72, 0.56, 0.58), segments=20, rings=12),
        pk.ellipsoid_bm((1.1, -1.42, 0.26), (0.72, 0.66, 0.27), segments=16, rings=10),
        pk.ellipsoid_bm((-1.1, -1.42, 0.26), (0.72, 0.66, 0.27), segments=16, rings=10),
        pk.ellipsoid_bm((0.8, -2.25, 0.2), (0.45, 0.28, 0.16), segments=12, rings=8),
        pk.ellipsoid_bm((-0.8, -2.25, 0.2), (0.45, 0.28, 0.16), segments=12, rings=8),
        pk.ellipsoid_bm((0.46, 1.15, 1.45), (0.25, 0.46, 0.5), segments=14, rings=8),
        pk.ellipsoid_bm((-0.46, 1.15, 1.45), (0.25, 0.46, 0.5), segments=14, rings=8),
        pk.ellipsoid_bm((0.24, -2.68, 0.3), (0.16, 0.42, 0.2)),
        pk.ellipsoid_bm((-0.24, -2.68, 0.3), (0.16, 0.42, 0.2)),
        pk.ellipsoid_bm((0.0, -0.9, -0.72), (0.9, 2.6, 0.54), segments=24, rings=12),
    ]
    upper = pk.sculpt(shapes, target, m['bone'], 'SkullUpper', 0.026, smooth=4, smooth_factor=0.5, faces=56000, rng=rng, carve=carve)
    roughen(upper, 2.6, 0.03)
    roughen(upper, 9.0, 0.008)
    hinge = Vector((0.0, 1.28, -0.9))
    open_jaw = Matrix.Translation(hinge) @ Matrix.Rotation(0.24, 4, 'X') @ Matrix.Translation(-hinge)
    jaw_shapes = [
        pk.capsule_bm((1.16, 1.38, -0.92), (0.56, -1.8, -1.34), 0.32, 0.2),
        pk.capsule_bm((-1.16, 1.38, -0.92), (-0.56, -1.8, -1.34), 0.32, 0.2),
        pk.capsule_bm((0.56, -1.8, -1.34), (0.0, -2.6, -1.42), 0.2, 0.22),
        pk.capsule_bm((-0.56, -1.8, -1.34), (0.0, -2.6, -1.42), 0.2, 0.22),
        pk.ellipsoid_bm((1.0, 0.65, -1.02), (0.22, 0.95, 0.44), segments=14, rings=10),
        pk.ellipsoid_bm((-1.0, 0.65, -1.02), (0.22, 0.95, 0.44), segments=14, rings=10),
    ]
    jaw_carve = [
        pk.ellipsoid_bm((1.0, 0.55, -1.02), (0.55, 0.58, 0.2), segments=14, rings=8),
        pk.ellipsoid_bm((-1.0, 0.55, -1.02), (0.55, 0.58, 0.2), segments=14, rings=8),
        pk.ellipsoid_bm((0.72, -0.9, -1.22), (0.5, 0.3, 0.08), segments=12, rings=8),
        pk.ellipsoid_bm((-0.72, -0.9, -1.22), (0.5, 0.3, 0.08), segments=12, rings=8),
    ]
    jaw = pk.sculpt(jaw_shapes, target, m['bone'], 'SkullJaw', 0.026, smooth=4, smooth_factor=0.5, faces=20000, rng=rng, carve=jaw_carve)
    jaw.data.transform(open_jaw)
    roughen(jaw, 2.6, 0.025)
    roughen(jaw, 9.0, 0.007)
    parts = [upper, jaw]
    upper_tree = BVHTree.FromObject(upper, bpy.context.evaluated_depsgraph_get())
    jaw_tree = BVHTree.FromObject(jaw, bpy.context.evaluated_depsgraph_get())
    lift = open_jaw.to_3x3() @ Vector((0.0, 0.0, 1.0))

    def ridge(tree, xs, y, start, direction, pick):
        best = None
        for x in xs:
            hit = tree.ray_cast(Vector((x, y, start)), Vector((0.0, 0.0, direction)), 6.0)[0]
            if hit is not None and (best is None or pick(hit.z, best.z)):
                best = hit
        return best

    for side in (-1.0, 1.0):
        for k in range(13):
            f = k / 12.0
            skip = rng.random() < 0.1
            y = -2.6 + 3.1 * f
            reach = 0.42 + 0.48 * min(max(1.0 + y / 2.75, 0.0), 1.0)
            spot = ridge(upper_tree, [side * reach * (0.35 + 0.7 * i / 23.0) for i in range(24)], y, -3.0, 1.0, lambda a, b: a < b)
            long_ = 1.0 if k in (1, 3, 6) else 0.5 + 0.34 * rng.random()
            if spot is not None and not skip:
                base = spot + Vector((side * -0.02, 0.0, 0.14))
                tip = base + Vector((side * 0.02, -0.08, -0.2 - (1.12 - 0.42 * f) * long_))
                parts.append(pk.part(f'SkullUpperTooth{int(side)}_{k}', fang(base, tip, 0.05 + 0.048 * long_, (0.0, 0.26, 0.0)), target, m['tooth'], rng=rng, smooth=50.0))
            yl = -2.35 + 2.9 * f
            line = 0.56 + (1.16 - 0.56) * min(max((yl + 1.8) / 3.18, 0.0), 1.0) if yl > -1.8 else 0.56 * min(max((yl + 2.6) / 0.8, 0.0), 1.0)
            low = ridge(jaw_tree, [side * max(line + (i / 15.0 - 0.5) * 0.6, 0.05) for i in range(16)], yl, 3.0, -1.0, lambda a, b: a > b)
            lower = 0.95 if k in (0, 2, 5) else 0.46 + 0.34 * rng.random()
            if low is not None and rng.random() > 0.08:
                base = low - lift * 0.13
                tip = base + lift * (0.18 + 0.82 * lower) + Vector((0.0, -0.05, 0.0))
                parts.append(pk.part(f'SkullLowerTooth{int(side)}_{k}', fang(base, tip, 0.05 + 0.046 * lower, open_jaw.to_3x3() @ Vector((0.0, -0.16, 0.0))), target, m['tooth'], rng=rng, smooth=50.0))
    for k in range(14):
        s_ = rng.choice((-1.0, 1.0))
        base = Vector((s_ * rng.uniform(0.6, 1.3), rng.uniform(-1.8, 1.0), -0.2 + rng.uniform(-0.05, 0.05)))
        parts.append(pk.part(f'SkullIcicle{k}', pk.cone_bm(base, base + Vector((0.0, 0.0, -rng.uniform(0.12, 0.4))), rng.uniform(0.02, 0.05), sides=6), target, m['icicle'], rng=rng, smooth=45.0))
    return kit.join(parts, 'Skull')


def build(target):
    m = {
        'tooth': tooth_material('LeviTooth'),
        'bone': bone_material('LeviBone'),
        'icicle': icicle_material('LeviIcicle'),
        'ice': clear_ice_material('LeviIce'),
    }
    return [ribcage(m, target, random.Random(1002)), bone_skull(m, target, random.Random(1003)), rib_ice(m, target, random.Random(1004)), skull_ice(m, target, random.Random(1005))]


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    for obj in objects:
        print(f'{obj.name}: {len(obj.data.polygons)} faces')
    pk.unwrap_all(objects, {'LeviTooth': 0.008, 'LeviBone': 0.003, 'LeviIcicle': 0.01, 'LeviIce': 0.01}, angle=52.0)
    if args.get('--preview'):
        os.makedirs(args['--preview'], exist_ok=True)
        kit.environment('forest.exr', strength=0.8)
        objects[1].hide_render = True
        kit.render(os.path.join(args['--preview'], 'bones_cage.png'), [objects[0]], yaw=30.0, pitch=25.0, fill=0.9, size=(1200, 720), samples=48)
        objects[0].hide_render = True
        objects[1].hide_render = False
        kit.render(os.path.join(args['--preview'], 'bones_skull.png'), [objects[1]], yaw=60.0, pitch=10.0, fill=0.75, size=(1000, 700), samples=48)
        for obj in objects:
            obj.hide_render = False
    if args.get('--out'):
        x = 0.0
        for obj in objects:
            obj.matrix_world = Matrix.Translation((x, 0.0, 0.0))
            x += 40.0
        pk.export(objects, args['--out'], NAME, ao_distance=0.45)


if __name__ == '__main__':
    main()
