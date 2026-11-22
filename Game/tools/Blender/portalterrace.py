import bpy
import bmesh
import math
import os
import random
import sys
import numpy as np
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

HEIGHT = 1.0
STEPS = 6
RISE = HEIGHT / STEPS
TREAD = 0.56
STAIR_IN = 2.5
STAIR_OUT = STAIR_IN + (STEPS - 1) * TREAD
SECTOR = math.radians(40.0)
NOSE = 0.035
TREAD_DEPTH = 0.26
TOP = 3.8
WALL = 3.72
WALL_IN = 3.36
COPE_IN = 3.24
COPE = (0.17, 0.012)
KERB_TOP = HEIGHT + COPE[1] + 0.006
CAP = (-0.04, 0.07, 0.07)
PLINTH = (3.86, -0.45, 0.1)
COURSES = (0.1, 0.47, HEIGHT - COPE[0])
CHEEK = 0.44
CHEEK_FOOT = 0.62
KERB = 0.32
PEDESTAL = (0.84, 1.28, STAIR_OUT - 0.12)
SIDE_BOWL = (3.47, math.radians(90.0), 0.62, 0.78)
POSTS = ((TOP - 0.26, 'corner'), (TOP - 0.26, math.radians(112.0)))
PILASTERS = tuple(math.radians(a) for a in (78.0, 112.0, 146.0, 180.0))
PILASTER = (0.56, 0.17)
FLOOR_EDGE = COPE_IN + 0.05
BANNER_ANGLE = math.radians(62.0)
BOWLS = (0.47, 0.4)
ROD = 0.017
JOINT = 0.006
BEVEL = 0.022
STONE_SRGB = (0.44, 0.415, 0.395)
STONE = tuple(((c + 0.055) / 1.055) ** 2.4 for c in STONE_SRGB)
STONE_DARK = tuple(((c + 0.055) / 1.055) ** 2.4 for c in (0.2, 0.19, 0.182))
GOLD_TONE = (1.0, 0.74, 0.3)
INLAY = (0.022, 0.021, 0.025)
MARBLE_DARK = tuple(((c + 0.055) / 1.055) ** 2.4 for c in (0.105, 0.11, 0.125))
MARBLE_VEIN = tuple(((c + 0.055) / 1.055) ** 2.4 for c in (0.7, 0.69, 0.66))
MARBLE_BANDS = (1, 3)
FLOOR_RINGS = (0.34, 1.26, 1.86, STAIR_IN - 0.2)
FLOOR_COURSES = (3, 2, 2)
BED = 0.02
PAVER_JOINT = 0.005
ARCH_FOOT = (((-2.65, -0.1), (-0.6, 3.5)), ((0.6, -0.1), (2.55, 3.5)))


def polar(r, phi, z=0.0):
    return Vector((r * math.sin(phi), -r * math.cos(phi), z))


def radial_frame(phi, r, z=0.0):
    out = Vector((math.sin(phi), -math.cos(phi), 0.0))
    side = Vector((math.cos(phi), math.sin(phi), 0.0))
    frame = Matrix.Identity(4)
    frame.col[0] = side.to_4d()
    frame.col[1] = out.to_4d()
    frame.col[2] = Vector((0.0, 0.0, 1.0, 0.0))
    frame.col[3] = (out * r + Vector((0.0, 0.0, z))).to_4d()
    frame.col[0][3] = frame.col[1][3] = frame.col[2][3] = 0.0
    frame.col[3][3] = 1.0
    return frame


def face_frame(origin, right, up):
    right = Vector(right).normalized()
    up = Vector(up).normalized()
    normal = right.cross(up)
    frame = Matrix.Identity(4)
    frame.col[0] = right.to_4d()
    frame.col[1] = up.to_4d()
    frame.col[2] = normal.to_4d()
    frame.col[0][3] = frame.col[1][3] = frame.col[2][3] = 0.0
    frame.col[3] = Vector(origin).to_4d()
    frame.col[3][3] = 1.0
    return frame


def sector(r0, r1, a0, a1, z0, z1, step=0.14, top=None):
    count = max(1, int(math.ceil(abs(a1 - a0) * r1 / step)))
    bm = bmesh.new()
    rings = []
    for i in range(count + 1):
        a = a0 + (a1 - a0) * i / count
        za, zb = (z0, z1) if top is None else (z0, top(a))
        rings.append([bm.verts.new(polar(r, a, z)) for r, z in ((r0, za), (r1, za), (r1, zb), (r0, zb))])
    for i in range(count):
        a, b = rings[i], rings[i + 1]
        for k in range(4):
            bm.faces.new((a[k], b[k], b[(k + 1) % 4], a[(k + 1) % 4]))
    bm.faces.new(rings[0][::-1])
    bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def prism(points, z0, z1):
    bm = bmesh.new()
    low = [bm.verts.new((x, y, z0)) for x, y in points]
    high = [bm.verts.new((x, y, z1)) for x, y in points]
    bm.faces.new(low[::-1])
    bm.faces.new(high)
    count = len(points)
    for i in range(count):
        j = (i + 1) % count
        bm.faces.new((low[i], low[j], high[j], high[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def ring_prism(outer, inner, z0, z1):
    bm = bmesh.new()
    loops = []
    for points in (outer, inner):
        loops.append(([bm.verts.new((x, y, z0)) for x, y in points], [bm.verts.new((x, y, z1)) for x, y in points]))
    (ol, oh), (il, ih) = loops
    count = len(outer)
    for i in range(count):
        j = (i + 1) % count
        bm.faces.new((oh[i], oh[j], ih[j], ih[i]))
        bm.faces.new((ol[j], ol[i], il[i], il[j]))
        bm.faces.new((ol[i], ol[j], oh[j], oh[i]))
        bm.faces.new((il[j], il[i], ih[i], ih[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def inset(points, distance):
    count = len(points)
    area = sum(points[i][0] * points[(i + 1) % count][1] - points[(i + 1) % count][0] * points[i][1] for i in range(count))
    sign = 1.0 if area > 0.0 else -1.0
    result = []
    for i in range(count):
        p0, p1, p2 = Vector(points[i - 1]), Vector(points[i]), Vector(points[(i + 1) % count])
        d0 = (p1 - p0).normalized()
        d1 = (p2 - p1).normalized()
        n0 = Vector((-d0.y, d0.x)) * sign
        n1 = Vector((-d1.y, d1.x)) * sign
        bisector = (n0 + n1).normalized()
        scale = distance / max(bisector.dot(n1), 0.2)
        result.append(tuple(p1 + bisector * scale))
    return result


def stroke(points, width):
    left, right = [], []
    for i, p in enumerate(points):
        p = Vector(p)
        before = Vector(points[i - 1]) if i > 0 else None
        after = Vector(points[i + 1]) if i + 1 < len(points) else None
        normals = []
        for a, b in ((before, p), (p, after)):
            if a is not None and b is not None:
                d = (b - a).normalized()
                normals.append(Vector((-d.y, d.x)))
        n = (normals[0] + normals[-1]).normalized()
        scale = 0.5 * width / max(n.dot(normals[-1]), 0.35)
        left.append(tuple(p + n * scale))
        right.append(tuple(p - n * scale))
    return left + right[::-1]


def merge(parts):
    bm = bmesh.new()
    for part in parts:
        mesh = bpy.data.meshes.new('merge')
        part.to_mesh(mesh)
        part.free()
        bm.from_mesh(mesh)
        bpy.data.meshes.remove(mesh)
    return bm


def transformed(bm, matrix):
    bmesh.ops.transform(bm, matrix=matrix, verts=bm.verts)
    if matrix.to_3x3().determinant() < 0.0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    return bm


def chip(bm, rng, count, depth, keep_bottom=True, planes=None):
    if count <= 0:
        return bm
    low = Vector((min(v.co.x for v in bm.verts), min(v.co.y for v in bm.verts), min(v.co.z for v in bm.verts)))
    high = Vector((max(v.co.x for v in bm.verts), max(v.co.y for v in bm.verts), max(v.co.z for v in bm.verts)))
    center = (low + high) * 0.5
    corners = [v.co.copy() for v in bm.verts if not keep_bottom or v.co.z > center.z]
    for _ in range(count):
        if not corners:
            break
        corner = rng.choice(corners)
        direction = (corner - center)
        direction = Vector((direction.x * rng.uniform(0.5, 1.0), direction.y * rng.uniform(0.5, 1.0), direction.z * rng.uniform(0.6, 1.0))).normalized()
        point = corner - direction * depth * rng.uniform(0.6, 1.3)
        kit.cut(bm, point, direction)
        if planes is not None:
            planes.append((point, direction))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def settle(bm, rng, above, slope, followers=()):
    tops = [v for v in bm.verts if v.co.z > above]
    if not tops:
        return bm
    cx = sum(v.co.x for v in tops) / len(tops)
    cy = sum(v.co.y for v in tops) / len(tops)
    angle = rng.uniform(0.0, math.tau)
    grade = rng.uniform(0.3, 1.0) * slope
    gx, gy = math.cos(angle) * grade, math.sin(angle) * grade
    for v in tops:
        v.co.z += gx * (v.co.x - cx) + gy * (v.co.y - cy)
    for other in followers:
        for v in other.verts:
            v.co.z += gx * (v.co.x - cx) + gy * (v.co.y - cy)
    return bm


class Builder:
    def __init__(self, target, seed=11):
        self.target = target
        self.rng = random.Random(seed)
        self.groups = {}

    def add(self, group, bm, material, bevel=BEVEL, angle=32.0, local=True, segments=None):
        mesh = bpy.data.meshes.new(group)
        if bevel > 0.0:
            edges = [e for e in bm.edges if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(angle)]
            if edges:
                bmesh.ops.bevel(bm, geom=edges, offset=bevel, offset_type='OFFSET', segments=segments or (1 if group == 'Gold' else 2), profile=0.6, affect='EDGES', clamp_overlap=True)
        bm.normal_update()
        bm.to_mesh(mesh)
        bm.free()
        obj = bpy.data.objects.new(group, mesh)
        self.target.objects.link(obj)
        obj.data.materials.append(material)
        if local:
            pk.tag(obj, rng=self.rng)
        self.groups.setdefault(group, []).append(obj)
        return obj

    def finish(self):
        result = {}
        for group, objects in self.groups.items():
            joined = kit.join(objects, group) if len(objects) > 1 else objects[0]
            joined.name = group
            kit.smooth_by_angle(joined, 34.0)
            result[group] = joined
        return result


def stone_material(name='TerraceStone', size=4096):
    s = kit.Shader(name)
    p = pk.local(s)
    island = s.geometry('Random Per Island')
    _, _, nz = s.separate(s.geometry('Normal'))
    _, _, z = s.separate(s.coords('Object'))
    large = s.noise(p, 1.4, detail=4.0, roughness=0.55)
    mid = s.noise(p, 4.5, detail=6.0, roughness=0.58)
    fine = s.noise(p, 22.0, detail=4.0, roughness=0.6)
    cells = s.voronoi(p, 9.0, 'F1', 1.0, 'Distance')
    speck = s.sub(1.0, s.smooth(0.0, 0.16, s.voronoi(s.offset(p, (2.0, 5.0, 9.0)), 70.0, 'F1', 1.0, 'Distance')))
    speck = s.mul(speck, s.smooth(0.4, 0.62, s.noise(p, 9.0, detail=2.0)))
    tone = s.clamp01(s.add(s.mul(island, 0.3), s.add(s.mul(s.smooth(0.3, 0.7, large), 0.4), s.mul(mid, 0.35))))
    base = s.mix(tone, STONE_DARK, STONE)
    warm_block = s.mul(s.smooth(0.5, 0.56, island), s.sub(1.0, s.smooth(0.72, 0.78, island)))
    cool_block = s.mul(s.smooth(0.76, 0.8, island), s.sub(1.0, s.smooth(0.9, 0.93, island)))
    dark_block = s.smooth(0.92, 0.95, island)
    pale_block = s.sub(1.0, s.smooth(0.1, 0.16, island))
    base = s.mix(s.mul(warm_block, 0.75), base, (STONE[0] * 1.14, STONE[1] * 0.99, STONE[2] * 0.82))
    base = s.mix(s.mul(cool_block, 0.75), base, (STONE[0] * 0.86, STONE[1] * 0.91, STONE[2] * 1.05))
    base = s.mix(s.mul(dark_block, 0.7), base, (STONE[0] * 0.58, STONE[1] * 0.56, STONE[2] * 0.56))
    base = s.mix(s.mul(pale_block, 0.6), base, (STONE[0] * 1.2, STONE[1] * 1.18, STONE[2] * 1.13))
    blotch = s.smooth(0.55, 0.78, s.noise(s.offset(p, (13.0, 7.0, 3.0)), 2.2, detail=4.0))
    base = s.mix(s.mul(blotch, 0.35), base, (STONE[0] * 0.78, STONE[1] * 0.76, STONE[2] * 0.72))
    base = s.mix(s.mul(speck, 0.5), base, (STONE_DARK[0] * 0.6, STONE_DARK[1] * 0.58, STONE_DARK[2] * 0.56))
    base = s.mix(s.mul(s.smooth(0.55, 0.75, fine), 0.18), base, (STONE[0] * 1.15, STONE[1] * 1.13, STONE[2] * 1.1))
    edge = pk.wear(s, 0.03)
    chips = s.mul(edge, s.smooth(0.4, 0.6, s.noise(p, 6.5, detail=4.0)))
    base = s.mix(s.mul(chips, 0.6), base, (STONE[0] * 1.34, STONE[1] * 1.32, STONE[2] * 1.28))
    dirt = pk.cavity(s, 0.16)
    base = s.mix(s.mul(dirt, 0.8), base, (0.03, 0.026, 0.022))
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.03, s.voronoi(p, 2.2, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.57, 0.68, s.noise(p, 1.4, detail=3.0)))
    hair = s.mul(s.sub(1.0, s.smooth(0.0, 0.014, s.voronoi(s.offset(p, (3.0, 9.0, 5.0)), 5.5, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.67, 0.77, s.noise(s.offset(p, (7.0, 2.0, 11.0)), 2.0, detail=3.0)))
    pits = s.mul(s.sub(1.0, s.smooth(0.0, 0.2, s.voronoi(p, 48.0, 'F1', 1.0, 'Distance'))), s.smooth(0.45, 0.7, s.noise(p, 5.0, detail=2.0)))
    base = s.mix(s.mul(cracks, 0.78), base, (0.022, 0.02, 0.018))
    base = s.mix(s.mul(hair, 0.4), base, (0.04, 0.036, 0.032))
    base = s.mix(s.mul(pits, 0.45), base, (0.05, 0.045, 0.04))
    low = s.mul(s.sub(1.0, s.smooth(-0.1, 0.45, z)), s.smooth(0.35, 0.65, s.noise(p, 2.4, detail=4.0)))
    base = s.mix(s.mul(low, 0.5), base, (0.07, 0.065, 0.05))
    up = s.smooth(0.45, 0.85, nz)
    moss_field = s.smooth(0.56, 0.7, s.noise(p, 2.1, detail=6.0, roughness=0.65))
    moss = s.clamp01(s.mul(s.add(s.mul(dirt, 1.4), s.mul(low, 0.8)), s.mul(moss_field, s.add(0.35, s.mul(up, 0.65)))))
    moss_tone = s.mix(s.noise(p, 30.0, detail=2.0), (0.05, 0.075, 0.025), (0.12, 0.15, 0.04))
    base = s.mix(s.mul(moss, 0.85), base, moss_tone)
    rough = s.remap(fine, 0.3, 0.7, 0.66, 0.93)
    rough = s.lerp(s.mul(s.mul(up, s.sub(1.0, dirt)), 0.7), rough, 0.56)
    rough = s.lerp(s.mul(blotch, 0.5), rough, 0.9)
    rough = s.lerp(s.mul(chips, 0.6), rough, 0.68)
    rough = s.lerp(s.clamp01(s.add(cracks, pits)), rough, 0.95)
    rough = s.lerp(moss, rough, 0.95)
    facets = s.smooth(0.0, 0.32, s.voronoi(s.offset(p, (1.0, 2.0, 3.0)), 3.2, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))
    gouge = s.smooth(0.0, 0.22, s.voronoi(s.offset(p, (6.0, 4.0, 1.0)), 7.5, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))
    height = s.add(s.add(s.mul(large, 0.4), s.mul(mid, 0.3)), s.add(s.mul(fine, 0.18), s.mul(s.smooth(0.0, 0.3, cells), 0.16)))
    height = s.add(height, s.add(s.mul(facets, 0.5), s.mul(gouge, 0.22)))
    height = s.sub(height, s.add(s.add(s.mul(chips, 0.7), s.mul(cracks, 0.8)), s.add(s.mul(hair, 0.4), s.add(s.mul(pits, 0.4), s.mul(speck, 0.12)))))
    return s.finish(base, rough, height=height, bump=1.0, bump_distance=0.028, bake_size=size)


def marble_material(name='PortalMarble', size=4096):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, nz = s.separate(s.geometry('Normal'))

    def vein(vector, scale, width, distortion, detail=5.0):
        n = s.noise(vector, scale, detail=detail, roughness=0.6, distortion=distortion)
        return s.sub(1.0, s.smooth(0.0, width, s.math('ABSOLUTE', s.sub(n, 0.5))))

    cloud = s.noise(p, 0.8, detail=4.0)
    smoke = s.noise(s.offset(p, (4.0, 1.0, 8.0)), 2.4, detail=4.0, distortion=1.1)
    base = s.mix(s.add(s.mul(cloud, 0.6), s.mul(smoke, 0.4)), (MARBLE_DARK[0] * 0.72, MARBLE_DARK[1] * 0.72, MARBLE_DARK[2] * 0.74), (MARBLE_DARK[0] * 1.42, MARBLE_DARK[1] * 1.37, MARBLE_DARK[2] * 1.32))
    main = vein(p, 0.8, 0.014, 1.6)
    branch = vein(s.offset(p, (11.0, 3.0, 7.0)), 2.6, 0.01, 2.3, 7.0)
    hair = vein(s.offset(p, (5.0, 17.0, 2.0)), 6.0, 0.006, 2.8, 7.0)
    seam = s.mul(vein(s.offset(p, (23.0, 9.0, 4.0)), 0.6, 0.01, 1.2), s.smooth(0.64, 0.76, s.noise(p, 0.5, detail=2.0)))
    base = s.mix(s.mul(main, 0.72), base, MARBLE_VEIN)
    base = s.mix(s.mul(branch, 0.3), base, (MARBLE_VEIN[0] * 0.7, MARBLE_VEIN[1] * 0.7, MARBLE_VEIN[2] * 0.72))
    base = s.mix(s.mul(hair, 0.14), base, (MARBLE_VEIN[0] * 0.55, MARBLE_VEIN[1] * 0.55, MARBLE_VEIN[2] * 0.57))
    base = s.mix(s.mul(seam, 0.6), base, (0.55, 0.36, 0.12))
    edge = pk.wear(s, 0.025)
    chips = s.mul(edge, s.smooth(0.4, 0.6, s.noise(p, 6.5, detail=4.0)))
    base = s.mix(s.mul(chips, 0.75), base, (0.16, 0.16, 0.17))
    dirt = pk.cavity(s, 0.12)
    base = s.mix(s.mul(dirt, 0.6), base, (0.006, 0.006, 0.006))
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.012, s.voronoi(p, 2.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.6, 0.7, s.noise(p, 1.3, detail=3.0)))
    base = s.mix(s.mul(cracks, 0.6), base, (0.11, 0.11, 0.11))
    up = s.smooth(0.5, 0.9, nz)
    scuff = s.mul(s.smooth(0.45, 0.75, s.noise(p, 2.2, detail=5.0)), up)
    rough = s.remap(s.noise(p, 14.0, detail=3.0), 0.3, 0.7, 0.1, 0.2)
    rough = s.lerp(s.mul(scuff, 0.85), rough, 0.42)
    rough = s.lerp(chips, rough, 0.62)
    rough = s.lerp(s.clamp01(s.add(dirt, cracks)), rough, 0.7)
    pits = s.sub(1.0, s.smooth(0.0, 0.15, s.voronoi(p, 40.0, 'F1', 1.0, 'Distance')))
    height = s.sub(s.mul(s.noise(p, 9.0, detail=3.0), 0.15), s.add(s.add(s.mul(cracks, 0.8), s.mul(chips, 0.6)), s.add(s.mul(pits, s.mul(scuff, 0.3)), s.mul(main, 0.04))))
    return s.finish(base, rough, height=height, bump=0.4, bump_distance=0.01, bake_size=size)


def gold_material(name='PortalGold', size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    noise = s.noise(p, 9.0, detail=4.0)
    broad = s.noise(p, 1.6, detail=3.0)
    dents = s.smooth(0.0, 0.38, s.voronoi(p, 30.0, 'F1', 1.0, 'Distance'))
    scratches = s.smooth(0.64, 0.72, s.noise(s.stretch(p, (70.0, 2.5, 2.5)), 2.6, detail=2.0))
    crack = s.mul(s.sub(1.0, s.smooth(0.0, 0.012, s.voronoi(p, 6.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.6, 0.7, s.noise(s.offset(p, (5.0, 1.0, 8.0)), 2.2, detail=3.0)))
    bright = (1.0, 0.86, 0.52)
    base = s.mix(s.add(s.mul(noise, 0.6), s.mul(broad, 0.4)), (GOLD_TONE[0] * 0.86, GOLD_TONE[1] * 0.82, GOLD_TONE[2] * 0.76), GOLD_TONE)
    pits = pk.cavity(s, 0.04)
    tarnish = s.clamp01(s.mul(pits, s.add(0.4, s.mul(s.smooth(0.45, 0.75, broad), 0.4))))
    base = s.mix(s.mul(tarnish, 0.85), base, (0.14, 0.1, 0.05))
    verdigris = s.mul(s.mul(pits, s.smooth(0.66, 0.78, s.noise(p, 4.0, detail=3.0))), 0.5)
    base = s.mix(verdigris, base, (0.09, 0.15, 0.11))
    base = s.mix(s.mul(crack, 0.85), base, (0.05, 0.035, 0.02))
    edge = pk.wear(s, 0.012)
    base = s.mix(s.mul(edge, 0.7), base, bright)
    base = s.mix(s.mul(scratches, 0.3), base, bright)
    rough = s.remap(noise, 0.3, 0.7, 0.12, 0.22)
    rough = s.lerp(tarnish, rough, 0.58)
    rough = s.lerp(crack, rough, 0.7)
    rough = s.lerp(s.mul(edge, 0.6), rough, 0.08)
    rough = s.lerp(s.mul(scratches, 0.5), rough, 0.3)
    metal = s.lerp(s.clamp01(s.add(s.add(tarnish, verdigris), crack)), 1.0, 0.45)
    height = s.add(s.add(s.mul(noise, 0.25), s.mul(broad, 0.15)), s.mul(dents, 0.5))
    height = s.sub(height, s.add(s.mul(crack, 0.8), s.mul(scratches, 0.12)))
    return s.finish(base, rough, height=height, bump=0.07, bump_distance=0.002, metallic=metal, bake_size=size)


def inlay_material(name='PortalInlay', size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    noise = s.noise(p, 14.0, detail=4.0)
    broad = s.noise(p, 2.0, detail=3.0)
    base = s.mix(s.add(s.mul(noise, 0.5), s.mul(broad, 0.5)), (INLAY[0] * 0.75, INLAY[1] * 0.75, INLAY[2] * 0.8), (INLAY[0] * 1.5, INLAY[1] * 1.45, INLAY[2] * 1.5))
    crazing = s.mul(s.sub(1.0, s.smooth(0.0, 0.01, s.voronoi(p, 10.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.42, 0.6, s.noise(p, 3.0, detail=3.0)))
    flake = s.smooth(0.66, 0.71, s.noise(s.offset(p, (4.0, 8.0, 2.0)), 6.0, detail=3.0))
    dirt = pk.cavity(s, 0.03)
    base = s.mix(s.mul(crazing, 0.55), base, (0.075, 0.07, 0.066))
    base = s.mix(s.mul(flake, 0.85), base, (0.13, 0.12, 0.11))
    base = s.mix(s.mul(dirt, 0.6), base, (0.02, 0.02, 0.02))
    rough = s.remap(noise, 0.3, 0.7, 0.34, 0.52)
    rough = s.lerp(flake, rough, 0.88)
    rough = s.lerp(s.mul(crazing, 0.7), rough, 0.75)
    height = s.sub(s.mul(noise, 0.5), s.add(s.mul(crazing, 0.6), s.mul(flake, 0.6)))
    return s.finish(base, rough, height=height, bump=0.09, bump_distance=0.003, bake_size=size)


def bezier(p0, p1, p2, p3, count=14):
    points = []
    for i in range(count + 1):
        t = i / count
        mt = 1.0 - t
        a, b, c, d = mt * mt * mt, 3.0 * mt * mt * t, 3.0 * mt * t * t, t * t * t
        points.append((a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0], a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]))
    return points


def arc_points(cx, cy, radius, a0, a1, count=16):
    return [(cx + radius * math.cos(math.radians(a0 + (a1 - a0) * i / count)), cy + radius * math.sin(math.radians(a0 + (a1 - a0) * i / count))) for i in range(count + 1)]


def spiral_points(cx, cy, r0, r1, a0, turns, count=22):
    return [(cx + (r0 + (r1 - r0) * i / count) * math.cos(math.radians(a0) + turns * 2.0 * math.pi * i / count), cy + (r0 + (r1 - r0) * i / count) * math.sin(math.radians(a0) + turns * 2.0 * math.pi * i / count)) for i in range(count + 1)]


def circle_points(cx, cy, radius, count=24):
    return [(cx + radius * math.cos(2.0 * math.pi * i / count), cy + radius * math.sin(2.0 * math.pi * i / count)) for i in range(count)]


def taper_stroke(points, width, tip=0.42):
    left, right = [], []
    count = len(points)
    for i, p in enumerate(points):
        a = Vector(points[max(i - 1, 0)])
        b = Vector(points[min(i + 1, count - 1)])
        d = b - a
        d = d.normalized() if d.length > 1e-9 else Vector((1.0, 0.0))
        n = Vector((-d.y, d.x))
        t = i / max(count - 1, 1)
        w = 0.5 * width * (tip + (1.0 - tip) * math.sin(math.pi * t) ** 0.6)
        left.append(tuple(Vector(p) + n * w))
        right.append(tuple(Vector(p) - n * w))
    return left + right[::-1]


def glyph_template(rng):
    kind = rng.randrange(8)
    if kind == 0:
        return [('path', bezier((0.0, 0.0), (0.42, 0.28), (-0.42, 0.58), (0.0, 0.74))), ('ring', (0.0, 0.87), 0.11)]
    if kind == 1:
        return [('path', arc_points(0.06, 0.5, 0.36, 60.0, 300.0, 22)), ('dot', (0.12, 0.5), 0.075)]
    if kind == 2:
        return [('path', bezier((0.0, 0.0), (0.14, 0.28), (-0.14, 0.5), (0.0, 0.64))), ('path', spiral_points(0.0, 0.8, 0.17, 0.04, -90.0, 1.15))]
    if kind == 3:
        return [('path', bezier((-0.4, 0.5), (-0.16, 0.86), (0.16, 0.86), (0.4, 0.5))), ('path', bezier((-0.4, 0.5), (-0.16, 0.14), (0.16, 0.14), (0.4, 0.5))), ('dot', (0.0, 0.5), 0.09)]
    if kind == 4:
        wave = [(-0.38 + 0.76 * i / 16, 0.86 + 0.07 * math.sin(math.pi * 2.0 * i / 16)) for i in range(17)]
        return [('path', wave), ('path', arc_points(0.0, 0.46, 0.3, 190.0, 350.0, 16)), ('dot', (0.0, 0.42), 0.06)]
    if kind == 5:
        return [('path', arc_points(0.0, 0.73, 0.2, 20.0, 290.0, 18)), ('path', arc_points(0.0, 0.27, 0.2, 200.0, 470.0, 18))]
    if kind == 6:
        return [('path', bezier((0.0, 0.04), (0.46, 0.34), (0.22, 0.8), (0.0, 0.96)) + bezier((0.0, 0.96), (-0.22, 0.8), (-0.46, 0.34), (0.06, 0.2))[1:]), ('dot', (0.0, 0.44), 0.065)]
    return [('ring', (0.0, 0.7), 0.19), ('path', bezier((0.0, 0.5), (0.0, 0.3), (0.26, 0.2), (0.22, 0.0)))]


def glyph_parts(rng, size, origin, z0, relief, width=0.13):
    parts = []
    mirror = rng.choice((-1.0, 1.0))
    jitter = rng.uniform(0.9, 1.06)
    place = lambda u, v: (origin[0] + mirror * u * size * jitter, origin[1] + v * size)
    for item in glyph_template(rng):
        z1 = z0 + rng.uniform(0.62, 0.95) * relief
        if item[0] == 'path':
            points = [place(u, v) for u, v in item[1]]
            pieces = [points]
            roll = rng.random()
            if roll < 0.28 and len(points) > 8:
                pieces = [points[:max(4, int(len(points) * rng.uniform(0.68, 0.88)))]]
            elif roll < 0.46 and len(points) > 10:
                cut = rng.randrange(4, len(points) - 5)
                pieces = [points[:cut], points[cut + 2:]]
            for piece in pieces:
                if len(piece) >= 3:
                    parts.append(prism(taper_stroke(piece, width * size * rng.uniform(0.85, 1.12)), z0, z1))
        elif item[0] == 'ring':
            (u, v), radius = item[1], item[2]
            cx, cy = place(u, v)
            r = radius * size
            w = 0.5 * width * size * 0.85
            parts.append(ring_prism(circle_points(cx, cy, r + w, 28), circle_points(cx, cy, max(r - w, 0.2 * r), 28), z0, z1))
        else:
            (u, v), radius = item[1], item[2]
            cx, cy = place(u, v)
            parts.append(prism(circle_points(cx, cy, radius * size, 14), z0, z1 + 0.15 * relief))
    return parts


def nick(bm, rng, count, depth, top=None):
    if count <= 0 or not bm.verts:
        return bm
    zmax = max(v.co.z for v in bm.verts) if top is None else top
    cx = sum(v.co.x for v in bm.verts) / len(bm.verts)
    cy = sum(v.co.y for v in bm.verts) / len(bm.verts)
    rim = [v.co.copy() for v in bm.verts if v.co.z > zmax - 1e-4]
    for _ in range(count):
        if not rim:
            break
        corner = rng.choice(rim)
        out = Vector((corner.x - cx, corner.y - cy, 0.0))
        out = out.normalized() if out.length > 1e-6 else Vector((1.0, 0.0, 0.0))
        normal = (out * rng.uniform(0.5, 1.0) + Vector((rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), 1.0))).normalized()
        kit.cut(bm, corner - normal * depth * rng.uniform(0.6, 1.3), normal)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def crack_apart(bm, point, normal, gap, drop):
    other = bm.copy()
    kit.cut(bm, Vector(point) - Vector(normal) * (0.5 * gap), Vector(normal))
    kit.cut(other, Vector(point) + Vector(normal) * (0.5 * gap), -Vector(normal))
    bmesh.ops.translate(other, verts=other.verts, vec=(0.0, 0.0, -drop))
    for part in (bm, other):
        bmesh.ops.recalc_face_normals(part, faces=part.faces[:])
    return merge([bm, other])


def weather(bm, seed, amplitude=0.0016, scale=14.0, step=0.022, depth=0.45):
    from mathutils import noise
    buckets = {}
    for e in bm.edges:
        cuts = min(int(e.calc_length() / step), 10)
        if cuts > 0:
            buckets.setdefault(cuts, []).append(e)
    for cuts, edges in sorted(buckets.items()):
        live = [e for e in edges if e.is_valid]
        if live:
            bmesh.ops.subdivide_edges(bm, edges=live, cuts=cuts)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    offset = Vector((seed * 3.1, seed * 1.7, seed * 2.3))
    for v in bm.verts:
        n = noise.noise_vector(v.co * scale + offset)
        v.co += Vector((n.x, n.y, n.z * depth)) * amplitude
    bm.normal_update()
    return bm


def plaque(frame, width, height, rng, panel=0.016, relief=0.016, embed=0.02, border=0.1, corner=0.22, runes=4, loop=True, diamond=True, medallion=True, tab=0.0, motif=None, steps=36):
    m = motif or width
    f = border * width
    rim = panel + relief
    sink = 0.3 * relief
    half = 0.5 * width
    c = corner * width
    outline = [(-half, c), (-half + c, 0.0), (half - c, 0.0), (half, c), (half, height), (-half, height)] if c > 0.0 else [(-half, 0.0), (half, 0.0), (half, height), (-half, height)]
    panel_bm = prism(inset(outline, 0.5 * f), -embed, panel)
    if rng.random() < 0.55:
        angle = rng.uniform(-0.6, 0.6) + (0.5 * math.pi if rng.random() < 0.5 else 0.0)
        through = (rng.uniform(-0.25, 0.25) * width, rng.uniform(0.3, 0.7) * height, 0.0)
        panel_bm = crack_apart(panel_bm, through, (math.cos(angle), math.sin(angle), 0.0), 0.0025, rng.uniform(0.0006, 0.0018))
    parts_inlay = [panel_bm]
    frame_bm = ring_prism(outline, inset(outline, f), -embed, rim)
    nick(frame_bm, rng, rng.choice((1, 2, 3)), 0.6 * f, rim)
    if rng.random() < 0.45:
        cx, cy = rng.choice(((-half, height), (half, height), (-half, 0.0), (half, 0.0)))
        out = Vector((cx, cy - 0.5 * height, 0.0)).normalized()
        out = Vector((out.x + rng.uniform(-0.3, 0.3), out.y + rng.uniform(-0.3, 0.3), 0.0)).normalized()
        point = Vector((cx, cy, 0.0)) - out * rng.uniform(1.2, 2.2) * f
        frame_bm = crack_apart(frame_bm, point, out, 0.002, rng.uniform(0.001, 0.0025))
    parts_gold = [frame_bm]
    sx, sy = half - 1.9 * f, 2.7 * f
    swag = bezier((-sx, sy), (-0.35 * sx, 0.9 * f), (0.35 * sx, 0.9 * f), (sx, sy), 18)
    parts_gold.append(prism(taper_stroke(swag, 0.7 * f, 0.5), panel - sink, panel + 0.6 * relief))
    for side in (-1.0, 1.0):
        curl = spiral_points(side * (sx + 0.05 * f), sy + 0.55 * f, 0.55 * f, 0.15 * f, -90.0 - side * 90.0, -side * 0.85, 12)
        parts_gold.append(prism(taper_stroke(curl, 0.5 * f, 0.6), panel - sink, panel + 0.55 * relief))
    top = height
    glyph_top = height - 1.6 * f
    if diamond:
        a, b = 0.64 * m, 0.5 * m
        cy = height + 0.1 * m
        outer = [(0.0, cy - b), (a, cy), (0.0, cy + b), (-a, cy)]
        plate = panel + 0.45 * relief
        parts_inlay.append(prism(inset(outer, 0.45 * f), -embed, plate))
        lozenge = ring_prism(outer, inset(outer, 0.95 * f), -embed, plate + 1.05 * relief)
        nick(lozenge, rng, rng.choice((1, 2)), 0.5 * f, plate + 1.05 * relief)
        parts_gold.append(lozenge)
        top = cy + b
        if loop:
            ring = pk.torus(0.17 * m, 0.22 * f, steps=24, sides=6)
            bmesh.ops.translate(ring, verts=ring.verts, vec=(0.0, cy + 0.14 * m, plate + 0.3 * relief))
            parts_gold.append(ring)
        if tab > 0.0:
            bar = [(-0.4 * f, cy + 0.3 * m), (0.4 * f, cy + 0.3 * m), (0.4 * f, top + tab), (-0.4 * f, top + tab)]
            parts_gold.append(prism(bar, plate - sink, plate + 0.7 * relief))
            top += tab
        if medallion:
            radius = 0.3 * m
            my = cy - 0.6 * m
            glyph_top = my - radius - 0.7 * f
    elif medallion:
        radius = 0.3 * m
        my = height - f - radius - 0.04 * m
        glyph_top = my - radius - 0.7 * f
    if medallion:
        h = lambda t: panel + t * relief
        profile = [(0.0, panel - sink), (radius, panel - sink), (radius, rim - 0.1 * relief), (radius - 0.12 * f, rim), (radius - 0.6 * f, rim), (radius - 0.72 * f, h(0.4)), (0.6 * radius, h(0.4)), (0.58 * radius, h(0.78)), (0.44 * radius, h(0.78)), (0.42 * radius, h(0.5)), (0.22 * radius, h(0.5)), (0.18 * radius, h(1.15)), (0.0, h(1.25))]
        disc = pk.lathe(profile, steps)
        bmesh.ops.translate(disc, verts=disc.verts, vec=(0.0, my, 0.0))
        nick(disc, rng, 1, 0.3 * f)
        parts_gold.append(disc)
    glyph_bottom = 3.4 * f
    if runes > 0 and glyph_top - glyph_bottom > 0.3 * width:
        slot = (glyph_top - glyph_bottom) / runes
        size = min(slot * 0.78, 0.62 * m)
        for k in range(runes):
            v0 = glyph_top - slot * (k + 1) + 0.5 * (slot - size)
            parts_gold += glyph_parts(rng, size, (0.0, v0), panel - sink, relief)
    seed = rng.uniform(0.0, 100.0)
    gold_bm = transformed(weather(merge(parts_gold), seed, amplitude=0.0012 + 0.004 * width), frame)
    inlay_bm = transformed(merge(parts_inlay), frame)
    return gold_bm, inlay_bm, top


def arc_tube(radius, a0, a1, z, tube, sides=6, step=0.14, profile=None):
    count = max(2, int(math.ceil(abs(a1 - a0) * radius / step)))
    bm = bmesh.new()
    rings = []
    for i in range(count + 1):
        a = a0 + (a1 - a0) * i / count
        center = polar(radius, a, z)
        out = Vector((math.sin(a), -math.cos(a), 0.0))
        ring = []
        for k in range(sides):
            t = 2.0 * math.pi * k / sides
            ring.append(bm.verts.new(center + out * (tube * math.cos(t)) + Vector((0.0, 0.0, tube * math.sin(t)))))
        rings.append(ring)
    for i in range(count):
        for k in range(sides):
            bm.faces.new((rings[i][k], rings[i + 1][k], rings[i + 1][(k + 1) % sides], rings[i][(k + 1) % sides]))
    bm.faces.new(rings[0][::-1])
    bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def line_tube(start, end, tube, sides=8):
    bm = pk.rod(start, end, tube, steps=sides)
    return bm


def bowl(rim_radius, rng, lugs=8):
    r = rim_radius
    profile = [
        (0.0, 0.0), (0.46 * r, 0.0), (0.5 * r, 0.05 * r), (0.42 * r, 0.1 * r), (0.2 * r, 0.16 * r), (0.17 * r, 0.3 * r),
        (0.26 * r, 0.36 * r), (0.2 * r, 0.42 * r), (0.32 * r, 0.47 * r), (0.62 * r, 0.56 * r), (0.86 * r, 0.7 * r),
        (0.97 * r, 0.86 * r), (1.0 * r, 0.93 * r), (1.08 * r, 0.96 * r), (1.08 * r, 1.03 * r), (0.98 * r, 1.04 * r),
        (0.9 * r, 0.96 * r), (0.8 * r, 0.82 * r), (0.6 * r, 0.7 * r), (0.0, 0.66 * r),
    ]
    body = pk.lathe(profile, 36)
    band = pk.torus(0.91 * r, 0.035 * r, steps=36, sides=6)
    bmesh.ops.translate(band, verts=band.verts, vec=(0.0, 0.0, 0.76 * r))
    parts = [body, band]
    for k in range(lugs):
        a = 2.0 * math.pi * (k + 0.5) / lugs
        c, s_ = math.cos(a), math.sin(a)
        lug = prism([(-0.09 * r, -0.07 * r), (0.09 * r, -0.07 * r), (0.09 * r, 0.07 * r), (-0.09 * r, 0.07 * r)], 0.0, 0.22 * r)
        bmesh.ops.transform(lug, matrix=Matrix.Translation((1.07 * r * c, 1.07 * r * s_, 0.88 * r)) @ Matrix.Rotation(a, 4, 'Z'), verts=lug.verts)
        point = pk.cone_bm((1.07 * r * c, 1.07 * r * s_, 0.88 * r + 0.22 * r), (1.07 * r * c, 1.07 * r * s_, 0.88 * r + 0.32 * r), 0.07 * r, sides=4)
        parts.extend([lug, point])
    coal = pk.lathe([(0.0, 0.69 * r), (0.84 * r, 0.69 * r), (0.8 * r, 0.8 * r), (0.5 * r, 0.86 * r), (0.0, 0.9 * r)], 32)
    return merge(parts), coal


def pedestal(builder, frame, width, height, stone, gold, inlay, rng, plaque_faces=(0,), cap=True):
    w = width
    plinth = pk.box((w + 0.18, w + 0.18, 0.26), (0.0, 0.0, 0.13 - 0.2))
    plinth_top = pk.box((w + 0.1, w + 0.1, 0.1), (0.0, 0.0, 0.11))
    shaft = chip(pk.box((w, w, height - 0.32), (0.0, 0.0, 0.16 + 0.5 * (height - 0.32))), rng, rng.randint(0, 2), 0.04)
    collar = pk.box((w + 0.08, w + 0.08, 0.08), (0.0, 0.0, height - 0.12))
    for bm in (plinth, plinth_top, shaft, collar):
        builder.add('Terrace', transformed(bm, frame), stone, bevel=0.025)
    top = height - 0.08
    if cap:
        slab = pk.box((w + 0.16, w + 0.16, 0.12), (0.0, 0.0, height + 0.02))
        builder.add('Terrace', transformed(slab, frame), stone, bevel=0.03)
        top = height + 0.08
    for face in plaque_faces:
        rotation = Matrix.Rotation(face * math.pi * 0.5, 4, 'Z')
        local = face_frame((0.0, 0.5 * w, 0.3), (-1.0, 0.0, 0.0), (0.0, 0.0, 1.0))
        g, i, _ = plaque(frame @ rotation @ local, 0.5 * w, height - 0.86, rng, panel=0.014, relief=0.014, embed=0.01, runes=2, loop=False, steps=20)
        builder.add('Gold', g, gold, bevel=0.0)
        builder.add('Inlay', i, inlay, bevel=0.0)
    band = transformed(pk.box((w + 0.012, w + 0.012, 0.035), (0.0, 0.0, height - 0.2)), frame)
    builder.add('Gold', band, gold, bevel=0.006)
    return top


def wall_angles():
    edge = SECTOR + CHEEK / TOP
    return edge, 2.0 * math.pi - edge


def build_wall(builder, stone, rng):
    a0, a1 = wall_angles()
    start = a0 - 0.02
    while start < a1 + 0.02 - 1e-4:
        end = min(a1 + 0.02, start + rng.uniform(0.8, 1.2) / PLINTH[0])
        if a1 + 0.02 - end < 0.35 / PLINTH[0]:
            end = a1 + 0.02
        gap = 0.5 * JOINT / PLINTH[0]
        block = sector(WALL_IN, PLINTH[0] + rng.uniform(-0.01, 0.01), start + gap, end - gap, PLINTH[1], PLINTH[2] + rng.uniform(-0.01, 0.006), step=0.12)
        chip(block, rng, rng.choice((0, 1)), 0.05)
        builder.add('Terrace', block, stone, bevel=0.035)
        start = end
    courses = list(zip(COURSES[:-1], COURSES[1:]))
    for c, (z0, z1) in enumerate(courses):
        a = a0 + (0.0 if c % 2 == 0 else 0.35 / WALL)
        start = a0
        while start < a1 - 1e-4:
            length = rng.uniform(0.62, 1.05) / WALL
            if c % 2 == 1 and start == a0:
                length *= 0.55
            end = min(a1, start + length)
            if a1 - end < 0.3 / WALL:
                end = a1
            gap = 0.5 * JOINT / WALL
            outer = WALL + rng.uniform(-0.02, 0.016)
            block = sector(WALL_IN, outer, start + gap, end - gap, z0 + 0.5 * JOINT, z1 - 0.5 * JOINT, step=0.12)
            chip(block, rng, rng.choice((0, 1, 1, 2)), 0.05)
            builder.add('Terrace', block, stone, bevel=0.026)
            start = end
    start = a0
    while start < a1 - 1e-4:
        end = min(a1, start + rng.uniform(0.66, 0.9) / TOP)
        if a1 - end < 0.3 / TOP:
            end = a1
        gap = 0.5 * JOINT / TOP
        cope = sector(COPE_IN, TOP + rng.uniform(-0.008, 0.006), start + gap, end - gap, HEIGHT - COPE[0], HEIGHT + COPE[1] + rng.uniform(-0.004, 0.003), step=0.1)
        settle(cope, rng, HEIGHT - 0.05, 0.012)
        chip(cope, rng, rng.choice((0, 1, 1, 2)), 0.045)
        builder.add('Terrace', cope, stone, bevel=0.028)
        start = end


def build_pilasters(builder, stone, gold, inlay, rng):
    width, depth = PILASTER
    for side in (-1.0, 1.0):
        for phi in PILASTERS:
            a = side * phi
            frame = radial_frame(a, WALL - 0.12)
            shaft = pk.box((width, depth + 0.12, PLINTH[2] - PLINTH[1] + COURSES[-1] - PLINTH[2] - 0.02), (0.0, 0.5 * (depth + 0.12), 0.5 * (PLINTH[1] + COURSES[-1] - 0.02)))
            builder.add('Terrace', transformed(chip(shaft, rng, 1, 0.04), frame), stone, bevel=0.03)
            foot = pk.box((width + 0.12, depth + 0.24, 0.3), (0.0, 0.5 * (depth + 0.24), PLINTH[1] + 0.15 + 0.2))
            builder.add('Terrace', transformed(foot, frame), stone, bevel=0.03)
            cap = pk.box((width + 0.12, depth + 0.26, COPE[0] + COPE[1] + 0.02), (0.0, 0.5 * (depth + 0.26) - 0.02, HEIGHT - 0.5 * (COPE[0] - COPE[1]) + 0.004))
            builder.add('Terrace', transformed(cap, frame), stone, bevel=0.03)
            if abs(phi) < math.radians(125.0):
                face = frame @ face_frame((0.0, depth + 0.12, 0.2), (-1.0, 0.0, 0.0), (0.0, 0.0, 1.0))
                g, i, _ = plaque(face, 0.24, 0.34, rng, panel=0.012, relief=0.013, embed=0.01, runes=2, loop=False, steps=20)
                builder.add('Gold', g, gold, bevel=0.0)
                builder.add('Inlay', i, inlay, bevel=0.0)


def tread_height(step):
    return HEIGHT - step * RISE


def stair_radius(step):
    return STAIR_IN + step * TREAD


def cheek_across(side):
    return Vector((math.cos(side * SECTOR) * side, math.sin(side * SECTOR) * side, 0.0))


def build_stairs(builder, marble, gold, rng):
    outer = CHEEK + CAP[1]
    reach = SECTOR + math.atan((outer + 0.2) / (STAIR_IN - KERB))
    kerb = sector(STAIR_IN - KERB, STAIR_IN + NOSE, -reach, reach, HEIGHT - 0.2, KERB_TOP, step=0.1)
    for side in (-1.0, 1.0):
        across = cheek_across(side)
        kit.cut(kerb, across * outer, across)
    bmesh.ops.recalc_face_normals(kerb, faces=kerb.faces[:])
    builder.add('Marble', kerb, marble, bevel=0.022)
    builder.add('Gold', arc_tube(STAIR_IN + NOSE - 0.012, -SECTOR, SECTOR, HEIGHT - 0.045, ROD), gold, bevel=0.0)
    for step in range(1, STEPS):
        r0 = stair_radius(step - 1) - 0.1
        r1 = stair_radius(step) + NOSE
        top = tread_height(step)
        reach = SECTOR + 0.6 * CHEEK / r1
        arc = 2.0 * reach * 0.5 * (r0 + r1)
        count = max(2, round(arc / 0.82))
        bounds = [-reach + 2.0 * reach * k / count for k in range(count + 1)]
        for k in range(count):
            gap = 0.5 * JOINT / r1
            a0 = bounds[k] + (gap if k > 0 else 0.0)
            a1 = bounds[k + 1] - (gap if k + 1 < count else 0.0)
            front = r1 + rng.uniform(-0.003, 0.003)
            lift = rng.uniform(-0.007, 0.003)
            slab = sector(r0, front, a0, a1, top - TREAD_DEPTH, top + lift, step=0.1)
            span0, span1 = max(a0 + 0.02 / r1, -SECTOR), min(a1 - 0.02 / r1, SECTOR)
            rod = arc_tube(front - 0.012, span0, span1, top + lift - 0.045, ROD, step=0.06) if span1 - span0 > 0.06 / r1 else None
            settle(slab, rng, top - 0.5 * TREAD_DEPTH, 0.014, followers=(rod,) if rod else ())
            planes = []
            chip(slab, rng, rng.choice((0, 1, 1, 2)), 0.04, planes=planes)
            builder.add('Marble', slab, marble, bevel=0.022)
            if rod:
                for point, normal in planes:
                    kit.cut(rod, point - normal * 0.006, normal)
                if rod.faces:
                    bmesh.ops.recalc_face_normals(rod, faces=rod.faces[:])
                    builder.add('Gold', rod, gold, bevel=0.0)
                else:
                    rod.free()


def cheek_frame(side):
    angle = side * SECTOR
    direction = Vector((math.sin(angle), -math.cos(angle), 0.0))
    across = Vector((math.cos(angle), math.sin(angle), 0.0)) * side
    frame = Matrix.Identity(4)
    frame.col[0] = across.to_4d()
    frame.col[1] = direction.to_4d()
    frame.col[2] = Vector((0.0, 0.0, 1.0, 0.0))
    frame.col[0][3] = frame.col[1][3] = 0.0
    frame.col[3] = Vector((0.0, 0.0, 0.0, 1.0))
    return frame, direction, across


def cheek_top(r):
    if r <= TOP:
        return KERB_TOP
    t = (r - TOP) / (PEDESTAL[2] - 0.5 * PEDESTAL[0] - TOP)
    return KERB_TOP + (CHEEK_FOOT - KERB_TOP) * min(max(t, 0.0), 1.0)


def build_cheeks(builder, stone, gold, inlay, rng):
    fronts = []
    for side in (-1.0, 1.0):
        frame, direction, across = cheek_frame(side)
        r_start = STAIR_IN + NOSE - 0.015
        r_end = PEDESTAL[2] - 0.5 * PEDESTAL[0] + 0.05
        segments = []
        r = r_start
        while r < r_end - 1e-4:
            nxt = min(r_end, r + rng.uniform(0.7, 1.0))
            if r_end - nxt < 0.35:
                nxt = r_end
            segments.append((r, nxt))
            r = nxt
        for r0, r1 in segments:
            for z0, z1 in ((PLINTH[1], 0.32), (0.32, None)):
                top0 = cheek_top(r0) - COPE[0] - COPE[1] if z1 is None else z1
                top1 = cheek_top(r1) - COPE[0] - COPE[1] if z1 is None else z1
                if z1 is not None and min(top0, top1) - 0.05 < z0:
                    continue
                if z1 is None and max(top0, top1) <= 0.32 + 0.05:
                    continue
                points = [(0.0, r0 + 0.5 * JOINT), (CHEEK, r0 + 0.5 * JOINT), (CHEEK, r1 - 0.5 * JOINT), (0.0, r1 - 0.5 * JOINT)]
                bm = prism(points, z0 + 0.5 * JOINT, max(top0, top1))
                for v in bm.verts:
                    if v.co.z > z0 + 0.01:
                        t = (v.co.y - r0) / max(r1 - r0, 1e-4)
                        v.co.z = (top0 + (top1 - top0) * t) - 0.5 * JOINT
                chip(bm, rng, rng.choice((0, 1)), 0.04)
                builder.add('Terrace', transformed(bm, frame), stone, bevel=0.026)
            cap0, cap1 = cheek_top(r0), cheek_top(r1)
            points = [(CAP[0], r0 + 0.5 * JOINT), (CHEEK + CAP[1], r0 + 0.5 * JOINT), (CHEEK + CAP[1], r1 - 0.5 * JOINT), (CAP[0], r1 - 0.5 * JOINT)]
            cap = prism(points, 0.0, 1.0)
            for v in cap.verts:
                t = (v.co.y - r0) / max(r1 - r0, 1e-4)
                top = cap0 + (cap1 - cap0) * t
                v.co.z = top if v.co.z > 0.5 else top - COPE[0] - COPE[1] - CAP[2]
            chip(cap, rng, rng.choice((1, 1, 2)), 0.045)
            builder.add('Terrace', transformed(cap, frame), stone, bevel=0.028)
        foot = Matrix.Translation(direction * PEDESTAL[2] + across * (0.5 * CHEEK)) @ frame
        fronts.append(foot)
        post = Matrix.Translation(direction * (TOP - 0.24) + across * (0.5 * CHEEK) + Vector((0.0, 0.0, HEIGHT + COPE[1]))) @ frame
        top = pedestal(builder, post, 0.4, 0.62, stone, gold, inlay, rng, plaque_faces=(), cap=False)
        tip = pk.cone_bm((0.0, 0.0, 0.0), (0.0, 0.0, 0.26), 0.3, sides=4)
        bmesh.ops.rotate(tip, verts=tip.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(math.pi * 0.25, 3, 'Z'))
        builder.add('Terrace', transformed(transformed(tip, Matrix.Translation((0.0, 0.0, top + 0.04))), post), stone, bevel=0.012)
        slab = pk.box((0.52, 0.52, 0.08), (0.0, 0.0, top))
        builder.add('Terrace', transformed(slab, post), stone, bevel=0.02)
    return fronts


def build_posts(builder, stone, gold, inlay, rng):
    for side in (-1.0, 1.0):
        a = side * POSTS[1][1]
        frame = radial_frame(a, POSTS[1][0], HEIGHT + COPE[1])
        top = pedestal(builder, frame, 0.4, 0.62, stone, gold, inlay, rng, plaque_faces=(), cap=False)
        tip = pk.cone_bm((0.0, 0.0, 0.0), (0.0, 0.0, 0.26), 0.3, sides=4)
        bmesh.ops.rotate(tip, verts=tip.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(math.pi * 0.25, 3, 'Z'))
        builder.add('Terrace', transformed(transformed(tip, Matrix.Translation((0.0, 0.0, top + 0.04))), frame), stone, bevel=0.012)
        slab = pk.box((0.52, 0.52, 0.08), (0.0, 0.0, top))
        builder.add('Terrace', transformed(slab, frame), stone, bevel=0.02)


def build_bowls(builder, stone, gold, inlay, ember, rng, fronts):
    fires = []
    for frame in fronts:
        top = pedestal(builder, frame, PEDESTAL[0], PEDESTAL[1], stone, gold, inlay, rng, plaque_faces=(0, 1, 3))
        body, coal = bowl(BOWLS[0], rng)
        place = frame @ Matrix.Translation((0.0, 0.0, top))
        builder.add('Gold', transformed(body, place), gold, bevel=0.004, angle=40.0)
        builder.add('Ember', transformed(coal, place), ember, bevel=0.0)
        fires.append(place @ Vector((0.0, 0.0, BOWLS[0] * 0.86)))
    radius, phi, width, height = SIDE_BOWL
    for side in (-1.0, 1.0):
        frame = radial_frame(side * phi, radius, HEIGHT + COPE[1])
        top = pedestal(builder, frame, width, height, stone, gold, inlay, rng, plaque_faces=(0,))
        body, coal = bowl(BOWLS[1], rng)
        place = frame @ Matrix.Translation((0.0, 0.0, top))
        builder.add('Gold', transformed(body, place), gold, bevel=0.004, angle=40.0)
        builder.add('Ember', transformed(coal, place), ember, bevel=0.0)
        fires.append(place @ Vector((0.0, 0.0, BOWLS[1] * 0.86)))
    return fires


def build_trim(builder, gold):
    a0, a1 = wall_angles()
    builder.add('Gold', arc_tube(WALL + 0.006, a0, a1, HEIGHT - COPE[0] - 0.035, ROD), gold, bevel=0.0)
    builder.add('Gold', arc_tube(PLINTH[0] - 0.05, a0, a1, PLINTH[2] + 0.005, ROD * 0.8), gold, bevel=0.0)


def build_rocks(builder, stone, rng):
    spots = []
    for side in (-1.0, 1.0):
        for phi, count in ((74.0, 2), (98.0, 4), (126.0, 3), (160.0, 3)):
            spots.append((side * math.radians(phi + rng.uniform(-4.0, 4.0)), count))
    for side in (-1.0, 1.0):
        spots.append((side * math.radians(44.0), 2))
    for phi, count in spots:
        for _ in range(count):
            radius = (PLINTH[0] if abs(phi) > SECTOR + 0.2 else PEDESTAL[2] + 0.4) + rng.uniform(0.0, 0.32)
            center = polar(radius, phi + rng.uniform(-0.05, 0.05), 0.0)
            size = rng.uniform(0.22, 0.5)
            dims = (size * rng.uniform(0.9, 1.5), size * rng.uniform(0.8, 1.2), size * rng.uniform(0.55, 0.9))
            rock = pk.box(dims)
            for _ in range(7):
                normal = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.3, 1))).normalized()
                kit.cut(rock, normal * (0.5 * min(dims) * rng.uniform(0.75, 1.0)), normal)
            bmesh.ops.recalc_face_normals(rock, faces=rock.faces[:])
            matrix = Matrix.Translation(center + Vector((0.0, 0.0, dims[2] * rng.uniform(0.05, 0.25)))) @ Matrix.Rotation(rng.uniform(0, math.tau), 4, 'Z') @ Matrix.Rotation(rng.uniform(-0.25, 0.25), 4, 'X')
            builder.add('Terrace', transformed(rock, matrix), stone, bevel=0.03, angle=25.0)


def polar_patch(bm, radii, a0, a1, segments, z):
    rows = []
    for r in radii:
        if r < 1e-6:
            rows.append([bm.verts.new((0.0, 0.0, z))] * (segments + 1))
        else:
            rows.append([bm.verts.new(polar(r, a0 + (a1 - a0) * i / segments, z)) for i in range(segments + 1)])
    for inner, outer in zip(rows[:-1], rows[1:]):
        for i in range(segments):
            quad = [inner[i], outer[i], outer[i + 1], inner[i + 1]]
            unique = list(dict.fromkeys(quad))
            if len(unique) >= 3:
                bm.faces.new(unique)


def flat_disc(target, name, z):
    reach = SECTOR + 0.6 * CHEEK / STAIR_IN
    cut = STAIR_IN - KERB + 0.04
    bm = bmesh.new()
    inner = [0.0] + [cut * (k + 1) / 8 for k in range(8)]
    polar_patch(bm, inner, 0.0, 2.0 * math.pi, 160, z)
    outer = [cut + (FLOOR_EDGE - cut) * (k + 1) / 4 for k in range(-1, 4)]
    polar_patch(bm, outer, reach, 2.0 * math.pi - reach, 160, z)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    for face in bm.faces:
        if face.normal.z < 0.0:
            face.normal_flip()
    obj = kit.mesh_object(name, bm, target)
    obj.data.shade_flat()
    return obj


def floor_bands():
    c0, c1, c2, c3 = FLOOR_RINGS
    return [0.0, c0, c1, c2, c3, FLOOR_EDGE + 0.1], [1, FLOOR_COURSES[0], FLOOR_COURSES[1], FLOOR_COURSES[2], 2]


def paver_cells():
    edges, courses = floor_bands()
    cells = []
    for b in range(1, len(edges) - 1):
        lo, hi = edges[b], edges[b + 1]
        width = (hi - lo) / courses[b]
        for k in range(courses[b]):
            r0, r1 = lo + k * width, lo + (k + 1) * width
            mid = 0.5 * (r0 + r1)
            count = max(int(round(2.0 * math.pi * mid / 0.62)), 8)
            stagger = float(hash01(float(k), float(b) * 3.1))
            for j in range(count):
                a0 = (j - stagger) / count * 2.0 * math.pi
                cells.append(dict(b=b, k=k, j=j, r0=r0, r1=r1, a0=a0, a1=a0 + 2.0 * math.pi / count, gold_in=k == 0, gold_out=k == courses[b] - 1 and b < len(FLOOR_RINGS)))
    return cells


def gold_ring_width(index):
    return (0.045, 0.06, 0.045, 0.075)[index]


def wrap(a):
    return (a + math.pi) % (2.0 * math.pi) - math.pi


def in_arch_foot(x, y):
    return any(x0 <= x <= x1 and y0 <= y <= y1 for (x0, y0), (x1, y1) in ARCH_FOOT)


def paver_outline(ri, ro, a0, a1, step=0.06):
    def arc(r, start, end):
        span = end - start
        count = max(2, int(math.ceil(abs(span) * r / step)))
        return [(r * math.sin(start + span * i / count), -r * math.cos(start + span * i / count)) for i in range(count + 1)]
    gi, go = PAVER_JOINT / ri, PAVER_JOINT / ro
    return arc(ri, a0 + gi, a1 - gi) + arc(ro, a1 - go, a0 + go)


def paver_chips(bm, rng, count, top):
    marks = []
    tops = [v for v in bm.verts if v.co.z > top - 0.02]
    if not tops:
        return marks
    cx = sum(v.co.x for v in tops) / len(tops)
    cy = sum(v.co.y for v in tops) / len(tops)
    corners = [v.co.copy() for v in sorted(tops, key=lambda v: -((v.co.x - cx) ** 2 + (v.co.y - cy) ** 2))[:8]]
    for _ in range(count):
        corner = rng.choice(corners).copy()
        out = Vector((corner.x - cx, corner.y - cy, 0.0)).normalized()
        side = Vector((-out.y, out.x, 0.0)) * rng.uniform(-0.6, 0.6)
        normal = (out + side + Vector((0.0, 0.0, rng.uniform(0.7, 1.4)))).normalized()
        point = corner - normal * rng.uniform(0.012, 0.03)
        kit.cut(bm, point, normal)
        marks.append(('chip', (point.x, point.y, point.z), (normal.x, normal.y, normal.z)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return marks


def paver_key(j, k, b):
    return float(hash01(float(j), float(k), float(b)))


def floor_diamonds():
    c0, c1, c2, _ = FLOOR_RINGS
    result = []
    for ring, count, size, offset in ((c1, 4, 0.13, 0.0), (c2, 8, 0.1, math.pi / 8.0), (c0 + 0.5 * (c1 - c0), 4, 0.075, 0.0)):
        for k in range(count):
            result.append((ring, offset + 2.0 * math.pi * k / count, size))
    return result


def angle_inside(angle, lo, hi):
    return wrap(angle - lo) >= 0.0 and wrap(angle - lo) <= hi - lo if hi - lo < 2.0 * math.pi else True


def build_inlays(builder, gold):
    c0, c1, _, c3 = FLOOR_RINGS
    low, high = HEIGHT - 0.006, HEIGHT + 0.0015
    for k in range(4):
        a = 0.5 * math.pi * k
        r0, r1 = c0 + 0.5 * gold_ring_width(0), c1 - 0.5 * gold_ring_width(1)
        side = Vector((math.cos(a), math.sin(a), 0.0)) * 0.0175
        p0, p1 = polar(r0, a), polar(r1, a)
        strip = prism([tuple(p0 - side)[:2], tuple(p1 - side)[:2], tuple(p1 + side)[:2], tuple(p0 + side)[:2]], low, high)
        builder.add('Gold', strip, gold, bevel=0.003, angle=40.0)
    for ring, a, size in floor_diamonds():
        centre = polar(ring, a)
        along = Vector((math.cos(a), math.sin(a), 0.0))
        out = Vector((math.sin(a), -math.cos(a), 0.0))
        points = [centre + along * 0.6 * size, centre + out * size, centre - along * 0.6 * size, centre - out * size]
        lozenge = prism([(p.x, p.y) for p in points], low, high + 0.001)
        builder.add('Gold', lozenge, gold, bevel=0.004, angle=40.0)
    thin = sector(c3 - 0.1 - 0.011, c3 - 0.1 + 0.011, 0.0, 2.0 * math.pi, low, high, step=0.05)
    builder.add('Gold', thin, gold, bevel=0.0)


def build_floor(builder, material, marble, gold):
    bed = flat_disc(builder.target, 'FloorBed', HEIGHT - BED)
    bed.data.materials.append(material)
    builder.groups.setdefault('Floor', []).append(bed)
    marks = []
    c0 = FLOOR_RINGS[0] - 0.5 * gold_ring_width(0) - 0.004
    disc = prism([(c0 * math.cos(2.0 * math.pi * i / 48), c0 * math.sin(2.0 * math.pi * i / 48)) for i in range(48)], HEIGHT - BED - 0.01, HEIGHT + 0.003)
    builder.add('Floor', disc, material, bevel=0.008, angle=32.0)
    edge_in = STAIR_IN - KERB
    diamonds = floor_diamonds()
    for cell in paver_cells():
        b, k = cell['b'], cell['k']
        ri = cell['r0'] + (0.5 * gold_ring_width(b - 1) + 0.004 if cell['gold_in'] else PAVER_JOINT)
        ro = cell['r1'] - (0.5 * gold_ring_width(b) + 0.004 if cell['gold_out'] else PAVER_JOINT)
        a0, a1 = cell['a0'], cell['a1']
        mid = wrap(0.5 * (a0 + a1))
        lo, hi = mid - 0.5 * (a1 - a0), mid + 0.5 * (a1 - a0)
        cut_side = 0.0
        if cell['r0'] >= edge_in - 1e-4 and abs(mid) < 0.5 * math.pi:
            if -SECTOR <= lo and hi <= SECTOR:
                continue
            if lo < SECTOR < hi:
                cut_side = 1.0
            elif lo < -SECTOR < hi:
                cut_side = -1.0
        prng = random.Random(b * 100003 + k * 1009 + cell['j'] * 13 + 7)
        key = paver_key(cell['j'], k, b)
        centre = polar(0.5 * (ri + ro), mid)
        foot = in_arch_foot(centre.x, centre.y)
        under_cope = ro > COPE_IN - 0.12
        roll = prng.random()
        stone_kind = b in MARBLE_BANDS
        flat = stone_kind or any(angle_inside(a, lo, hi) and abs(ring - cell['r0']) < 0.01 or angle_inside(a, lo, hi) and abs(ring - cell['r1']) < 0.01 for ring, a, _ in diamonds)
        if foot or flat:
            lift, slope = (0.002 if foot else 0.0), 0.0
        elif roll < 0.16:
            lift, slope = prng.uniform(-0.013, -0.008), prng.uniform(0.01, 0.024)
        elif roll < 0.26:
            lift, slope = prng.uniform(0.004, 0.008), prng.uniform(0.01, 0.02)
        else:
            lift, slope = prng.uniform(-0.005, 0.003), prng.uniform(0.004, 0.016)
        if under_cope:
            lift = min(lift, 0.0)
        top = HEIGHT + lift
        outline = paver_outline(ri, ro, lo, hi)
        area = 0.5 * (ro * ro - ri * ri) * (hi - lo)
        pieces = []
        if not foot and area > 0.09 and prng.random() < 0.16:
            angle = mid + prng.uniform(-0.5, 0.5)
            along = Vector((math.cos(angle), math.sin(angle), 0.0))
            normal = Vector((-along.y, along.x, prng.uniform(-0.15, 0.15))).normalized()
            through = polar(0.5 * (ri + ro) + prng.uniform(-0.06, 0.06), mid + prng.uniform(-0.25, 0.25) * (hi - lo), top)
            marks.append(('crack', (through.x, through.y, through.z), (normal.x, normal.y, normal.z), key))
            for sign in (1.0, -1.0):
                piece = prism(outline, HEIGHT - BED - 0.01, top)
                kit.cut(piece, through - normal * sign * 0.0015, normal * sign)
                bmesh.ops.recalc_face_normals(piece, faces=piece.faces[:])
                drop = prng.uniform(-0.002, 0.0) if flat else prng.uniform(-0.006, 0.002)
                for v in piece.verts:
                    if v.co.z > top - 0.005:
                        v.co.z += drop
                pieces.append(piece)
        else:
            pieces.append(prism(outline, HEIGHT - BED - 0.01, top))
        for piece in pieces:
            if cut_side != 0.0:
                kit.cut(piece, Vector((0.0, 0.0, 0.0)), -cheek_across(cut_side))
                bmesh.ops.recalc_face_normals(piece, faces=piece.faces[:])
            if not piece.faces:
                piece.free()
                continue
            if slope > 0.0:
                settle(piece, prng, top - 0.006, slope)
            if not foot and not under_cope:
                marks += [mark + (key,) for mark in paver_chips(piece, prng, prng.choice((0, 0, 1, 1, 2)), top)]
            if stone_kind:
                builder.add('Marble', piece, marble, bevel=0.006, angle=32.0)
            else:
                builder.add('Floor', piece, material, bevel=0.007, angle=32.0)
    for index, ring in enumerate(FLOOR_RINGS):
        w = gold_ring_width(index)
        band = sector(ring - 0.5 * w + 0.0015, ring + 0.5 * w - 0.0015, 0.0, 2.0 * math.pi, HEIGHT - BED - 0.004, HEIGHT - 0.0025, step=0.05)
        builder.add('Floor', band, material, bevel=0.0)
    build_inlays(builder, gold)
    return marks


def smoothstep(low, high, value):
    t = np.clip((np.asarray(value, dtype=np.float32) - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def hash01(*values):
    h = np.zeros(np.broadcast(*values).shape, np.float64)
    for i, v in enumerate(values):
        h = h + np.asarray(v, np.float64) * (12.9898 + 37.719 * i)
    return np.abs(np.sin(h) * 43758.5453) % 1.0


def value_noise(x, y, scale, seed):
    gx, gy = x / scale, y / scale
    ix, iy = np.floor(gx), np.floor(gy)
    fx, fy = gx - ix, gy - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    n = lambda dx, dy: hash01(ix + dx, iy + dy, seed)
    return ((n(0, 0) * (1 - fx) + n(1, 0) * fx) * (1 - fy) + (n(0, 1) * (1 - fx) + n(1, 1) * fx) * fy).astype(np.float32)


def fbm(x, y, scale, seed, octaves=4):
    total, weight, amplitude = 0.0, 0.0, 1.0
    for octave in range(octaves):
        total = total + amplitude * value_noise(x, y, scale / (2.0 ** octave), seed + octave * 7)
        weight += amplitude
        amplitude *= 0.5
    return total / weight


def blur(values, passes):
    for _ in range(passes):
        padded = np.pad(values, ((1, 1), (1, 1)) + ((0, 0),) * (values.ndim - 2), mode='edge')
        values = (padded[:-2, 1:-1] + padded[2:, 1:-1] + padded[1:-1, :-2] + padded[1:-1, 2:] + 2.0 * values) / 6.0
    return values


def srgb_to_linear(color):
    color = np.asarray(color, dtype=np.float64)
    return np.where(color <= 0.04045, color / 12.92, ((color + 0.055) / 1.055) ** 2.4)


def band(distance, width, texel):
    return 1.0 - smoothstep(0.5 * width - texel, 0.5 * width + texel, distance)


def window(x, y, cx, cy, radius, texel, half):
    size = x.shape[0]
    i0 = max(int((cy - radius + half) / texel), 0)
    i1 = min(int((cy + radius + half) / texel) + 1, size)
    j0 = max(int((cx - radius + half) / texel), 0)
    j1 = min(int((cx + radius + half) / texel) + 1, size)
    return slice(i0, i1), slice(j0, j1)


def crack_line(rng, start, end, steps=9, wander=0.025):
    points = []
    for i in range(steps + 1):
        t = i / steps
        p = Vector(start).lerp(Vector(end), t)
        if 0 < i < steps:
            d = (Vector(end) - Vector(start)).normalized()
            p += Vector((-d.y, d.x)) * rng.uniform(-wander, wander)
        points.append(p)
    return points


def segment_distance(x, y, points):
    best = np.full(x.shape, 1e3, np.float32)
    for a, b in zip(points[:-1], points[1:]):
        dx, dy = b.x - a.x, b.y - a.y
        length2 = max(dx * dx + dy * dy, 1e-9)
        t = np.clip(((x - a.x) * dx + (y - a.y) * dy) / length2, 0.0, 1.0)
        best = np.minimum(best, np.hypot(x - (a.x + t * dx), y - (a.y + t * dy)).astype(np.float32))
    return best


def floor_maps(half, size, ao, marks, seed=5):
    rng = random.Random(seed)
    texel = 2.0 * half / size
    coords = ((np.arange(size) + 0.5) * texel - half).astype(np.float32)
    x, y = np.meshgrid(coords, coords)
    r = np.hypot(x, y)
    phi = np.arctan2(x, -y)
    c0, c1, c2, c3 = FLOOR_RINGS
    gold = band(np.abs(r - c0), gold_ring_width(0), texel) + band(np.abs(r - c1), gold_ring_width(1), texel) + band(np.abs(r - c2), gold_ring_width(2), texel) + band(np.abs(r - c3), gold_ring_width(3), texel)
    gold += 1.0 - smoothstep(0.09 - texel, 0.09 + texel, r)
    gold += band(np.abs(r - 0.18), 0.03, texel)
    gold = np.clip(gold, 0.0, 1.0)
    edges, courses = floor_bands()
    joint = np.full_like(r, 1e3)
    paver = np.full_like(r, 0.5)
    band_id = np.zeros(r.shape, np.int8)
    for b in range(1, len(edges) - 1):
        lo, hi = edges[b], edges[b + 1]
        inside = (r >= lo) & (r < hi)
        width = (hi - lo) / courses[b]
        index = np.floor((r - lo) / width)
        frac = (r - lo) / width - index
        radial = np.minimum(frac, 1.0 - frac) * width
        mid = lo + (index + 0.5) * width
        count = np.maximum(np.round(2.0 * math.pi * mid / 0.62), 8.0)
        stagger = hash01(index, float(b) * 3.1)
        cell = (phi / (2.0 * math.pi)) * count + stagger
        cell_index = np.floor(cell)
        around = (0.5 - np.abs(cell - cell_index - 0.5)) * (2.0 * math.pi * mid / count)
        joint = np.where(inside, np.minimum(radial, around), joint)
        paver = np.where(inside, hash01(np.mod(cell_index, count), index, float(b)), paver).astype(np.float32)
        band_id = np.where(inside, b, band_id).astype(np.int8)
    ring_edges = np.min(np.stack([np.abs(r - e) for e in edges[1:-1]]), axis=0)
    joint = np.minimum(joint, ring_edges).astype(np.float32)
    gap = np.zeros_like(r)
    for index, ring in enumerate(FLOOR_RINGS):
        w = gold_ring_width(index)
        gap = np.maximum(gap, band(np.abs(r - ring), w + 0.011, texel) - band(np.abs(r - ring), w, texel))
    groove = np.clip((1.0 - smoothstep(0.0045, 0.008, joint)) * (1.0 - gold) + gap, 0.0, 1.0)
    rim = (1.0 - smoothstep(0.006, 0.02, joint)) * (1.0 - groove)

    chip = np.zeros_like(r)
    crack = np.zeros_like(r)
    for kind, point, normal, key in marks:
        p, n = Vector(point), Vector(normal)
        if kind == 'chip':
            rows, cols = window(x, y, p.x, p.y, 0.07, texel, half)
            wx, wy = x[rows, cols], y[rows, cols]
            depth = ((wx - p.x) * n.x + (wy - p.y) * n.y + (HEIGHT - p.z) * n.z)
            own = (np.abs(paver[rows, cols] - key) < 1e-5) & (joint[rows, cols] > 0.003)
            chip[rows, cols] = np.maximum(chip[rows, cols], smoothstep(-0.002, 0.004, depth) * own)
        else:
            along = Vector((-n.y, n.x)).normalized()
            start, end = Vector((p.x, p.y)) - along * 0.45, Vector((p.x, p.y)) + along * 0.45
            line = crack_line(rng, start, end, 14, 0.01)
            rows, cols = window(x, y, p.x, p.y, 0.5, texel, half)
            d = segment_distance(x[rows, cols], y[rows, cols], line)
            own = np.abs(paver[rows, cols] - key) < 1e-5
            crack[rows, cols] = np.maximum(crack[rows, cols], (1.0 - smoothstep(0.0015, 0.006, d)) * own)
    for cell in paver_cells():
        prng = random.Random(cell['b'] * 7919 + cell['k'] * 131 + cell['j'] * 17 + 3)
        if prng.random() > 0.26:
            continue
        mid = 0.5 * (cell['a0'] + cell['a1'])
        span = cell['a1'] - cell['a0']
        ra = prng.uniform(cell['r0'], cell['r1'])
        rb = prng.uniform(cell['r0'], cell['r1'])
        start = polar(ra, mid - 0.5 * span * prng.uniform(0.4, 0.9))
        end = polar(rb, mid + 0.5 * span * prng.uniform(0.2, 0.9))
        line = crack_line(prng, (start.x, start.y), (end.x, end.y), 10, 0.018)
        centre = polar(0.5 * (cell['r0'] + cell['r1']), mid)
        rows, cols = window(x, y, centre.x, centre.y, 0.6, texel, half)
        d = segment_distance(x[rows, cols], y[rows, cols], line)
        own = np.abs(paver[rows, cols] - paver_key(cell['j'], cell['k'], cell['b'])) < 1e-5
        crack[rows, cols] = np.maximum(crack[rows, cols], (1.0 - smoothstep(0.0008, 0.0035, d)) * 0.8 * own)
    crack *= 1.0 - gold

    wear = fbm(x, y, 1.1, 11.0)
    mottle = fbm(x, y, 0.28, 21.0)
    grain = fbm(x, y, 0.06, 31.0, 3)
    pitted = fbm(x, y, 0.02, 41.0, 2)
    path = np.exp(-(x / 0.75) ** 2) * smoothstep(-STAIR_OUT, -1.2, y) * (1.0 - smoothstep(1.0, 1.6, y))
    path = np.clip(path + 0.6 * np.exp(-(r / 0.9) ** 2), 0.0, 1.0)

    family = paver
    tint = np.ones(r.shape + (3,), np.float32)
    warm = (family > 0.55) & (family <= 0.75)
    cool = (family > 0.75) & (family <= 0.9)
    dark = family > 0.9
    tint[warm] = (1.1, 0.99, 0.84)
    tint[cool] = (0.9, 0.95, 1.06)
    tint[dark] = (0.68, 0.66, 0.66)
    shade = 0.82 + 0.36 * hash01(paver, 3.7).astype(np.float32)
    stone = np.asarray(STONE, np.float32)[None, None, :] * tint * ((shade * (0.82 + 0.34 * mottle) * (0.9 + 0.2 * grain))[..., None])
    speck = smoothstep(0.62, 0.8, fbm(x, y, 0.008, 71.0, 2))
    stone = stone * (1.0 - 0.18 * speck)[..., None]
    stone = stone * (1.0 + 0.22 * rim * smoothstep(0.4, 0.7, grain))[..., None]
    fresh = np.clip(chip, 0.0, 1.0)
    stone = stone * (1.0 - fresh[..., None]) + (np.asarray(STONE, np.float32) * 1.28)[None, None, :] * (0.9 + 0.2 * grain)[..., None] * fresh[..., None]
    grime = np.clip(0.9 * (1.0 - ao) + 0.7 * groove + 0.35 * (1.0 - path) * smoothstep(0.45, 0.75, wear), 0.0, 1.0) * (0.55 + 0.45 * wear)
    grime = grime * (1.0 - 0.6 * path)
    stone = stone * (1.0 - 0.55 * grime)[..., None] + srgb_to_linear((0.16, 0.145, 0.13))[None, None, :].astype(np.float32) * (0.22 * grime)[..., None]
    stone = stone * (1.0 - 0.75 * crack)[..., None]
    moss = smoothstep(0.6, 0.76, fbm(x, y, 0.55, 51.0)) * (1.0 - smoothstep(0.004, 0.024, joint)) * (1.0 - gold)
    moss = np.clip(moss + 0.6 * smoothstep(0.66, 0.82, fbm(x, y, 0.35, 61.0)) * groove, 0.0, 1.0) * smoothstep(1.4, 2.8, r) * (1.0 - path)
    moss_tone = srgb_to_linear((0.22, 0.27, 0.09))[None, None, :].astype(np.float32) * (0.7 + 0.5 * grain)[..., None]
    stone = stone * (1.0 - moss[..., None]) + moss_tone * moss[..., None]
    stone = stone * (1.0 - 0.62 * groove)[..., None]
    stone_rough = np.clip(0.82 + 0.1 * (mottle - 0.5) + 0.08 * grime - 0.22 * path - 0.1 * rim + 0.08 * fresh + 0.06 * moss + 0.1 * crack, 0.45, 0.97)

    marble = np.isin(band_id, MARBLE_BANDS).astype(np.float32) * (1.0 - gold)
    stone = stone * (1.0 - 0.75 * marble)[..., None]

    scratch = np.zeros_like(r)
    for _ in range(140):
        a = rng.uniform(0.0, 2.0 * math.pi)
        ring = rng.choice((c0, c1, c2, c3, 0.18, c0 + 0.5 * (c1 - c0)))
        p = polar(ring + rng.uniform(-0.03, 0.03), a)
        d = Vector((math.cos(a + rng.uniform(-0.6, 0.6)), math.sin(a + rng.uniform(-0.6, 0.6))))
        line = [Vector((p.x, p.y)) - d * rng.uniform(0.02, 0.08), Vector((p.x, p.y)) + d * rng.uniform(0.02, 0.08)]
        rows, cols = window(x, y, p.x, p.y, 0.1, texel, half)
        scratch[rows, cols] = np.maximum(scratch[rows, cols], 1.0 - smoothstep(0.0004, 0.0016, segment_distance(x[rows, cols], y[rows, cols], line)))
    gold_edge = smoothstep(0.15, 0.85, blur(gold, 2)) * (1.0 - smoothstep(0.85, 1.0, blur(gold, 4)))
    tarnish = np.clip(0.55 * (1.0 - ao) + 0.5 * gold_edge + 0.35 * smoothstep(0.55, 0.85, mottle) * (1.0 - path), 0.0, 1.0)
    gold_base = np.asarray(GOLD_TONE, np.float32)[None, None, :] * (0.88 + 0.16 * mottle + 0.1 * scratch)[..., None]
    gold_base = gold_base * (1.0 - 0.75 * tarnish[..., None]) + srgb_to_linear((0.26, 0.2, 0.11))[None, None, :].astype(np.float32) * (0.75 * tarnish)[..., None]
    base = stone * (1.0 - gold)[..., None] + gold_base * gold[..., None]
    base = base * (0.72 + 0.28 * ao)[..., None]

    undulation = fbm(x, y, 0.15, 111.0, 3)
    hewn = fbm(x, y, 0.045, 117.0, 3)
    facets = np.abs(fbm(x, y, 0.09, 121.0, 2) - 0.5)
    rough_stone = (1.0 - marble) * (1.0 - gold)
    height = (0.0028 * (pitted - 0.5) + 0.0018 * (grain - 0.5) + 0.0045 * (undulation - 0.5) + 0.0025 * (hewn - 0.5) + 0.0035 * facets - 0.0008 * speck) * rough_stone
    height = height - 0.003 * (1.0 - smoothstep(0.004, 0.03, joint)) - 0.0014 * crack - 0.0004 * gold_edge - 0.00015 * scratch * gold
    gy, gx = np.gradient(height.astype(np.float32), texel)
    gx, gy = blur(gx, 1), blur(gy, 1)
    normal = np.stack((-gx, -gy, np.ones_like(gx)), axis=-1)
    normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
    orm = np.ones(base.shape[:2] + (3,), np.float32)
    orm[..., 0] = np.clip(0.4 + 0.6 * ao, 0.0, 1.0) * (1.0 - 0.45 * groove) * (1.0 - 0.3 * crack)
    gold_rough = np.clip(0.16 + 0.08 * grain + 0.38 * tarnish - 0.08 * scratch - 0.06 * path, 0.1, 0.8)
    orm[..., 1] = np.clip(stone_rough * (1.0 - gold) + gold_rough * gold, 0.05, 1.0)
    orm[..., 2] = gold * (1.0 - 0.35 * tarnish)
    return np.clip(base, 0.0, 1.0), normal * 0.5 + 0.5, orm


def materials():
    return {'stone': stone_material(), 'marble': marble_material(), 'gold': gold_material(), 'inlay': inlay_material(), 'ember': pk.ember_material('PortalEmber', bake_size=512, strength=3.0)}


def populate(builder, mats):
    rng = builder.rng
    stone, gold, inlay, ember = mats['stone'], mats['gold'], mats['inlay'], mats['ember']
    build_wall(builder, stone, rng)
    build_pilasters(builder, stone, gold, inlay, rng)
    build_stairs(builder, mats['marble'], gold, rng)
    fronts = build_cheeks(builder, stone, gold, inlay, rng)
    build_posts(builder, stone, gold, inlay, rng)
    fires = build_bowls(builder, stone, gold, inlay, ember, rng, fronts)
    build_trim(builder, gold)
    build_rocks(builder, stone, rng)
    return fires


def build(target, seed=11):
    builder = Builder(target, seed)
    mats = materials()
    fires = populate(builder, mats)
    marks = build_floor(builder, bpy.data.materials.new('PortalFloor'), mats['marble'], mats['gold'])
    groups = builder.finish()
    return groups, fires, mats, marks


def preview_floor_material(floor, folder, marks, size=2048):
    half = FLOOR_EDGE + 0.12
    ao = np.ones((size, size), np.float32)
    base, normal, orm = floor_maps(half, size, ao, marks)
    files = {}
    for key, data, srgb in (('albedo', base, True), ('normal', normal, False), ('orm', orm, False)):
        files[key] = os.path.join(folder, f'floor_preview_{key}.png')
        kit.save_png(files[key], data, srgb=srgb)
    me = floor.data
    layer = me.uv_layers.new(name='UVMap')
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    loops = np.empty(len(me.loops), np.int32)
    me.loops.foreach_get('vertex_index', loops)
    uv = np.stack((co[loops, 0] / (2.0 * half) + 0.5, co[loops, 1] / (2.0 * half) + 0.5), axis=1)
    layer.data.foreach_set('uv', uv.astype(np.float32).ravel())
    placeholder = bpy.data.materials.new('FloorPreview')
    me.materials.clear()
    me.materials.append(kit.baked_material(placeholder, files))


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    folder = os.path.abspath(argv[argv.index('--preview') + 1]) if '--preview' in argv else None
    views = argv[argv.index('--views') + 1].split(',') if '--views' in argv else ['game', 'q', 'front']
    kit.reset()
    kit.enable_gpu()
    target = kit.collection('Terrace')
    groups, fires, _, marks = build(target)
    for name, obj in groups.items():
        print(name, len(obj.data.polygons), 'faces')
    print('fires', [tuple(round(c, 3) for c in f) for f in fires])
    if folder:
        os.makedirs(folder, exist_ok=True)
        preview_floor_material(groups['Floor'], folder, marks)
        kit.environment('forest.exr', 1.1, 1.2)
        objects = list(groups.values())
        shots = {'game': (0.0, 57.0, 1.0), 'q': (35.0, 30.0, 1.0), 'front': (0.0, 12.0, 1.0), 'side': (90.0, 15.0, 1.0), 'top': (0.0, 89.0, 1.0), 'low': (20.0, 8.0, 0.6)}
        for view in views:
            yaw, pitch, fill = shots[view]
            kit.render(os.path.join(folder, f'terrace_{view}.png'), objects, yaw=yaw, pitch=pitch, size=(1200, 900), samples=32, fill=fill)


if __name__ == '__main__':
    main()
