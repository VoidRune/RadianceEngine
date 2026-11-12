import bmesh
import math
import numpy as np
from mathutils import Vector, kdtree

import assetkit as kit

LAYERS = ('Albedo', 'Surface', 'Mask', 'Kind', 'Glow', 'Param')
WEIGHTS = {}


def look(tone, rough, metal=0.0, fabric=0.0, plate=0.0, leather=0.0, glow=0.0, trim=0.0, veins=0.0, gem=0.0):
    return {'Albedo': tuple(tone), 'Surface': (rough, metal, fabric), 'Mask': (veins, trim, gem), 'Kind': (plate, leather, 0.0), 'Glow': (glow, 0.0, 0.0)}


def smoothstep(low, high, value):
    t = np.clip((np.asarray(value, dtype=np.float64) - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def unit(v):
    v = np.asarray(v, dtype=np.float64)
    return v / max(np.linalg.norm(v), 1e-12)


def orth(v, axis):
    v = np.asarray(v, dtype=np.float64)
    return unit(v - axis * float(np.dot(v, axis)))


def table_at(table, z):
    table = np.asarray(table, dtype=np.float64)
    return [float(np.interp(z, table[:, 0], table[:, k])) for k in range(1, table.shape[1])]


def transport(points, up):
    frames = []
    tangent = unit(points[1] - points[0])
    normal = orth(up, tangent)
    for k in range(len(points)):
        new = unit(points[min(k + 1, len(points) - 1)] - points[max(k - 1, 0)])
        axis = np.cross(tangent, new)
        if np.linalg.norm(axis) > 1e-9:
            angle = math.atan2(np.linalg.norm(axis), float(np.dot(tangent, new)))
            c, s = math.cos(angle), math.sin(angle)
            a = unit(axis)
            normal = normal * c + np.cross(a, normal) * s + a * np.dot(a, normal) * (1.0 - c)
        tangent = new
        normal = orth(normal, tangent)
        frames.append((tangent, normal, np.cross(tangent, normal)))
    return frames


class Surface:
    eps = 1e-4

    def point(self, s, v):
        raise NotImplementedError

    def center(self, s, v):
        raise NotImplementedError

    def at(self, s, v):
        p = self.point(s, v)
        ds = self.point(s + self.eps, v) - self.point(s - self.eps, v)
        dv = self.point(s, v + self.eps) - self.point(s, v - self.eps)
        n = np.cross(ds, dv)
        length = np.linalg.norm(n)
        if length < 1e-12:
            n = p - self.center(s, v)
        if np.dot(n, p - self.center(s, v)) < 0.0:
            n = -n
        return p, unit(n)


class Ring(Surface):
    def __init__(self, table, samples=360):
        self.table = np.array(sorted(table), dtype=np.float64)
        self.samples = samples
        self.cache = {}

    def ring(self, z):
        key = round(float(z), 7)
        found = self.cache.get(key)
        if found is not None:
            return found
        a, f, b, n, yc = table_at(self.table, z)
        theta = np.linspace(-math.pi, math.pi, 2 * self.samples + 1)
        sn, cs = np.sin(theta), np.cos(theta)
        e = 2.0 / n
        x = a * np.sign(sn) * np.abs(sn) ** e
        depth = np.where(cs >= 0.0, f, b)
        y = yc - depth * np.sign(cs) * np.abs(cs) ** e
        points = np.stack([x, y, np.full_like(x, z)], axis=1)
        arc = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(points, axis=0), axis=1))])
        half = max(arc[-1] * 0.5, 1e-9)
        sigma = (arc - arc[self.samples]) / half
        if len(self.cache) > 20000:
            self.cache.clear()
        self.cache[key] = (sigma, points, half, yc)
        return self.cache[key]

    def point(self, s, z):
        sigma, points, half, _ = self.ring(z)
        s = (s + 1.0) % 2.0 - 1.0
        return np.array([np.interp(s, sigma, points[:, k]) for k in range(3)])

    def half(self, z):
        return self.ring(z)[2]

    def center(self, s, z):
        return np.array((0.0, self.ring(z)[3], z))


class Tube(Surface):
    def __init__(self, points, up, radius):
        self.points = np.asarray(points, dtype=np.float64)
        self.frames = transport(self.points, np.asarray(up, dtype=np.float64))
        self.length = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(self.points, axis=0), axis=1))])
        self.radius = radius

    def frame(self, l):
        l = float(np.clip(l, 0.0, self.length[-1]))
        k = int(np.clip(np.searchsorted(self.length, l) - 1, 0, len(self.points) - 2))
        t = (l - self.length[k]) / max(self.length[k + 1] - self.length[k], 1e-9)
        center = self.points[k] * (1.0 - t) + self.points[k + 1] * t
        normal = unit(self.frames[k][1] * (1.0 - t) + self.frames[k + 1][1] * t)
        tangent = unit(self.frames[k][0] * (1.0 - t) + self.frames[k + 1][0] * t)
        normal = orth(normal, tangent)
        return center, tangent, normal, np.cross(tangent, normal)

    def point(self, s, l):
        center, _, normal, binormal = self.frame(l)
        theta = s * math.pi
        return center + (normal * math.cos(theta) + binormal * math.sin(theta)) * self.radius(l, theta)

    def center(self, s, l):
        return self.frame(l)[0]

    def half(self, l):
        return math.pi * self.radius(l, 0.0)


class Grid(Surface):
    def __init__(self, rows, values, points, axis):
        self.rows = np.asarray(rows, dtype=np.float64)
        self.values = np.asarray(values, dtype=np.float64)
        self.table = np.asarray(points, dtype=np.float64)
        self.axis = axis

    def point(self, s, v):
        order = np.argsort(self.rows)
        rows = self.rows[order]
        i = float(np.interp(v, rows, np.arange(len(rows))))
        k = int(min(math.floor(i), len(rows) - 2))
        t = i - k
        j = float(np.interp(s, self.values, np.arange(len(self.values))))
        m = int(min(math.floor(j), len(self.values) - 2))
        u = j - m
        a = self.table[order[k]]
        b = self.table[order[k + 1]]
        row_a = a[m] * (1.0 - u) + a[m + 1] * u
        row_b = b[m] * (1.0 - u) + b[m + 1] * u
        return row_a * (1.0 - t) + row_b * t

    def center(self, s, v):
        return np.array((0.0, self.axis, v))


class Piece:
    def __init__(self, name, slot, weights):
        self.name = name
        self.slot = slot
        self.weights = weights
        self.bm = bmesh.new()
        self.points = []
        self.values = []
        self.normals = []

    def vert(self, co, value, param=(0.0, 0.0, 0.0), normal=None):
        entry = dict(value)
        entry['Param'] = (float(param[0]), float(param[1]), float(param[2]) if len(param) > 2 else 0.0)
        co = Vector(tuple(float(c) for c in co))
        self.points.append(co.copy())
        self.values.append(entry)
        self.normals.append(None if normal is None else Vector(tuple(float(c) for c in normal)))
        return self.bm.verts.new(co)

    def grid(self, grid, wrap=False):
        rows = len(grid)
        cols = len(grid[0])
        for i in range(rows - 1):
            for j in range(cols - 1 + (1 if wrap else 0)):
                n = (j + 1) % cols
                quad = (grid[i][j], grid[i + 1][j], grid[i + 1][n], grid[i][n])
                if all(v is not None for v in quad) and len(set(quad)) >= 3:
                    try:
                        self.bm.faces.new(tuple(dict.fromkeys(quad)))
                    except ValueError:
                        pass

    def orient(self):
        self.bm.verts.index_update()
        self.bm.normal_update()
        for face in self.bm.faces:
            wanted = Vector()
            for v in face.verts:
                normal = self.normals[v.index]
                if normal is not None:
                    wanted += normal
            if wanted.length > 0.0 and face.normal.dot(wanted) < 0.0:
                face.normal_flip()
        self.bm.normal_update()

    def build(self, target, thickness=0.0, offset=-1.0, bevel=0.0, merge=0.0, smooth=True):
        if merge > 0.0:
            bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts[:], dist=merge)
        self.bm.verts.index_update()
        self.orient()
        obj = kit.mesh_object(self.name, self.bm, target)
        if thickness > 0.0:
            modifier = obj.modifiers.new('Solidify', 'SOLIDIFY')
            modifier.thickness = thickness
            modifier.offset = offset
            modifier.use_rim = True
            modifier.use_even_offset = False
            kit.apply_modifiers(obj)
        if bevel > 0.0:
            kit.bevel(obj, bevel, segments=1, angle=50.0, harden=False)
        paint_nearest(obj, self.points, self.values)
        if smooth:
            obj.data.shade_smooth()
        obj['slot'] = self.slot
        WEIGHTS[obj.name] = self.weights
        return obj


def paint_nearest(obj, points, values):
    tree = kdtree.KDTree(len(points))
    for index, point in enumerate(points):
        tree.insert(point, index)
    tree.balance()
    mesh = obj.data
    count = len(mesh.vertices)
    layers = {name: np.zeros((count, 3)) for name in LAYERS}
    for vertex in mesh.vertices:
        value = values[tree.find(vertex.co)[1]]
        for name in LAYERS:
            layers[name][vertex.index] = value.get(name, (0.0, 0.0, 0.0))
    for name, data in layers.items():
        set_color(obj, name, data)
    return obj


def set_color(obj, name, values):
    mesh = obj.data
    layer = mesh.color_attributes.get(name) or mesh.color_attributes.new(name, 'FLOAT_COLOR', 'POINT')
    data = np.ones((len(mesh.vertices), 4), dtype=np.float32)
    data[:, :values.shape[1]] = values
    layer.data.foreach_set('color', data.ravel())
    return layer


def ensure_layers(obj, value):
    for name in LAYERS:
        if name not in obj.data.color_attributes:
            data = np.tile(np.array(value.get(name, (0.0, 0.0, 0.0)), dtype=np.float64), (len(obj.data.vertices), 1))
            set_color(obj, name, data)


def patch(piece, surface, region, rows, cols, value, offset=0.0, wrap=False):
    grid = []
    for i in range(rows + 1):
        tr = i / rows
        row = []
        count = cols if wrap else cols + 1
        for j in range(count):
            tc = j / cols
            s, v = region(tc, tr)
            p, n = surface.at(s, v)
            entry = value(s, v, tc, tr) if callable(value) else value
            row.append(piece.vert(p + n * offset, entry, (s, v), n))
        grid.append(row)
    piece.grid(grid, wrap=wrap)
    return grid


def sample_path(surface, path, spacing=0.006, offset=0.0):
    dense = []
    for (s0, v0), (s1, v1) in zip(path, path[1:]):
        for k in range(24):
            t = k / 24.0
            dense.append((s0 + (s1 - s0) * t, v0 + (v1 - v0) * t))
    dense.append(path[-1])
    points, normals, params = [], [], []
    for s, v in dense:
        p, n = surface.at(s, v)
        points.append(p + n * offset)
        normals.append(n)
        params.append((s, v))
    points = np.array(points)
    arc = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(points, axis=0), axis=1))])
    count = max(2, int(arc[-1] / spacing) + 1)
    keep = [int(np.searchsorted(arc, arc[-1] * k / (count - 1)).clip(0, len(arc) - 1)) for k in range(count)]
    return points[keep], np.array(normals)[keep], [params[k] for k in keep]


SECTION = ((-1.0, 0.0), (-1.0, 0.7), (-0.72, 1.0), (0.72, 1.0), (1.0, 0.7), (1.0, 0.0), (0.72, -0.25), (-0.72, -0.25))


def bar(piece, points, normals, params, width, height, value, center=0.0, depth=None, closed=False, section=SECTION, taper=None):
    points = np.asarray(points, dtype=np.float64)
    count = len(points)
    if closed and np.linalg.norm(points[0] - points[-1]) < 1e-5:
        points, normals, params = points[:-1], normals[:-1], params[:-1]
        count -= 1
    rings = []
    down = height * 0.25 if depth is None else depth
    for k in range(count):
        a = points[(k - 1) % count] if closed else points[max(k - 1, 0)]
        b = points[(k + 1) % count] if closed else points[min(k + 1, count - 1)]
        tangent = unit(b - a)
        normal = orth(normals[k], tangent)
        across = np.cross(normal, tangent)
        scale = 1.0 if taper is None else taper(k / max(count - 1, 1))
        ring = []
        for cx, cy in section:
            lift = cy * height if cy >= 0.0 else cy / 0.25 * down
            p = points[k] + across * (cx * width * 0.5 * scale) + normal * (center + lift * scale)
            ring.append(piece.vert(p, value, params[k], normal * (1.0 if cy >= 0.0 else -1.0) + across * cx))
        rings.append(ring)
    sides = len(section)
    loops = count if closed else count - 1
    for k in range(loops):
        a, b = rings[k], rings[(k + 1) % count]
        for j in range(sides):
            n = (j + 1) % sides
            piece.bm.faces.new((a[j], b[j], b[n], a[n]))
    if not closed:
        piece.bm.faces.new(tuple(reversed(rings[0])))
        piece.bm.faces.new(tuple(rings[-1]))
    return rings


def edge_path(region, edge, steps=48):
    if edge == 'left':
        return [region(0.0, k / steps) for k in range(steps + 1)]
    if edge == 'right':
        return [region(1.0, k / steps) for k in range(steps + 1)]
    if edge == 'top':
        return [region(k / steps, 0.0) for k in range(steps + 1)]
    return [region(k / steps, 1.0) for k in range(steps + 1)]


def outline(region, steps=48):
    path = edge_path(region, 'left', steps)
    path += edge_path(region, 'bottom', steps)[1:]
    path += list(reversed(edge_path(region, 'right', steps)))[1:]
    path += list(reversed(edge_path(region, 'top', steps)))[1:]
    return path


def gem(piece, center, normal, up, wide, tall, gold, stone, depth=0.012, lift=0.016):
    normal = unit(normal)
    up = orth(up, normal)
    right = np.cross(up, normal)
    center = np.asarray(center, dtype=np.float64)

    def ring(scale, height, value):
        return [piece.vert(center + right * (a * wide * scale) + up * (b * tall * scale) + normal * height, value, (0.0, 0.0), normal)
                for a, b in ((1.0, 0.0), (0.0, 1.0), (-1.0, 0.0), (0.0, -1.0))]

    base = ring(1.0, -depth, gold)
    rim = ring(0.97, 0.004, gold)
    lip = ring(0.72, 0.006, gold)
    seat = ring(0.66, 0.003, stone)
    crown = ring(0.34, lift * 0.85, stone)
    apex = piece.vert(center + normal * lift, stone, (0.0, 0.0), normal)
    bm = piece.bm
    for k in range(4):
        n = (k + 1) % 4
        bm.faces.new((base[k], base[n], rim[n], rim[k]))
        bm.faces.new((rim[k], rim[n], lip[n], lip[k]))
        bm.faces.new((lip[k], lip[n], seat[n], seat[k]))
        bm.faces.new((seat[k], seat[n], crown[n], crown[k]))
        bm.faces.new((crown[k], crown[n], apex))
    bm.faces.new(tuple(reversed(base)))


def star(piece, center, normal, up, size, value, thickness=0.01, inner=0.36, points=4, ratio=1.0):
    normal = unit(normal)
    up = orth(up, normal)
    right = np.cross(up, normal)
    center = np.asarray(center, dtype=np.float64)
    outline_points = []
    for k in range(points * 2):
        angle = math.pi * k / points
        radius = size if k % 2 == 0 else size * inner
        stretch = ratio if k % 2 == 0 and (k // 2) % 2 == 0 else 1.0
        outline_points.append(right * (math.sin(angle) * radius) + up * (math.cos(angle) * radius * stretch))
    top = [piece.vert(center + p * 0.92 + normal * thickness, value, (0.0, 0.0), normal) for p in outline_points]
    mid = [piece.vert(center + p + normal * (thickness * 0.4), value, (0.0, 0.0), normal) for p in outline_points]
    bottom = [piece.vert(center + p * 0.96 - normal * 0.002, value, (0.0, 0.0), -normal) for p in outline_points]
    apex = piece.vert(center + normal * (thickness * 1.9), value, (0.0, 0.0), normal)
    count = len(outline_points)
    bm = piece.bm
    for k in range(count):
        n = (k + 1) % count
        bm.faces.new((top[k], top[n], apex))
        bm.faces.new((mid[k], mid[n], top[n], top[k]))
        bm.faces.new((bottom[k], bottom[n], mid[n], mid[k]))
    bm.faces.new(tuple(reversed(bottom)))
