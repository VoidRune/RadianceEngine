import bpy
import bmesh
import math
import os
import random
import re
import sys
import time
import numpy as np
from mathutils import Matrix, Vector, noise
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.append(HERE)
import assetkit as kit
import propkit as pk

NAME = 'EmberIsland'
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
SOURCE = os.path.join(HERE, 'data', 'FloatingIsland2.glb')
ENVELOPE = os.path.join(HERE, 'data', 'EmberIslandEnvelope.npz')
SCALE = 34.0
PLAZA_TOP = 0.4956
SMOOTH_CELLS = 5
RIM_THICKNESS = (3.8, 6.0)
SHELF_THICKNESS = (2.2, 4.5)
SHELF_TOP = (0.5, 2.4)
SHELF_SPACING = (1.8, 3.2)
SHELF_SHARE = 0.6
DIP = (0.06, 0.2)
SLABS = 16
COURSE = (0.44, 0.6)
COURT_STONE = (0.5, 0.95)
FIELD_SPACING = (0.75, 1.1)
STONE_RISE = (0.015, 0.045)
STONE_TILT = 0.045
STONE_DOME = (0.0, 0.018)
STONE_BEVEL = (0.008, 0.024)
STONE_DEPTH = 0.12
BED_TEXTURE = 4096
SPACING = (1.5, 3.4)
CAVERN_SPACING = (1.15, 1.7)
RIM_SPACING = (1.6, 2.6)
SEGMENT = (1.3, 3.0)
SEGMENT_SHRINK = (0.9, 0.985)
SEGMENT_SHIFT = 0.12
GAP = (0.03, 0.07)
CRACK_GAP = (0.1, 0.2)
CRACK_LEVEL = 0.22
FORMATION = 6.5
FORMATION_DEPTH = (0.82, 1.18)
TIP = (0.45, 1.1)
PIECE_LEVELS = (0, 1, 2)
PIECE_MIN = 0.9
PIECE_GAP = (0.04, 0.09)
PIECE_PUSH = (-0.05, 0.12)
PIECE_TURN = 2.5
TOP_TURN = 0.7
RIM_TOP = (0.04, 0.1)
DECK_DROP = 0.45
DECK_OVERHANG = 0.25
LIMESTONE_DEPTH = 2.6
BEVEL = (0.025, 0.07)
FRAGMENTS = 34
EDGE_CLUSTERS = 30
EDGE_OPENING = 2.4
SECTORS = 8
HIDE_DISTANCE = 0.9
HIDE_RAYS = 10
PAVING_TEXTURE = 4096
AO_DISTANCE = 0.6
SEED = 7


def read_header():
    with open(os.path.join(ROOT, 'GameShared', 'src', 'GameShared', 'Content', 'EmberIsland.h'), 'r', encoding='utf-8') as file:
        text = file.read()
    number = r'(-?\d+(?:\.\d+)?)f'
    values = {m.group(1): float(m.group(2)) for m in re.finditer(r'inline constexpr float (\w+) = ' + number + ';', text)}
    for m in re.finditer(r'inline constexpr float (\w+)\[\] = \{([^}]*)\};', text):
        values[m.group(1)] = [float(v) for v in re.findall(r'-?\d+(?:\.\d+)?', m.group(2))]
    for m in re.finditer(r'inline constexpr Vec2 (\w+)\{ ' + number + ', ' + number + r' \};', text):
        values[m.group(1)] = (float(m.group(2)), float(m.group(3)))
    return values


H = read_header()
HEART = Vector((0.0, 0.0, -H['EmberHeartDepth']))
HEART_R = H['EmberHeartRadius']
FLOOR = H['EmberCavernDepths'][-1]
CAVERN_REACH = max(H["EmberCavernRadii"])
CAVERN_CUT = 0.8
SURROUND = H["EmberSurroundRadius"]


def edge(angle):
    radius = H['EmberRadius']
    values = H['EmberHarmonics']
    for i in range(0, len(values) - 2, 3):
        radius += values[i + 1] * math.cos(values[i] * angle) + values[i + 2] * math.sin(values[i] * angle)
    degrees = math.degrees(angle)
    lobes = H['EmberLobes']
    for i in range(0, len(lobes) - 2, 3):
        delta = math.remainder(degrees - lobes[i], 360.0) / lobes[i + 1]
        radius += lobes[i + 2] * math.exp(-delta * delta)
    return radius


def edge_distance(x, y):
    return edge(math.atan2(y, x)) - math.hypot(x, y)


def max_edge():
    return max(edge(a) for a in np.linspace(0, 2 * math.pi, 720))


PATH_SAMPLES = 32


def paths():
    p = H['EmberPaths']
    return [([Vector((p[i + 2 * k], p[i + 2 * k + 1])) for k in range(4)], p[i + 8]) for i in range(0, len(p) - 8, 9)]


def bezier(points, t):
    u = 1.0 - t
    return points[0] * (u * u * u) + points[1] * (3 * u * u * t) + points[2] * (3 * u * t * t) + points[3] * (t * t * t)


def path_polyline(points, samples=PATH_SAMPLES):
    return [bezier(points, s / samples) for s in range(samples + 1)]


PATH_LINES = None


def path_distance(x, y):
    global PATH_LINES
    if PATH_LINES is None:
        PATH_LINES = []
        for points, half in paths():
            line = np.array([tuple(q) for q in path_polyline(points)])
            PATH_LINES.append((line[:-1], line[1:] - line[:-1], half))
    p = np.array((x, y))
    best = 1e9
    for a, span, half in PATH_LINES:
        t = np.clip(((p - a) * span).sum(1) / np.maximum((span * span).sum(1), 1e-6), 0.0, 1.0)
        nearest = float(np.sqrt(((a + span * t[:, None] - p) ** 2).sum(1)).min())
        best = min(best, nearest - half)
    return best


def plazas():
    p = H['EmberPlazas']
    return [(Vector((p[i], p[i + 1])), p[i + 2]) for i in range(0, len(p) - 2, 3)]


def flat_zones():
    pad = H['EmberPadOffset']
    return [
        (pad[0], pad[1], H['EmberPadRadius'] + 0.5),
        (-pad[0], -pad[1], H['EmberPadRadius'] + 0.5),
        (H['EmberGateOffset'][0], H['EmberGateOffset'][1], 4.8),
        (H['EmberMapleOffset'][0], H['EmberMapleOffset'][1], 3.4),
        (H['EmberBoardOffset'][0], H['EmberBoardOffset'][1], 1.5),
        (H['EmberWishOffset'][0], H['EmberWishOffset'][1], 1.9),
        (H['EmberTrainingOffset'][0], H['EmberTrainingOffset'][1], 1.9),
    ]


def flatness(x, y):
    court, blend = H['EmberCourtRadius'], H['EmberFlatBlend']
    distance = math.hypot(x, y) - (court + 0.4)
    distance = min(distance, path_distance(x, y) - 0.3)
    for center, radius in plazas():
        distance = min(distance, (Vector((x, y)) - center).length - radius - 0.3)
    for cx, cy, radius in flat_zones():
        distance = min(distance, math.hypot(x - cx, y - cy) - radius)
    t = min(max(distance / blend, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def bump(x, y, cx, cy, radius, height):
    d = ((x - cx) ** 2 + (y - cy) ** 2) / (radius * radius)
    return 0.0 if d >= 1.0 else height * (1.0 - d) * (1.0 - d)


def rim(x, y, height):
    inside = edge_distance(x, y)
    if inside < 0.0:
        return H['EmberRimLevel'] + H['EmberRimSlope'] * inside
    t = min(inside / H['EmberRimBlend'], 1.0)
    return H['EmberRimLevel'] + (height - H['EmberRimLevel']) * t * t * (3.0 - 2.0 * t)


def ground(x, y):
    flat = flatness(x, y)
    if flat <= 0.0:
        return rim(x, y, 0.0)
    height = 0.05 * math.sin(1.3 * x + 0.7) * math.sin(1.1 * y - 0.4) + 0.03 * math.sin(2.9 * x - 1.7 * y)
    m = H['EmberMounds']
    for i in range(0, len(m) - 3, 4):
        height += bump(x, y, m[i], m[i + 1], m[i + 2], m[i + 3])
    o = H['EmberOutcrops']
    for i in range(0, len(o) - 3, 4):
        height += bump(x, y, o[i], o[i + 1], o[i + 2] + 1.6, 0.45)
    return rim(x, y, flat * height)


def outcrop_list():
    o = H['EmberOutcrops']
    return [(o[i], o[i + 1], o[i + 2], o[i + 3]) for i in range(0, len(o) - 3, 4)]


def cavern_wall(depth, angle):
    depths, radii = H['EmberCavernDepths'], H['EmberCavernRadii']
    if depth > depths[-1]:
        return 0.0
    lobes = H['EmberCavernLobes']
    shape = 1.0 + sum(lobes[i + 1] * math.cos(lobes[i] * angle) + lobes[i + 2] * math.sin(lobes[i] * angle) for i in range(0, len(lobes) - 2, 3))
    return float(np.interp(max(depth, 0.0), depths, radii)) * shape


def cavern_radius(z, angle=0.0):
    wall = cavern_wall(-z, angle)
    return wall + CAVERN_CUT if wall > 0.0 else 0.0


NEED_STEP = 0.25
NEED = []


def cavern_need(r):
    if not NEED:
        depths = np.linspace(0.0, H['EmberCavernDepths'][-1], 120)
        widest = [max(cavern_radius(-d, a) for a in np.linspace(0.0, 2.0 * math.pi, 24, endpoint=False)) for d in depths]
        for k in range(100):
            reach = [d for d, w in zip(depths, widest) if w + 0.6 > k * NEED_STEP]
            NEED.append((max(reach) + 0.8) if reach else 0.0)
    return NEED[min(int(r / NEED_STEP), len(NEED) - 1)]


def smoothstep(low, high, value):
    t = np.clip((np.asarray(value, dtype=np.float32) - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def smooth_grid(values, mask, passes):
    values = np.where(mask, values, 0.0)
    weight = mask.astype(np.float64)
    for _ in range(passes):
        for axis in (0, 1):
            values = (np.roll(values, 1, axis) + values * 2.0 + np.roll(values, -1, axis)) * 0.25
            weight = (np.roll(weight, 1, axis) + weight * 2.0 + np.roll(weight, -1, axis)) * 0.25
    return np.where(weight > 1e-6, values / np.maximum(weight, 1e-6), 0.0)


def extract_envelope():
    kit.reset()
    bpy.ops.import_scene.gltf(filepath=SOURCE)
    obj = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
    me = obj.data
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get('co', co)
    matrix = np.array(obj.matrix_world)
    co = co.reshape(-1, 3) @ matrix[:3, :3].T + matrix[:3, 3]
    grid = 256
    low, high = co[:, :2].min(0), co[:, :2].max(0)
    size = float((high - low).max() * 1.04)
    origin = (low + high) / 2 - size / 2
    ij = ((co[:, :2] - origin) / size * grid).astype(int).clip(0, grid - 1)
    depth = np.full((grid, grid), np.inf, np.float32)
    np.minimum.at(depth, (ij[:, 1], ij[:, 0]), co[:, 2])
    top = np.full((grid, grid), -np.inf, np.float32)
    np.maximum.at(top, (ij[:, 1], ij[:, 0]), co[:, 2])
    np.savez(ENVELOPE, depth=depth, top=top, org=origin, size=size)
    print('envelope written', ENVELOPE)


class Envelope:
    def __init__(self):
        data = np.load(ENVELOPE)
        depth, top = data['depth'], data['top']
        self.org, self.size = data['org'], float(data['size'])
        self.grid = depth.shape[0]
        foot = np.isfinite(depth)
        plaza = foot & (top >= PLAZA_TOP - 0.03)
        self.thick = smooth_grid(np.where(foot, PLAZA_TOP - depth, 0.0), foot, SMOOTH_CELLS) * SCALE
        cells = np.argwhere(plaza).mean(axis=0)
        self.offset = np.array(((cells[1] + 0.5) / self.grid * self.size + self.org[0], (cells[0] + 0.5) / self.grid * self.size + self.org[1])) * SCALE

    def thickness(self, x, y):
        world = np.array((x, y)) + self.offset
        g = (world / SCALE - self.org) / self.size * self.grid - 0.5
        i = np.clip(g.astype(int), 0, self.grid - 2)
        f = np.clip(g - i, 0.0, 1.0)
        t = self.thick
        a = t[i[1], i[0]] * (1 - f[0]) + t[i[1], i[0] + 1] * f[0]
        b = t[i[1] + 1, i[0]] * (1 - f[0]) + t[i[1] + 1, i[0] + 1] * f[0]
        return float(a * (1 - f[1]) + b * f[1])


def fbm2(x, y, scale, seed):
    return noise.fractal(Vector((x * scale + seed, y * scale - seed * 0.7, seed * 0.31)), 0.5, 2.0, 3, noise_basis='PERLIN_ORIGINAL')


def spacing_at(x, y):
    if math.hypot(x, y) < CAVERN_REACH + 3.0:
        return CAVERN_SPACING[0] + (CAVERN_SPACING[1] - CAVERN_SPACING[0]) * (0.5 + 0.5 * math.tanh(fbm2(x, y, 0.3, 5.3) * 2.0))
    t = 0.5 + 0.5 * math.tanh(fbm2(x, y, 0.09, 3.1) * 2.2)
    return SPACING[0] + (SPACING[1] - SPACING[0]) * t


def seeds(rng):
    result = []
    angle = 0.0
    while angle < 2.0 * math.pi - 0.5:
        r = edge(angle) - rng.uniform(0.3, 0.9)
        d = rng.uniform(*RIM_SPACING)
        result.append((math.cos(angle) * r, math.sin(angle) * r, d, 'rim'))
        angle += d / edge(angle)
    angle = rng.uniform(0.0, 1.0)
    while angle < 2.0 * math.pi + 0.5:
        d = rng.uniform(*SHELF_SPACING)
        if rng.random() < SHELF_SHARE:
            r = edge(angle) + rng.uniform(0.5, 1.5)
            result.append((math.cos(angle) * r, math.sin(angle) * r, d, 'shelf'))
        angle += d / edge(angle)
    extent = max_edge() + 2.0
    for _ in range(70000):
        x, y = rng.uniform(-extent, extent), rng.uniform(-extent, extent)
        if math.hypot(x, y) > edge(math.atan2(y, x)) - 1.4:
            continue
        d = spacing_at(x, y)
        if all((px - x) ** 2 + (py - y) ** 2 >= (0.5 * (d + pd)) ** 2 for (px, py, pd, _) in result):
            result.append((x, y, d, 'core'))
    return result


def clip_polygon(poly, point, normal):
    out = []
    count = len(poly)
    for i in range(count):
        a, b = poly[i], poly[(i + 1) % count]
        da, db = (a - point).dot(normal), (b - point).dot(normal)
        if da <= 0.0:
            out.append(a)
        if (da <= 0.0) != (db <= 0.0):
            out.append(a + (b - a) * (da / (da - db)))
    return out


def area(poly):
    total = 0.0
    for i in range(len(poly)):
        a, b = poly[i], poly[(i + 1) % len(poly)]
        total += a.x * b.y - b.x * a.y
    return total * 0.5


def centroid(poly):
    return sum(poly, Vector((0.0, 0.0))) / len(poly)


def voronoi(points, rng, reach_scale=1.25, outward=None):
    tree = KDTree(len(points))
    for i, p in enumerate(points):
        tree.insert((p[0], p[1], 0.0), i)
    tree.balance()
    cells = []
    for i, (x, y, d, kind) in enumerate(points):
        seed = Vector((x, y))
        reach = d * (1.05 if kind != 'core' else reach_scale)
        poly = [seed + Vector((math.cos(a), math.sin(a))) * reach for a in np.linspace(0, 2 * math.pi, 10, endpoint=False) + rng.uniform(0, 0.6)]
        for (co, j, dist) in tree.find_range((x, y, 0.0), d * 2.6):
            if j == i:
                continue
            other = Vector((co[0], co[1]))
            poly = clip_polygon(poly, (seed + other) * 0.5, (other - seed).normalized())
            if len(poly) < 3:
                break
        if outward is not None:
            angle = math.atan2(y, x)
            direction = Vector((math.cos(angle), math.sin(angle)))
            limit = outward(angle, kind, rng)
            if limit is not None:
                poly = clip_polygon(poly, direction * limit, direction)
        if len(poly) < 3 or area(poly) < 0.02:
            cells.append(None)
            continue
        merged = [poly[0]]
        for p in poly[1:]:
            if (p - merged[-1]).length > 0.04:
                merged.append(p)
        if (merged[0] - merged[-1]).length < 0.04 and len(merged) > 3:
            merged.pop()
        cells.append(merged if len(merged) >= 3 else None)
    return cells, tree


def island_outward(angle, kind, rng):
    if kind == 'shelf':
        return edge(angle) + rng.uniform(1.0, 2.6)
    return edge(angle) + rng.uniform(-0.4, 1.4)


def shrink(poly, amount):
    c = centroid(poly)
    out = []
    count = len(poly)
    sign = -1.0 if area(poly) < 0 else 1.0
    for i in range(count):
        prev, cur, nxt = poly[i - 1], poly[i], poly[(i + 1) % count]
        e1 = (cur - prev).normalized()
        e2 = (nxt - cur).normalized()
        n1 = Vector((e1.y, -e1.x)) * sign
        n2 = Vector((e2.y, -e2.x)) * sign
        bis = n1 + n2
        if bis.length < 1e-4:
            bis = n1
        bis.normalize()
        moved = cur - bis * (amount / max(bis.dot(n1), 0.35))
        if (moved - c).dot(cur - c) <= 0.0:
            moved = c + (cur - c) * 0.2
        out.append(moved)
    return out


def prism_bm(poly, z_top, z_bottom, scale_bottom, shift_bottom):
    bm = bmesh.new()
    c = centroid(poly)
    top = [bm.verts.new((p.x, p.y, z_top)) for p in poly]
    bottom = []
    for p in poly:
        q = c + (p - c) * scale_bottom + shift_bottom
        bottom.append(bm.verts.new((q.x, q.y, z_bottom)))
    if area(poly) < 0:
        top.reverse()
        bottom.reverse()
    count = len(top)
    bm.faces.new(top)
    bm.faces.new(list(reversed(bottom)))
    for i in range(count):
        j = (i + 1) % count
        bm.faces.new((top[j], top[i], bottom[i], bottom[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def tip_bm(poly, z_top, length, rng):
    bm = bmesh.new()
    c = centroid(poly)
    width = math.sqrt(abs(area(poly)))
    for p in poly:
        bm.verts.new((p.x, p.y, z_top))
    tip = c + Vector((rng.uniform(-1, 1), rng.uniform(-1, 1))) * width * 0.18
    bm.verts.new((tip.x, tip.y, z_top - length))
    for _ in range(rng.randint(2, 4)):
        p = poly[rng.randrange(len(poly))]
        t = rng.uniform(0.25, 0.65)
        q = p + (tip - p) * t * rng.uniform(0.75, 1.0)
        bm.verts.new((q.x, q.y, z_top - length * t * rng.uniform(0.8, 1.15)))
    bmesh.ops.convex_hull(bm, input=bm.verts[:])
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(4.0), verts=bm.verts[:], edges=bm.edges[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def random_unit(rng):
    while True:
        v = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)))
        if 0.05 < v.length <= 1.0:
            return v.normalized()


def fracture(bm, rng, size, chips, wedges, keep_top=None):
    for _ in range(chips):
        verts = list(bm.verts)
        if not verts:
            return
        corner = verts[rng.randrange(len(verts))].co.copy()
        center = sum((v.co for v in verts), Vector()) / len(verts)
        normal = ((corner - center).normalized() + random_unit(rng) * 0.45).normalized()
        if keep_top is not None and corner.z > keep_top - 0.05:
            normal.z = min(normal.z, 0.15)
            normal.normalize()
        kit.cut(bm, corner - normal * size * rng.uniform(0.05, 0.16), normal)
    sides = [f for f in bm.faces if abs(f.normal.z) < 0.5 and f.calc_area() > 0.4]
    rng.shuffle(sides)
    for face in sides[:wedges]:
        if not face.is_valid:
            continue
        n = face.normal.copy()
        tangent = n.cross(Vector((0, 0, 1)))
        if tangent.length < 1e-3:
            continue
        if rng.random() < 0.5:
            axis = (tangent.normalized() * rng.uniform(-1, 1) + Vector((0, 0, 1)).cross(tangent).normalized() * rng.uniform(-0.4, 0.4)).normalized()
        else:
            axis = Vector((0, 0, 1)) * (1 if rng.random() < 0.5 else -1)
        normal = Matrix.Rotation(math.radians(rng.uniform(9.0, 24.0)), 3, axis) @ n
        if keep_top is not None:
            normal.z = min(normal.z, 0.2)
            normal.normalize()
        kit.cut(bm, face.calc_center_median() - n * rng.uniform(0.01, 0.06), normal)


def finish(bm, rng, size, weathered):
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(2.0), verts=bm.verts[:], edges=bm.edges[:])
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.01)
    width = min(max(size * 0.02, BEVEL[0]), BEVEL[1]) * rng.uniform(0.7, 1.3) * (1.6 if weathered else 1.0)
    edges = [e for e in bm.edges if e.calc_face_angle(0.0) > math.radians(25.0)]
    if edges:
        bmesh.ops.bevel(bm, geom=edges, offset=width, offset_type='OFFSET', segments=2 if weathered else 1, profile=0.5, affect='EDGES', clamp_overlap=True)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])


def bounds(bm):
    co = [v.co for v in bm.verts]
    low = Vector((min(c.x for c in co), min(c.y for c in co), min(c.z for c in co)))
    high = Vector((max(c.x for c in co), max(c.y for c in co), max(c.z for c in co)))
    return low, high


def split(bm, rng, levels, min_size):
    out = [bm]
    for _ in range(levels):
        nxt = []
        for piece in out:
            low, high = bounds(piece)
            ext = high - low
            if max(ext) < min_size:
                nxt.append(piece)
                continue
            center = (low + high) * 0.5
            weights = [max(e - min_size * 0.5, 0.0) ** 2 for e in ext]
            pick = rng.uniform(0.0, sum(weights))
            axis = 0 if pick < weights[0] else (1 if pick < weights[0] + weights[1] else 2)
            normal = (Vector([1.0 if i == axis else 0.0 for i in range(3)]) + random_unit(rng) * 0.55).normalized()
            point = center + Vector([rng.uniform(-0.18, 0.18) * e for e in ext])
            a = piece.copy()
            kit.cut(a, point, normal)
            b = piece.copy()
            kit.cut(b, point, -normal)
            piece.free()
            for q in (a, b):
                if len(q.faces) >= 4 and len(q.verts) >= 4:
                    nxt.append(q)
                else:
                    q.free()
        out = nxt
    return out


def group_of(center):
    if math.hypot(center.x, center.y) < CAVERN_REACH + 1.6 and -FLOOR - 1.5 < center.z < -0.2:
        return 'Cavern'
    sector = int(((math.atan2(center.y, center.x) / (2.0 * math.pi)) % 1.0) * SECTORS) % SECTORS
    return f'Rock{sector}'


BASALT, LIMESTONE, MAGMA, HIDDEN = range(4)


def emit(sink, bm, rng, detail, tone, axis, top=None, group=None):
    if detail == 0:
        bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(2.0), verts=bm.verts[:], edges=bm.edges[:])
        center = sum((v.co for v in bm.verts), Vector()) / max(len(bm.verts), 1)
        sink.add(bm, tone, group or group_of(center))
        return
    for piece in split(bm, rng, PIECE_LEVELS[detail], PIECE_MIN):
        if not piece.verts:
            piece.free()
            continue
        low, high = bounds(piece)
        ext = max((high - low).length * 0.6, 0.3)
        center = sum((v.co for v in piece.verts), Vector()) / len(piece.verts)
        on_top = top is not None and high.z > top - 0.02
        scale = max(0.82, 1.0 - rng.uniform(*PIECE_GAP) / ext)
        outward = Vector((center.x - axis.x, center.y - axis.y, 0.0))
        outward = outward.normalized() if outward.length > 1e-3 else Vector((0.0, 0.0, 0.0))
        if on_top:
            push = outward * rng.uniform(-0.03, 0.05) + Vector((0.0, 0.0, rng.uniform(-0.012, 0.02)))
            turn = Matrix.Rotation(math.radians(rng.uniform(-TOP_TURN, TOP_TURN)), 3, random_unit(rng))
        else:
            push = outward * rng.uniform(*PIECE_PUSH) + Vector((0.0, 0.0, rng.uniform(-0.03, 0.03)))
            turn = Matrix.Rotation(math.radians(rng.uniform(-PIECE_TURN, PIECE_TURN)), 3, random_unit(rng))
        pivot = Vector((center.x, center.y, high.z)) if on_top else center
        for v in piece.verts:
            local = v.co - pivot
            v.co = pivot + push + turn @ Vector((local.x * scale, local.y * scale, local.z * (scale if not on_top else 1.0)))
        fracture(piece, rng, ext * 1.4, rng.randint(1, 3), rng.randint(1, 2), keep_top=high.z if on_top else None)
        if not piece.verts:
            piece.free()
            continue
        finish(piece, rng, ext, rng.random() < 0.2)
        sink.add(piece, tone, group or group_of(center))


class Sink:
    def __init__(self):
        self.verts = []
        self.faces = []
        self.material = []
        self.group = []

    def add(self, bm, material, group):
        index = {}
        for v in bm.verts:
            index[v] = len(self.verts)
            self.verts.append(v.co.to_tuple())
        for f in bm.faces:
            self.faces.append([index[v] for v in f.verts])
            self.material.append(material)
            self.group.append(group)
        bm.free()

    def build(self, target, materials, smooth=False):
        objects = []
        for name in sorted(set(self.group)):
            faces = [i for i, g in enumerate(self.group) if g == name]
            remap = {}
            verts = []
            polys = []
            for i in faces:
                poly = []
                for v in self.faces[i]:
                    if v not in remap:
                        remap[v] = len(verts)
                        verts.append(self.verts[v])
                    poly.append(remap[v])
                polys.append(poly)
            used = sorted(set(self.material[i] for i in faces))
            mesh = bpy.data.meshes.new(name)
            mesh.from_pydata(verts, [], polys)
            mesh.update()
            for m in used:
                mesh.materials.append(materials[m])
            mesh.polygons.foreach_set('material_index', [used.index(self.material[i]) for i in faces])
            if smooth:
                mesh.shade_smooth()
            else:
                mesh.shade_flat()
            obj = bpy.data.objects.new(name, mesh)
            target.objects.link(obj)
            objects.append(obj)
        return objects


class Formation:
    def __init__(self, rng, center, dip):
        self.center = center
        self.levels = []
        z = 0.0
        while z < 40.0:
            z += rng.uniform(*SEGMENT)
            self.levels.append(z)
        self.shrink = [rng.uniform(*SEGMENT_SHRINK) for _ in self.levels]
        self.drift = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1))) * SEGMENT_SHIFT
        self.depth = rng.uniform(*FORMATION_DEPTH)
        self.flare = [rng.random() < 0.2 for _ in self.levels]
        self.dip = dip + Vector((rng.uniform(-1, 1), rng.uniform(-1, 1))) * 0.07

    def level(self, k, point):
        return -self.levels[k] + self.dip.dot(point - self.center)


def formations(points, rng):
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    centers = []
    for _ in range(4000):
        c = Vector((rng.uniform(min(xs), max(xs)), rng.uniform(min(ys), max(ys))))
        if all((c - o).length > FORMATION for o in centers):
            centers.append(c)
    heading = rng.uniform(0.0, 2.0 * math.pi)
    dip = Vector((math.cos(heading), math.sin(heading))) * rng.uniform(*DIP)
    kinds = [Formation(rng, c, dip) for c in centers]
    tree = KDTree(len(centers))
    for i, c in enumerate(centers):
        tree.insert((c.x, c.y, 0.0), i)
    tree.balance()
    return [kinds[tree.find((p[0] + rng.uniform(-1, 1), p[1] + rng.uniform(-1, 1), 0.0))[1]] for p in points]


def inside_cavern(poly, z):
    return all(p.length < cavern_radius(z, math.atan2(p.y, p.x)) for p in poly)


def tilted_segment(poly, z_top, z_bottom, scale, shift, top_normal, bottom_normal, axis, top_flat):
    bm = prism_bm(poly, z_top + 0.8, z_bottom - 0.8, scale, shift)
    if top_flat:
        kit.cut(bm, Vector((axis.x, axis.y, z_top)), Vector((0.0, 0.0, 1.0)))
    else:
        kit.cut(bm, Vector((axis.x, axis.y, z_top)), top_normal)
    kit.cut(bm, Vector((axis.x, axis.y, z_bottom)), -bottom_normal)
    return bm


def column(sink, poly, z_top, z_bottom, kind, rng, formation, near_cavern):
    axis = centroid(poly)
    width = math.sqrt(abs(area(poly)))
    plane = Vector((-formation.dip.x, -formation.dip.y, 1.0)).normalized()
    levels = [formation.level(k, axis) + rng.uniform(-0.12, 0.12) for k in range(len(formation.levels))]
    floor = None
    if near_cavern:
        floor = -FLOOR + 0.5 * (axis.length / CAVERN_REACH) ** 2 + rng.uniform(-0.25, 0.3)
        bands = [-d + rng.uniform(-0.2, 0.2) for d in np.arange(1.2, -floor - 0.4, 1.1)]
        levels = [z for z in levels if z < floor - 0.6 or z > -0.2] + [-DECK_DROP - 0.05 - H['EmberCavernDepths'][1]] + bands + [floor]
    levels.sort(reverse=True)
    cuts = [z_top]
    for z in levels:
        if z < cuts[-1] - 0.5 and z > z_bottom + 0.6:
            cuts.append(z)
    if len(cuts) == 1 and z_top - z_bottom > 2.4:
        cuts.append(z_top - (z_top - z_bottom) * rng.uniform(0.45, 0.65))
    cuts.append(z_bottom)
    current = list(poly)
    first = cuts[1]
    tip_length = max(width * rng.uniform(*TIP), 0.6)
    flat_top = kind == 'rim'
    lean = None
    if kind != 'core':
        radial = Vector((axis.x, axis.y, 0.0)).normalized()
        lean = (Matrix.Rotation(rng.uniform(-0.09, 0.2), 3, Vector((-radial.y, radial.x, 0.0))), Vector((axis.x, axis.y, z_top)))
    for k in range(len(cuts) - 1):
        z, z_next = cuts[k], cuts[k + 1]
        last = k == len(cuts) - 2
        chamber = near_cavern and floor is not None and z_next >= floor - 0.05 and z <= 0.0
        tone = LIMESTONE if -z < LIMESTONE_DEPTH * rng.uniform(0.7, 1.25) and not near_cavern else BASALT
        s = formation.shrink[min(k, len(formation.shrink) - 1)] * rng.uniform(0.985, 1.01)
        shift = formation.drift * rng.uniform(0.5, 1.2) + Vector((rng.uniform(-1, 1), rng.uniform(-1, 1))) * 0.04
        if kind == 'shelf':
            shift += Vector((axis.x, axis.y)).normalized() * rng.uniform(-0.25, 0.1)
        if chamber and inside_cavern(current, 0.5 * (z + z_next)):
            bm = None
        elif last:
            bm = tip_bm(current, z + (0.35 if k else 0.0), (z - z_next) + tip_length + (0.35 if k else 0.0), rng)
            if rng.random() < 0.6:
                c = centroid(current)
                kit.cut(bm, Vector((c.x, c.y, z_next - tip_length * rng.uniform(0.1, 0.6))), Vector((rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), -1.0)).normalized())
        else:
            top_z = z + (0.05 if k else 0.0)
            bm = tilted_segment(current, top_z, z_next, s, shift, plane, plane, axis, flat_top and k == 0 or chamber)
        if bm is not None and chamber:
            direction = Vector((axis.x, axis.y, 0.0))
            if direction.length < 0.3:
                bm.free()
                bm = None
            else:
                direction.normalize()
                kit.cut(bm, direction * cavern_radius(0.5 * (z + z_next), math.atan2(direction.y, direction.x)), -direction)
                if not bm.faces:
                    bm.free()
                    bm = None
        if bm is not None and k == 0 and kind != 'core':
            radial = Vector((axis.x, axis.y, 0.0)).normalized()
            reach = max(Vector((v.co.x, v.co.y, 0.0)).dot(radial) for v in bm.verts)
            slope = math.radians(rng.uniform(32.0, 58.0))
            normal = (radial * math.sin(slope) + Vector((0.0, 0.0, math.cos(slope)))).normalized()
            kit.cut(bm, radial * (reach - rng.uniform(0.3, 0.85)) + Vector((0.0, 0.0, z_top)), normal)
        if bm is not None and not bm.faces:
            bm.free()
            bm = None
        if bm is not None and lean is not None:
            rotation, pivot = lean
            for v in bm.verts:
                v.co = pivot + rotation @ (v.co - pivot)
        if bm is not None:
            if chamber:
                detail = 2
            elif kind != 'core':
                detail = 2 if k < 2 else 1
            else:
                detail = 0 if k == 0 else 1
            emit(sink, bm, rng, detail, tone, axis, top=z_top if (k == 0 and kind == 'rim') else None)
        if not last:
            c = centroid(current)
            current = [c + (p - c) * s + shift for p in current]
            if formation.flare[min(k, len(formation.flare) - 1)]:
                cc = centroid(current)
                grow = rng.uniform(1.03, 1.09)
                current = [cc + (p - cc) * grow for p in current]
    return first


def magma_sheets(sink, cells, points, tree, bottoms, rng):
    count = 0
    for i, poly in enumerate(cells):
        if poly is None:
            continue
        x, y = points[i][0], points[i][1]
        for (co, j, dist) in tree.find_range((x, y, 0.0), points[i][2] * 2.6):
            if j <= i or cells[j] is None or points[i][3] != 'core' or points[j][3] != 'core' or i not in bottoms or j not in bottoms:
                continue
            mid = Vector(((x + co[0]) * 0.5, (y + co[1]) * 0.5))
            if fbm2(mid.x, mid.y, 0.16, 11.7) < CRACK_LEVEL or mid.length < CAVERN_REACH + 1.5:
                continue
            normal = (Vector((co[0], co[1])) - Vector((x, y))).normalized()
            tangent = Vector((-normal.y, normal.x))
            along = [(p - mid).dot(tangent) for p in poly if abs((p - mid).dot(normal)) < 0.25]
            if len(along) < 2:
                continue
            a, b = min(along), max(along)
            if b - a < 0.4:
                continue
            a, b = a + min(0.35, (b - a) * 0.25), b - min(0.35, (b - a) * 0.25)
            p0, p1 = mid + tangent * a, mid + tangent * b
            top = -0.35
            bottom = max(bottoms[i], bottoms[j]) + 0.15
            if bottom > top - 0.5:
                continue
            bm = bmesh.new()
            bm.faces.new([bm.verts.new((p0.x, p0.y, top)), bm.verts.new((p1.x, p1.y, top)), bm.verts.new((p1.x, p1.y, bottom)), bm.verts.new((p0.x, p0.y, bottom))])
            sink.add(bm, MAGMA, group_of(Vector((mid.x, mid.y, 0.5 * (top + bottom)))))
            count += 1
    return count


def fragments(sink, rng):
    for _ in range(FRAGMENTS):
        angle = rng.uniform(0, 2 * math.pi)
        r = edge(angle) + rng.uniform(1.0, 7.5)
        size = rng.uniform(0.35, 1.6) * (1.0 if rng.random() < 0.8 else 1.8)
        c = Vector((math.cos(angle) * r, math.sin(angle) * r))
        z = -rng.uniform(1.5, 11.0)
        poly = [c + Vector((math.cos(a), math.sin(a))) * size * rng.uniform(0.7, 1.1) for a in np.linspace(0, 2 * math.pi, rng.randint(5, 7), endpoint=False) + rng.uniform(0, 1)]
        height = size * rng.uniform(0.6, 1.4)
        bm = prism_bm(poly, z + height * 0.5, z - height * 0.5, rng.uniform(0.8, 0.95), Vector((0.0, 0.0)))
        kit.cut(bm, Vector((c.x, c.y, z + height * 0.5)), Vector((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 1)).normalized())
        emit(sink, bm, rng, 1, BASALT, Vector((0.0, 0.0)), group='Shards')
        bm = tip_bm([c + (p - c) * 0.88 for p in poly], z - height * 0.5 + 0.05, size * rng.uniform(1.2, 2.6), rng)
        emit(sink, bm, rng, 1, BASALT, Vector((0.0, 0.0)), group='Shards')


def edge_stones(sink, rng):
    placed = 0
    for _ in range(400):
        if placed >= EDGE_CLUSTERS:
            break
        angle = rng.uniform(0.0, 2.0 * math.pi)
        r = edge(angle)
        tangent = Vector((-math.sin(angle), math.cos(angle)))
        if any(abs(math.remainder(angle - axis, 2.0 * math.pi)) * r < EDGE_OPENING for axis in (0.0, 0.5 * math.pi, math.pi, 1.5 * math.pi)):
            continue
        base = Vector((math.cos(angle), math.sin(angle))) * (r - rng.uniform(0.0, 0.35))
        for _ in range(rng.randint(1, 3)):
            size = rng.uniform(0.3, 0.7)
            c = base + tangent * rng.uniform(-1.0, 1.0) + Vector((math.cos(angle), math.sin(angle))) * rng.uniform(-0.25, 0.2)
            g = ground(c.x, c.y)
            poly = [c + Vector((math.cos(a), math.sin(a))) * size * rng.uniform(0.7, 1.1) for a in np.linspace(0, 2 * math.pi, rng.randint(5, 7), endpoint=False) + rng.uniform(0, 1)]
            height = g + rng.uniform(0.2, 0.55)
            bm = prism_bm(poly, height, g - 0.3, rng.uniform(1.0, 1.12), Vector((0.0, 0.0)))
            kit.cut(bm, Vector((c.x, c.y, height)), Vector((rng.uniform(-0.35, 0.35), rng.uniform(-0.35, 0.35), 1.0)).normalized())
            emit(sink, bm, rng, 1, LIMESTONE, c)
        placed += 1


def outcrops(sink, rng):
    for (x, y, radius, height) in outcrop_list():
        center = Vector((x, y))
        base = ground(x, y) - 0.4
        blocks = [(center, radius * 0.72, height, 0.0)]
        for _ in range(rng.randint(3, 5)):
            angle = rng.uniform(0.0, 2.0 * math.pi)
            offset = Vector((math.cos(angle), math.sin(angle))) * radius * rng.uniform(0.55, 0.95)
            blocks.append((center + offset, radius * rng.uniform(0.3, 0.5), height * rng.uniform(0.35, 0.75), rng.uniform(0.12, 0.4)))
        for (c, size, h, lean) in blocks:
            poly = [c + Vector((math.cos(a), math.sin(a))) * size * rng.uniform(0.75, 1.15) for a in np.linspace(0, 2 * math.pi, rng.randint(5, 7), endpoint=False) + rng.uniform(0, 1)]
            top = ground(c.x, c.y) + h
            bm = prism_bm(poly, top + 0.6, base, rng.uniform(1.05, 1.2), Vector((0.0, 0.0)))
            outward = (c - center)
            outward = outward.normalized() if outward.length > 1e-3 else Vector((1.0, 0.0))
            for _ in range(2):
                normal = Vector((rng.uniform(-0.45, 0.45), rng.uniform(-0.45, 0.45), 1.0)).normalized()
                kit.cut(bm, Vector((c.x, c.y, top + rng.uniform(-0.1, 0.15))), normal)
            if lean > 0.0:
                axis = Vector((-outward.y, outward.x, 0.0))
                pivot = Vector((c.x, c.y, base))
                rotation = Matrix.Rotation(lean, 3, axis)
                for v in bm.verts:
                    v.co = pivot + rotation @ (v.co - pivot)
            emit(sink, bm, rng, 2, LIMESTONE, center)
        for _ in range(rng.randint(3, 6)):
            angle = rng.uniform(0.0, 2.0 * math.pi)
            c = center + Vector((math.cos(angle), math.sin(angle))) * radius * rng.uniform(1.0, 1.6)
            size = rng.uniform(0.12, 0.3)
            g = ground(c.x, c.y)
            poly = [c + Vector((math.cos(a), math.sin(a))) * size * rng.uniform(0.7, 1.1) for a in np.linspace(0, 2 * math.pi, 5, endpoint=False) + rng.uniform(0, 1)]
            bm = prism_bm(poly, g + size * rng.uniform(0.6, 1.2), g - 0.1, 1.1, Vector((0.0, 0.0)))
            kit.cut(bm, Vector((c.x, c.y, g + size * 0.8)), Vector((rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), 1.0)).normalized())
            emit(sink, bm, rng, 1, LIMESTONE, c)


def outer_radius(tree, angle, z):
    direction = Vector((math.cos(angle), math.sin(angle), 0.0))
    hit = tree.ray_cast(direction * 40.0 + Vector((0.0, 0.0, z)), -direction, 40.0)
    return None if hit[0] is None else Vector((hit[0].x, hit[0].y)).length


def slabs(sink, tree, rng):
    for _ in range(SLABS):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        z = -rng.uniform(1.4, 8.0)
        r = outer_radius(tree, angle, z)
        if r is None:
            continue
        radial = Vector((math.cos(angle), math.sin(angle)))
        tangent = Vector((-radial.y, radial.x))
        width = rng.uniform(2.2, 4.5)
        reach = rng.uniform(1.0, 2.6)
        inner = r - rng.uniform(1.2, 2.2)
        outer = r + reach
        corners = [radial * inner - tangent * width * 0.5, radial * outer - tangent * width * rng.uniform(0.2, 0.45), radial * (outer + rng.uniform(-0.3, 0.4)) + tangent * width * rng.uniform(0.15, 0.4), radial * inner + tangent * width * 0.5]
        thickness = rng.uniform(0.7, 1.5)
        bm = prism_bm(corners, z + thickness * 0.5, z - thickness * 0.5, rng.uniform(0.85, 0.97), Vector((0.0, 0.0)))
        pivot = Vector((radial.x * inner, radial.y * inner, z))
        rotation = Matrix.Rotation(rng.uniform(-0.45, 0.2), 3, Vector((tangent.x, tangent.y, 0.0))) @ Matrix.Rotation(rng.uniform(-0.15, 0.15), 3, Vector((radial.x, radial.y, 0.0)))
        for v in bm.verts:
            v.co = pivot + rotation @ (v.co - pivot)
        kit.cut(bm, Vector((radial.x * outer, radial.y * outer, z)) - Vector((radial.x, radial.y, 0.0)) * rng.uniform(0.2, 0.6), Vector((radial.x, radial.y, rng.uniform(-0.6, 0.6))).normalized())
        emit(sink, bm, rng, 2, LIMESTONE if z > -LIMESTONE_DEPTH else BASALT, Vector((0.0, 0.0)))


def solid_ring(r0, r1, top, bottom, steps=180, rings=14):
    bm = bmesh.new()
    tops, bottoms = [], []
    for i in range(steps):
        angle = 2.0 * math.pi * i / steps
        c, s_ = math.cos(angle), math.sin(angle)
        a, b = r0(angle), r1(angle)
        row_t, row_b = [], []
        for j in range(rings + 1):
            r = a + (b - a) * j / rings
            x, y = c * r, s_ * r
            row_t.append(bm.verts.new((x, y, top)))
            row_b.append(bm.verts.new((x, y, min(bottom(x, y), top - 0.6))))
        tops.append(row_t)
        bottoms.append(row_b)
    for i in range(steps):
        n = (i + 1) % steps
        for j in range(rings):
            bm.faces.new((tops[i][j], tops[i][j + 1], tops[n][j + 1], tops[n][j]))
            bm.faces.new((bottoms[i][j], bottoms[n][j], bottoms[n][j + 1], bottoms[i][j + 1]))
        bm.faces.new((tops[i][rings], bottoms[i][rings], bottoms[n][rings], tops[n][rings]))
        bm.faces.new((tops[i][0], tops[n][0], bottoms[n][0], bottoms[i][0]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-4)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def mass(env):
    inner = CAVERN_REACH + 0.9

    def bottom(x, y):
        return -env.thickness(x, y) * 0.78 + 0.6

    outer = solid_ring(lambda a: inner, lambda a: edge(a) - 1.7, -DECK_DROP - 0.15, bottom)
    return outer


def deck(target, material):
    bm = bmesh.new()
    steps = 540
    inner_rings = 30
    rim_offsets = (-1.6, -1.25, -0.95, -0.7, -0.48, -0.3, -0.15, 0.0, 0.12, DECK_OVERHANG)
    radial = inner_rings + len(rim_offsets) - 1
    grid = []
    for i in range(steps):
        angle = 2.0 * math.pi * i / steps
        outer = edge(angle)
        radii = [SURROUND + (outer + rim_offsets[0] - SURROUND) * j / inner_rings for j in range(inner_rings)] + [outer + o for o in rim_offsets]
        row = []
        for r in radii:
            x, y = math.cos(angle) * r, math.sin(angle) * r
            row.append(bm.verts.new((x, y, ground(x, y))))
        grid.append(row)
    for i in range(steps):
        a, b = grid[i], grid[(i + 1) % steps]
        for j in range(radial):
            bm.faces.new((a[j], a[j + 1], b[j + 1], b[j]))
    for ring_index in (radial, 0):
        lows = [bm.verts.new((row[ring_index].co.x, row[ring_index].co.y, min(-DECK_DROP - 0.1, row[ring_index].co.z - 0.45))) for row in grid]
        for i in range(steps):
            j = (i + 1) % steps
            if ring_index == radial:
                bm.faces.new((grid[i][ring_index], lows[i], lows[j], grid[j][ring_index]))
            else:
                bm.faces.new((grid[j][ring_index], lows[j], lows[i], grid[i][ring_index]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    for f in bm.faces:
        if abs(f.normal.z) > 0.5 and f.normal.z < 0:
            f.normal_flip()
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
    obj = kit.mesh_object('Deck', bm, target, material)
    obj.data.shade_smooth()
    planar_uv(obj, deck_half())
    return obj


STONE, KERB = 4, 5


def stone_bm(poly, rng, rise=None, tilt=None, chips=None, sink_depth=STONE_DEPTH):
    if len(poly) < 3 or abs(area(poly)) < 0.006:
        return None
    if area(poly) < 0:
        poly = list(reversed(poly))
    count = len(poly)
    chips = rng.choice((0, 0, 1, 1, 2)) if chips is None else chips
    for _ in range(chips):
        i = rng.randrange(len(poly))
        a, b, c = poly[i - 1], poly[i], poly[(i + 1) % len(poly)]
        cut = rng.uniform(0.025, 0.07)
        if (b - a).length < cut * 2.2 or (c - b).length < cut * 2.2:
            continue
        poly = poly[:i] + [b + (a - b).normalized() * cut, b + (c - b).normalized() * cut * rng.uniform(0.7, 1.3)] + poly[i + 1:]
    center = centroid(poly)
    rise = rng.uniform(*STONE_RISE) if rise is None else rise
    tilt = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1))) * STONE_TILT if tilt is None else tilt
    bevel = rng.uniform(*STONE_BEVEL)
    dome = rng.uniform(*STONE_DOME)
    inner = shrink(poly, bevel)

    def height(p):
        return ground(p.x, p.y) + rise + tilt.dot(p - center)

    bm = bmesh.new()
    top = bm.verts.new((center.x, center.y, height(center) + dome))
    rim_in = [bm.verts.new((p.x, p.y, height(p))) for p in inner]
    rim_out = [bm.verts.new((p.x, p.y, height(p) - bevel * rng.uniform(0.5, 0.9))) for p in poly]
    base = [bm.verts.new((p.x, p.y, ground(p.x, p.y) - sink_depth)) for p in poly]
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((top, rim_in[i], rim_in[j]))
        bm.faces.new((rim_in[i], rim_out[i], rim_out[j], rim_in[j]))
        bm.faces.new((rim_out[i], base[i], base[j], rim_out[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def crack_stone(poly, rng):
    if abs(area(poly)) < 0.18 or rng.random() > 0.09:
        return [poly]
    c = centroid(poly)
    angle = rng.uniform(0.0, math.pi)
    normal = Vector((math.cos(angle), math.sin(angle)))
    a = clip_polygon(poly, c + normal * 0.008, normal)
    b = clip_polygon(poly, c - normal * 0.008, -normal)
    return [p for p in (a, b) if len(p) >= 3]


class Paving:
    def __init__(self, rng):
        self.rng = rng
        self.sink = Sink()
        self.polys = []

    def add(self, poly, material, rise=None, tilt=None):
        for part in crack_stone(poly, self.rng):
            bm = stone_bm(part, self.rng, rise, tilt)
            if bm is None:
                continue
            c = centroid(part)
            sector = int(((math.atan2(c.y, c.x) / (2.0 * math.pi)) % 1.0) * SECTORS) % SECTORS
            self.sink.add(bm, material, f'Paving{sector}')
            self.polys.append(part)


def jitter(p, rng, amount):
    return p + Vector((rng.uniform(-1, 1), rng.uniform(-1, 1))) * amount


def arc_stone(r0, r1, a0, a1, rng):
    am = 0.5 * (a0 + a1)
    pts = [(r0, a0), (r0, am), (r0, a1), (r1, a1), (r1, am), (r1, a0)]
    return [jitter(Vector((math.cos(a) * r, math.sin(a) * r)), rng, 0.018) for r, a in pts]


def ring_courses(paving, start, stop, center, rng, material, widths=COURSE, lengths=COURT_STONE, accept=None, joint=(0.012, 0.03)):
    r = start
    while r < stop - 0.12:
        width = min(rng.uniform(*widths), stop - r)
        r1 = r + width
        angle = rng.uniform(0.0, 2.0 * math.pi)
        end = angle + 2.0 * math.pi
        mid = r + width * 0.5
        while angle < end - 1e-3:
            length = rng.uniform(*lengths) / mid
            a1 = min(angle + length, end)
            if end - a1 < 0.3 / mid:
                a1 = end
            poly = arc_stone(r + rng.uniform(-0.01, 0.01), r1 + rng.uniform(-0.01, 0.01), angle, a1, rng)
            poly = [center + p for p in poly]
            c = centroid(poly)
            if accept is None or accept(c):
                paving.add(shrink(poly, rng.uniform(*joint) * 0.5), material)
            angle = a1
        r = r1


FLAG_SPACING = (0.5, 0.82)
FLAG_GAP = (0.03, 0.06)
PATH_WEAR = 3.0
PATH_DARK = 0.14
STEP_CHANCE = 0.42
STEP_SIZE = (0.2, 0.36)
STEP_GAP = (0.12, 0.65)
STEP_PITCH = (0.6, 1.2)


def blocked(c, margin=0.0):
    if c.length < H['EmberCourtRadius'] + 0.04 + margin:
        return True
    if edge_distance(c.x, c.y) < 0.4:
        return True
    pad = Vector(H['EmberPadOffset'])
    if (c - pad).length < H['EmberPadRadius'] + 0.05 + margin or (c + pad).length < H['EmberPadRadius'] + 0.05 + margin:
        return True
    if (c - Vector(H['EmberGateOffset'])).length < 2.6 + margin:
        return True
    for (x, y, radius, _) in outcrop_list():
        if (c - Vector((x, y))).length < radius + 0.35:
            return True
    return (c - Vector(H['EmberMapleOffset'])).length < 2.2 + margin


def path_frame(points, samples=240):
    pts = [bezier(points, s / samples) for s in range(samples + 1)]
    lengths = [0.0]
    for a, b in zip(pts, pts[1:]):
        lengths.append(lengths[-1] + (b - a).length)
    total = lengths[-1]

    def at(s, v):
        s = min(max(s, 0.0), total)
        k = min(max(int(np.searchsorted(lengths, s)), 1), samples)
        a, b = pts[k - 1], pts[k]
        span = max(lengths[k] - lengths[k - 1], 1e-6)
        tangent = (b - a).normalized()
        point = a + (b - a) * ((s - lengths[k - 1]) / span)
        return point + Vector((-tangent.y, tangent.x)) * v

    return at, total


def irregular(center, size, rng, sides=None, stretch=1.0, angle=None):
    sides = sides or rng.randint(6, 8)
    turn = rng.uniform(0.0, 2.0 * math.pi) if angle is None else angle
    out = []
    for k in range(sides):
        a = 2.0 * math.pi * (k + rng.uniform(-0.25, 0.25)) / sides
        r = size * rng.uniform(0.78, 1.08)
        local = Vector((math.cos(a) * r * stretch, math.sin(a) * r))
        out.append(center + Vector((local.x * math.cos(turn) - local.y * math.sin(turn), local.x * math.sin(turn) + local.y * math.cos(turn))))
    return out


def paved_distance(c):
    d = path_distance(c.x, c.y)
    for center, radius in plazas():
        d = min(d, (c - center).length - radius + 0.2 * fbm2(c.x, c.y, 0.8, 17.0))
    return d


def obstacles():
    pad = Vector(H['EmberPadOffset'])
    pad_r = H['EmberPadRadius'] + 0.05
    circles = [(Vector((0.0, 0.0)), H['EmberCourtRadius'] + 0.04), (pad, pad_r), (-pad, pad_r), (Vector(H['EmberGateOffset']), 2.6), (Vector(H['EmberMapleOffset']), 2.2)]
    circles += [(Vector((x, y)), radius + 0.35) for (x, y, radius, _) in outcrop_list()]
    return circles


def clip_circles(poly, seed, circles):
    for center, radius in circles:
        offset = center - seed
        if offset.length > radius + 3.0 or offset.length < 1e-6:
            continue
        normal = offset.normalized()
        poly = clip_polygon(poly, center - normal * radius, normal)
        if len(poly) < 3:
            return None
    return poly


def path_ends():
    return [points[-1] for points, _ in paths() if edge_distance(points[-1].x, points[-1].y) < 1.5]


def flagstones(paving, rng):
    ends = path_ends()
    circles = obstacles()
    corners = [p for points, _ in paths() for p in path_polyline(points)] + [center for center, _ in plazas()]
    low = Vector((min(p.x for p in corners), min(p.y for p in corners))) - Vector((2.5, 2.5))
    high = Vector((max(p.x for p in corners), max(p.y for p in corners))) + Vector((2.5, 2.5))
    cell = 0.5
    grid = {}
    points = []
    for _ in range(60000):
        x, y = rng.uniform(low.x, high.x), rng.uniform(low.y, high.y)
        c = Vector((x, y))
        pd = paved_distance(c)
        if pd > 0.05 or blocked(c):
            continue
        d = rng.uniform(*FLAG_SPACING) * (0.8 if pd > -0.4 else 1.0)
        gx, gy = int(math.floor(x / cell)), int(math.floor(y / cell))
        if any((px - x) ** 2 + (py - y) ** 2 < (0.5 * (d + q)) ** 2 for ox in range(-4, 5) for oy in range(-4, 5) for (px, py, q) in grid.get((gx + ox, gy + oy), ())):
            continue
        grid.setdefault((gx, gy), []).append((x, y, d))
        points.append((x, y, d, 'core'))
    cells, _ = voronoi(points, rng, reach_scale=1.1)
    for (x, y, d, _), poly in zip(points, cells):
        if poly is None:
            continue
        poly = clip_circles([jitter(q, rng, 0.022) for q in poly], Vector((x, y)), circles)
        if poly is None or abs(area(poly)) < 0.05:
            continue
        c = centroid(poly)
        pd = paved_distance(c)
        border = float(smoothstep(-0.5, 0.05, pd))
        wear = max((float(smoothstep(PATH_WEAR, 0.0, (c - e).length)) for e in ends), default=0.0)
        if pd > 0.1 or rng.random() < 0.18 * border + 0.55 * wear:
            continue
        sunk = border * 0.5 + wear * 0.5
        rise = max(rng.uniform(*STONE_RISE) - 0.015 * sunk, 0.022)
        tilt = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1))) * (0.025 * (1.0 + 0.6 * sunk))
        paving.add(shrink(poly, rng.uniform(*FLAG_GAP) * 0.5), KERB if rng.random() < PATH_DARK else STONE, rise, tilt)


def stepping_stones(paving, rng):
    placed = []
    for points, half in paths():
        at, length = path_frame(points)
        s = rng.uniform(0.2, 0.8)
        while s < length:
            for side in (-1.0, 1.0):
                if rng.random() > STEP_CHANCE:
                    continue
                size = rng.uniform(*STEP_SIZE)
                center = at(s + rng.uniform(-0.2, 0.2), side * (half + rng.uniform(*STEP_GAP) + size))
                if blocked(center, 0.2) or paved_distance(center) < size + 0.1 or any((center - p).length < size + q + 0.15 for p, q in placed):
                    continue
                placed.append((center, size))
                paving.add(irregular(center, size, rng, stretch=rng.uniform(1.0, 1.35)), KERB if rng.random() < PATH_DARK else STONE, rise=rng.uniform(0.022, 0.04), tilt=Vector((rng.uniform(-1, 1), rng.uniform(-1, 1))) * 0.02)
            s += rng.uniform(*STEP_PITCH)


def build_paving(rng):
    paving = Paving(rng)
    court = H['EmberCourtRadius']
    origin = Vector((0.0, 0.0))
    gate = Vector(H['EmberGateOffset'])
    ring_courses(paving, SURROUND + 0.03, court - 0.42, origin, rng, STONE, accept=lambda c: (c - gate).length > 2.6)
    ring_courses(paving, court - 0.4, court, origin, rng, KERB, widths=(0.4, 0.4), lengths=(0.9, 1.4), accept=lambda c: (c - gate).length > 2.6)
    pad = Vector(H['EmberPadOffset'])
    pad_r = H['EmberPadRadius']
    for center in (pad, -pad):
        paving.add([center + Vector((math.cos(a), math.sin(a))) * 0.34 for a in np.linspace(0, 2 * math.pi, 8, endpoint=False)], STONE)
        ring_courses(paving, 0.38, pad_r - 0.36, center, rng, STONE, widths=(0.4, 0.5), lengths=(0.45, 0.75))
        ring_courses(paving, pad_r - 0.34, pad_r, center, rng, KERB, widths=(0.34, 0.34), lengths=(0.8, 1.2))
    flagstones(paving, rng)
    stepping_stones(paving, rng)
    return paving


def stone_mask(polys, half, size):
    mask = np.zeros((size, size), np.float32)
    texel = 2.0 * half / size
    for poly in polys:
        xs = [p.x for p in poly]
        ys = [p.y for p in poly]
        i0 = max(int((min(xs) + half) / texel) - 1, 0)
        i1 = min(int((max(xs) + half) / texel) + 2, size)
        j0 = max(int((min(ys) + half) / texel) - 1, 0)
        j1 = min(int((max(ys) + half) / texel) + 2, size)
        if i1 <= i0 or j1 <= j0:
            continue
        gx = (np.arange(i0, i1) + 0.5) * texel - half
        gy = (np.arange(j0, j1) + 0.5) * texel - half
        px, py = np.meshgrid(gx, gy)
        inside = np.zeros(px.shape, bool)
        n = len(poly)
        for k in range(n):
            a, b = poly[k], poly[(k + 1) % n]
            crosses = ((a.y > py) != (b.y > py)) & (px < (b.x - a.x) * (py - a.y) / ((b.y - a.y) if abs(b.y - a.y) > 1e-9 else 1e-9) + a.x)
            inside ^= crosses
        region = mask[j0:j1, i0:i1]
        mask[j0:j1, i0:i1] = np.maximum(region, inside.astype(np.float32))
    return mask


def deck_half():
    return max_edge() + DECK_OVERHANG + 0.15


def planar_uv(obj, half):
    me = obj.data
    layer = me.uv_layers.new(name='UVMap') if not me.uv_layers else me.uv_layers[0]
    for poly in me.polygons:
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            layer.data[li].uv = ((co.x + half) / (2.0 * half), (co.y + half) / (2.0 * half))


def loop_lathe(profile, steps):
    bm = bmesh.new()
    rings = []
    for i in range(steps):
        angle = 2.0 * math.pi * i / steps
        rings.append([bm.verts.new((r * math.cos(angle), r * math.sin(angle), z)) for r, z in profile])
    count = len(profile)
    for i in range(steps):
        a, b = rings[i], rings[(i + 1) % steps]
        for j in range(count):
            k = (j + 1) % count
            bm.faces.new((a[j], b[j], b[k], a[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def visibility(sink, extra):
    verts = [Vector(v) for v in sink.verts]
    polys = [list(f) for f in sink.faces]
    for obj in extra:
        base = len(verts)
        me = obj.data
        verts.extend(obj.matrix_world @ v.co for v in me.vertices)
        polys.extend([base + i for i in p.vertices] for p in me.polygons)
    tree = BVHTree.FromPolygons(verts, polys)
    golden = math.pi * (3.0 - math.sqrt(5.0))
    local = []
    for i in range(HIDE_RAYS):
        z = 1.0 - (i + 0.5) / HIDE_RAYS * 0.92
        r = math.sqrt(max(1.0 - z * z, 0.0))
        local.append((math.cos(golden * i) * r, math.sin(golden * i) * r, z))
    hidden = 0
    for index, face in enumerate(sink.faces):
        if sink.material[index] == MAGMA:
            continue
        pts = [verts[v] for v in face]
        center = sum(pts, Vector()) / len(pts)
        normal = Vector()
        for k in range(len(pts)):
            a, b = pts[k], pts[(k + 1) % len(pts)]
            normal += Vector(((a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)))
        if normal.length < 1e-9:
            continue
        normal.normalize()
        tangent = normal.cross(Vector((0.0, 0.0, 1.0)) if abs(normal.z) < 0.9 else Vector((1.0, 0.0, 0.0))).normalized()
        bitangent = normal.cross(tangent)
        origin = center + normal * 0.004
        blocked = True
        for (lx, ly, lz) in local:
            direction = tangent * lx + bitangent * ly + normal * lz
            if tree.ray_cast(origin, direction, HIDE_DISTANCE)[0] is None:
                blocked = False
                break
        if blocked:
            sink.material[index] = HIDDEN
            hidden += 1
    return hidden, tree


def rock_material(name, dark, light, moss, glow, bake_size):
    s = kit.Shader(name)
    p = s.coords('Object')
    _, _, z = s.separate(p)
    island = s.geometry('Random Per Island')
    q = s.offset(p, s.combine(s.mul(island, 37.0), s.mul(island, 53.0), s.mul(island, 71.0)))
    large = s.noise(p, 0.16, detail=3.0)
    mid = s.noise(q, 1.5, detail=5.0, roughness=0.55)
    fine = s.noise(q, 9.0, detail=3.0)
    tone = s.clamp01(s.add(s.mul(large, 0.45), s.add(s.mul(mid, 0.3), s.mul(island, 0.35))))
    base = s.mix(tone, dark, light)
    blotch = s.smooth(0.55, 0.75, s.noise(q, 0.7, detail=3.0))
    base = s.mix(s.mul(blotch, 0.3), base, (light[0] * 0.85, light[1] * 0.72, light[2] * 0.6))
    warped = s.offset(q, s.mul(s.noise_color(q, 1.1, detail=2.0), 0.35))
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.016, s.voronoi(warped, 0.7, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.52, 0.64, s.noise(q, 0.45, detail=3.0)))
    hair = s.mul(s.sub(1.0, s.smooth(0.0, 0.012, s.voronoi(warped, 1.8, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.62, 0.76, s.noise(q, 1.0, detail=2.0)))
    pits = s.mul(s.sub(1.0, s.smooth(0.0, 0.22, s.voronoi(q, 26.0, 'F1', 1.0, 'Distance'))), s.smooth(0.45, 0.7, s.noise(q, 2.4, detail=2.0)))
    edge = pk.wear(s, 0.04)
    chipped = s.mul(edge, s.smooth(0.4, 0.6, s.noise(q, 6.0, detail=3.0)))
    streaks = s.smooth(0.55, 0.78, s.noise(s.stretch(q, (6.0, 6.0, 0.35)), 1.3, detail=4.0, roughness=0.6))
    dirt = pk.cavity(s, 0.3)
    base = s.mix(s.mul(streaks, 0.35), base, (dark[0] * 0.7, dark[1] * 0.68, dark[2] * 0.62))
    base = s.mix(s.mul(pits, 0.6), base, (dark[0] * 0.5, dark[1] * 0.5, dark[2] * 0.5))
    base = s.mix(s.mul(chipped, 0.6), base, (light[0] * 1.15, light[1] * 1.12, light[2] * 1.08))
    base = s.mix(s.mul(s.add(cracks, s.mul(hair, 0.6)), 0.85), base, (0.012, 0.01, 0.009))
    base = s.mix(s.mul(dirt, 0.75), base, (0.02, 0.016, 0.013))
    up = s.smooth(0.45, 0.85, s.separate(s.geometry('Normal'))[2])
    patches = s.smooth(0.42, 0.62, s.noise(p, 1.4, detail=5.0, roughness=0.6))
    growth = s.clamp01(s.mul(s.add(s.mul(up, patches), s.mul(s.mul(dirt, patches), 0.5)), moss))
    moss_tone = s.mix(s.noise(p, 6.0, detail=3.0), (0.045, 0.07, 0.02), (0.11, 0.15, 0.04))
    base = s.mix(growth, base, moss_tone)
    rough = s.remap(fine, 0.3, 0.7, 0.78, 0.92)
    rough = s.lerp(s.mul(chipped, 0.5), rough, 0.7)
    height = s.add(s.mul(mid, 0.5), s.mul(fine, 0.12))
    height = s.sub(height, s.add(s.mul(cracks, 0.6), s.add(s.mul(hair, 0.2), s.add(s.mul(chipped, 0.3), s.mul(pits, 0.2)))))
    emission = None
    if glow > 0.0:
        veins_warp = s.offset(p, s.mul(s.noise_color(p, 0.6, detail=3.0), 0.8))
        veins = s.sub(1.0, s.smooth(0.0, 0.025, s.voronoi(veins_warp, 0.55, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
        near = s.sub(1.0, s.smooth(5.0, 9.0, s.vector('LENGTH', s.combine(s.separate(p)[0], s.separate(p)[1], 0.0))))
        field = s.noise(p, 0.14, detail=2.0)
        mask = s.smooth(s.sub(0.6, s.mul(near, 0.08)), s.sub(0.68, s.mul(near, 0.08)), field)
        deep = s.sub(1.0, s.smooth(-6.0, -2.5, z))
        emission = s.clamp01(s.add(s.mul(cracks, mask), s.mul(veins, s.mul(mask, deep))))
        base = s.mix(s.mul(emission, 0.8), base, (0.05, 0.015, 0.005))
        height = s.sub(height, s.mul(veins, s.mul(mask, 0.4)))
    return s.finish(base, rough, height=height, bump=0.9, bump_distance=0.04, emission=emission, emission_color=(1.0, 0.3, 0.05), emission_strength=glow, bake_size=bake_size)


def hidden_material():
    s = kit.Shader('EmberHidden')
    return s.finish((0.02, 0.018, 0.016), 0.92, bake_size=256)


def magma_material():
    s = kit.Shader('EmberMagma')
    p = s.coords('Object')
    hot = s.smooth(0.35, 0.7, s.noise(p, 1.3, detail=3.0))
    return s.finish(s.mix(hot, (0.08, 0.02, 0.008), (0.5, 0.12, 0.02)), 0.6, emission=s.add(0.35, s.mul(hot, 0.65)), emission_color=(1.0, 0.28, 0.04), emission_strength=5.0, bake_size=128)


def bed_maps(half, size, ao, stones):
    texel = 2.0 * half / size
    coords = ((np.arange(size, dtype=np.float32) + 0.5) * texel - half).astype(np.float32)
    x, y = np.meshgrid(coords, coords)
    r = np.hypot(x, y)

    def hash01(*keys):
        h = np.zeros_like(keys[0], dtype=np.float64)
        for k, key in enumerate(keys):
            h = h + np.asarray(key, np.float64) * (127.1 + 311.7 * k)
        return (np.sin(h) * 43758.5453) % 1.0

    def fbm(scale, seed, octaves=4):
        total = np.zeros_like(x)
        amp = 0.5
        for o in range(octaves):
            f = scale * (2.0 ** o)
            gx, gy = x * f + seed * 13.1, y * f - seed * 7.7
            ix, iy = np.floor(gx), np.floor(gy)
            fx, fy = gx - ix, gy - iy
            ux, uy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
            a = hash01(ix, iy, seed + o)
            b = hash01(ix + 1, iy, seed + o)
            c = hash01(ix, iy + 1, seed + o)
            d = hash01(ix + 1, iy + 1, seed + o)
            total += amp * ((a * (1 - ux) + b * ux) * (1 - uy) + (c * (1 - ux) + d * ux) * uy).astype(np.float32)
            amp *= 0.5
        return total / (1.0 - 0.5 ** octaves)

    near = stones.copy()
    for _ in range(6):
        near = np.maximum(near, np.maximum(np.maximum(np.roll(near, 1, 0), np.roll(near, -1, 0)), np.maximum(np.roll(near, 1, 1), np.roll(near, -1, 1))) * 0.86)
    joint = np.clip(near - stones, 0.0, 1.0)
    blades = fbm(9.0, 61.0, 3)
    grain = fbm(3.5, 31.0, 3)
    patches = fbm(0.4, 41.0)
    soilmask = smoothstep(0.55, 0.72, fbm(1.2, 51.0, 3))
    open_ground = 1.0 - near
    grass = np.clip(open_ground * smoothstep(0.25, 0.5, blades + 0.25 * patches) + joint * 0.5 * smoothstep(0.4, 0.7, blades), 0.0, 1.0)
    soil = np.clip(joint * 0.7 + open_ground * soilmask * 0.6, 0.0, 1.0) * (1.0 - 0.6 * grass)
    blade_light = smoothstep(0.35, 0.8, blades)
    grass_tone = np.array((0.035, 0.062, 0.016), np.float32) * (0.55 + 0.9 * blade_light)[..., None] + np.array((0.05, 0.04, 0.0), np.float32) * (smoothstep(0.55, 0.8, patches) * blade_light)[..., None]
    soil_tone = np.array((0.06, 0.047, 0.034), np.float32) * (0.7 + 0.5 * grain)[..., None]
    moss_tone = np.array((0.045, 0.065, 0.02), np.float32) * (0.7 + 0.6 * grain)[..., None]
    base = moss_tone * (1.0 - soil)[..., None] + soil_tone * soil[..., None]
    base = base * (1.0 - grass)[..., None] + grass_tone * grass[..., None]
    flowers = (hash01(np.floor(x * 22.0), np.floor(y * 22.0), 71.0) > 0.988) * grass * smoothstep(0.5, 0.65, patches)
    base = base * (1.0 - flowers)[..., None] + np.array((0.55, 0.42, 0.6), np.float32) * flowers[..., None]
    base = base * (0.4 + 0.6 * ao)[..., None]
    height = 0.006 * blades * grass - 0.004 * joint
    gy_, gx_ = np.gradient(height, texel)
    normal = np.stack((-gx_, -gy_, np.ones_like(gx_)), axis=-1)
    normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
    normal_rgb = (normal * 0.5 + 0.5).astype(np.float32)
    orm = np.ones(base.shape[:2] + (3,), np.float32)
    orm[..., 0] = np.clip(0.3 + 0.7 * ao, 0.0, 1.0)
    orm[..., 1] = np.clip(0.88 + 0.08 * (grain - 0.5), 0.6, 0.98)
    orm[..., 2] = 0.0
    heat = 1.0 - smoothstep(SURROUND, SURROUND + 1.2, r)
    emit = np.clip(joint * heat * (0.6 + 0.4 * grain), 0.0, 1.0)
    return np.clip(base, 0.0, 1.0), normal_rgb, orm, emit


def paving_stone_material(name, light, dark, bake_size):
    s = kit.Shader(name)
    p = s.coords('Object')
    island = s.geometry('Random Per Island')
    q = s.offset(p, s.combine(s.mul(island, 37.0), s.mul(island, 53.0), s.mul(island, 71.0)))
    mid = s.noise(q, 3.0, detail=5.0, roughness=0.55)
    fine = s.noise(q, 18.0, detail=3.0)
    tone = s.clamp01(s.add(s.mul(mid, 0.35), s.mul(island, 0.55)))
    base = s.mix(tone, dark, light)
    warm = s.smooth(0.6, 0.85, s.noise(q, 1.2, detail=2.0))
    base = s.mix(s.mul(warm, 0.35), base, (light[0] * 1.05, light[1] * 0.92, light[2] * 0.78))
    pits = s.mul(s.sub(1.0, s.smooth(0.0, 0.25, s.voronoi(q, 40.0, 'F1', 1.0, 'Distance'))), s.smooth(0.4, 0.7, s.noise(q, 3.0, detail=2.0)))
    warped = s.offset(q, s.mul(s.noise_color(q, 2.0, detail=2.0), 0.25))
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.012, s.voronoi(warped, 1.6, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.55, 0.68, s.noise(q, 0.9, detail=2.0)))
    lichen = s.mul(s.sub(1.0, s.smooth(0.0, 0.06, s.voronoi(q, 9.0, 'F1', 1.0, 'Distance'))), s.smooth(0.55, 0.75, s.noise(q, 1.8, detail=3.0)))
    edge = pk.wear(s, 0.02)
    dirt = pk.cavity(s, 0.08)
    base = s.mix(s.mul(pits, 0.6), base, (dark[0] * 0.6, dark[1] * 0.6, dark[2] * 0.6))
    base = s.mix(s.mul(lichen, 0.55), base, (0.32, 0.31, 0.22))
    base = s.mix(s.mul(edge, 0.4), base, (light[0] * 1.2, light[1] * 1.18, light[2] * 1.12))
    base = s.mix(s.mul(cracks, 0.9), base, (0.02, 0.018, 0.015))
    base = s.mix(s.mul(dirt, 0.8), base, (0.04, 0.045, 0.02))
    rough = s.remap(fine, 0.3, 0.7, 0.72, 0.9)
    rough = s.lerp(s.mul(edge, 0.4), rough, 0.6)
    height = s.sub(s.add(s.mul(mid, 0.5), s.mul(fine, 0.12)), s.add(s.mul(cracks, 0.7), s.mul(pits, 0.25)))
    return s.finish(base, rough, height=height, bump=0.6, bump_distance=0.01, bake_size=bake_size)


def deck_ao(deck_obj, others, size):
    scene = bpy.context.scene
    scene.cycles.samples = 64
    scene.world.light_settings.distance = 1.2
    original = list(deck_obj.data.materials)
    temp = bpy.data.materials.new('DeckAOTarget')
    temp.use_nodes = True
    holder = temp.node_tree.nodes.new('ShaderNodeTexImage')
    temp.node_tree.nodes.active = holder
    image = bpy.data.images.new('deck_ao', size, size, alpha=False, float_buffer=True)
    holder.image = image
    deck_obj.data.materials.clear()
    deck_obj.data.materials.append(temp)
    hidden = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o is not deck_obj and o not in others]
    for obj in hidden:
        obj.hide_render = True
    kit.select_only([deck_obj])
    bpy.ops.object.bake(type='AO', margin=16)
    for obj in hidden:
        obj.hide_render = False
    ao = kit.image_pixels(image)[..., 0]
    bpy.data.images.remove(image)
    deck_obj.data.materials.clear()
    for m in original:
        deck_obj.data.materials.append(m)
    return ao


def write_maps(folder, slot, base, normal, orm, emit=None):
    files = {}
    files['albedo'] = os.path.join(folder, f'{NAME}_{slot}_albedo.png')
    kit.save_png(files['albedo'], base, srgb=True)
    files['normal'] = os.path.join(folder, f'{NAME}_{slot}_normal.png')
    kit.save_png(files['normal'], normal)
    files['orm'] = os.path.join(folder, f'{NAME}_{slot}_orm.png')
    kit.save_png(files['orm'], orm)
    if emit is not None:
        files['emit'] = os.path.join(folder, f'{NAME}_{slot}_emit.png')
        kit.save_png(files['emit'], np.repeat(emit[..., None], 3, axis=2))
    return files


def mirror(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for v in bm.verts:
        v.co.x = -v.co.x
    bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def build(target, rng):
    started = time.time()
    env = Envelope()
    mats = {
        BASALT: rock_material('EmberBasalt', (0.028, 0.024, 0.021), (0.1, 0.082, 0.068), 0.25, 5.0, 4096),
        LIMESTONE: rock_material('EmberLimestone', (0.13, 0.118, 0.1), (0.36, 0.33, 0.28), 0.9, 0.0, 4096),
        MAGMA: magma_material(),
        HIDDEN: hidden_material(),
        STONE: paving_stone_material('EmberPavingStone', (0.42, 0.39, 0.34), (0.2, 0.185, 0.16), 4096),
        KERB: paving_stone_material('EmberKerb', (0.17, 0.155, 0.14), (0.07, 0.064, 0.058), 2048),
    }
    points = seeds(rng)
    cells, tree = voronoi(points, rng, outward=island_outward)
    kinds = formations(points, rng)
    sink = Sink()
    bottoms = {}
    columns = 0
    for i, poly in enumerate(cells):
        if poly is None:
            continue
        x, y, d, kind = points[i]
        c = centroid(poly)
        crack = fbm2(c.x, c.y, 0.16, 11.7) > CRACK_LEVEL
        near = c.length < CAVERN_REACH + 2.5 and kind == 'core'
        gap = rng.uniform(*(CRACK_GAP if crack and kind == 'core' and not near else GAP))
        poly = shrink(poly, gap * 0.5)
        if len(poly) < 3 or abs(area(poly)) < 0.1:
            continue
        thick = max(env.thickness(c.x, c.y) * kinds[i].depth, rng.uniform(*RIM_THICKNESS)) * rng.uniform(0.93, 1.06)
        if near:
            thick = max(thick, cavern_need(c.length))
        if kind == 'rim':
            z_top = ground(c.x, c.y) + rng.uniform(*RIM_TOP)
        elif kind == 'shelf':
            z_top = -rng.uniform(*SHELF_TOP)
            thick = rng.uniform(*SHELF_THICKNESS) - z_top
        else:
            z_top = -DECK_DROP
        bottoms[i] = column(sink, poly, z_top, -thick, kind, rng, kinds[i], near)
        columns += 1
    sheets = magma_sheets(sink, cells, points, tree, bottoms, rng)
    edge_stones(sink, rng)
    outcrops(sink, rng)
    rough_tree = BVHTree.FromPolygons([Vector(v) for v in sink.verts], sink.faces)
    slabs(sink, rough_tree, rng)
    fragments(sink, rng)
    sink.add(mass(env), HIDDEN, 'Mass')
    print(f'columns {columns} sheets {sheets} faces {len(sink.faces)} in {time.time() - started:.0f} s')
    paving_mesh = build_paving(rng)
    print(f'paving stones {len(paving_mesh.polys)} faces {len(paving_mesh.sink.faces)} in {time.time() - started:.0f} s')
    bed = bpy.data.materials.new('EmberBed')
    bed.use_nodes = True
    bed.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.05, 0.07, 0.025, 1.0)
    plaza = deck(target, bed)
    stones = paving_mesh.sink.build(target, mats, smooth=True)
    for obj in stones:
        obj.data.set_sharp_from_angle(angle=math.radians(35.0))
    hidden, _ = visibility(sink, [plaza] + stones)
    print(f'hidden faces {hidden}')
    rocks = sink.build(target, mats)
    objects = rocks + stones + [plaza]
    for obj in objects:
        tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
        print('TRIS', obj.name, tris)
    print('TRIS total', sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in objects))
    print(f'built in {time.time() - started:.0f} s')
    return objects, plaza, paving_mesh.polys


def report():
    print('== engine constants (Blender x, y, z; engine = x, z, -y in model space) ==')
    print('heart', tuple(round(v, 3) for v in HEART), 'radius', HEART_R)
    print('deck half', round(deck_half(), 3))


def setup_sky():
    world = bpy.context.scene.world
    nt = world.node_tree
    nt.nodes.clear()
    sky = nt.nodes.new('ShaderNodeTexSky')
    sky.sky_type = 'NISHITA'
    sky.sun_elevation = math.radians(38)
    sky.sun_rotation = math.radians(215)
    bg = nt.nodes.new('ShaderNodeBackground')
    bg.inputs['Strength'].default_value = 0.2
    out = nt.nodes.new('ShaderNodeOutputWorld')
    nt.links.new(sky.outputs[0], bg.inputs[0])
    nt.links.new(bg.outputs[0], out.inputs[0])
    sun = bpy.data.lights.new('Sun', 'SUN')
    sun.energy = 3.0
    sun.angle = math.radians(1.5)
    obj = bpy.data.objects.new('Sun', sun)
    bpy.context.scene.collection.objects.link(obj)
    obj.rotation_euler = (math.radians(50), 0, math.radians(35))
    light = bpy.data.lights.new('HeartLight', 'POINT')
    light.energy = 12000.0
    light.color = (1.0, 0.45, 0.15)
    light.shadow_soft_size = 1.0
    holder = bpy.data.objects.new('HeartLight', light)
    bpy.context.scene.collection.objects.link(holder)
    holder.location = HEART
    for name in ('HeartCore', 'HeartCrust'):
        if name in bpy.data.objects:
            bpy.data.objects[name].visible_shadow = False
    bpy.context.scene.view_settings.view_transform = 'AgX'
    bpy.context.scene.view_settings.exposure = -0.6


def camera_shot(path, focus, yaw, pitch, distance, size=(1400, 900), samples=48, ortho=None, fov=40.0):
    scene = bpy.context.scene
    data = bpy.data.cameras.new('Shot')
    data.angle = math.radians(fov)
    if ortho:
        data.type = 'ORTHO'
        data.ortho_scale = ortho
    data.clip_end = 1000
    camera = bpy.data.objects.new('Shot', data)
    scene.collection.objects.link(camera)
    yaw_r, pitch_r = math.radians(yaw), math.radians(pitch)
    direction = Vector((math.cos(pitch_r) * math.sin(yaw_r), -math.cos(pitch_r) * math.cos(yaw_r), math.sin(pitch_r)))
    camera.location = Vector(focus) + direction * distance
    camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = camera
    scene.render.resolution_x, scene.render.resolution_y = size
    scene.render.resolution_percentage = 100
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera)


def preview(folder, views):
    setup_sky()
    extent = H['EmberRadius']
    shots = {
        'q': ((0, 0, -5), 35, 24, extent * 3.0, None),
        'front': ((0, 0, -7), 0, 0, 200, extent * 2.6),
        'bottom': ((0, 0, -6), 20, -40, extent * 2.9, None),
        'top': ((0, 0, 0), 0, 89.9, 200, extent * 2.4),
        'game': ((0, -2, 0), 0, 57, 26.0, None),
        'field': ((-9.0, -7.0, 0), 0, 57, 13.0, None),
        'heart': ((0, 0, -2), 0, 57, 13, None),
        'rim': ((0, -edge(-math.pi / 2), -1.0), 15, 25, 9, None),
        'edge': ((6, -edge(-math.pi / 2) + 3.0, 0), 0, 57, 11, None),
        'under': ((0, -edge(-math.pi / 2) * 0.6, -9), 10, -28, 18, None),
        'cavern': (tuple(HEART), 0, 12, 5.2, None),
        'chamber': (tuple(HEART + Vector((0, 1.5, -1.0))), 200, 25, 4.6, None),
    }
    for view in views:
        focus, yaw, pitch, dist, ortho = shots[view]
        camera_shot(os.path.join(folder, f'island_{view}.png'), focus, yaw, pitch, dist, ortho=ortho, fov=40.0 if view not in ('cavern', 'chamber') else 85.0)


def export(objects, folder, paving_files):
    os.makedirs(folder, exist_ok=True)
    pk.unwrap_all([o for o in objects if not o.get('keep_uv') and o.name != 'Deck'], {'EmberLimestone': 0.0015, 'EmberBasalt': 0.0015, 'EmberHidden': 0.0005, 'EmberMagma': 0.004})
    materials = []
    for obj in objects:
        for slot in obj.material_slots:
            if slot.material and slot.material not in materials:
                materials.append(slot.material)
    baked, emissive, clear = {}, {}, {}
    for material in materials:
        started = time.time()
        if material.name == 'EmberBed':
            files = paving_files
            emissive[material.name] = ((1.0, 0.42, 0.12), 1.6)
        else:
            users = [obj for obj in objects if material.name in obj.data.materials]
            files = kit.bake_material(material, users, folder, NAME, ao_distance=AO_DISTANCE)
            if 'emit' in files:
                emissive[material.name] = (tuple(material['emit_color']), float(material['emit_strength']))
            if material.get('transmission', 0.0) > 0.0:
                clear[material.name] = (float(material['transmission']), float(material['ior']), [float(v) for v in material['attenuation']])
        baked[material.name] = kit.baked_material(material, files)
        print(f'baked {material.name} in {time.time() - started:.0f} s')
    for obj in objects:
        for slot in obj.material_slots:
            slot.material = baked[slot.material.name]
    for obj in objects:
        kit.select_only([obj])
        with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
            bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    kit.export_gltf(objects, os.path.join(folder, NAME + '.gltf'), emissive, None, None, False, None, clear)


def bed_files(objects, plaza, polys, folder):
    started = time.time()
    ao = deck_ao(plaza, [o for o in objects if o is not plaza and o.name != 'Glass'], BED_TEXTURE)
    half = deck_half()
    base, normal, orm, emit_map = bed_maps(half, BED_TEXTURE, ao, stone_mask(polys, half, BED_TEXTURE))
    files = write_maps(folder, 'EmberBed', base, normal, orm, emit=emit_map)
    print(f'bed maps in {time.time() - started:.0f} s')
    return files


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    opts = {}
    for i, arg in enumerate(argv):
        if arg.startswith('--'):
            opts[arg] = argv[i + 1] if i + 1 < len(argv) and not argv[i + 1].startswith('--') else '1'
    if '--envelope' in opts:
        extract_envelope()
        return
    kit.reset()
    bpy.context.preferences.filepaths.save_version = 0
    rng = random.Random(int(opts.get('--seed', SEED)))
    target = kit.collection(NAME)
    objects, plaza, polys = build(target, rng)
    for obj in objects:
        mirror(obj)
    report()
    if '--paving' in opts:
        bed_files(objects, plaza, polys, os.path.abspath(opts['--paving']))
        return
    out = opts.get('--out')
    files = None
    if out:
        out = os.path.abspath(out)
        os.makedirs(out, exist_ok=True)
        files = bed_files(objects, plaza, polys, out)
    if opts.get('--preview'):
        folder = os.path.abspath(opts['--preview'])
        os.makedirs(folder, exist_ok=True)
        if files:
            plaza.data.materials[0] = kit.baked_material(bpy.data.materials['EmberBed'], files)
        preview(folder, opts.get('--views', 'q,game,heart,rim').split(','))
        if files:
            plaza.data.materials[0] = bpy.data.materials['EmberBed']
    if out:
        export(objects, out, files)
        bpy.ops.outliner.orphans_purge(do_recursive=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, NAME + '.blend'), compress=True)


if __name__ == '__main__':
    main()
