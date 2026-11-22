import bpy
import bmesh
import math
import os
import sys
import time
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import hooded
import propkit as pk
import portalterrace as pt

NAME = 'MainPortal'
SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'PortalArch.glb')
SCALE = 7.0
CENTER_X = -0.002
SHEET_Y = -0.032
BASE_Z = 0.0165
ARCH_Y = 1.3
WELD = 1e-5
SHEET_SHAPE = (0.1435, 0.5135)
LINER = (0.53, 0.008, 40, (-0.079, -0.076), (-0.0745, 0.004), 0.0012, 0.0035, 0.0022)
LINER_ARCH = ((-0.066, -0.005), (0.47, 0.53))
SHEET_BAND = (-0.068, 0.004)
FRAGMENT = 400
FACES = 95000
TEXTURE = 4096
FLOOR_TEXTURE = 4096
UV_ANGLE = 55.0
CAGE = (0.024, 0.072)
AO_DISTANCE = 0.6
KIT_AO = 0.5
GOLD_SCORE = ((16.0, 22.0, 48.0, 56.0), (0.3, 0.42), (0.42, 0.55))
GOLD_SPECK = 0.0006
STONE_SPECK = 0.0003
MOSS = ((42.0, 55.0, 150.0, 170.0), (0.18, 0.3), (0.07, 0.14))
PLAQUE_HEIGHT = 0.24
PLAQUE_BOTTOM = 0.172
PLAQUE_TAB = 0.473
PLAQUE_BORDER = 0.15
MOTIF = 0.06
PLAQUE_GROW = 0.003
PLAQUE_DROP = 0.003
PLAQUES = (
    dict(origin=(-0.2085, -0.072, PLAQUE_BOTTOM), right=(1.0, 0.0, 0.0), width=0.060, panel=0.016, relief=0.0065),
    dict(origin=(0.2025, -0.072, PLAQUE_BOTTOM), right=(1.0, 0.0, 0.0), width=0.060, panel=0.016, relief=0.0065),
    dict(origin=(0.243, -0.028, PLAQUE_BOTTOM), right=(0.0, 1.0, 0.0), width=0.075, panel=0.0237, relief=0.0059),
    dict(origin=(-0.2485, -0.028, PLAQUE_BOTTOM), right=(0.0, -1.0, 0.0), width=0.075, panel=0.0257, relief=0.0052),
)
KEYSTONE = dict(origin=(-0.004, -0.037, 0.696), panel=0.0372, relief=0.0068, half=0.039, foot=0.013, shoulder=0.024, top=0.141, border=0.0072)
SPIKES = (dict(origin=(-0.2445, -0.0725, 0.583)), dict(origin=(0.2375, -0.0725, 0.583)))
SPIKE = (0.031, 0.095, 0.012, 0.0085)
BANNER_GRID = (5, 8)
BANNER_MESH = (10, 34)
BANNER_SIZE = (0.46, 0.6)
BANNER_NOTCH = 0.14
BANNER_THICK = 0.003
BANNER_RINGS = 3
RING_UV = (0.5, 0.02)
BANNER_RING = (0.03, 0.006)
BANNER_RED = (0.5, 0.055, 0.075)
BANNER_TRANSLUCENCY = 0.5
BANNER_TEXTURE = 1024


def smoothstep(low, high, value):
    t = np.clip((np.asarray(value, dtype=np.float32) - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def place(co):
    return np.stack(((co[:, 0] - CENTER_X) * SCALE, (co[:, 1] - SHEET_Y) * SCALE + ARCH_Y, (co[:, 2] - BASE_Z) * SCALE + pt.HEIGHT), axis=1)


def place_point(p):
    return Vector(place(np.array((tuple(p),), dtype=np.float64))[0])


def face_arrays(me):
    count = len(me.polygons)
    centers = np.empty(count * 3, np.float32)
    normals = np.empty(count * 3, np.float32)
    areas = np.empty(count, np.float32)
    totals = np.empty(count, np.int32)
    me.polygons.foreach_get('center', centers)
    me.polygons.foreach_get('normal', normals)
    me.polygons.foreach_get('area', areas)
    me.polygons.foreach_get('loop_total', totals)
    return centers.reshape(-1, 3), normals.reshape(-1, 3), areas, totals


def load(target):
    bpy.ops.import_scene.gltf(filepath=SOURCE)
    obj = [o for o in bpy.context.selected_objects if o.type == 'MESH'][0]
    world = obj.matrix_world.copy()
    for other in list(bpy.context.selected_objects):
        if other is not obj:
            bpy.data.objects.remove(other, do_unlink=True)
    obj.parent = None
    obj.matrix_world = world
    for collection in list(obj.users_collection):
        collection.objects.unlink(obj)
    target.objects.link(obj)
    kit.select_only([obj])
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=WELD)
    bm.to_mesh(obj.data)
    bm.free()
    obj.name = 'PortalSource'
    print('source', len(obj.data.polygons), 'faces')
    return obj


def source_colors(obj):
    me = obj.data
    material = me.materials[0]
    image = next(n.image for n in material.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image and 'normal' not in n.image.name and 'metal' not in n.image.name)
    width, height = image.size
    pixels = np.empty(width * height * 4, np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape(height, width, 4)
    uv = np.empty(len(me.loops) * 2, np.float32)
    me.uv_layers.active.data.foreach_get('uv', uv)
    uv = uv.reshape(-1, 2)
    starts = np.empty(len(me.polygons), np.int32)
    totals = np.empty(len(me.polygons), np.int32)
    me.polygons.foreach_get('loop_start', starts)
    me.polygons.foreach_get('loop_total', totals)
    center = np.add.reduceat(uv, starts, axis=0) / totals[:, None]
    x = np.clip((center[:, 0] % 1.0) * width, 0, width - 1).astype(int)
    y = np.clip((center[:, 1] % 1.0) * height, 0, height - 1).astype(int)
    srgb = kit.linear_to_srgb(pixels[y, x, :3])
    top, bottom = srgb.max(axis=1), srgb.min(axis=1)
    saturation = np.where(top > 1e-4, (top - bottom) / np.maximum(top, 1e-4), 0.0)
    r, g, b = srgb[:, 0], srgb[:, 1], srgb[:, 2]
    hue = np.degrees(np.arctan2(math.sqrt(3.0) * (g - b), 2.0 * r - g - b)) % 360.0
    return hue, saturation, top


class FaceGraph:
    def __init__(self, me):
        self.faces = len(me.polygons)
        self.verts = len(me.vertices)
        totals = np.empty(self.faces, np.int32)
        me.polygons.foreach_get('loop_total', totals)
        self.totals = totals
        self.loop_vert = np.empty(len(me.loops), np.int32)
        me.loops.foreach_get('vertex_index', self.loop_vert)
        self.loop_face = np.repeat(np.arange(self.faces), totals)
        edges = np.empty(len(me.loops), np.int32)
        me.loops.foreach_get('edge_index', edges)
        order = np.argsort(edges, kind='stable')
        sorted_edges = edges[order]
        sorted_faces = self.loop_face[order]
        same = sorted_edges[1:] == sorted_edges[:-1]
        self.a = sorted_faces[:-1][same]
        self.b = sorted_faces[1:][same]

    def smooth(self, values, passes):
        values = np.asarray(values, dtype=np.float64)
        counts = np.maximum(np.bincount(self.loop_vert, minlength=self.verts), 1)
        for _ in range(passes):
            at_verts = np.bincount(self.loop_vert, weights=values[self.loop_face], minlength=self.verts) / counts
            values = np.bincount(self.loop_face, weights=at_verts[self.loop_vert], minlength=self.faces) / self.totals
        return values

    def components(self, mask):
        joined = mask[self.a] == mask[self.b]
        a, b = self.a[joined], self.b[joined]
        parent = np.arange(self.faces)
        for _ in range(200):
            ra, rb = parent[a], parent[b]
            low = np.minimum(ra, rb)
            np.minimum.at(parent, ra, low)
            np.minimum.at(parent, rb, low)
            while True:
                hop = parent[parent]
                if np.array_equal(hop, parent):
                    break
                parent = hop
            if np.array_equal(parent[a], parent[b]):
                break
        return np.unique(parent, return_inverse=True)[1]

    def despeckle(self, mask, areas, small_true, small_false):
        for value, limit in ((True, small_true), (False, small_false)):
            labels = self.components(mask)
            size = np.bincount(labels, weights=areas)
            members = np.bincount(labels, weights=(mask == value).astype(np.float64))
            flip = (members > 0) & (size < limit)
            mask = np.where(flip[labels], not value, mask)
        return mask


def delete_faces(obj, doomed, label):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in np.nonzero(doomed)[0]], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(obj.data)
    bm.free()
    print(f'removed {int(doomed.sum())} {label} faces, {len(obj.data.polygons)} left')


def in_polygon(u, v, points):
    inside = np.zeros(u.shape, bool)
    count = len(points)
    for i in range(count):
        x0, y0 = points[i]
        x1, y1 = points[(i + 1) % count]
        crosses = (y0 > v) != (y1 > v)
        at = x0 + (v - y0) * (x1 - x0) / np.where(np.abs(y1 - y0) > 1e-12, y1 - y0, 1e-12)
        inside ^= crosses & (u < at)
    return inside


def strip(obj):
    me = obj.data
    centers, normals, _, _ = face_arrays(me)
    hue, saturation, value = source_colors(obj)
    x, y, z = centers[:, 0] - CENTER_X, centers[:, 1], centers[:, 2]
    half, spring = SHEET_SHAPE
    shrink = half - 0.003
    opening = np.where(z > spring, np.hypot(x, z - spring) < shrink, np.abs(x) < shrink)
    band = (y > SHEET_BAND[0]) & (y < SHEET_BAND[1])
    purple = (hue > 225.0) & (hue < 305.0) & (saturation > 0.25) & (value > 0.35)
    grown = np.where(z > spring, np.hypot(x, z - spring) < half + 0.008, np.abs(x) < half + 0.008)
    sheet = band & ((opening & ((np.abs(normals[:, 1]) > 0.6) | purple)) | (grown & purple & (np.abs(normals[:, 0]) < 0.9)))
    delete_faces(obj, sheet, 'portal sheet')
    bm = bmesh.new()
    bm.from_mesh(me)
    geometry = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.bisect_plane(bm, geom=geometry, dist=1e-6, plane_co=(0.0, 0.0, BASE_Z), plane_no=(0.0, 0.0, 1.0), clear_inner=True)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(me)
    bm.free()
    graph = FaceGraph(me)
    labels = graph.components(np.ones(len(me.polygons), bool))
    sizes = np.bincount(labels)
    delete_faces(obj, sizes[labels] < FRAGMENT, 'loose')


def plaque_frame(spec, source=True):
    right = Vector(spec['right'])
    up = Vector((0.0, 0.0, 1.0))
    origin = Vector(spec['origin'])
    if not source:
        origin = place_point(origin)
    return pt.face_frame(origin, right, up)


def local_coords(centers, frame):
    inverse = np.array(frame.inverted())
    homogeneous = np.concatenate((centers, np.ones((len(centers), 1))), axis=1)
    local = homogeneous @ inverse.T
    return local[:, 0], local[:, 1], local[:, 2]


def plaque_regions(spec):
    w = spec['width']
    a, b = 0.64 * MOTIF, 0.5 * MOTIF
    cy = PLAQUE_HEIGHT + 0.1 * MOTIF
    m = 0.0
    rect = [(-0.5 * w, 0.004), (0.5 * w, 0.004), (0.5 * w, PLAQUE_HEIGHT), (-0.5 * w, PLAQUE_HEIGHT)]
    diamond = [(0.0, cy - b + m), (a - m, cy), (0.0, cy + b - m), (-a + m, cy)]
    tab = [(-0.005, cy), (0.005, cy), (0.005, PLAQUE_TAB - PLAQUE_BOTTOM - 0.001), (-0.005, PLAQUE_TAB - PLAQUE_BOTTOM - 0.001)]
    return (rect, diamond, tab)


def keystone_outline(k):
    h, s, t, f = k['half'], k['shoulder'], k['top'], k['foot']
    return [(-f, 0.0), (f, 0.0), (h, s), (h, t), (-h, t), (-h, s)]


def keystone_stem():
    return [(-0.0115, 0.008), (0.0115, 0.008), (0.0115, -0.044), (0.0, -0.066), (-0.0115, -0.044)]


def keystone_crown(k):
    h, t = k['half'], k['top']
    return [(-h, t - 0.01), (h, t - 0.01), (h, t + 0.024), (0.006, t + 0.024), (0.006, t + 0.042), (-0.006, t + 0.042), (-0.006, t + 0.024), (-h, t + 0.024)]


def spike_outline():
    w, h, tip, _ = SPIKE
    return [(-0.5 * w, tip), (0.0, 0.0), (0.5 * w, tip), (0.5 * w, h), (-0.5 * w, h)]


def remove_ornaments(obj):
    me = obj.data
    centers, _, _, _ = face_arrays(me)
    doomed = np.zeros(len(centers), bool)
    for spec in PLAQUES:
        u, v, depth = local_coords(centers, plaque_frame(spec))
        inside = np.zeros(len(centers), bool)
        for polygon in plaque_regions(spec):
            inside |= in_polygon(u, v, polygon)
        doomed |= inside & (depth > 0.0012)
    frame = pt.face_frame(KEYSTONE['origin'], (1.0, 0.0, 0.0), (0.0, 0.0, 1.0))
    u, v, depth = local_coords(centers, frame)
    inside = in_polygon(u, v, pt.inset(keystone_outline(KEYSTONE), 0.0012)) | in_polygon(u, v, pt.inset(keystone_stem(), 0.001)) | in_polygon(u, v, pt.inset(keystone_crown(KEYSTONE), 0.001))
    doomed |= inside & (depth > 0.0)
    for spec in SPIKES:
        frame = pt.face_frame(spec['origin'], (1.0, 0.0, 0.0), (0.0, 0.0, 1.0))
        u, v, depth = local_coords(centers, frame)
        doomed |= in_polygon(u, v, pt.inset(spike_outline(), 0.001)) & (depth > 0.0008)
    delete_faces(obj, doomed, 'ornament')


def classify(obj):
    me = obj.data
    hue, saturation, value = source_colors(obj)
    centers, normals, areas, _ = face_arrays(me)
    graph = FaceGraph(me)
    (h0, h1, h2, h3), sat, val = GOLD_SCORE
    warm = smoothstep(h0, h1, hue) * (1.0 - smoothstep(h2, h3, hue))
    colour = warm * smoothstep(sat[0], sat[1], saturation) * smoothstep(val[0], val[1], value)
    x, y, z = centers[:, 0] - CENTER_X, centers[:, 1], centers[:, 2]
    ax = np.abs(x)
    molding = (z > 0.05) & (z < 0.205) & (ax > 0.11)
    capital = (z > 0.462) & (z < 0.53) & (ax > 0.15) & (ax < 0.28)
    ring = np.hypot(x, z - LINER[0])
    bands = (z > 0.48) & (ring > 0.18) & (ring < 0.32)
    back = (y > -0.005) & (z > 0.12)
    zone = molding | capital | bands | back
    gold = (graph.smooth(colour, 3) > 0.45) & zone
    gold = graph.despeckle(gold, areas, GOLD_SPECK, STONE_SPECK)
    (m0, m1, m2, m3), msat, mval = MOSS
    green = smoothstep(m0, m1, hue) * (1.0 - smoothstep(m2, m3, hue)) * smoothstep(msat[0], msat[1], saturation) * smoothstep(mval[0], mval[1], value)
    moss = graph.smooth(green, 2) * (~gold)
    print('gold faces', int(gold.sum()), 'moss', int((moss > 0.5).sum()))
    counts = np.maximum(np.bincount(graph.loop_vert, minlength=graph.verts), 1)
    for name, values in (('Gold', gold.astype(np.float64)), ('Moss', moss)):
        at_verts = np.bincount(graph.loop_vert, weights=values[graph.loop_face], minlength=graph.verts) / counts
        attribute = me.attributes.new(name, 'FLOAT', 'POINT')
        attribute.data.foreach_set('value', at_verts.astype(np.float32))


def transform(obj):
    me = obj.data
    co = np.empty(len(me.vertices) * 3, np.float64)
    me.vertices.foreach_get('co', co)
    me.vertices.foreach_set('co', place(co.reshape(-1, 3)).ravel())
    me.update()


def median(values, width):
    padded = np.pad(values, width // 2, mode='edge')
    return np.array([np.median(padded[i:i + width]) for i in range(len(values))])


def liner_path(obj):
    me = obj.data
    co = np.empty(len(me.vertices) * 3, np.float64)
    me.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    tree = BVHTree.FromPolygons([Vector(v) for v in co], [p.vertices[:] for p in me.polygons])
    center_z = LINER[0]
    heights = list(np.arange(BASE_Z + 0.003, center_z, LINER[1]))
    directions = [(Vector((-1.0, 0.0, 0.0)), z) for z in heights]
    for k in range(1, LINER[2]):
        a = math.pi - math.pi * k / LINER[2]
        directions.append((Vector((math.cos(a), 0.0, math.sin(a))), center_z))
    directions += [(Vector((1.0, 0.0, 0.0)), z) for z in reversed(heights)]
    distances = []
    for direction, z in directions:
        found = [hit[3] for hit in (tree.ray_cast(Vector((CENTER_X, y, z)), direction, 0.5) for y in LINER[3]) if hit[0] is not None]
        distances.append(min(found) if found else np.nan)
    distances = np.array(distances)
    valid = ~np.isnan(distances)
    distances = np.interp(np.arange(len(distances)), np.nonzero(valid)[0], distances[valid])
    distances = median(distances, 7)
    count = len(heights)
    left, arc, right = distances[:count], distances[count:len(distances) - count], distances[len(distances) - count:][::-1]
    shoulder = np.array(heights) > center_z - 0.13
    arc = np.full(len(arc), 0.5 * (np.median(left[shoulder]) + np.median(right[shoulder])))
    distances = np.concatenate((left, arc, right[::-1]))
    angles = np.array([math.pi - math.pi * k / LINER[2] for k in range(1, LINER[2])])

    def reach(x, z):
        x = np.asarray(x) - CENTER_X
        z = np.asarray(z)
        side = np.where(x < 0.0, np.interp(z, heights, left), np.interp(z, heights, right))
        angle = np.arctan2(np.maximum(z - center_z, 0.0), x)
        curve = np.interp(angle, angles[::-1], arc[::-1])
        return np.where(z < center_z, np.abs(x) - side, np.hypot(x, np.maximum(z - center_z, 0.0)) - curve)

    directions = [(directions[0][0], BASE_Z - 0.008)] + directions + [(directions[-1][0], BASE_Z - 0.008)]
    distances = np.concatenate(([distances[0]], distances, [distances[-1]]))
    points = [(Vector((CENTER_X, 0.0, z)) + direction * d, direction) for (direction, z), d in zip(directions, distances)]
    sides = [d for (direction, z), d in zip(directions, distances) if abs(direction.z) < 0.1 and 0.2 < z < center_z - 0.02]
    top = arc[len(arc) // 2]
    return points, float(np.median(sides)), float(top), reach


def groove_band(z):
    t = smoothstep(*LINER_ARCH[1], z)
    (p0, p1), (a0, a1) = LINER[4], LINER_ARCH[0]
    return p0 + (a0 - p0) * t, p1 + (a1 - p1) * t


def clear_groove(obj, reach):
    centers, normals, _, _ = face_arrays(obj.data)
    y0, y1 = groove_band(centers[:, 2])
    inside = (centers[:, 1] > y0 + 0.003) & (centers[:, 1] < y1 - 0.005) & (reach(centers[:, 0], centers[:, 2]) < 0.004) & (np.abs(normals[:, 1]) < 0.6)
    delete_faces(obj, inside, 'groove')


def sweep(points, radius, sides=6):
    bm = bmesh.new()
    rings = []
    for i, p in enumerate(points):
        ahead = points[min(i + 1, len(points) - 1)] - points[max(i - 1, 0)]
        ahead.normalize()
        side = ahead.cross(Vector((0.0, 1.0, 0.0)))
        if side.length < 1e-6:
            side = Vector((1.0, 0.0, 0.0))
        side.normalize()
        up = side.cross(ahead)
        rings.append([bm.verts.new(p + (side * math.cos(2.0 * math.pi * k / sides) + up * math.sin(2.0 * math.pi * k / sides)) * radius) for k in range(sides)])
    for i in range(len(rings) - 1):
        for k in range(sides):
            bm.faces.new((rings[i][k], rings[i + 1][k], rings[i + 1][(k + 1) % sides], rings[i][(k + 1) % sides]))
    bm.faces.new(rings[0][::-1])
    bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def build_liner(builder, mats, path):
    bm = bmesh.new()
    sections = []
    for point, direction in path:
        inner = point - direction * LINER[5]
        outer = point + direction * LINER[6]
        y0, y1 = (float(v) for v in groove_band(point.z))
        section = []
        for base, y in ((inner, y0), (inner, y1), (outer, y1), (outer, y0)):
            section.append(bm.verts.new(place_point((base.x, y, base.z))))
        sections.append(section)
    for i in range(len(sections) - 1):
        a, b = sections[i], sections[i + 1]
        for k in range(4):
            bm.faces.new((a[k], b[k], b[(k + 1) % 4], a[(k + 1) % 4]))
    bm.faces.new(sections[0][::-1])
    bm.faces.new(sections[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    builder.add('Inlay', bm, mats['inlay'], bevel=0.0)
    for edge in (0, 1):
        rod = [place_point(((point - direction * LINER[5]).x, float(groove_band(point.z)[edge]), (point - direction * LINER[5]).z)) for point, direction in path]
        builder.add('Gold', sweep(rod, LINER[7] * SCALE), mats['gold'], bevel=0.0)


def build_ornaments(builder, mats):
    rng = builder.rng
    gold, inlay = mats['gold'], mats['inlay']
    for spec in PLAQUES:
        frame = plaque_frame(spec, source=False)
        frame = frame @ Matrix.Translation((0.0, -PLAQUE_DROP * SCALE, 0.0))
        w = (spec['width'] + PLAQUE_GROW) * SCALE
        g, i, _ = pt.plaque(frame, w, (PLAQUE_HEIGHT + PLAQUE_DROP) * SCALE, rng, panel=spec['panel'] * SCALE, relief=spec['relief'] * SCALE, embed=0.07, border=PLAQUE_BORDER, corner=0.0, runes=4, loop=True, tab=(PLAQUE_TAB - PLAQUE_BOTTOM - PLAQUE_HEIGHT - 0.6 * MOTIF - PLAQUE_DROP) * SCALE, motif=MOTIF * SCALE)
        builder.add('Gold', g, gold, bevel=0.009, angle=40.0)
        builder.add('Inlay', i, inlay, bevel=0.012, angle=40.0)
    build_keystone(builder, gold, inlay, rng)
    for spec in SPIKES:
        build_spike(builder, gold, spec)


def build_keystone(builder, gold, inlay, rng):
    k = KEYSTONE
    frame = pt.face_frame(place_point(k['origin']), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0))
    S = SCALE
    outline = [(u * S, v * S) for u, v in keystone_outline(k)]
    panel, relief, border = k['panel'] * S, k['relief'] * S, k['border'] * S
    rim = panel + relief
    sink = 0.3 * relief
    gold_parts = [pt.nick(pt.ring_prism(outline, pt.inset(outline, border), panel - sink, rim), rng, 2, 0.6 * border, rim)]
    inlay_parts = [pt.prism(outline, 0.0, panel)]
    h, t = k['half'] * S, k['top'] * S
    for side in (-1.0, 1.0):
        post = [(side * (h - border), 0.75 * t), (side * h, 0.75 * t), (side * h, t + 0.1), (side * (h - 0.5 * border), t + 0.16), (side * (h - border), t + 0.1)]
        post = post if side > 0.0 else post[::-1]
        gold_parts.append(pt.prism(post, panel - sink, rim + 0.25 * relief))
    spine = [(-0.035, 0.66 * t), (0.035, 0.66 * t), (0.035, t + 0.17), (0.0, t + 0.27), (-0.035, t + 0.17)]
    gold_parts.append(pt.prism(spine, panel - sink, rim + 0.45 * relief))
    crown = [(-h, t - 0.07), (h, t - 0.07), (h, t + 0.012), (-h, t + 0.012)]
    inlay_parts.append(pt.prism(crown, 0.0, panel + 0.2 * relief))
    radius = 0.105
    my = 0.068 * S
    hgt = lambda f: panel + f * relief
    profile = [(0.0, panel - sink), (radius, panel - sink), (radius, rim - 0.1 * relief), (radius - 0.008, rim), (radius - 0.03, rim), (radius - 0.036, hgt(0.4)), (0.58 * radius, hgt(0.4)), (0.56 * radius, hgt(0.8)), (0.42 * radius, hgt(0.8)), (0.4 * radius, hgt(0.5)), (0.2 * radius, hgt(0.5)), (0.16 * radius, hgt(1.2)), (0.0, hgt(1.3))]
    disc = pk.lathe(profile, 36)
    bmesh.ops.translate(disc, verts=disc.verts, vec=(0.0, my, 0.0))
    gold_parts.append(disc)
    stud = [(0.0, 0.2), (0.06, 0.27), (0.0, 0.34), (-0.06, 0.27)]
    gold_parts.append(pt.prism(stud, panel - sink, hgt(0.8)))
    stem = [(u * S, v * S) for u, v in keystone_stem()]
    depth = (KEYSTONE['origin'][1] - (-0.045)) * S
    gold_parts.append(pt.prism(stem, depth, rim - 0.1 * relief))
    groove = pt.taper_stroke(pt.bezier((0.0, 0.03), (0.045, -0.06), (-0.045, -0.17), (0.0, -0.26)), 0.034, 0.5)
    gold_parts.append(pt.prism(groove, rim - 0.2 * relief, rim + 0.15 * relief))
    builder.add('Gold', pt.transformed(pt.weather(pt.merge(gold_parts), rng.uniform(0.0, 100.0), amplitude=0.003), frame), gold, bevel=0.009, angle=40.0)
    builder.add('Inlay', pt.transformed(pt.merge(inlay_parts), frame), inlay, bevel=0.012, angle=40.0)


def build_spike(builder, gold, spec):
    frame = pt.face_frame(place_point(spec['origin']), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0))
    S = SCALE
    w, h, tip, front = SPIKE
    outline = [(u * S, v * S) for u, v in spike_outline()]
    parts = [pt.prism(outline, -0.012, 0.016)]
    for side in (-1.0, 1.0):
        ridge = [(side * 0.038 - 0.022, 0.2), (side * 0.038 + 0.022, 0.2), (side * 0.038 + 0.022, h * S - 0.04), (side * 0.038 - 0.022, h * S - 0.04)]
        parts.append(pt.prism(ridge, 0.01, front * S))
    point = [(-0.06, 0.13), (0.0, 0.045), (0.06, 0.13), (0.0, 0.19)]
    parts.append(pt.prism(point, 0.01, front * S + 0.004))
    builder.add('Gold', pt.transformed(pt.weather(pt.nick(pt.merge(parts), builder.rng, 2, 0.012), builder.rng.uniform(0.0, 100.0), amplitude=0.0025), frame), gold, bevel=0.008, angle=40.0)


def high_material(source):
    tree_source = source.node_tree
    image = next(n.image for n in tree_source.nodes if n.type == 'TEX_IMAGE' and n.image and 'normal' not in n.image.name and 'metal' not in n.image.name)
    material = bpy.data.materials.new('PortalHigh')
    material.use_nodes = True
    tree = material.node_tree
    tree.nodes.clear()
    output = tree.nodes.new('ShaderNodeOutputMaterial')
    bsdf = tree.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.name = 'BSDF'
    emission = tree.nodes.new('ShaderNodeEmission')
    emission.name = 'Emit'
    tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    texture = tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    texture.name = 'OUT_src'

    def attribute(name):
        node = tree.nodes.new('ShaderNodeAttribute')
        node.attribute_name = name
        node.attribute_type = 'GEOMETRY'
        return node.outputs['Fac']

    def combine(name, r, g, b):
        node = tree.nodes.new('ShaderNodeCombineColor')
        node.name = name
        for socket, value in zip(node.inputs, (r, g, b)):
            if isinstance(value, float):
                socket.default_value = value
            else:
                tree.links.new(value, socket)
        return node

    combine('OUT_mask', attribute('Gold'), attribute('Moss'), 0.0)
    geometry = tree.nodes.new('ShaderNodeNewGeometry')
    split = tree.nodes.new('ShaderNodeSeparateXYZ')
    tree.links.new(geometry.outputs['Normal'], split.inputs['Vector'])
    up = tree.nodes.new('ShaderNodeMapRange')
    up.inputs['From Min'].default_value = -1.0
    tree.links.new(split.outputs['Z'], up.inputs['Value'])
    coords = tree.nodes.new('ShaderNodeTexCoord')

    def noise(scale, detail=3.0):
        node = tree.nodes.new('ShaderNodeTexNoise')
        node.inputs['Scale'].default_value = scale
        node.inputs['Detail'].default_value = detail
        node.inputs['Roughness'].default_value = 0.55
        tree.links.new(coords.outputs['Object'], node.inputs['Vector'])
        return node.outputs['Fac']

    combine('OUT_shape', geometry.outputs['Pointiness'], up.outputs['Result'], noise(4.0, 2.0))
    combine('OUT_noise', noise(0.16), noise(0.8), noise(2.0))
    blocks = tree.nodes.new('ShaderNodeTexVoronoi')
    blocks.inputs['Scale'].default_value = 0.32
    tree.links.new(coords.outputs['Object'], blocks.inputs['Vector'])
    pick = tree.nodes.new('ShaderNodeSeparateColor')
    tree.links.new(blocks.outputs['Color'], pick.inputs['Color'])
    cracks = tree.nodes.new('ShaderNodeTexVoronoi')
    cracks.feature = 'DISTANCE_TO_EDGE'
    cracks.inputs['Scale'].default_value = 1.4
    tree.links.new(coords.outputs['Object'], cracks.inputs['Vector'])
    combine('OUT_detail', pick.outputs['Red'], noise(0.065, 2.0), cracks.outputs['Distance'])
    grain = tree.nodes.new('ShaderNodeTexNoise')
    grain.inputs['Scale'].default_value = 1.3
    grain.inputs['Detail'].default_value = 5.0
    grain.inputs['Roughness'].default_value = 0.6
    tree.links.new(coords.outputs['Object'], grain.inputs['Vector'])
    plain = tree.nodes.new('ShaderNodeMath')
    plain.operation = 'SUBTRACT'
    plain.inputs[0].default_value = 1.0
    tree.links.new(attribute('Gold'), plain.inputs[1])
    strength = tree.nodes.new('ShaderNodeMath')
    strength.operation = 'MULTIPLY'
    strength.inputs[1].default_value = 0.55
    tree.links.new(plain.outputs[0], strength.inputs[0])
    bump = tree.nodes.new('ShaderNodeBump')
    bump.inputs['Distance'].default_value = 0.004
    seam = tree.nodes.new('ShaderNodeMapRange')
    seam.inputs['From Min'].default_value = 0.0
    seam.inputs['From Max'].default_value = 0.012
    seam.inputs['To Min'].default_value = 1.0
    seam.inputs['To Max'].default_value = 0.0
    tree.links.new(cracks.outputs['Distance'], seam.inputs['Value'])
    gate = tree.nodes.new('ShaderNodeMath')
    gate.operation = 'MULTIPLY'
    tree.links.new(seam.outputs['Result'], gate.inputs[0])
    reach = tree.nodes.new('ShaderNodeMapRange')
    reach.inputs['From Min'].default_value = 0.5
    reach.inputs['From Max'].default_value = 0.7
    tree.links.new(noise(0.065, 2.0), reach.inputs['Value'])
    tree.links.new(reach.outputs['Result'], gate.inputs[1])
    carve = tree.nodes.new('ShaderNodeMath')
    carve.operation = 'MULTIPLY_ADD'
    carve.inputs[1].default_value = -0.7
    tree.links.new(gate.outputs[0], carve.inputs[0])
    tree.links.new(grain.outputs['Fac'], carve.inputs[2])
    tree.links.new(carve.outputs[0], bump.inputs['Height'])
    tree.links.new(strength.outputs[0], bump.inputs['Strength'])
    tree.links.new(bump.outputs['Normal'], bsdf.inputs['Normal'])
    return material


def bake_from_high(high, low, channels, size):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 4
    tree = high.data.materials[0].node_tree
    output = next(n for n in tree.nodes if n.type == 'OUTPUT_MATERIAL')
    emission = tree.nodes['Emit']
    holder = hooded.target_image(low, low.name + 'Bake')
    maps = {}
    for channel in channels:
        image = bpy.data.images.new(f'{low.name}_{channel}', size, size, alpha=False, float_buffer=True)
        holder.image = image
        if channel == 'normal':
            tree.links.new(tree.nodes['BSDF'].outputs['BSDF'], output.inputs['Surface'])
            options = {'type': 'NORMAL', 'normal_space': 'TANGENT'}
        else:
            node = tree.nodes['OUT_' + channel]
            tree.links.new(node.outputs[0], emission.inputs['Color'])
            tree.links.new(emission.outputs['Emission'], output.inputs['Surface'])
            options = {'type': 'EMIT'}
        kit.select_only([high, low], active=low)
        started = time.time()
        bpy.ops.object.bake(use_selected_to_active=True, cage_extrusion=CAGE[0], max_ray_distance=CAGE[1], margin=16, **options)
        print(f'  baked {low.name} {channel} in {time.time() - started:.1f} s')
        maps[channel] = kit.image_pixels(image)
        bpy.data.images.remove(image)
    tree.links.new(tree.nodes['BSDF'].outputs['BSDF'], output.inputs['Surface'])
    return maps


def bake_ao(node, others, size):
    scene = bpy.context.scene
    scene.cycles.samples = 64
    scene.world.light_settings.distance = AO_DISTANCE
    holder = hooded.target_image(node, node.name + 'AO')
    image = bpy.data.images.new(f'{node.name}_ao', size, size, alpha=False, float_buffer=True)
    holder.image = image
    hidden = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o is not node and o not in others and not o.hide_render]
    for obj in hidden:
        obj.hide_render = True
    kit.select_only([node])
    bpy.ops.object.bake(type='AO', margin=16)
    for obj in hidden:
        obj.hide_render = False
    ao = kit.image_pixels(image)[..., 0]
    bpy.data.images.remove(image)
    return ao


def decimate(obj, faces):
    modifier = obj.modifiers.new('Decimate', 'DECIMATE')
    modifier.ratio = min(1.0, faces / max(len(obj.data.polygons), 1))
    if 'Keep' in obj.vertex_groups:
        modifier.vertex_group = 'Keep'
        modifier.vertex_group_factor = 1.0
    kit.apply_modifiers(obj)
    obj.vertex_groups.clear()
    print('decimated to', len(obj.data.polygons), 'faces')


def protect_boundary(obj):
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    edge = np.zeros(len(bm.verts), bool)
    for e in bm.edges:
        if e.is_boundary:
            edge[e.verts[0].index] = True
            edge[e.verts[1].index] = True
    bm.free()
    group = obj.vertex_groups.new(name='Keep')
    group.add(np.nonzero(~edge)[0].tolist(), 1.0, 'REPLACE')
    group.add(np.nonzero(edge)[0].tolist(), 0.0, 'REPLACE')
    return int(edge.sum())


def planar_uv(obj, half):
    me = obj.data
    layer = me.uv_layers.new(name='UVMap')
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    loops = np.empty(len(me.loops), np.int32)
    me.loops.foreach_get('vertex_index', loops)
    uv = np.stack((co[loops, 0] / (2.0 * half) + 0.5, co[loops, 1] / (2.0 * half) + 0.5), axis=1)
    layer.data.foreach_set('uv', uv.astype(np.float32).ravel())


def unwrap(obj):
    kit.select_only([obj])
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(UV_ANGLE), island_margin=0.0015, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    hooded.separate_overlaps(obj)
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.select_all(action='SELECT')
    bpy.ops.uv.average_islands_scale()
    bpy.ops.uv.pack_islands(rotate=True, margin=0.002, shape_method='CONCAVE')
    bpy.ops.object.mode_set(mode='OBJECT')


def normalized(values, low=5.0, high=95.0, mask=None):
    sample = values[mask] if mask is not None and mask.any() else values
    a, b = np.percentile(sample, low), np.percentile(sample, high)
    return np.clip((values - a) / max(b - a, 1e-6), 0.0, 1.0)


def compose_arch(maps, ao):
    mask = maps['mask']
    gold = smoothstep(0.35, 0.65, mask[..., 0])
    moss = smoothstep(0.25, 0.75, mask[..., 1]) * (1.0 - gold)
    shape = maps['shape']
    noise = maps['noise']
    src = maps['src'][..., :3]
    covered = src.sum(axis=-1) > 0.0
    point = normalized(shape[..., 0], 2.0, 98.0, covered)
    convex = smoothstep(0.55, 0.85, point)
    concave = 1.0 - smoothstep(0.15, 0.45, point)
    up = shape[..., 1]
    fine = shape[..., 2]
    large, medium, small = noise[..., 0], noise[..., 1], noise[..., 2]
    block, broad, crack = maps['detail'][..., 0], maps['detail'][..., 1], maps['detail'][..., 2]
    tone = src @ np.array((0.2126, 0.7152, 0.0722), dtype=np.float32)
    srgb = kit.linear_to_srgb(np.clip(src, 0.0, 1.0))
    top_c, low_c = srgb.max(axis=-1), srgb.min(axis=-1)
    chroma = np.where(top_c > 1e-4, (top_c - low_c) / np.maximum(top_c, 1e-4), 0.0)
    hue_deg = np.degrees(np.arctan2(math.sqrt(3.0) * (srgb[..., 1] - srgb[..., 2]), 2.0 * srgb[..., 0] - srgb[..., 1] - srgb[..., 2])) % 360.0
    glow = smoothstep(0.15, 0.35, chroma) * (hue_deg > 220.0) * (hue_deg < 310.0)
    painted = pt.blur(tone, 1)
    stone_mask = (gold < 0.5) & covered
    median = np.median(painted[stone_mask]) if stone_mask.any() else 0.2
    variation = np.clip(1.0 + 0.5 * (painted / max(median, 1e-4) - 1.0), 0.68, 1.32)
    variation = variation * (1.0 - glow) + glow * np.clip(variation, 0.68, 1.0)
    stone_light = np.asarray(pt.STONE, np.float32)
    stone_dark = np.asarray(pt.STONE_DARK, np.float32)
    mix = np.clip(0.5 * block + 0.28 * broad + 0.22 * large, 0.0, 1.0)
    stone_base = stone_dark[None, None, :] + (stone_light - stone_dark)[None, None, :] * (0.25 + 0.75 * mix)[..., None]
    hue = 1.0 + np.array((0.04, 0.0, -0.05), np.float32)[None, None, :] * (block - 0.5)[..., None] * 2.0
    stone_base = stone_base * hue * (variation * (0.93 + 0.14 * fine))[..., None]
    for low_edge, high_edge, tint in ((0.56, 0.74, (1.1, 0.98, 0.84)), (0.76, 0.9, (0.88, 0.92, 1.04)), (0.93, 1.01, (0.66, 0.64, 0.64)), (-0.01, 0.12, (1.16, 1.14, 1.1))):
        family = smoothstep(low_edge, low_edge + 0.02, block) * (1.0 - smoothstep(high_edge - 0.02, high_edge, block))
        stone_base = stone_base * (1.0 + (np.asarray(tint, np.float32)[None, None, :] - 1.0) * (0.8 * family)[..., None])
    wear = convex * smoothstep(0.38, 0.62, small * 0.6 + medium * 0.4)
    stone_base = stone_base * (1.0 + 0.38 * wear)[..., None]
    fissure = 0.6 * (1.0 - smoothstep(0.002, 0.009, crack)) * smoothstep(0.5, 0.7, broad)
    grime = np.clip(0.85 * (1.0 - ao) + 0.45 * concave + 0.6 * fissure, 0.0, 1.0) * (0.65 + 0.35 * medium)
    stone_base = stone_base * (1.0 - 0.6 * grime)[..., None] + pt.srgb_to_linear((0.17, 0.15, 0.14))[None, None, :] * (0.25 * grime)[..., None]
    low = 1.0 - smoothstep(0.0, 0.6, up)
    moss_tone = pt.srgb_to_linear((0.24, 0.29, 0.1))[None, None, :] * (0.65 + 0.6 * small)[..., None]
    moss = np.clip(moss * (0.6 + 0.4 * smoothstep(0.35, 0.6, large)), 0.0, 1.0)
    stone_base = stone_base * (1.0 - moss[..., None]) + moss_tone * moss[..., None]
    stone_base = stone_base * (1.0 - 0.5 * fissure)[..., None]
    stone_rough = np.clip(0.8 + 0.1 * (medium - 0.5) + 0.1 * grime - 0.2 * wear + 0.06 * moss + 0.1 * fissure, 0.5, 0.96)
    gold_shade = (0.86 + 0.16 * large + 0.08 * fine) * (1.0 + 0.2 * convex)
    tarnish = np.clip(0.55 * (1.0 - ao) + 0.45 * concave, 0.0, 1.0) * (0.4 + 0.4 * medium)
    gold_crack = (1.0 - smoothstep(0.002, 0.006, crack)) * smoothstep(0.62, 0.78, broad)
    gold_base = np.asarray(pt.GOLD_TONE)[None, None, :] * gold_shade[..., None]
    gold_base = gold_base * (1.0 - tarnish[..., None]) + pt.srgb_to_linear((0.26, 0.2, 0.11))[None, None, :] * tarnish[..., None]
    gold_base = gold_base * (1.0 - 0.8 * gold_crack)[..., None]
    base = np.clip(stone_base * (1.0 - gold)[..., None] + gold_base * gold[..., None], 0.0, 1.0)
    gold_rough = np.clip(0.14 + 0.08 * small + 0.34 * tarnish - 0.06 * convex + 0.4 * gold_crack, 0.1, 0.75)
    rough = stone_rough * (1.0 - gold) + gold_rough * gold
    metal = np.clip(gold * (1.0 - 0.4 * tarnish - 0.5 * gold_crack), 0.0, 1.0)
    orm = np.ones(base.shape[:2] + (3,), np.float32)
    orm[..., 0] = np.clip(0.25 + 0.75 * ao, 0.0, 1.0) * (1.0 - 0.3 * concave * (1.0 - gold))
    orm[..., 1] = np.clip(rough, 0.05, 1.0)
    orm[..., 2] = metal
    return base, orm


def banner_length(u, length):
    return length - BANNER_NOTCH * (1.0 - np.abs(2.0 * u - 1.0))


def build_banner(target, material):
    width, length = BANNER_SIZE
    columns, rows = BANNER_MESH
    top = -BANNER_RING[0] - 0.004
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new('UVMap')
    for side, offset in ((1.0, -0.5 * BANNER_THICK), (-1.0, 0.5 * BANNER_THICK)):
        grid = []
        for j in range(rows + 1):
            row = []
            t = j / rows
            for i in range(columns + 1):
                u = i / columns
                z = top - banner_length(u, length) * t
                vert = bm.verts.new(((u - 0.5) * width, offset, z))
                row.append((vert, u, t))
            grid.append(row)
        for j in range(rows):
            for i in range(columns):
                quad = [grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]]
                if side > 0.0:
                    quad = quad[::-1]
                face = bm.faces.new([q[0] for q in quad])
                for loop, (_, u, v) in zip(face.loops, quad):
                    loop[uv_layer].uv = (u, 1.0 - v)
    bm.normal_update()
    obj = kit.mesh_object('Banner', bm, target, material)
    rings = []
    for k in range(BANNER_RINGS):
        x = ((k + 0.5) / BANNER_RINGS - 0.5) * width * 0.86
        major, minor = BANNER_RING
        ring_bm = bmesh.new()
        ring_uv = ring_bm.loops.layers.uv.new('UVMap')
        segments, sides = 16, 6
        verts = []
        for s in range(segments):
            a = 2.0 * math.pi * s / segments
            column = []
            for q in range(sides):
                b = 2.0 * math.pi * q / sides
                radius = major + minor * math.cos(b)
                column.append(ring_bm.verts.new((x + minor * math.sin(b), radius * math.cos(a), radius * math.sin(a))))
            verts.append(column)
        for s in range(segments):
            for q in range(sides):
                face = ring_bm.faces.new((verts[s][q], verts[(s + 1) % segments][q], verts[(s + 1) % segments][(q + 1) % sides], verts[s][(q + 1) % sides]))
                for loop in face.loops:
                    loop[ring_uv].uv = RING_UV
        ring_bm.normal_update()
        rings.append(kit.mesh_object('BannerRing', ring_bm, target, material))
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for ring in rings:
        bm.from_mesh(ring.data)
        bpy.data.objects.remove(ring, do_unlink=True)
    bmesh.ops.recalc_face_normals(bm, faces=[f for f in bm.faces if f.calc_center_median().z > top + 0.001])
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.shade_smooth()
    return obj, width, length, top


def banner_joints(width, length, top):
    columns, rows = BANNER_GRID
    joints = []
    for r in range(rows):
        t = r / (rows - 1)
        for c in range(columns):
            u = c / (columns - 1)
            joints.append((f'c{c}r{r}', u, t, Vector(((u - 0.5) * width, 0.0, top - float(banner_length(u, length)) * t))))
    return joints


def rig_banner(target, obj, width, length, top):
    data = bpy.data.armatures.new('PortalBanner')
    rig = bpy.data.objects.new('PortalBanner', data)
    target.objects.link(rig)
    kit.select_only([rig])
    bpy.ops.object.mode_set(mode='EDIT')
    root = data.edit_bones.new('banner')
    root.head = (0.0, 0.0, 0.0)
    root.tail = (0.0, 0.0, 0.1)
    joints = banner_joints(width, length, top)
    for name, u, t, position in joints:
        bone = data.edit_bones.new(name)
        bone.head = position
        bone.tail = position + Vector((0.0, 0.0, -0.08))
        bone.parent = root
    bpy.ops.object.mode_set(mode='OBJECT')
    columns, rows = BANNER_GRID
    groups = {name: obj.vertex_groups.new(name=name) for name, _, _, _ in joints}
    for vertex in obj.data.vertices:
        u = float(np.clip(vertex.co.x / width + 0.5, 0.0, 1.0))
        span = float(banner_length(u, length))
        t = float(np.clip((top - vertex.co.z) / span, 0.0, 1.0)) if vertex.co.z < top else 0.0
        fc = u * (columns - 1)
        fr = t * (rows - 1)
        c0 = min(int(fc), columns - 2)
        r0 = min(int(fr), rows - 2)
        wc = fc - c0
        wr = fr - r0
        for dc, dr, weight in ((0, 0, (1 - wc) * (1 - wr)), (1, 0, wc * (1 - wr)), (0, 1, (1 - wc) * wr), (1, 1, wc * wr)):
            if weight > 1e-4:
                groups[f'c{c0 + dc}r{r0 + dr}'].add([vertex.index], weight, 'REPLACE')
    obj.parent = rig
    obj.matrix_parent_inverse = Matrix.Identity(4)
    modifier = obj.modifiers.new('Armature', 'ARMATURE')
    modifier.object = rig
    return rig


def banner_maps(width, length, size):
    v_coords = (np.arange(size) + 0.5) / size
    u_coords = (np.arange(size) + 0.5) / size
    u, v = np.meshgrid(u_coords, v_coords)
    x = (u - 0.5) * width
    z = v * length
    texel = length / size
    edge_length = banner_length(u, length)
    distance_side = np.minimum(u, 1.0 - u) * width
    notch_slope = BANNER_NOTCH / (0.5 * width)
    distance_bottom = (edge_length - z) / math.sqrt(1.0 + notch_slope * notch_slope)
    distance_edge = np.minimum(distance_side, distance_bottom)
    distance_top = z
    inset, band = 0.024, 0.014
    border = 1.0 - smoothstep(band * 0.5 - texel, band * 0.5 + texel, np.abs(np.minimum(distance_edge, distance_top - 0.035) - inset - band * 0.5))
    hem = 1.0 - smoothstep(0.04 - texel, 0.04 + texel, distance_top)
    cx, cz = 0.0, 0.3
    rx, rz = x - cx, z - cz
    rr = np.hypot(rx, rz)
    ring = 1.0 - smoothstep(0.007 - texel, 0.007 + texel, np.abs(rr - 0.095))
    angle = np.arctan2(rz, rx) % (2.0 * math.pi)
    pitch = 0.075 / 2.2
    best = np.full(rr.shape, 1e9)
    for k in range(4):
        theta = angle + 2.0 * math.pi * k
        valid = (theta > 0.9) & (theta < 2.2 * 2.0 * math.pi)
        best = np.minimum(best, np.where(valid, np.abs(rr - pitch / (2.0 * math.pi) * theta), 1e9))
    swirl = (1.0 - smoothstep(0.005 - texel, 0.005 + texel, best)) * (rr < 0.08)
    dot = 1.0 - smoothstep(0.01 - texel, 0.01 + texel, rr)
    gold = np.clip(border + ring + swirl + dot, 0.0, 1.0)
    gold = np.where(distance_bottom < 0.0, 0.0, gold)
    weave = 0.5 + 0.5 * np.sin(x * 2.0 * math.pi / 0.004) * np.sin(z * 2.0 * math.pi / 0.004)
    mottle = pt.fbm(x * 3.0, z * 3.0, 0.9, 41.0)
    dirt = smoothstep(0.55, 1.0, z / length) * 0.3
    cloth = pt.srgb_to_linear(BANNER_RED)[None, None, :] * ((0.82 + 0.3 * mottle) * (0.96 + 0.06 * weave) * (1.0 - dirt) * (1.0 - 0.25 * hem))[..., None]
    thread = np.asarray(pt.GOLD_TONE)[None, None, :] * (0.8 + 0.2 * mottle)[..., None]
    ring_zone = (np.abs(u - RING_UV[0]) < 0.04) & (v > 1.0 - 2.0 * RING_UV[1])
    gold = np.where(ring_zone, 1.0, gold)
    base = cloth * (1.0 - gold)[..., None] + thread * gold[..., None]
    height = 0.0008 * pt.blur(gold, 2)
    gy, gx = np.gradient(height, texel, width / size)
    normal = np.stack((-gx, gy, np.ones_like(gx)), axis=-1)
    normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
    orm = np.ones(base.shape[:2] + (3,), np.float32)
    orm[..., 1] = np.clip(0.86 * (1.0 - gold) + 0.38 * gold, 0.05, 1.0)
    orm[..., 2] = 0.85 * gold
    thin = 1.0 - 0.75 * gold
    flipped = lambda image: image[::-1]
    return flipped(np.clip(base, 0.0, 1.0)), flipped(normal * 0.5 + 0.5), flipped(orm), flipped(thin)


def write_maps(folder, slot, base, normal, orm, emit=None, thin=None):
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
    if thin is not None:
        files['thin'] = os.path.join(folder, f'{NAME}_{slot}_thin.png')
        kit.save_png(files['thin'], np.repeat(thin[..., None], 3, axis=2), alpha=thin)
    return files


def assign(node, slot, files):
    placeholder = bpy.data.materials.new(slot)
    node.data.materials.clear()
    node.data.materials.append(kit.baked_material(placeholder, files))


def engine_point(p):
    return (round(float(p[0]), 3), round(float(p[2]) - pt.HEIGHT, 3), round(float(p[1]), 3))


def banner_anchors():
    result = []
    for side in (-1.0, 1.0):
        phi = side * pt.BANNER_ANGLE
        anchor = pt.polar(pt.WALL + 0.006, phi, pt.HEIGHT - pt.COPE[0] - 0.035)
        result.append((phi, anchor))
    return result


def opening(measured):
    side, top = measured
    base = place_point((CENTER_X, SHEET_Y, BASE_Z))
    springing = place_point((CENTER_X, SHEET_Y, LINER[0]))
    return dict(half=round(side * SCALE, 3), floor=round(float(base.z) - pt.HEIGHT, 3), spring=round(float(springing.z) - pt.HEIGHT, 3), radius=round(top * SCALE, 3), depth=round(float(base.y), 3), groove=(round(LINER[4][0] * SCALE, 3), round(LINER[4][1] * SCALE, 3)))


def footprint(obj):
    me = obj.data
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    low = co[:, 2] < pt.HEIGHT + 0.6
    result = []
    for side in (-1.0, 1.0):
        pick = low & (co[:, 0] * side > 0.4)
        lo, hi = co[pick].min(axis=0), co[pick].max(axis=0)
        result.append(((round(float(lo[0]), 3), round(float(lo[1]), 3)), (round(float(hi[0]), 3), round(float(hi[1]), 3))))
    return result


def report(arch, fires, measured):
    print('== engine constants (game model space: x, height above terrace, depth) ==')
    print('fires', [engine_point(f) for f in fires])
    print('banners', [(round(math.degrees(phi), 1), engine_point(a)) for phi, a in banner_anchors()], 'size', BANNER_SIZE)
    print('opening', opening(measured))
    print('arch footprint (blender x, y)', footprint(arch))
    print('terrace height', pt.HEIGHT, 'top', pt.TOP, 'stairs', pt.STAIR_IN, pt.STAIR_OUT, 'sector', round(math.degrees(pt.SECTOR), 1), 'rise', round(pt.RISE, 4), 'tread', pt.TREAD)


def prepare(target, fast=False):
    started = time.time()
    source = load(target)
    strip(source)
    path, side, top, reach = liner_path(source)
    clear_groove(source, reach)
    remove_ornaments(source)
    classify(source)
    transform(source)
    print(f'cleaned in {time.time() - started:.0f} s')
    builder = pt.Builder(target)
    mats = pt.materials()
    fires = pt.populate(builder, mats)
    build_ornaments(builder, mats)
    build_liner(builder, mats, path)
    mats['marks'] = pt.build_floor(builder, bpy.data.materials.new('PortalFloorTemp'), mats['marble'], mats['gold'])
    groups = builder.finish()
    return source, groups, fires, mats, (side, top)


def build(target, folder):
    started = time.time()
    source, groups, fires, mats, measured = prepare(target)
    original = source.data.materials[0]
    source.data.materials.clear()
    source.data.materials.append(high_material(original))
    print('protected', protect_boundary(source), 'boundary vertices')
    low = source.copy()
    low.data = source.data.copy()
    low.name = 'Arch'
    target.objects.link(low)
    for name in ('Gold', 'Moss'):
        if name in low.data.attributes:
            low.data.attributes.remove(low.data.attributes[name])
    decimate(low, FACES)
    source.vertex_groups.clear()
    kit.smooth_by_angle(low, 45.0)
    while low.data.uv_layers:
        low.data.uv_layers.remove(low.data.uv_layers[0])
    unwrap(low)
    floor = groups.pop('Floor')
    floor_half = pt.FLOOR_EDGE + 0.12
    planar_uv(floor, floor_half)
    kit_objects = [groups[name] for name in ('Terrace', 'Marble', 'Gold', 'Inlay', 'Ember')]
    pk.unwrap_all(kit_objects, margins={'TerraceStone': 0.0015, 'PortalMarble': 0.002, 'PortalGold': 0.002, 'PortalInlay': 0.003, 'PortalEmber': 0.004}, angle=50.0)
    print(f'low ready in {time.time() - started:.0f} s:', len(low.data.polygons), 'arch faces')
    maps = bake_from_high(source, low, ('mask', 'src', 'shape', 'noise', 'detail', 'normal'), TEXTURE)
    bpy.data.objects.remove(source, do_unlink=True)
    arch_ao = bake_ao(low, kit_objects + [floor], TEXTURE)
    proxy = pt.flat_disc(target, 'FloorProxy', pt.HEIGHT + 0.006)
    planar_uv(proxy, floor_half)
    floor_ao = bake_ao(proxy, kit_objects + [low], FLOOR_TEXTURE)
    bpy.data.objects.remove(proxy, do_unlink=True)
    base, orm = compose_arch(maps, arch_ao)
    arch_files = write_maps(folder, 'PortalStone', base, kit.sanitize_normals(maps['normal'], max_tilt=50.0), orm)
    floor_base, floor_normal, floor_orm = pt.floor_maps(floor_half, FLOOR_TEXTURE, floor_ao, mats['marks'])
    floor_files = write_maps(folder, 'PortalFloor', floor_base, floor_normal, floor_orm)
    emissive = {}
    baked = {}
    for key, obj in zip(('stone', 'marble', 'gold', 'inlay', 'ember'), kit_objects):
        material = mats[key]
        files = kit.bake_material(material, [obj], folder, NAME, ao_distance=KIT_AO)
        baked[obj.name] = (material, files)
        if 'emit' in files:
            emissive[material.name] = (tuple(material['emit_color']), float(material['emit_strength']))
        print(f'  baked {material.name} in {time.time() - started:.0f} s')
    for obj in kit_objects:
        material, files = baked[obj.name]
        obj.data.materials.clear()
        obj.data.materials.append(kit.baked_material(material, files))
    banner_material = bpy.data.materials.new('PortalBannerTemp')
    banner, width, length, top = build_banner(target, banner_material)
    banner_base, banner_normal, banner_orm, thin = banner_maps(width, length, BANNER_TEXTURE)
    banner_files = write_maps(folder, 'PortalBanner', banner_base, banner_normal, banner_orm, thin=thin)
    assign(low, 'PortalStone', arch_files)
    assign(floor, 'PortalFloor', floor_files)
    assign(banner, 'PortalBanner', banner_files)
    rig = rig_banner(target, banner, width, length, top)
    report(low, fires, measured)
    print(f'built in {time.time() - started:.0f} s')
    objects = [low, floor, banner] + kit_objects
    return objects, rig, emissive, {'PortalBanner': (BANNER_TRANSLUCENCY, banner_files['thin'])}


def closeup(path, focus, yaw, pitch, distance, size=1000, samples=48):
    scene = bpy.context.scene
    data = bpy.data.cameras.new('Closeup')
    data.angle = math.radians(40.0)
    camera = bpy.data.objects.new('Closeup', data)
    scene.collection.objects.link(camera)
    yaw_r, pitch_r = math.radians(yaw), math.radians(pitch)
    direction = Vector((math.cos(pitch_r) * math.sin(yaw_r), -math.cos(pitch_r) * math.cos(yaw_r), math.sin(pitch_r)))
    camera.location = Vector(focus) + direction * distance
    camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = camera
    scene.render.resolution_x = scene.render.resolution_y = size
    scene.cycles.samples = samples
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera)


SHOTS = {'game': (0.0, 57.0, 1.0), 'q': (35.0, 30.0, 1.0), 'front': (0.0, 12.0, 1.0), 'side': (90.0, 15.0, 1.0), 'top': (0.0, 89.0, 1.0), 'back': (180.0, 20.0, 1.0)}
CLOSEUPS = {
    'arch': ((0.0, ARCH_Y - 0.6, 4.2), 0.0, 8.0, 5.0),
    'plaque': ((-1.45, ARCH_Y - 0.4, 2.6), 15.0, 8.0, 2.2),
    'keystone': ((0.0, ARCH_Y - 0.4, 5.9), 0.0, 8.0, 2.2),
    'sideplaque': ((-1.9, ARCH_Y, 2.6), -80.0, 10.0, 2.4),
    'stairs': ((0.0, -3.6, 0.6), 25.0, 22.0, 5.0),
    'bowl': ((-3.5, -3.8, 1.5), 20.0, 30.0, 2.4),
    'wall': ((-3.6, -1.6, 0.5), -60.0, 15.0, 4.0),
    'base': ((-2.0, ARCH_Y - 0.6, 1.4), 20.0, 18.0, 2.6),
    'inner': ((-1.0, ARCH_Y, 2.2), 70.0, 10.0, 2.6),
    'junction': ((1.7, -2.3, 0.9), 0.0, 85.0, 2.4),
    'junction2': ((-1.7, -2.3, 0.9), 0.0, 85.0, 2.4),
}


def preview(objects, folder, views, banner=None):
    kit.environment('forest.exr', 1.1, 1.2)
    bpy.context.scene.view_settings.view_transform = 'Standard'
    statics = [o for o in objects if o.name != 'Banner']
    copies = []
    if banner is not None:
        banner.hide_render = True
        for phi, anchor in banner_anchors():
            copy = banner.copy()
            copy.data = banner.data
            copy.modifiers.clear()
            copy.parent = None
            bpy.context.scene.collection.objects.link(copy)
            copy.matrix_world = Matrix.Translation(anchor) @ Matrix.Rotation(phi, 4, 'Z')
            copies.append(copy)
    for view in views:
        if view in CLOSEUPS:
            closeup(os.path.join(folder, f'portal_{view}.png'), *CLOSEUPS[view])
            continue
        yaw, pitch, fill = SHOTS[view]
        kit.render(os.path.join(folder, f'portal_{view}.png'), statics + copies, yaw=yaw, pitch=pitch, size=(1200, 1100), samples=40, fill=fill)
    for copy in copies:
        bpy.data.objects.remove(copy, do_unlink=True)
    if banner is not None:
        banner.hide_render = False


def fast_preview(target, folder, views):
    source, groups, fires, mats, measured = prepare(target, fast=True)
    floor = groups['Floor']
    pt.preview_floor_material(floor, folder, mats['marks'])
    report(source, fires, measured)
    banner_material = pt.gold_material('BannerPreview', 256)
    banner, _, _, _ = build_banner(target, banner_material)
    preview([source] + list(groups.values()), folder, views, banner)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    out = None
    preview_dir = None
    views = ['game', 'q', 'front']
    fast = '--fast' in argv
    for i, arg in enumerate(argv):
        if arg == '--out':
            out = os.path.abspath(argv[i + 1])
        if arg == '--preview':
            preview_dir = os.path.abspath(argv[i + 1])
        if arg == '--views':
            views = argv[i + 1].split(',')
    folder = out or preview_dir
    os.makedirs(folder, exist_ok=True)
    kit.reset()
    kit.enable_gpu()
    bpy.context.preferences.filepaths.save_version = 0
    target = kit.collection(NAME)
    if fast:
        fast_preview(target, preview_dir, views)
        return
    objects, rig, emissive, translucent = build(target, folder)
    if out:
        kit.export_gltf(objects, os.path.join(out, NAME + '.gltf'), emissive, None, rig, False, translucent, None)
        bpy.ops.outliner.orphans_purge(do_recursive=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, NAME + '.blend'))
    if preview_dir:
        preview(objects, preview_dir, views, next(o for o in objects if o.name == 'Banner'))


if __name__ == '__main__':
    main()
