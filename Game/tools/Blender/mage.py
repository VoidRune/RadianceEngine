import bpy
import bmesh
import heapq
import math
import os
import sys
import numpy as np
from mathutils import Matrix, Vector, kdtree
from mathutils.bvhtree import BVHTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import heroanim
import magegear as gear
import paladinkit as pkit

NAME = 'Hero'
SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'Mage.obj')
SHIFT = Vector((-0.05, 0.04, 0.0))
STAFF = (Vector((-0.275, -0.215, 0.0)), Vector((-0.335, -0.25, 1.25)))
SIDES = (('l', 1.0), ('r', -1.0))
CLOTH, CAPE, PLATE, TABARD, LEATHER, GOLD, SKIN, PAPER, LINING, BOOT, UNDER = range(11)
TONE = {
    CLOTH: (0.036, 0.024, 0.03), CAPE: (0.036, 0.024, 0.03), PLATE: (0.04, 0.026, 0.03), TABARD: (0.5, 0.42, 0.3), LEATHER: (0.075, 0.045, 0.03),
    GOLD: gear.GOLD, SKIN: (0.5, 0.33, 0.245), PAPER: (0.58, 0.51, 0.38), LINING: (0.07, 0.03, 0.036), BOOT: (0.06, 0.036, 0.024), UNDER: (0.02, 0.014, 0.018),
}
ROUGH = {CLOTH: 0.86, CAPE: 0.86, PLATE: 0.84, TABARD: 0.8, LEATHER: 0.5, GOLD: 0.28, SKIN: 0.58, PAPER: 0.82, LINING: 0.8, BOOT: 0.42, UNDER: 0.9}
FABRIC = {CLOTH: 1.0, CAPE: 1.0, PLATE: 1.0, TABARD: 0.8, LEATHER: 0.4, GOLD: 0.0, SKIN: 0.0, PAPER: 0.1, LINING: 0.8, BOOT: 0.35, UNDER: 1.0}
TRIM = {CLOTH: 0.016, CAPE: 0.0, PLATE: 0.0, TABARD: 0.017, LEATHER: 0.009, GOLD: 0.0, SKIN: 0.0, PAPER: 0.0, LINING: 0.0, BOOT: 0.007, UNDER: 0.0}
GOLD_TONE = np.array(gear.GOLD)
MATERIALS = ('Garb', 'Gear', 'Skin', 'Cape')
SIZES = {'Garb': 2048, 'Gear': 2048, 'Skin': 512, 'Cape': 2048}
NODES = ('Wizard_Garb', 'Wizard_Cape', 'Hero_Head')
GLOW = ((1.0, 0.5, 0.12), 5.0)
ROBE_CHAINS = 6
ROBE_CENTER = (0.0, 0.02)
ROBE_ROWS = ((0.98, 0.17), (0.52, 0.24), (0.1, 0.31))
ROBE_LIFT = (0.1, 0.05)
ROBE_FLARE = (0.2, 0.3)
ROBE_FOLDS = 0.011
WIDEN = (0.09, 0.04)
POUCH_DEPTH = (0.19, 0.55)
SOURCE_LEGS = {
    'r': ((-0.09, 0.03, 0.95), (-0.12, 0.06, 0.5), (-0.135, 0.1, 0.09), (-0.16, 0.0, 0.03), (-0.17, -0.07, 0.02)),
    'l': ((0.09, -0.01, 0.95), (0.15, -0.07, 0.5), (0.21, -0.06, 0.09), (0.215, -0.2, 0.03), (0.22, -0.28, 0.02)),
}
LEGS = {
    "r": ((-0.09, 0.02, 0.95), (-0.125, 0.0, 0.5), (-0.155, 0.025, 0.09), (-0.17, -0.09, 0.03), (-0.178, -0.165, 0.02)),
    "l": ((0.09, 0.02, 0.95), (0.125, 0.0, 0.5), (0.155, 0.025, 0.09), (0.17, -0.09, 0.03), (0.178, -0.165, 0.02)),
}
OLD_ARMS = {
    'r': ((-0.045, 0.0, 1.47), (-0.19, 0.03, 1.46), (-0.27, 0.11, 1.23), (-0.345, -0.11, 1.25), (-0.36, -0.14, 1.25), (-0.42, -0.2, 1.25)),
    'l': ((0.03, 0.0, 1.47), (0.21, 0.02, 1.45), (0.28, -0.05, 1.21), (0.275, -0.16, 0.97), (0.275, -0.165, 0.93), (0.27, -0.17, 0.82)),
}
ARMS = {}
DORSAL = {}
SLOTS = {}
CAPE_CHAINS = []
PATCHES = []
CHEST_GEM = (-0.012, 1.318)
BELT_GEM = (-0.08, 1.112)


def smoothstep(low, high, value):
    t = np.clip((value - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def flood(bm, seeds, keep):
    seen = set(v.index for v in seeds)
    stack = list(seeds)
    while stack:
        a = stack.pop()
        for e in a.link_edges:
            b = e.other_vert(a)
            if b.index not in seen and keep(b.co):
                seen.add(b.index)
                stack.append(b)
    return seen


def load_source(target):
    bpy.ops.wm.obj_import(filepath=SOURCE)
    obj = bpy.context.selected_objects[0]
    for collection in list(obj.users_collection):
        collection.objects.unlink(obj)
    target.objects.link(obj)
    kit.select_only([obj])
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    low = min(v.co.z for v in bm.verts)
    for v in bm.verts:
        v.co.z -= low
    lower = [v for v in bm.verts if v.co.z < 1.0 and (v.co.xy - STAFF[0].lerp(STAFF[1], v.co.z / STAFF[1].z).xy).length < 0.05]
    upper = [v for v in bm.verts if v.co.z > 1.4 and v.co.x < -0.3 and v.co.y < -0.17]
    doomed = flood(bm, lower, lambda co: co.z < 1.192) | flood(bm, upper, lambda co: co.z > 1.303)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.index in doomed], context='VERTS')
    bm.verts.index_update()
    bm.verts.ensure_lookup_table()
    seen = set()
    parts = []
    for v in bm.verts:
        if v.index not in seen:
            part = flood(bm, [v], lambda co: True)
            seen |= part
            parts.append(part)
    parts.sort(key=len, reverse=True)
    for part in parts[1:]:
        bmesh.ops.delete(bm, geom=[bm.verts[i] for i in part], context='VERTS')
        bm.verts.ensure_lookup_table()
    filled = bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0)
    bmesh.ops.triangulate(bm, faces=filled['faces'])
    for v in bm.verts:
        v.co += SHIFT
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for _ in range(2):
        bmesh.ops.smooth_vert(bm, verts=bm.verts, factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.to_mesh(obj.data)
    bm.free()
    obj.name = 'MageSource'
    obj.data.shade_smooth()
    return obj


def refine(obj, limit=0.014):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for _ in range(3):
        long = [e for e in bm.edges if e.calc_length() > limit]
        if not long:
            break
        bmesh.ops.subdivide_edges(bm, edges=long, cuts=1)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


class Surface:
    def __init__(self, obj):
        mesh = obj.data
        n = len(mesh.vertices)
        self.count = n
        self.co = np.zeros(n * 3)
        mesh.vertices.foreach_get('co', self.co)
        self.co = self.co.reshape(n, 3)
        self.normal = np.zeros(n * 3)
        mesh.vertices.foreach_get('normal', self.normal)
        self.normal = self.normal.reshape(n, 3)
        edges = np.zeros(len(mesh.edges) * 2, dtype=np.int64)
        mesh.edges.foreach_get('vertices', edges)
        self.edges = edges.reshape(-1, 2)
        lengths = np.linalg.norm(self.co[self.edges[:, 0]] - self.co[self.edges[:, 1]], axis=1)
        self.links = [[] for _ in range(n)]
        for (a, b), length in zip(self.edges.tolist(), lengths.tolist()):
            self.links[a].append((b, length))
            self.links[b].append((a, length))

    def flood(self, seeds, keep):
        seen = np.zeros(self.count, dtype=bool)
        seen[seeds] = True
        stack = list(seeds)
        while stack:
            a = stack.pop()
            for b, _ in self.links[a]:
                if not seen[b] and keep[b]:
                    seen[b] = True
                    stack.append(b)
        return seen

    def pick(self, low, high, box):
        inside = (self.co[:, 2] >= low) & (self.co[:, 2] < high)
        todo = set(np.nonzero(inside)[0].tolist())
        best = None
        while todo:
            seed = todo.pop()
            part = [seed]
            stack = [seed]
            while stack:
                a = stack.pop()
                for b, _ in self.links[a]:
                    if b in todo:
                        todo.remove(b)
                        part.append(b)
                        stack.append(b)
            p = self.co[part]
            lo, hi = p.min(axis=0), p.max(axis=0)
            if lo[0] >= box[0] and hi[0] <= box[1] and lo[1] >= box[2] and hi[1] <= box[3] and len(part) > 20 and (best is None or len(part) > len(best)):
                best = part
        return best

    def distance(self, sources, limit):
        dist = np.full(self.count, limit)
        heap = [(0.0, int(s)) for s in sources]
        dist[sources] = 0.0
        heapq.heapify(heap)
        while heap:
            d, a = heapq.heappop(heap)
            if d > dist[a]:
                continue
            for b, length in self.links[a]:
                nd = d + length
                if nd < dist[b]:
                    dist[b] = nd
                    heapq.heappush(heap, (nd, b))
        return dist

    def coherence(self, passes):
        averaged = self.soften(self.normal.copy(), passes)
        return np.linalg.norm(averaged, axis=1)

    def soften(self, values, passes):
        a, b = self.edges[:, 0], self.edges[:, 1]
        degree = np.maximum(np.bincount(a, minlength=self.count) + np.bincount(b, minlength=self.count), 1)[:, None]
        for _ in range(passes):
            total = np.zeros_like(values)
            np.add.at(total, a, values[b])
            np.add.at(total, b, values[a])
            values = 0.5 * values + 0.5 * total / degree
        return values


def capsule(co, a, b, radius=None):
    a, b = np.array(a), np.array(b)
    ab = b - a
    t = np.clip(((co - a) @ ab) / (ab @ ab), 0.0, 1.0)
    distance = np.linalg.norm(co - (a + t[:, None] * ab), axis=1)
    return (distance < radius if radius is not None else distance), t


def source_zones(surface):
    co, normal = surface.co, surface.normal
    x, y, z = co[:, 0], co[:, 1], co[:, 2]
    zone = np.full(surface.count, CLOTH, dtype=np.int32)
    radial = np.stack([x + 0.02, y - 0.02, np.zeros_like(x)], axis=1)
    radial /= np.maximum(np.linalg.norm(radial, axis=1, keepdims=True), 1e-6)
    facing = np.sum(normal * radial, axis=1)
    zone[surface.flood(surface.pick(0.62, 0.68, (-0.15, 0.45, 0.05, 0.5)), z < 1.25)] = CAPE
    zone[(zone == CLOTH) & (y > 0.07) & (z >= 1.2) & (z < 1.52) & (normal[:, 1] > 0.2)] = CAPE
    zone[(zone == CAPE) & (facing < -0.2)] = LINING
    near_right, _ = capsule(co, OLD_ARMS['r'][1], OLD_ARMS['r'][2], 0.085)
    near_left, _ = capsule(co, OLD_ARMS['l'][1], OLD_ARMS['l'][2], 0.085)
    side = np.abs(x + 0.01)
    mantle = ((zone == CLOTH) | (zone == CAPE)) & (z > 1.2) & (z < 1.63) & ((side > 0.19) | ((z > 1.42) & (side > 0.1))) & ~near_right & ~near_left & (np.hypot(x, y + 0.06) > 0.13 + 0.9 * np.maximum(z - 1.5, 0.0))
    zone[mantle] = PLATE
    for name in 'rl':
        hip, knee, ankle, ball, toe = SOURCE_LEGS[name]
        foot, _ = capsule(co, ankle, toe, 0.1)
        shin, _ = capsule(co, ankle, (ankle[0], ankle[1], 0.33), 0.085)
        zone[(foot | shin) & (z < 0.33)] = BOOT
    arms = np.zeros(surface.count, dtype=bool)
    for name, joints in OLD_ARMS.items():
        dist = np.full(surface.count, 9.0)
        for a, b in zip(joints[1:], joints[2:]):
            d, _ = capsule(co, a, b)
            dist = np.minimum(dist, d)
        start, elbow = np.array(joints[1]), np.array(joints[2])
        along = (co - start) @ ((elbow - start) / np.linalg.norm(elbow - start))
        keep = (dist < 0.1) & (along > 0.07)
        seeds = np.nonzero(keep & (np.linalg.norm(co - np.array(joints[4]), axis=1) < 0.05))[0]
        arms |= surface.flood(seeds, keep)
    for name, (hip, knee, ankle, ball, toe) in SOURCE_LEGS.items():
        dist = np.minimum.reduce([capsule(co, a, b)[0] for a, b in ((knee, ankle), (ankle, ball), (ball, toe))])
        keep = (dist < 0.13) & (z < 0.62)
        arms |= surface.flood(np.nonzero(keep & (zone == BOOT))[0], keep)
    return zone, arms


def flaps(obj, surface):
    co, normal = surface.co, surface.normal
    bvh = BVHTree.FromPolygons([Vector(p) for p in co], [tuple(poly.vertices) for poly in obj.data.polygons])
    region = (co[:, 2] > 1.24) & (co[:, 2] < 1.5) & (np.abs(co[:, 0] + 0.01) > 0.045) & (co[:, 1] < 0.02)
    thin = np.zeros(surface.count, dtype=bool)
    for index in np.nonzero(region)[0]:
        hit = bvh.ray_cast(Vector(co[index] - normal[index] * 1e-4), Vector(-normal[index]), 0.03)
        thin[index] = hit[0] is not None and hit[3] < 0.011
    thin = surface.soften(thin.astype(np.float64)[:, None], 2)[:, 0] > 0.5
    print('flap vertices', int(thin.sum()))
    return thin & region


def strip(obj):
    surface = Surface(obj)
    zone, arms = source_zones(surface)
    co = surface.co
    robe = np.isin(zone, (CLOTH, UNDER, TABARD)) & (co[:, 2] < 0.85)
    front = np.clip(-(co[:, 1] - 0.02) / np.maximum(np.hypot(co[:, 0], co[:, 1] - 0.02), 1e-6), 0.0, 1.0)
    lift = robe * (ROBE_LIFT[0] + ROBE_LIFT[1] * front) * smoothstep(0.85, 0.2, co[:, 2])
    lift = surface.soften(lift[:, None], 6)[:, 0]
    lift[zone == BOOT] = 0.0
    dx, dy = co[:, 0] - ROBE_CENTER[0], co[:, 1] - ROBE_CENTER[1]
    radius = np.maximum(np.hypot(dx, dy), 1e-6)
    angle = np.arctan2(dx, -dy)
    hem = smoothstep(0.8, 0.15, co[:, 2]) * np.isin(zone, (CLOTH, UNDER))
    hem = surface.soften(hem[:, None], 4)[:, 0]
    inward = np.maximum(radius - ROBE_FLARE[0], 0.0) * ROBE_FLARE[1] * smoothstep(0.75, 0.15, co[:, 2]) * hem
    ripple = ROBE_FOLDS * hem * (0.6 * np.sin(11.0 * angle + 0.7) + 0.4 * np.sin(5.0 * angle + 2.1 + 3.0 * co[:, 2]))
    pouch = ((co[:, 0] > -0.33) & (co[:, 0] < -0.13) & (co[:, 2] > 0.92) & (co[:, 2] < 1.12) & (co[:, 1] > -0.16) & (co[:, 1] < 0.14)).astype(np.float64)
    pouch = surface.soften(pouch[:, None], 4)[:, 0]
    inward += np.maximum(radius - POUCH_DEPTH[0], 0.0) * POUCH_DEPTH[1] * pouch
    shift = (ripple - inward) / radius
    co[:, 0] += dx * shift
    co[:, 1] += dy * shift
    co[:, 2] += lift
    chest = smoothstep(1.0, 1.25, co[:, 2]) * (1.0 - smoothstep(1.5, 1.66, co[:, 2]))
    co[:, 0] += (co[:, 0] - ROBE_CENTER[0]) * WIDEN[0] * chest
    co[:, 1] += (co[:, 1] - ROBE_CENTER[1]) * WIDEN[1] * chest
    obj.data.vertices.foreach_set('co', co.ravel())
    doomed = arms | np.isin(zone, (CAPE, LINING, PLATE))
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in np.nonzero(doomed)[0]], context='VERTS')
    bm.verts.index_update()
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context='VERTS')
    seen = set()
    parts = []
    for v in bm.verts:
        if v.index not in seen:
            part = flood(bm, [v], lambda co: True)
            seen |= part
            parts.append(part)
    parts.sort(key=len, reverse=True)
    bm.verts.ensure_lookup_table()
    small = [bm.verts[i] for part in parts[1:] for i in part]
    bmesh.ops.delete(bm, geom=small, context='VERTS')
    filled = bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0)
    made = bmesh.ops.triangulate(bm, faces=filled['faces'])['faces']
    ring = {v for face in made for v in face.verts}
    for _ in range(2):
        long = list({e for v in ring if v.is_valid for e in v.link_edges if e.calc_length() > 0.015})
        result = bmesh.ops.subdivide_edges(bm, edges=long, cuts=1)
        ring |= {v for v in result['geom_inner'] + result['geom_split'] if isinstance(v, bmesh.types.BMVert)}
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    for _ in range(3):
        ring = {v for v in ring if v.is_valid}
        ring |= {e.other_vert(v) for v in list(ring) for e in v.link_edges}
    inner = [v for v in ring if v.is_valid]
    for _ in range(25):
        bmesh.ops.smooth_vert(bm, verts=inner, factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    PATCHES[:] = [tuple(v.co) for v in inner]
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    print('stripped', int(doomed.sum()), 'vertices, robe lift max', float(lift.max()), 'patch', len(PATCHES))


def body_zones(surface):
    co, normal = surface.co, surface.normal
    x, y, z = co[:, 0], co[:, 1], co[:, 2]
    zone = np.full(surface.count, CLOTH, dtype=np.int32)
    radial = np.stack([x + 0.02, y - 0.02, np.zeros_like(x)], axis=1)
    radial /= np.maximum(np.linalg.norm(radial, axis=1, keepdims=True), 1e-6)
    facing = np.sum(normal * radial, axis=1)
    zone[(x > -0.165) & (x < 0.005) & (z > 0.39) & (z < 1.05) & (y < -0.1) & (normal[:, 1] < 0.1)] = TABARD
    zone[(zone == CLOTH) & (z < 1.0) & (facing < -0.25) & (np.hypot(x + 0.02, y - 0.02) > 0.1)] = UNDER
    zone[(zone == CLOTH) & (z > 1.06) & (z < 1.15) & (np.hypot(x + 0.03, y) < 0.23)] = LEATHER
    zone[(x > -0.31) & (x < -0.15) & (z > 0.94) & (z < 1.1) & (y > -0.14) & (y < 0.12) & (np.hypot(x + 0.03, y) > 0.15)] = LEATHER
    book = (x > 0.0) & (x < 0.21) & (z > 0.86) & (z < 1.115) & (y < -0.17)
    zone[book] = np.where(np.abs(normal[book][:, 1]) > 0.55, LEATHER, PAPER)
    face = (x > -0.055) & (x < 0.045) & (z > 1.625) & (z < 1.765) & (y < -0.105) & (y > -0.2) & (normal[:, 1] < -0.45) & ((z < 1.745) | (y > -0.19))
    zone[face] = SKIN
    return zone, face


def set_color(obj, name, values):
    return gear.set_color(obj, name, values)


def book_frame(surface, zone):
    co = surface.co
    box = (co[:, 0] > 0.0) & (co[:, 0] < 0.21) & (co[:, 2] > 0.86) & (co[:, 2] < 1.115) & (co[:, 1] < -0.17) & np.isin(zone, (PAPER, LEATHER))
    points = co[box]
    center = points.mean(axis=0)
    _, _, axes = np.linalg.svd(points - center, full_matrices=False)
    local = (points - center) @ axes.T
    low, high = np.percentile(local, 2, axis=0), np.percentile(local, 98, axis=0)
    center = center + ((low + high) * 0.5) @ axes
    return center, axes, (high - low) * 0.5, box


def build_book(surface, zone, target):
    center, axes, half, box = book_frame(surface, zone)
    if axes[0][2] < 0.0:
        axes[0] = -axes[0]
    if axes[2][1] > 0.0:
        axes[2] = -axes[2]
    axes[1] = np.cross(axes[2], axes[0])
    local = (surface.co[box] - center) @ axes.T
    back = float(np.percentile(local[:, 2], 3))
    surface.co[box] = center + (local * np.array((0.75, 0.75, 0.0)) + np.array((0.0, 0.0, back + 0.006))) @ axes
    zone[box] = CLOTH
    tall, wide, thick = float(half[0]) * 0.92, float(half[1]) * 0.9, 0.03
    center = center + axes[2] * (back + thick + 0.004)
    frame = Matrix(((axes[0][0], axes[1][0], axes[2][0], center[0]), (axes[0][1], axes[1][1], axes[2][1], center[1]), (axes[0][2], axes[1][2], axes[2][2], center[2]), (0.0, 0.0, 0.0, 1.0)))
    leather, paper, gold = (0.085, 0.03, 0.022), TONE[PAPER], TONE[GOLD]
    pieces = [((tall * 0.93, wide * 0.9, thick * 0.8), (0.0, wide * 0.06, 0.0), paper, PAPER)]
    for side in (1.0, -1.0):
        pieces.append(((tall, wide, thick * 0.12), (0.0, 0.0, side * thick * 0.92), leather, LEATHER))
        for u in (1.0, -1.0):
            pieces.append(((tall * 0.16, wide * 0.2, thick * 0.16), (u * tall * 0.86, wide * 0.82, side * thick * 0.92), gold, GOLD))
            pieces.append(((tall * 0.16, wide * 0.2, thick * 0.16), (u * tall * 0.86, -wide * 0.82, side * thick * 0.92), gold, GOLD))
        pieces.append(((tall * 0.2, tall * 0.2, thick * 0.15), (0.0, 0.0, side * thick * 0.97), gold, GOLD))
    pieces.append(((tall * 1.0, wide * 0.1, thick * 1.02), (0.0, -wide * 0.93, 0.0), leather, LEATHER))
    for u in (0.55, -0.55):
        pieces.append(((tall * 0.05, wide * 0.13, thick * 1.06), (u * tall, -wide * 0.93, 0.0), gold, GOLD))
    pieces.append(((tall * 0.09, wide * 1.03, thick * 1.05), (tall * 0.3, 0.03 * wide, 0.0), leather, LEATHER))
    pieces.append(((tall * 0.13, wide * 0.09, thick * 0.3), (tall * 0.3, wide * 1.0, 0.0), gold, GOLD))
    bm = bmesh.new()
    tones = []
    for size, offset, tone, kind in pieces:
        made = bmesh.ops.create_cube(bm, size=2.0)['verts']
        for v in made:
            v.co = Vector((v.co.x * size[0] + offset[0], v.co.y * size[1] + offset[1], v.co.z * size[2] + offset[2]))
        tones.append((made, tone, kind))
    index = {}
    for made, tone, kind in tones:
        for v in made:
            index[v] = (tone, kind)
    bmesh.ops.transform(bm, matrix=frame, verts=bm.verts)
    points, values = [], []
    for v, (tone, kind) in index.items():
        points.append(v.co.copy())
        values.append(gear.look(tone, ROUGH[kind], metal=1.0 if kind == GOLD else 0.0, fabric=FABRIC[kind], leather=1.0 if kind == LEATHER else 0.0))
    book = kit.mesh_object('Book', bm, target)
    kit.bevel(book, 0.004, segments=2)
    gear.paint_nearest(book, points, values)
    book.data.shade_smooth()
    return book


def surface_spot(bvh, x, z):
    hit, normal, _, _ = bvh.ray_cast(Vector((x, -1.0, z)), Vector((0.0, 1.0, 0.0)))
    return np.array(hit), np.array(normal)


def build_gear(high, surface, target, torso=()):
    arms = {side: gear.Arm(side) for side, _ in SIDES}
    for side, arm in arms.items():
        ARMS[side] = (tuple(arm.clavicle), tuple(arm.shoulder), tuple(arm.elbow), tuple(arm.wrist), tuple(arm.palm), tuple(arm.tip))
        DORSAL[side] = arm.dorsal
        SLOTS[side] = arm.grip()
    extras = []

    def add(low, weights, slot='Gear', node=0, kind=GOLD, high=None):
        extras.append({'low': low, 'high': high, 'weights': weights, 'slot': slot, 'node': node, 'kind': kind})

    for side, arm in arms.items():
        chain = arm_chain(side)
        add(gear.sleeve(arm, target, 50, 24, True), chain, slot='Garb', kind=CLOTH)
        add(gear.bracer(arm, target), {f'lowerarm.{side}': 1.0})
        fist = gear.fist(arm, target, 1.0 if side == 'r' else 0.0)
        add(gear.hand_plate(arm, fist, target), {f"hand.{side}": 1.0})
        add(fist, hand_chain(side))
    for side, _ in SIDES:
        add(gear.trousers(side, LEGS[side], target), trouser_chain(side), slot='Garb', kind=UNDER)
        shaft, rest = build_boot(side, target)
        add(gear.shin_plate(side, LEGS[side], shaft, target), {f"lowerleg.{side}": 1.0})
        for made in [shaft] + rest:
            add(made, boot_chain(side))
    bvh = BVHTree.FromPolygons([Vector(p) for p in surface.co], [tuple(poly.vertices) for poly in high.data.polygons])
    sleeves = [entry['low'] for entry in extras if entry['kind'] == CLOTH and entry['low'].name.startswith('Sleeve')]
    drape = drape_tree(high, sleeves + list(torso))
    for side, arm in arms.items():
        add(gear.pauldron(arm, target, bvh), {f"shoulder.{side}": 1.0})
    CAPE_CHAINS[:] = gear.cape_joints(drape)
    cape = gear.cape(drape, target, 100, 40, True)
    add(cape, gear.cape_weights, slot='Cape', node=1, kind=CAPE)
    return extras


TORSO = pkit.Ring((
    (0.93, 0.17, 0.18, 0.13, 2.3, -0.03),
    (1.0, 0.162, 0.172, 0.125, 2.3, -0.03),
    (1.07, 0.148, 0.165, 0.115, 2.4, -0.025),
    (1.14, 0.142, 0.16, 0.112, 2.4, -0.02),
    (1.2, 0.15, 0.162, 0.115, 2.4, -0.02),
    (1.27, 0.172, 0.166, 0.125, 2.6, -0.02),
    (1.34, 0.188, 0.168, 0.13, 2.8, -0.02),
    (1.4, 0.195, 0.162, 0.132, 2.8, -0.015),
    (1.45, 0.192, 0.15, 0.128, 2.7, -0.01),
    (1.49, 0.175, 0.13, 0.115, 2.5, -0.012),
    (1.52, 0.13, 0.11, 0.095, 2.2, -0.018),
    (1.545, 0.088, 0.095, 0.075, 2.0, -0.024),
    (1.57, 0.07, 0.09, 0.066, 2.0, -0.028),
    (1.62, 0.064, 0.085, 0.062, 2.0, -0.034),
    (1.66, 0.06, 0.08, 0.06, 2.0, -0.035),
    (1.72, 0.055, 0.07, 0.055, 2.0, -0.03),
))
CARVE = (1.0, 1.56)
TUCK = 0.1
COWL = (1.6, -0.1)
HEM_REACH = 1.67
BELT = (1.07, 1.15)
GEM_Z = 1.318
LAPEL = 1.05


def zone_look(kind):
    return gear.look(TONE[kind], ROUGH[kind], metal=1.0 if kind == GOLD else 0.0, fabric=FABRIC[kind], leather=1.0 if kind in (LEATHER, BOOT) else 0.0)


def trim_line(piece, surface, path, offset, width, value, height=0.003, closed=False):
    points, normals, params = pkit.sample_path(surface, path, offset=offset)
    pkit.bar(piece, points, normals, params, width, height, value, center=0.0, depth=0.0015, closed=closed)


def metric(surface, s, z):
    return s / surface.half(z)


def build_torso(target):
    cloth, under, cream, leather, gold = zone_look(CLOTH), zone_look(UNDER), zone_look(TABARD), zone_look(LEATHER), gear.GOLD_LOOK
    made = []

    def keep(obj, slot, kind, weights=clean_torso_weights):
        made.append({'low': obj, 'high': None, 'weights': weights, 'slot': slot, 'node': 0, 'kind': kind})
        return obj

    coat = pkit.Piece('Torso', 'Garb', None)
    pkit.patch(coat, TORSO, lambda tc, tr: (-1.0 + 2.0 * tc, 1.72 - (1.72 - 0.93) * tr), 60, 96, cloth, wrap=True)
    keep(coat.build(target), 'Garb', CLOTH)

    def lapel(z):
        return min((z - GEM_Z) / LAPEL, 0.17)

    def neck_region(tc, tr):
        z = 1.66 - (1.66 - GEM_Z - 0.004) * tr
        w = lapel(z)
        return metric(TORSO, -w + 2.0 * w * tc, z), z

    inset = pkit.Piece('ChestInset', 'Garb', None)
    pkit.patch(inset, TORSO, neck_region, 24, 24, under, offset=0.003)
    keep(inset.build(target), 'Garb', UNDER)

    def stripe_width(z):
        return 0.036 + 0.014 * (GEM_Z - z) / (GEM_Z - BELT[1])

    def stripe_region(tc, tr):
        z = GEM_Z - (GEM_Z - BELT[1] + 0.004) * tr
        w = stripe_width(z)
        return metric(TORSO, -w + 2.0 * w * tc, z), z

    stripe = pkit.Piece('ChestStripe', 'Garb', None)
    pkit.patch(stripe, TORSO, stripe_region, 22, 10, cream, offset=0.003)
    keep(stripe.build(target), 'Gear', TABARD)

    trims = pkit.Piece('ChestTrims', 'Gear', None)
    for sign in (1.0, -1.0):
        path = [(metric(TORSO, sign * lapel(z), z), z) for z in np.linspace(GEM_Z, GEM_Z + LAPEL * 0.17, 30)]
        trim_line(trims, TORSO, path, 0.004, 0.022, gold, height=0.005)
        path = [(metric(TORSO, sign * stripe_width(z), z), z) for z in np.linspace(BELT[1], GEM_Z - 0.01, 24)]
        trim_line(trims, TORSO, path, 0.004, 0.009, gold, height=0.003)
    keep(trims.build(target), 'Gear', GOLD)

    belt = pkit.Piece('Belt', 'Gear', None)
    pkit.patch(belt, TORSO, lambda tc, tr: (-1.0 + 2.0 * tc, BELT[1] - (BELT[1] - BELT[0]) * tr), 6, 120, leather, offset=0.016, wrap=True)
    keep(belt.build(target, thickness=0.006), 'Gear', LEATHER)
    rims = pkit.Piece('BeltRims', 'Gear', None)
    for z in (BELT[0] + 0.004, BELT[1] - 0.004):
        trim_line(rims, TORSO, [(-1.0 + 2.0 * k / 120, z) for k in range(121)], 0.016, 0.007, gold, height=0.003, closed=True)
    hem = pkit.Piece("TorsoHem", "Gear", None)
    trim_line(hem, TORSO, [(-1.0 + 2.0 * k / 120, 0.938) for k in range(121)], 0.002, 0.014, gold, height=0.004, closed=True)
    keep(hem.build(target), "Gear", GOLD)
    keep(rims.build(target), 'Gear', GOLD)

    p, n = TORSO.at(0.0, (BELT[0] + BELT[1]) * 0.5)
    keep(gear.brooch('BeltGem', p + n * 0.026, n, (0.0, 0.0, 1.0), 0.042, 0.06, target, depth=0.03), 'Gear', GOLD)
    p, n = TORSO.at(0.0, GEM_Z)
    keep(gear.brooch('ChestGem', p + n * 0.008, n, (0.0, 0.0, 1.0), 0.042, 0.064, target, depth=0.03), 'Gear', GOLD, {'chest': 1.0})

    return [entry['low'] for entry in made], made


def hem_height(x, y):
    angle = np.degrees(np.arctan2(np.abs(x), -(np.asarray(y) - 0.012)))
    return CARVE[1] + 0.04 * smoothstep(55.0, 35.0, angle)


def carve_torso(high, surface, zone):
    co = surface.co
    doomed = (co[:, 2] >= CARVE[0]) & (co[:, 2] <= hem_height(co[:, 0], co[:, 1])) & (zone != SKIN)
    doomed |= (co[:, 2] >= CARVE[0]) & (co[:, 2] < COWL[0]) & (co[:, 1] < COWL[1]) & (zone != SKIN)
    alive = ~doomed
    seen = np.zeros(surface.count, dtype=bool)
    for seed in np.nonzero(alive)[0]:
        if seen[seed]:
            continue
        part = surface.flood([int(seed)], alive)
        seen |= part
        if part.sum() < 600:
            doomed |= part
    bm = bmesh.new()
    bm.from_mesh(high.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in np.nonzero(doomed)[0]], context='VERTS')
    tucked = 0
    for v in bm.verts:
        z = v.co.z
        hem = float(hem_height(v.co.x, v.co.y))
        if v.is_boundary and hem < z < hem + 0.02:
            v.co.z = hem + 0.002
            z = v.co.z
        if CARVE[1] < z < HEM_REACH and math.atan2(abs(v.co.x), -(v.co.y - 0.012)) > math.radians(30.0):
            dx, dy = v.co.x, v.co.y - 0.012
            radius = math.hypot(dx, dy)
            limit = TORSO.half(round(min(z, 1.72), 3)) / math.pi + 0.012
            if radius > limit:
                pull = float(smoothstep(HEM_REACH, CARVE[1] + 0.005, z))
                scale = (radius - pull * (radius - limit)) / radius
                v.co.x, v.co.y = dx * scale, 0.012 + dy * scale
        if CARVE[0] - TUCK < z < CARVE[0]:
            _, points, _, yc = TORSO.ring(round(max(z, 0.93), 3))
            angles = np.arctan2(points[:, 0], -(points[:, 1] - yc))
            radii = np.hypot(points[:, 0], points[:, 1] - yc)
            dx, dy = v.co.x, v.co.y - yc
            radius = math.hypot(dx, dy)
            limit = float(np.interp(math.atan2(dx, -dy), angles, radii)) - 0.012
            if radius > limit:
                shrink = float(smoothstep(CARVE[0] - TUCK, CARVE[0] - 0.012, z))
                scale = (radius - shrink * (radius - limit)) / radius
                v.co.x, v.co.y = dx * scale, yc + dy * scale
                tucked += 1
    print('tucked', tucked, 'robe vertices under the torso')
    bm.to_mesh(high.data)
    bm.free()
    high.data.update()
    keep = ~doomed
    carved = Surface(high)
    carved.reach = surface.reach[keep]
    print('carved', int(doomed.sum()), 'torso vertices from the source')
    return carved, zone[keep]


def build_boot(side, target):
    hip, knee, ankle, ball, toe = (np.array(p, dtype=np.float64) for p in LEGS[side])
    forward = gear.unit((ball - ankle) * np.array((1.0, 1.0, 0.0)))
    lateral = np.cross(forward, np.array((0.0, 0.0, 1.0)))
    heel, front = 0.08, float(np.dot(toe - ankle, forward)) + 0.035
    length = heel + front
    origin = ankle - forward * heel
    origin[2] = 0.0
    sole_top = 0.024
    boot, sole_look, silver = zone_look(BOOT), gear.SOLE_LOOK, gear.SILVER_LOOK

    def plan(u):
        return float(np.interp(u, (0.0, 0.1, 0.3, 0.62, 0.85, 1.0), (0.036, 0.043, 0.046, 0.053, 0.047, 0.032)))

    def top(u):
        return float(np.interp(u, (0.0, 0.12, 0.3, 0.45, 0.62, 0.8, 1.0), (0.078, 0.11, 0.142, 0.118, 0.078, 0.056, 0.043)))

    def rounding(u, back=0.09, tip=0.12):
        if u < back:
            return math.sqrt(max(0.0, 1.0 - ((back - u) / back) ** 2))
        if u > 1.0 - tip:
            return math.sqrt(max(0.0, 1.0 - ((u - 1.0 + tip) / tip) ** 2))
        return 1.0

    def loft(name, rows, cols, section, value):
        part = gear.Part(name)
        grid = []
        for i in range(rows + 1):
            u = i / rows
            row = []
            for j in range(cols):
                psi = 2.0 * math.pi * j / cols
                x, z = section(u, psi)
                p = origin + forward * (u * length) + lateral * x
                p[2] = z
                row.append(part.vert(p, value))
            row.append(row[0])
            grid.append(row)
        part.quad_grid(grid)
        bmesh.ops.remove_doubles(part.bm, verts=part.bm.verts[:], dist=1e-5)
        core = lambda c: Vector(origin + forward * float(np.dot(np.array(c) - origin, forward)) + np.array((0.0, 0.0, 0.06)))
        gear.orient_outward(part.bm, core)
        obj = part.build(target)
        return part.paint(obj)

    def foot_section(u, psi):
        f = rounding(u)
        w = plan(u) * f
        bottom, ceiling = sole_top - 0.002, top(u)
        mid = bottom + (ceiling - bottom) * (0.5 - 0.35 * (1.0 - f))
        half = (ceiling - bottom) * 0.5 * f
        s, c = math.sin(psi), math.cos(psi)
        n = 2.4 if c > 0.0 else 5.0
        x = w * math.copysign(abs(s) ** (2.0 / n), s)
        z = mid + half * math.copysign(abs(c) ** (2.0 / n), c)
        return x, z

    def sole_section(u, psi):
        f = rounding(u, 0.07, 0.1)
        w = (plan(u) + 0.006) * f + 0.004 * (1.0 - f)
        lift = 0.036 if u < 0.27 else sole_top + 0.002
        s, c = math.sin(psi), math.cos(psi)
        x = w * math.copysign(abs(s) ** 0.25, s)
        z = lift * 0.5 + lift * 0.5 * math.copysign(abs(c) ** 0.25, c)
        return x, z

    parts = [loft(f'Foot_{side}', 32, 28, foot_section, boot), loft(f'Sole_{side}', 32, 28, sole_section, sole_look)]
    shin = gear.unit(knee - ankle)
    reach = float(np.linalg.norm(knee - ankle)) - 0.035
    start = ankle - shin * 0.03
    points = np.array([start + shin * t for t in np.linspace(0.0, reach + 0.03, 10)])
    span = reach + 0.03

    def shaft_radius(l, theta):
        r = float(np.interp(l, (0.0, 0.07, 0.2, span), (0.054, 0.056, 0.064, 0.07)))
        return r * (1.0 + 0.05 * math.cos(theta) ** 2)

    surface = pkit.Tube(points, forward, shaft_radius)
    shaft = pkit.Piece(f'BootShaft_{side}', 'Gear', None)
    pkit.patch(shaft, surface, lambda tc, tr: (-1.0 + 2.0 * tc, span * tr), 24, 32, boot, wrap=True)
    shaft_obj = shaft.build(target)
    cuff = pkit.Piece(f'BootCuff_{side}', 'Gear', None)
    pkit.patch(cuff, surface, lambda tc, tr: (-1.0 + 2.0 * tc, span - 0.045 + 0.045 * tr), 4, 32, silver, offset=0.006, wrap=True)
    parts.append(cuff.build(target, thickness=0.005))
    strap = pkit.Piece(f'AnkleStrap_{side}', 'Gear', None)
    pkit.patch(strap, surface, lambda tc, tr: (-1.0 + 2.0 * tc, 0.055 + 0.034 * tr), 3, 32, zone_look(LEATHER), offset=0.005, wrap=True)
    parts.append(strap.build(target, thickness=0.004))
    buckle = pkit.Piece(f'Buckle_{side}', 'Gear', None)
    p, n = surface.at(0.5 if side == "l" else -0.5, 0.072)
    pkit.star(buckle, p + n * 0.008, n, shin, 0.016, silver, thickness=0.005, inner=0.55)
    parts.append(buckle.build(target))
    for obj in [shaft_obj] + parts:
        for name in [n for n in obj.keys()]:
            del obj[name]
    return shaft_obj, parts


def drape_tree(high, extra):
    objects = [high] + extra
    verts, faces = [], []
    for obj in objects:
        base = len(verts)
        verts += [obj.matrix_world @ v.co for v in obj.data.vertices]
        faces += [tuple(base + i for i in poly.vertices) for poly in obj.data.polygons]
    return BVHTree.FromPolygons(verts, faces)


CLEAN_CENTER = (-0.01, 0.01)
HOOD_CENTER = (-0.005, 0.0, 1.66)


def clean_torso_weights(co):
    x, y, z = co
    hips = 1.0 - float(smoothstep(0.98, 1.26, z))
    chest = float(smoothstep(1.14, 1.46, z))
    entry = {'hips': hips, 'spine': max(0.0, 1.0 - hips - chest), 'chest': chest}
    neck = float(smoothstep(1.47, 1.62, z))
    if neck > 0.0:
        head = 0.6 * float(smoothstep(1.58, 1.66, z)) + 0.4 * float(smoothstep(1.66, 1.72, z))
        entry = blend_weights(entry, {"neck": 1.0 - head, "head": head}, neck)
    for side, s in SIDES:
        shoulder = np.array(ARMS[side][1])
        reach = float(np.linalg.norm(np.array(co) - shoulder))
        amount = 0.5 * float(smoothstep(0.17, 0.06, reach)) * float(smoothstep(0.08, 0.14, s * (x - CLEAN_CENTER[0])))
        if amount > 0.0:
            entry = blend_weights(entry, {f'shoulder.{side}': 1.0}, amount)
    return entry


def clean_robe_weights(co):
    x, y, z = co
    angle = math.atan2(x - ROBE_CENTER[0], -(y - ROBE_CENTER[1])) % (2.0 * math.pi)
    position = angle / (2.0 * math.pi) * ROBE_CHAINS
    first = int(position) % ROBE_CHAINS
    t = position - int(position)
    joint = ROBE_ROWS[1][0]
    lower = float(smoothstep(joint + 0.06, joint - 0.06, z))
    top = float(smoothstep(0.93, 1.03, z))
    weights = {'hips': top}
    for chain, amount in ((first, 1.0 - t), ((first + 1) % ROBE_CHAINS, t)):
        weights[f'robe.{chain}.1'] = weights.get(f'robe.{chain}.1', 0.0) + amount * (1.0 - lower) * (1.0 - top)
        weights[f'robe.{chain}.2'] = weights.get(f'robe.{chain}.2', 0.0) + amount * lower * (1.0 - top)
    return weights


def upper_weights(co):
    x, y, z = co
    reach = math.hypot(x - HOOD_CENTER[0], y - HOOD_CENTER[1] + 0.02)
    core = float(smoothstep(1.58, 1.62, z)) * float(smoothstep(0.13, 0.1, reach))
    head = max(core, float(smoothstep(1.46, 1.68, z)) * float(smoothstep(0.3, 0.22, reach)))
    neck = 0.5 * (1.0 - head) * float(smoothstep(1.43, 1.55, z)) * float(smoothstep(0.32, 0.24, reach))
    return {'head': head, 'neck': neck, 'chest': max(0.0, 1.0 - head - neck)}


def body_field(co):
    if co[2] < 1.03:
        return clean_robe_weights(co)
    if co[2] < 1.47:
        return clean_torso_weights(co)
    return upper_weights(co)


def build_clean(high, surface, zone, target, ratio):
    low, vertex_zone, node = build_low(high, surface, zone, target, ratio)
    return low, vertex_zone, node, [body_field] * len(low.data.vertices)


def arm_chain(side):
    def weights(co):
        return arm_weights(co, side)
    return weights


def trouser_chain(side):
    names = [f'upperleg.{side}', f'lowerleg.{side}']

    def weights(co):
        hip, knee, ankle, _, _ = LEGS[side]
        entry, index, along = chain_weights(co, (hip, knee, ankle), names, blend=0.07)
        if index == 0:
            top = float(smoothstep(0.1, 0.0, along))
            entry = blend_weights(entry, {'hips': 1.0}, 0.6 * top)
        return entry
    return weights


def boot_chain(side):
    def weights(co):
        return leg_weights(co, side)
    return weights


def hand_chain(side):
    names = [f'{name}.{side}' for name in ('lowerarm', 'wrist', 'hand')]

    def weights(co):
        joints = ARMS[side]
        return chain_weights(co, (joints[2], joints[3], joints[4], joints[5]), names, blend=0.025)[0]
    return weights


def paint(obj, target):
    for _ in range(2):
        surface = Surface(obj)
        zone, face = body_zones(surface)
        a, b = surface.edges[:, 0], surface.edges[:, 1]
        near = np.zeros(surface.count, dtype=bool)
        differ = zone[a] != zone[b]
        near[a[differ]] = True
        near[b[differ]] = True
        grown = near.copy()
        grown[a[near[b]]] = True
        grown[b[near[a]]] = True
        mixed = np.nonzero(grown[a] | grown[b])[0]
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bm.edges.ensure_lookup_table()
        bmesh.ops.subdivide_edges(bm, edges=[bm.edges[index] for index in mixed], cuts=1)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
        bm.to_mesh(obj.data)
        bm.free()
        obj.data.update()
    surface = Surface(obj)
    zone, face = body_zones(surface)
    gap = np.full(surface.count, 1.0)
    if PATCHES:
        tree = kdtree.KDTree(len(PATCHES))
        for index, point in enumerate(PATCHES):
            tree.insert(point, index)
        tree.balance()
        gap = np.array([tree.find(point)[2] for point in surface.co])
    zone[(zone == TABARD) & (gap < 0.04)] = UNDER
    coherence = surface.coherence(3)
    book = build_book(surface, zone, target)
    obj.data.vertices.foreach_set('co', surface.co.ravel())
    obj.data.update()
    a, b = surface.edges[:, 0], surface.edges[:, 1]
    hide = np.isin(zone, (LEATHER, BOOT))
    seam = hide[a] != hide[b]
    edging = (surface.distance(np.unique(np.concatenate([a[seam], b[seam]])), 0.03) < 0.006) & hide
    print('zones', np.bincount(zone, minlength=len(TONE)).tolist())
    kinds = len(TONE)
    onehot = np.zeros((surface.count, kinds))
    onehot[np.arange(surface.count), zone] = 1.0
    weights = surface.soften(onehot, 1)

    def table(values):
        return weights @ np.array([values[k] for k in range(kinds)])

    reach = surface.soften(surface.distance(np.nonzero(coherence < 0.75)[0], 0.08)[:, None], 3)[:, 0]
    surface.reach = reach
    width = table(TRIM)
    trim = np.maximum(np.clip((width - reach) / 0.003 + 0.5, 0.0, 1.0) * (width > 0.002), surface.soften(edging.astype(np.float64)[:, None], 1)[:, 0])
    trim *= smoothstep(0.012, 0.03, gap)
    albedo = table(TONE) * (1.0 - trim[:, None]) + GOLD_TONE * trim[:, None]
    rough = table(ROUGH) * (1.0 - trim) + 0.32 * trim
    metal = np.clip(weights[:, GOLD] + trim, 0.0, 1.0)
    fabric = table(FABRIC) * (1.0 - trim)
    set_color(obj, 'Albedo', albedo)
    set_color(obj, 'Surface', np.stack([rough, metal, fabric], axis=1))
    set_color(obj, 'Mask', np.stack([surface.soften(face.astype(np.float64)[:, None], 2)[:, 0], trim, np.zeros(surface.count)], axis=1))
    set_color(obj, 'Kind', np.stack([np.zeros(surface.count), weights[:, LEATHER] + weights[:, BOOT], weights[:, TABARD]], axis=1))
    set_color(obj, 'Glow', np.zeros((surface.count, 3)))
    return surface, zone, book


def master_material():
    s = kit.Shader('MageSource')
    p = s.coords('Object')

    def attribute(name):
        return s.node('ShaderNodeAttribute', attribute_name=name, attribute_type='GEOMETRY').outputs['Color']

    def split(color):
        node = s.node('ShaderNodeSeparateColor')
        s.feed(node.inputs['Color'], color)
        return node.outputs['Red'], node.outputs['Green'], node.outputs['Blue']

    def tint(color, scale):
        return s.mix(1.0, color, scale, blend='MULTIPLY')

    def line(value, width):
        return s.sub(1.0, s.smooth(width * 0.5, width, s.math('ABSOLUTE', value)))

    albedo = attribute('Albedo')
    rough, metal, fabric = split(attribute('Surface'))
    face, trim, cape = split(attribute('Mask'))
    plate, leather, tabard = split(attribute('Kind'))
    x, y, z = s.separate(p)
    dirt = pk.cavity(s, 0.05)
    edge = pk.wear(s, 0.012)
    weave = s.mul(s.wave(p, 70.0, kind='BANDS', direction='X', distortion=0.5), s.wave(p, 70.0, kind='BANDS', direction='Z', distortion=0.5))
    mottle = s.noise(p, 7.0, detail=4.0)
    grain = s.noise(p, 90.0, detail=3.0)
    crackle = s.voronoi(p, 240.0, 'F1', 1.0, 'Distance')
    hammer = s.voronoi(p, 55.0, 'F1', 1.0, 'Distance')
    scratches = s.noise(s.stretch(p, (900.0, 900.0, 30.0)), 1.0, detail=2.0)
    shade = s.madd(mottle, 0.5, 0.75)
    base = tint(albedo, s.combine(shade, shade, shade))
    sheen = s.mul(s.smooth(0.55, 0.85, s.noise(s.stretch(p, (1.0, 1.0, 0.3)), 3.0, detail=3.0)), s.mul(fabric, 0.22))
    base = s.mix(sheen, base, tint(albedo, (1.7, 1.65, 1.7)))
    base = s.mix(s.mul(leather, s.mul(edge, 0.6)), base, tint(albedo, (1.9, 1.75, 1.6)))
    base = s.mix(s.mul(plate, s.smooth(0.35, 0.75, s.noise(p, 16.0, detail=3.0))), base, tint(albedo, (0.86, 0.82, 0.76)))
    base = s.mix(s.mul(plate, s.mul(edge, 0.6)), base, tint(albedo, (1.22, 1.2, 1.14)))

    u = s.sub(x, 0.02)
    side = s.math('ABSOLUTE', u)
    chevron = s.math('MAXIMUM', line(s.sub(z, s.sub(1.05, s.mul(side, 0.9))), 0.013), line(s.sub(z, s.sub(0.97, s.mul(side, 0.9))), 0.007))
    diamond = s.add(s.mul(side, 1.0 / 0.06), s.mul(s.math('ABSOLUTE', s.sub(z, 0.7)), 1.0 / 0.11))
    emblem = s.math('MAXIMUM', line(s.sub(diamond, 1.0), 0.16), line(s.sub(diamond, 0.45), 0.12))
    banner = s.mul(s.math('MAXIMUM', chevron, emblem), s.mul(cape, s.mul(s.sub(1.0, s.smooth(0.2, 0.24, side)), s.smooth(0.1, 0.16, y))))
    tu = s.math('ABSOLUTE', s.add(x, 0.08))
    cell = s.mul(s.sub(s.math('FRACT', s.mul(s.sub(z, 0.48), 1.0 / 0.17)), 0.5), 0.17)
    motif = s.add(s.mul(tu, 1.0 / 0.028), s.mul(s.math('ABSOLUTE', cell), 1.0 / 0.05))
    panel = s.mul(s.math('MAXIMUM', line(s.sub(tu, 0.046), 0.005), line(s.sub(motif, 1.0), 0.2)), s.mul(tabard, s.smooth(0.46, 0.5, z)))
    ornament = s.math('MAXIMUM', banner, panel)
    base = s.mix(ornament, base, tuple(GOLD_TONE))
    metal = s.math('MAXIMUM', metal, s.mul(ornament, 0.95))
    rough = s.lerp(ornament, rough, 0.32)
    base = s.mix(s.mul(dirt, 0.6), base, tint(albedo, (0.3, 0.28, 0.3)))
    base = s.mix(s.mul(metal, s.mul(edge, 0.5)), base, tint(albedo, (1.45, 1.4, 1.35)))

    fx = s.math('ABSOLUTE', s.add(x, 0.007))
    hair = (0.045, 0.026, 0.015)
    beard = s.math('MAXIMUM', s.smooth(1.678, 1.668, z), s.mul(s.smooth(0.04, 0.05, fx), s.smooth(1.725, 1.7, z)))
    beard = s.math('MAXIMUM', beard, s.mul(line(s.sub(z, 1.681), 0.007), s.sub(1.0, s.smooth(0.024, 0.03, fx))))
    beard = s.mul(beard, s.madd(s.smooth(0.3, 0.7, s.noise(s.stretch(p, (260.0, 260.0, 40.0)), 1.0, detail=2.0)), 0.12, 0.88))
    brow = s.mul(line(s.sub(s.add(z, s.mul(fx, 0.12)), 1.739), 0.006), s.mul(s.smooth(0.006, 0.01, fx), s.sub(1.0, s.smooth(0.042, 0.05, fx))))
    dx = s.mul(s.sub(fx, 0.023), 1.0 / 0.0115)
    dz = s.mul(s.sub(z, 1.7215), 1.0 / 0.0048)
    eye = s.sub(1.0, s.smooth(0.8, 1.1, s.math('SQRT', s.add(s.mul(dx, dx), s.mul(dz, dz)))))
    socket = s.mul(s.sub(1.0, s.smooth(0.006, 0.016, s.math('ABSOLUTE', s.sub(z, 1.722)))), s.sub(1.0, s.smooth(0.04, 0.055, fx)))
    skin = s.mix(s.mul(socket, 0.45), base, (0.2, 0.11, 0.085))
    skin = s.mix(s.math('MAXIMUM', beard, brow), skin, hair)
    skin = s.mix(eye, skin, (0.012, 0.01, 0.01))
    base = s.mix(face, base, skin)

    rough = s.lerp(s.mul(face, beard), rough, 0.75)
    rough = s.add(rough, s.mul(s.sub(grain, 0.5), 0.16), clamp=True)
    rough = s.lerp(s.mul(leather, s.mul(edge, 0.7)), rough, 0.3)
    rough = s.lerp(s.mul(metal, s.mul(scratches, 0.5)), rough, 0.55)
    rough = s.lerp(s.mul(dirt, 0.5), rough, 0.9)
    folds = s.noise(s.stretch(p, (9.0, 9.0, 1.6)), 1.0, detail=2.0)
    height = s.mul(fabric, s.add(s.madd(weave, 0.5, s.mul(mottle, 0.25)), s.mul(folds, 3.0)))
    height = s.add(height, s.mul(leather, s.mul(s.sub(1.0, crackle), 0.22)))
    height = s.add(height, s.mul(plate, s.mul(s.sub(1.0, hammer), 0.22)))
    height = s.add(height, s.mul(s.math('MAXIMUM', trim, ornament), 0.8))
    height = s.add(height, s.mul(metal, s.mul(grain, 0.15)))
    glow, _, _ = split(attribute('Glow'))
    return s.finish(base, rough, metallic=metal, height=height, bump=0.35, bump_distance=0.002, bake_size=2048, emission=glow, emission_color=GLOW[0], emission_strength=GLOW[1])


def joint_layout():
    joints = [
        ('root', None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.12), (0.0, -1.0, 0.0)),
        ('hips', 'root', (0.0, 0.01, 1.0), (-0.01, 0.01, 1.13), (0.0, -1.0, 0.0)),
        ('spine', 'hips', (-0.01, 0.01, 1.13), (-0.015, 0.0, 1.3), (0.0, -1.0, 0.0)),
        ('chest', 'spine', (-0.015, 0.0, 1.3), (-0.01, -0.02, 1.5), (0.0, -1.0, 0.0)),
        ('neck', 'chest', (-0.01, -0.02, 1.5), (0.0, -0.05, 1.6), (0.0, -1.0, 0.0)),
        ('head', 'neck', (0.0, -0.05, 1.6), (0.0, -0.09, 1.86), (0.0, -1.0, 0.0)),
    ]
    for column, chain in CAPE_CHAINS:
        parent = 'chest'
        for index in range(len(chain) - 1):
            name = f'cape.{column}.{index + 1}'
            joints.append((name, parent, tuple(chain[index]), tuple(chain[index + 1]), (0.0, -1.0, 0.0)))
            parent = name
    for chain in range(ROBE_CHAINS):
        angle = chain * 2.0 * math.pi / ROBE_CHAINS
        parent = 'hips'
        for index, ((z0, r0), (z1, r1)) in enumerate(zip(ROBE_ROWS, ROBE_ROWS[1:])):
            name = f'robe.{chain}.{index + 1}'
            head = (ROBE_CENTER[0] + math.sin(angle) * r0, ROBE_CENTER[1] - math.cos(angle) * r0, z0)
            tail = (ROBE_CENTER[0] + math.sin(angle) * r1, ROBE_CENTER[1] - math.cos(angle) * r1, z1)
            joints.append((name, parent, head, tail, (math.sin(angle), -math.cos(angle), 0.0)))
            parent = name
    for side, _ in SIDES:
        clavicle, shoulder, elbow, wrist, palm, tip = ARMS[side]
        hip, knee, ankle, ball, toe = LEGS[side]
        dorsal = DORSAL[side]
        joints += [
            (f'shoulder.{side}', 'chest', clavicle, shoulder, (0.0, 0.0, 1.0)),
            (f'upperarm.{side}', f'shoulder.{side}', shoulder, elbow, dorsal),
            (f'lowerarm.{side}', f'upperarm.{side}', elbow, wrist, dorsal),
            (f'wrist.{side}', f'lowerarm.{side}', wrist, palm, dorsal),
            (f'hand.{side}', f'wrist.{side}', palm, tip, dorsal),
            (f'upperleg.{side}', 'hips', hip, knee, (0.0, 1.0, 0.0)),
            (f'lowerleg.{side}', f'upperleg.{side}', knee, ankle, (0.0, 1.0, 0.0)),
            (f'foot.{side}', f'lowerleg.{side}', ankle, ball, (0.0, 0.0, 1.0)),
            (f'toes.{side}', f'foot.{side}', ball, toe, (0.0, 0.0, 1.0)),
        ]
    return joints


def build_rig(target):
    data = bpy.data.armatures.new(NAME)
    rig = bpy.data.objects.new(NAME, data)
    target.objects.link(rig)
    kit.select_only([rig])
    bpy.ops.object.mode_set(mode='EDIT')
    bones = {}
    for name, parent, head, tail, up in joint_layout():
        bone = data.edit_bones.new(name)
        bone.head = head
        bone.tail = tail
        bone.align_roll(Vector(up))
        bone.use_deform = name != 'root'
        if parent:
            bone.parent = bones[parent]
        bones[name] = bone
    for side, _ in SIDES:
        slot = data.edit_bones.new(f'handslot.{side}')
        slot.tail = (0.0, 0.06, 0.0)
        slot.matrix = SLOTS[side]
        slot.parent = bones[f'hand.{side}']
        slot.use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')
    for side, _ in SIDES:
        for name in ('upperarm', 'lowerarm', 'wrist', 'hand'):
            data.bones[f'{name}.{side}']['dorsal'] = DORSAL[side]
    return rig


def build_low(high, surface, zone, target, ratio):
    low = high.copy()
    low.data = high.data.copy()
    low.name = 'Low'
    target.objects.link(low)
    for name in [attribute.name for attribute in low.data.color_attributes]:
        low.data.color_attributes.remove(low.data.color_attributes[name])
    group = low.vertex_groups.new(name='Keep')
    for index, weight in enumerate(np.clip(surface.reach / 0.02, 0.2, 1.0).tolist()):
        group.add([index], weight, 'REPLACE')
    modifier = low.modifiers.new('Decimate', 'DECIMATE')
    modifier.ratio = min(1.0, ratio * 100000.0 / len(low.data.polygons))
    modifier.vertex_group = 'Keep'
    modifier.vertex_group_factor = 1.0
    kit.apply_modifiers(low)
    low.vertex_groups.clear()
    kit.smooth_by_angle(low, 70.0)
    tree = kdtree.KDTree(surface.count)
    for index, co in enumerate(surface.co):
        tree.insert(co, index)
    tree.balance()
    mesh = low.data
    vertex_zone = np.array([zone[tree.find(v.co)[1]] for v in mesh.vertices], dtype=np.int32)
    mesh.materials.clear()
    for name in MATERIALS:
        material = bpy.data.materials.new(name)
        material.use_nodes = True
        mesh.materials.append(material)
    node = np.zeros(len(mesh.polygons), dtype=np.int32)
    indices = []
    for poly in mesh.polygons:
        tally = np.bincount(vertex_zone[list(poly.vertices)], minlength=len(TONE))
        best = int(tally.argmax())
        if best == SKIN and poly.center.z > 1.5:
            indices.append(MATERIALS.index('Skin'))
            node[poly.index] = 2
        elif best in (CLOTH, UNDER):
            indices.append(MATERIALS.index('Garb'))
        else:
            indices.append(MATERIALS.index('Gear'))
    mesh.polygons.foreach_set('material_index', indices)
    return low, vertex_zone, node


def robe_weights(co):
    x, y, z = co
    point = np.array([co])
    weights = {}
    share = 0.0
    legs = {}
    for side in 'lr':
        hip, knee, ankle, _, _ = LEGS[side]
        upper, _ = capsule(point, hip, knee)
        lower, _ = capsule(point, knee, ankle)
        near = float(smoothstep(0.17, 0.08, min(upper[0], lower[0])))
        shin = float(smoothstep(0.58, 0.42, z))
        legs[side] = (near, shin)
        share = max(share, near)
    total = sum(near for near, _ in legs.values())
    for side, (near, shin) in legs.items():
        if total > 0.0:
            weights[f'upperleg.{side}'] = share * near / total * (1.0 - shin)
            weights[f'lowerleg.{side}'] = share * near / total * shin
    loose = 1.0 - share
    angle = math.atan2(x - ROBE_CENTER[0], -(y - ROBE_CENTER[1])) % (2.0 * math.pi)
    position = angle / (2.0 * math.pi) * ROBE_CHAINS
    first = int(position) % ROBE_CHAINS
    t = position - int(position)
    lower = float(smoothstep(0.62, 0.42, z))
    for chain, amount in ((first, 1.0 - t), ((first + 1) % ROBE_CHAINS, t)):
        weights[f'robe.{chain}.1'] = weights.get(f'robe.{chain}.1', 0.0) + loose * amount * (1.0 - lower)
        weights[f'robe.{chain}.2'] = weights.get(f'robe.{chain}.2', 0.0) + loose * amount * lower
    top = float(smoothstep(0.84, 1.0, z))
    weights = {name: weight * (1.0 - top) for name, weight in weights.items()}
    weights['hips'] = top
    return weights


def chain_weights(co, joints, names, blend=0.05):
    point = np.array(co)
    best = None
    for index in range(len(names)):
        a, b = np.array(joints[index]), np.array(joints[index + 1])
        ab = b - a
        length = float(np.linalg.norm(ab))
        t = float(np.clip((point - a) @ ab / (length * length), 0.0, 1.0))
        distance = float(np.linalg.norm(point - (a + ab * t)))
        if best is None or distance < best[0]:
            best = (distance, index, t * length, length)
    _, index, along, length = best
    weights = {names[index]: 1.0}
    if along < blend and index > 0:
        share = 0.5 * (1.0 - along / blend)
        weights = {names[index]: 1.0 - share, names[index - 1]: share}
    elif length - along < blend and index + 1 < len(names):
        share = 0.5 * (1.0 - (length - along) / blend)
        weights = {names[index]: 1.0 - share, names[index + 1]: share}
    return weights, index, along


def arm_weights(co, side):
    names = [f'{name}.{side}' for name in ('upperarm', 'lowerarm', 'wrist', 'hand')]
    weights, index, along = chain_weights(co, ARMS[side][1:], names, blend=0.07)
    if index == 0:
        keep = float(smoothstep(-0.02, 0.09, along))
        weights = {name: weight * keep for name, weight in weights.items()}
        weights[f'shoulder.{side}'] = weights.get(f'shoulder.{side}', 0.0) + 0.7 * (1.0 - keep)
        weights['chest'] = 0.3 * (1.0 - keep)
    return weights


def leg_weights(co, side):
    hip, knee, ankle, ball, toe = LEGS[side]
    names = [f'{name}.{side}' for name in ('lowerleg', 'foot', 'toes')]
    return chain_weights(co, (knee, ankle, ball, toe), names, blend=0.04)[0]


def torso_weights(co):
    hips = 1.0 - float(smoothstep(1.02, 1.14, co[2]))
    chest = float(smoothstep(1.2, 1.34, co[2]))
    return {'hips': hips, 'spine': 1.0 - hips - chest, 'chest': chest}


def blend_weights(a, b, factor):
    result = {name: weight * (1.0 - factor) for name, weight in a.items()}
    for name, weight in b.items():
        result[name] = result.get(name, 0.0) + weight * factor
    return result


def body_weights(co, zone):
    x, y, z = co
    if zone == PAPER or (zone == LEATHER and z < 1.12 and 0.0 < x < 0.21 and y < -0.17) or (zone == LEATHER and x < -0.15 and 0.94 < z < 1.1 and y > -0.14):
        return {'hips': 1.0}
    if zone == LEATHER and 1.04 < z < 1.17 and math.hypot(x + 0.03, y) < 0.24:
        return {'hips': 0.6, 'spine': 0.4}
    if zone == BOOT:
        return leg_weights(co, 'l' if x > 0.03 else 'r')
    if z > 1.5 and zone in (CLOTH, UNDER, SKIN):
        head = float(smoothstep(1.56, 1.63, z))
        neck = float(smoothstep(1.5, 1.57, z)) * (1.0 - head)
        entry = {'head': head, 'neck': neck, 'chest': 1.0 - head - neck}
    elif z < 1.0:
        return robe_weights(co)
    else:
        entry = torso_weights(co)
    for side, _ in SIDES:
        shoulder = np.array(ARMS[side][1])
        reach = float(np.linalg.norm(np.array(co) - shoulder))
        if reach < 0.15 and abs(x) > 0.1:
            entry = blend_weights(entry, {f'shoulder.{side}': 1.0}, 0.6 * float(smoothstep(0.15, 0.06, reach)))
    return entry


def skin(low, rig, vertex_zone, override):
    mesh = low.data
    entries = []
    for vertex, zone, special in zip(mesh.vertices, vertex_zone, override):
        co = tuple(vertex.co)
        if special is None:
            entry = body_weights(co, zone)
        elif callable(special):
            entry = special(co)
        else:
            entry = dict(special)
        entries.append(entry)
    names = sorted({name for entry in entries for name in entry})
    column = {name: index for index, name in enumerate(names)}
    weights = np.zeros((len(entries), len(names)))
    for row, entry in enumerate(entries):
        for name, weight in entry.items():
            weights[row, column[name]] = max(weight, 0.0)
    edges = np.zeros(len(mesh.edges) * 2, dtype=np.int64)
    mesh.edges.foreach_get('vertices', edges)
    edges = edges.reshape(-1, 2)
    soft = np.array([special is None or callable(special) for special in override])
    degree = np.maximum(np.bincount(edges.ravel(), minlength=len(entries)), 1)[:, None]
    for _ in range(3):
        total = np.zeros_like(weights)
        np.add.at(total, edges[:, 0], weights[edges[:, 1]])
        np.add.at(total, edges[:, 1], weights[edges[:, 0]])
        weights = np.where(soft[:, None], 0.5 * weights + 0.5 * total / degree, weights)
    low.vertex_groups.clear()
    groups = [low.vertex_groups.new(name=name) for name in names]
    for row in range(len(entries)):
        order = np.argsort(-weights[row])[:4]
        chosen = [(int(k), float(weights[row, k])) for k in order if weights[row, k] > 0.015]
        total = sum(weight for _, weight in chosen)
        if total <= 0.0:
            chosen, total = [(column['hips'], 1.0)], 1.0
        for k, weight in chosen:
            groups[k].add([row], weight / total, 'REPLACE')
    low.parent = rig
    low.matrix_parent_inverse = Matrix.Identity(4)
    low.modifiers.new('Armature', 'ARMATURE').object = rig


def unwrap(low):
    kit.select_only([low])
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.context.tool_settings.mesh_select_mode = (False, False, True)
    for index, name in enumerate(MATERIALS):
        if name == 'Cape':
            continue
        bm = bmesh.from_edit_mesh(low.data)
        bm.select_mode = {'FACE'}
        for face in bm.faces:
            face.select_set(face.material_index == index)
        bm.select_flush_mode()
        bmesh.update_edit_mesh(low.data)
        margin = 0.006 if name == 'Skin' else 0.003
        bpy.ops.uv.smart_project(angle_limit=math.radians(66.0), island_margin=margin, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
        bpy.ops.uv.select_all(action='SELECT')
        bpy.ops.uv.average_islands_scale()
        bpy.ops.uv.pack_islands(rotate=True, margin=margin, shape_method='CONCAVE')
    bpy.ops.object.mode_set(mode='OBJECT')


def split_nodes(low, node, target):
    parts = []
    for index, name in enumerate(NODES):
        part = low.copy()
        part.data = low.data.copy()
        part.name = name
        part.data.name = name
        target.objects.link(part)
        bm = bmesh.new()
        bm.from_mesh(part.data)
        bm.faces.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[face for face in bm.faces if node[face.index] != index], context='FACES')
        bm.to_mesh(part.data)
        bm.free()
        used = sorted({poly.material_index for poly in part.data.polygons})
        remap = {old: new for new, old in enumerate(used)}
        materials = [part.data.materials[old] for old in used]
        indices = [remap[poly.material_index] for poly in part.data.polygons]
        part.data.materials.clear()
        for material in materials:
            part.data.materials.append(material)
        part.data.polygons.foreach_set('material_index', indices)
        parts.append(part)
    bpy.data.objects.remove(low)
    return parts


def bake(high, parts, folder):
    scene = bpy.context.scene
    master = high.data.materials[0]
    tree = master.node_tree
    output = tree.nodes['Material Output']
    bsdf = tree.nodes['Principled BSDF']
    emission = tree.nodes.new('ShaderNodeEmission')
    scratch = bpy.data.images.new('scratch', 16, 16, alpha=False, float_buffer=True)
    targets = {}
    for name in MATERIALS:
        material = bpy.data.materials[name]
        node = material.node_tree.nodes.new('ShaderNodeTexImage')
        node.image = scratch
        material.node_tree.nodes.active = node
        targets[name] = node
    scene.world.light_settings.distance = 0.3

    def bake_layer(name, users, size, cage, reach):
        maps = {}
        for channel in ('base', 'rough', 'metal', 'emit', 'normal', 'ao'):
            image = bpy.data.images.new(f'{name}_{channel}', size, size, alpha=False, float_buffer=True)
            targets[name].image = image
            if channel in ('base', 'rough', 'metal', 'emit'):
                tree.links.new(tree.nodes['OUT_' + channel].outputs[0], emission.inputs['Color'])
                tree.links.new(emission.outputs[0], output.inputs['Surface'])
                kind, samples, options = 'EMIT', 8, {}
            else:
                tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
                kind, samples = ('NORMAL', 8) if channel == 'normal' else ('AO', 64)
                options = {'normal_space': 'TANGENT'} if channel == 'normal' else {}
            scene.cycles.samples = samples
            for index, part in enumerate(users):
                kit.select_only([high, part], active=part)
                bpy.ops.object.bake(type=kind, use_selected_to_active=True, cage_extrusion=cage, max_ray_distance=reach, margin=0, use_clear=index == 0, **options)
            maps[channel] = kit.image_pixels(image)
            bpy.data.images.remove(image)
        return maps

    baked = {}
    for name in MATERIALS:
        users = [part for part in parts if name in part.data.materials]
        size = SIZES[name]
        layers = [bake_layer(name, users, size, 0.005, 0.03), bake_layer(name, users, size, 0.03, 0.07)]
        targets[name].image = scratch
        near, far = layers
        hit = near['rough'][..., 0] > 0.02
        covered = hit | (far['rough'][..., 0] > 0.02)
        maps = {channel: kit.fill_outside(np.where(hit[..., None], near[channel], far[channel]), covered) for channel in near}
        files = {}
        base_name = f'{NAME}_{name}'
        files['albedo'] = os.path.join(folder, base_name + '_albedo.png')
        kit.save_png(files['albedo'], maps['base'], srgb=True)
        files['normal'] = os.path.join(folder, base_name + '_normal.png')
        kit.save_png(files['normal'], kit.sanitize_normals(maps['normal']))
        orm = np.zeros_like(maps['base'])
        orm[..., 0] = maps['ao'][..., 0]
        orm[..., 1] = maps['rough'][..., 0]
        orm[..., 2] = maps['metal'][..., 0]
        files['orm'] = os.path.join(folder, base_name + '_orm.png')
        kit.save_png(files['orm'], orm)
        if name == 'Gear':
            files['emit'] = os.path.join(folder, base_name + '_emit.png')
            kit.save_png(files['emit'], np.repeat(maps['emit'][..., :1], 3, axis=2))
        baked[name] = kit.baked_material(bpy.data.materials[name], files)
    tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    tree.nodes.remove(emission)
    for part in parts:
        for slot in part.material_slots:
            slot.material = baked[slot.material.name]
    high.hide_render = True


def build_high(target):
    high = load_source(target)
    strip(high)
    refine(high)
    surface, zone, book = paint(high, target)
    extras = [{'low': book, 'high': None, 'weights': {'hips': 1.0}, 'slot': 'Gear', 'node': 0, 'kind': GOLD}]
    torso, more = build_torso(target)
    surface, zone = carve_torso(high, surface, zone)
    extras += more
    extras += build_gear(high, surface, target, torso)
    return high, surface, zone, extras


def build(target, ratio=0.27):
    high, surface, zone, extras = build_high(target)
    rig = build_rig(target)
    low, vertex_zone, node, override = build_clean(high, surface, zone, target, ratio)
    for entry in extras:
        extra = entry['low']
        cover = extra.copy()
        cover.data = extra.data.copy()
        target.objects.link(cover)
        for name in [attribute.name for attribute in cover.data.color_attributes]:
            cover.data.color_attributes.remove(cover.data.color_attributes[name])
        if 'Local' in cover.data.attributes:
            cover.data.attributes.remove(cover.data.attributes['Local'])
        cover.data.materials.clear()
        for material in low.data.materials:
            cover.data.materials.append(material)
        cover.data.polygons.foreach_set('material_index', [MATERIALS.index(entry['slot'])] * len(cover.data.polygons))
        added, faces = len(cover.data.vertices), len(cover.data.polygons)
        low = kit.join([low, cover], 'Low')
        vertex_zone = np.concatenate([vertex_zone, np.full(added, entry['kind'], dtype=np.int32)])
        override += [entry['weights']] * added
        node = np.concatenate([node, np.full(faces, entry['node'], dtype=np.int32)])
    sources = []
    for entry in extras:
        if entry['high'] is not None:
            sources.append(entry['high'])
            bpy.data.objects.remove(entry['low'], do_unlink=True)
        else:
            sources.append(entry['low'])
    for obj in sources:
        if 'Local' in obj.data.attributes:
            obj.data.attributes.remove(obj.data.attributes['Local'])
    high = kit.join([high] + sources, 'MageSource')
    high.data.materials.clear()
    high.data.materials.append(master_material())
    skin(low, rig, vertex_zone, override)
    unwrap(low)
    parts = split_nodes(low, node, target)
    print('low poly', sum(len(part.data.polygons) for part in parts), 'triangles')
    return high, rig, parts


def quick(target):
    high, surface, zone, extras = build_high(target)
    sources = []
    for entry in extras:
        sources.append(entry['high'] if entry['high'] is not None else entry['low'])
        if entry['high'] is not None:
            bpy.data.objects.remove(entry['low'], do_unlink=True)
    for obj in sources:
        if 'Local' in obj.data.attributes:
            obj.data.attributes.remove(obj.data.attributes['Local'])
    high = kit.join([high] + sources, 'MageSource')
    high.data.materials.clear()
    high.data.materials.append(master_material())
    rig = build_rig(target)
    return high, rig


def pose_test(rig):
    turns = {
        'upperarm.l': (0.0, 0.0, 70.0), 'upperarm.r': (0.0, 0.0, -40.0), 'lowerarm.r': (60.0, 0.0, 0.0), 'upperleg.l': (-35.0, 0.0, 0.0), 'upperleg.r': (30.0, 0.0, 0.0),
        'lowerleg.r': (40.0, 0.0, 0.0), 'spine': (10.0, 0.0, 0.0), 'head': (0.0, 30.0, 0.0),
    }
    for name, angles in turns.items():
        bone = rig.pose.bones[name]
        bone.rotation_mode = 'XYZ'
        bone.rotation_euler = [math.radians(a) for a in angles]


VIEWS = {
    'front': (20.0, 12.0, 1.05, 0.0), 'back': (200.0, 12.0, 1.05, 0.0), 'upper': (15.0, 12.0, 0.5, 0.42), 'upperback': (195.0, 12.0, 0.5, 0.42),
    'left': (100.0, 12.0, 0.5, 0.42), 'right': (260.0, 12.0, 0.5, 0.42), 'hand': (330.0, 12.0, 0.5, 0.0), 'top': (20.0, 55.0, 1.05, 0.0), 'game': (0.0, 57.0, 1.05, 0.0),
    'side': (90.0, 8.0, 1.05, 0.0), 'lefthand': (40.0, 12.0, 0.4, 0.0), 'shoulder': (60.0, 30.0, 0.42, 0.45),
}


SHOTS = {
    'full': ((0.0, 0.0, 0.95), 20.0, 8.0, 5.6), 'fullback': ((0.0, 0.05, 0.95), 200.0, 8.0, 5.6), 'fullside': ((0.0, 0.0, 0.95), 95.0, 8.0, 5.6),
    'game': ((0.0, 0.0, 0.9), 0.0, 57.0, 5.6), 'gameback': ((0.0, 0.0, 0.9), 180.0, 57.0, 5.6),
    'bust': ((0.0, -0.02, 1.4), 15.0, 10.0, 1.9), 'bustback': ((0.0, 0.08, 1.35), 195.0, 18.0, 1.9),
    'shoulderl': ((0.24, 0.0, 1.42), 70.0, 22.0, 1.15), 'shoulderr': ((-0.24, 0.0, 1.42), 290.0, 22.0, 1.15),
    'fistr': (None, 320.0, 10.0, 0.62), 'fistl': (None, 40.0, 10.0, 0.62), 'fistrfront': (None, 0.0, 5.0, 0.62), 'fistrside': (None, 270.0, 5.0, 0.62),
    'face': ((0.0, -0.06, 1.66), 10.0, 6.0, 0.95), 'legs': ((0.0, -0.05, 0.35), 15.0, 10.0, 2.0), 'legsback': ((0.0, 0.05, 0.35), 200.0, 10.0, 2.0), 'legsright': ((-0.1, 0.0, 0.3), 280.0, 8.0, 1.6),
}


def shot(path, center, yaw, pitch, distance, samples):
    scene = bpy.context.scene
    data = bpy.data.cameras.new('ShotCamera')
    data.angle = math.radians(30.0)
    data.sensor_fit = 'VERTICAL'
    camera = bpy.data.objects.new('ShotCamera', data)
    scene.collection.objects.link(camera)
    yaw, pitch = math.radians(yaw), math.radians(pitch)
    direction = Vector((math.cos(pitch) * math.sin(yaw), -math.cos(pitch) * math.cos(yaw), math.sin(pitch)))
    camera.location = Vector(center) + direction * distance
    camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = camera
    scene.render.resolution_x, scene.render.resolution_y = (700, 1000)
    scene.render.resolution_percentage = 100
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera)


def preview(folder, parts, prefix='mage', views=None, samples=48):
    kit.environment('forest.exr', 1.0)
    pk.ground_preview(4.0)
    for label in views or ('full', 'fullback', 'bust', 'bustback', 'shoulderl', 'fistr', 'fistl', 'game'):
        if label in SHOTS:
            center, yaw, pitch, distance = SHOTS[label]
            if center is None:
                center = ARMS['r' if label.startswith('fistr') else 'l'][4]
            shot(os.path.join(folder, f'{prefix}_{label}.png'), center, yaw, pitch, distance, samples)
            continue
        yaw, pitch, fill, lift = VIEWS[label]
        kit.render(os.path.join(folder, f'{prefix}_{label}.png'), parts, yaw=yaw, pitch=pitch, fov=30.0, size=(700, 1000), samples=samples, fill=fill, look_offset=(0.0, 0.0, lift))


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    folder = args.get('--out') or args.get('--preview') or args.get('--quick') or args.get('--rigtest')
    os.makedirs(folder, exist_ok=True)
    views = args['--views'].split(',') if args.get('--views') else None
    if args.get('--quick'):
        high, rig = quick(target)
        if args.get('--prop'):
            import random
            import armory
            made = getattr(armory, args['--prop'])(armory.materials(), target, random.Random(23))
            made.matrix_world = SLOTS['r'] @ Matrix.Rotation(-math.pi * 0.5, 4, 'X')
        preview(folder, [high], args.get('--prefix', 'quick'), views, samples=int(args.get('--samples', '24')))
        return
    high, rig, parts = build(target, float(args.get('--ratio', '0.27')))
    if args.get('--rigtest'):
        heroanim.build_actions(rig)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, NAME + '.blend'))
        return
    if args.get('--out'):
        heroanim.build_actions(rig)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, NAME + '.blend'), compress=True)
    bake(high, parts, folder)
    if args.get('--out'):
        kit.export_gltf(parts, os.path.join(folder, NAME + '.gltf'), {'Gear': GLOW}, armature=rig, animations=True)
    if args.get('--preview'):
        if args.get('--pose'):
            pose_test(rig)
        preview(args['--preview'], parts, args.get('--prefix', 'mage'), views)


if __name__ == '__main__':
    main()
