import bmesh
import math
import numpy as np
from mathutils import Matrix, Vector, kdtree
from mathutils.bvhtree import BVHTree

import assetkit as kit
import propkit as pk

GOLD = (0.5, 0.33, 0.1)
PLATE = (0.6, 0.52, 0.4)
LEATHER = (0.07, 0.042, 0.03)
GLOVE = (0.055, 0.036, 0.028)
CLOTH = (0.036, 0.024, 0.03)
LINING = (0.42, 0.33, 0.22)
SKIN = (0.5, 0.33, 0.245)
GEM = (0.95, 0.42, 0.06)
AXIS = (0.0, 0.03)

SHOULDERS = {'l': (0.222, 0.025, 1.445), 'r': (-0.222, 0.03, 1.45)}
CLAVICLES = {'l': (0.03, 0.0, 1.47), 'r': (-0.045, 0.0, 1.47)}
ARM_POSE = {'l': (12.0, 3.0, 22.0), 'r': (12.0, 3.0, 22.0)}
UPPER, FORE, PALM, FINGERS = 0.285, 0.26, 0.045, 0.065
FINGER_X = (0.026, 0.0085, -0.009, -0.025)
FINGER_RADIUS = (0.0112, 0.0118, 0.0112, 0.0099)
FINGER_LENGTH = (0.092, 0.1, 0.094, 0.075)
GRIP_RADIUS = 0.025
GRIP_CENTER = (0.043, -0.004)
CAPE_PHI = 128.0
CAPE_NARROW = 0.2
CAPE_CLEARANCE = 0.013
CAPE_COLUMNS = (-56.0, -28.0, 0.0, 28.0, 56.0)
CAPE_ROWS = (1.31, 1.12, 0.88, 0.62, 0.36, 0.13)


def unit(v):
    v = np.asarray(v, dtype=np.float64)
    return v / np.linalg.norm(v)


def orth(v, axis):
    v = np.asarray(v, dtype=np.float64)
    return unit(v - axis * float(np.dot(v, axis)))


def smoothstep(low, high, value):
    t = np.clip((np.asarray(value, dtype=np.float64) - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def look(tone, rough, metal=0.0, fabric=0.0, plate=0.0, leather=0.0, glow=0.0, trim=0.0, cape=0.0):
    return {'Albedo': tuple(tone), 'Surface': (rough, metal, fabric), 'Mask': (0.0, trim, cape), 'Kind': (plate, leather, 0.0), 'Glow': (glow, 0.0, 0.0)}


GOLD_LOOK = look(GOLD, 0.28, metal=1.0, trim=1.0)
PLATE_LOOK = look(PLATE, 0.4, plate=1.0)
LEATHER_LOOK = look(LEATHER, 0.5, fabric=0.35, leather=1.0)
GLOVE_LOOK = look(GLOVE, 0.55, fabric=0.3, leather=1.0)
SKIN_LOOK = look(SKIN, 0.55)
GEM_LOOK = look(GEM, 0.1, glow=1.0)
SILVER = (0.66, 0.67, 0.7)
SILVER_LOOK = look(SILVER, 0.24, metal=1.0, trim=1.0)
STEEL_LOOK = look((0.4, 0.41, 0.44), 0.38, metal=1.0, plate=1.0)
BOOT_LOOK = look((0.055, 0.036, 0.026), 0.46, fabric=0.3, leather=1.0)
SOLE_LOOK = look((0.025, 0.02, 0.018), 0.7)
TROUSER_LOOK = look((0.03, 0.022, 0.026), 0.9, fabric=1.0)


class Part:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.points = []
        self.values = []

    def vert(self, co, value):
        co = Vector(tuple(co))
        self.points.append(co.copy())
        self.values.append(value)
        return self.bm.verts.new(co)

    def quad_grid(self, grid, flip=False):
        for i in range(len(grid) - 1):
            for j in range(len(grid[i]) - 1):
                quad = (grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1])
                if all(v is not None for v in quad):
                    self.bm.faces.new(tuple(reversed(quad)) if flip else quad)

    def build(self, target):
        obj = kit.mesh_object(self.name, self.bm, target)
        return obj

    def paint(self, obj, smooth=True):
        paint_nearest(obj, self.points, self.values)
        if smooth:
            obj.data.shade_smooth()
        return obj


def paint_nearest(obj, points, values, override=None):
    tree = kdtree.KDTree(len(points))
    for index, point in enumerate(points):
        tree.insert(point, index)
    tree.balance()
    mesh = obj.data
    count = len(mesh.vertices)
    layers = {name: np.zeros((count, 3)) for name in ('Albedo', 'Surface', 'Mask', 'Kind', 'Glow')}
    for vertex in mesh.vertices:
        value = values[tree.find(vertex.co)[1]]
        if override:
            value = override(vertex, value)
        for name in layers:
            layers[name][vertex.index] = value[name]
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


def solidify(obj, thickness, offset=-1.0, rim=True):
    modifier = obj.modifiers.new('Solidify', 'SOLIDIFY')
    modifier.thickness = thickness
    modifier.offset = offset
    modifier.use_rim = rim
    modifier.use_even_offset = False
    kit.apply_modifiers(obj)
    return obj


def orient_outward(bm, center_of):
    bm.normal_update()
    score = 0.0
    for face in bm.faces:
        c = face.calc_center_median()
        score += (c - center_of(c)).dot(face.normal)
    if score < 0.0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    bm.normal_update()


class Arm:
    def __init__(self, side):
        self.side = side
        self.s = 1.0 if side == 'l' else -1.0
        abduct, forward, flex = (math.radians(a) for a in ARM_POSE[side])
        s = self.s
        self.clavicle = np.array(CLAVICLES[side])
        self.shoulder = np.array(SHOULDERS[side])
        self.d1 = unit((s * math.sin(abduct), -math.sin(forward), -math.cos(abduct) * math.cos(forward)))
        ahead = orth((0.0, -1.0, 0.0), self.d1)
        self.d2 = unit(self.d1 * math.cos(flex) + ahead * math.sin(flex))
        ahead = orth((0.0, -1.0, 0.0), self.d2)
        self.d3 = unit(self.d2 * math.cos(0.3 * flex) + ahead * math.sin(0.3 * flex))
        self.elbow = self.shoulder + self.d1 * UPPER
        self.wrist = self.elbow + self.d2 * FORE
        self.palm = self.wrist + self.d3 * PALM
        self.tip = self.palm + self.d3 * FINGERS
        self.palmar = orth((-s, 0.0, 0.0), self.d3)
        radial = np.array((0.0, -1.0, 0.0))
        radial = radial - self.d3 * radial.dot(self.d3) - self.palmar * radial.dot(self.palmar)
        self.radial = unit(radial)
        self.frame = np.stack([self.radial, self.palmar, self.d3], axis=1)
        self.mirrored = np.linalg.det(self.frame) < 0.0
        self.dorsal = (s, 0.0, 0.0)

    def hand(self, local):
        return self.wrist + self.frame @ np.asarray(local, dtype=np.float64)

    def knuckle_z(self, x):
        return 0.093 - 0.2 * x

    def grip(self):
        center = self.hand((0.0, GRIP_CENTER[0], self.knuckle_z(0.0) + GRIP_CENTER[1]))
        axis = unit(self.frame @ unit((1.0, 0.0, -0.2)))
        side = orth((self.s, 0.0, 0.0), axis)
        x = np.cross(axis, side)
        m = Matrix.Identity(4)
        for row in range(3):
            m[row][0], m[row][1], m[row][2], m[row][3] = x[row], axis[row], side[row], center[row]
        return m

    def centerline(self, count):
        start = self.shoulder - self.d1 * 0.05
        corner = 0.05
        a = self.elbow - self.d1 * corner
        b = self.elbow + self.d2 * corner
        points = []
        lengths = [np.linalg.norm(a - start), 0.08, np.linalg.norm(self.wrist + self.d2 * 0.01 - b)]
        total = sum(lengths)
        for k in range(count):
            d = total * k / (count - 1)
            if d <= lengths[0]:
                points.append(start + (a - start) * (d / lengths[0]))
            elif d <= lengths[0] + lengths[1]:
                t = (d - lengths[0]) / lengths[1]
                points.append((1 - t) ** 2 * a + 2 * (1 - t) * t * self.elbow + t * t * b)
            else:
                t = (d - lengths[0] - lengths[1]) / lengths[2]
                points.append(b + (self.wrist + self.d2 * 0.01 - b) * t)
        return np.array(points), total


def transport(points, up):
    frames = []
    tangent = unit(points[1] - points[0])
    normal = orth(up, tangent)
    for k in range(len(points)):
        if k + 1 < len(points):
            new = unit(points[k + 1] - points[k])
        axis = np.cross(tangent, new)
        if np.linalg.norm(axis) > 1e-9:
            angle = math.atan2(np.linalg.norm(axis), float(np.dot(tangent, new)))
            normal = np.array(Matrix.Rotation(angle, 3, Vector(unit(axis))) @ Vector(normal))
        tangent = new
        normal = orth(normal, tangent)
        frames.append((tangent, normal, np.cross(tangent, normal)))
    return frames


def sleeve(arm, target, rings, sides, folds):
    points, total = arm.centerline(rings)
    frames = transport(points, np.array(arm.dorsal))
    part = Part(f'Sleeve_{arm.side}')
    cloth = look(CLOTH, 0.88, fabric=1.0)
    grid = []
    rng = np.random.default_rng(7 if arm.side == 'l' else 11)
    phases = rng.uniform(0.0, 2.0 * math.pi, 6)
    elbow_t = (np.linalg.norm(arm.elbow - points[0]) + 0.0) / total
    for k, (center, (tangent, normal, binormal)) in enumerate(zip(points, frames)):
        t = k / (rings - 1)
        d = t * total
        base = np.interp(d, (0.0, 0.05, 0.16, UPPER + 0.03, UPPER + 0.12, total), (0.081, 0.076, 0.068, 0.058, 0.054, 0.043))
        row = []
        for j in range(sides):
            theta = 2.0 * math.pi * j / sides
            r = base
            if folds:
                env = smoothstep(0.03, 0.1, d) * (1.0 - smoothstep(total - 0.12, total - 0.06, d))
                r += env * (0.0032 * math.sin(2.0 * theta + 13.0 * t + phases[0]) + 0.0022 * math.sin(3.0 * theta - 23.0 * t + phases[1]) + 0.0016 * math.sin(5.0 * theta + 31.0 * t + phases[2]))
                inner = max(0.0, math.cos(theta - math.pi * 0.5)) ** 3
                bunch = math.exp(-((t - elbow_t) / 0.06) ** 2)
                r += 0.006 * bunch * inner * math.sin(70.0 * t + phases[3])
            offset = normal * math.cos(theta) * r + binormal * math.sin(theta) * r * 0.94
            row.append(part.vert(center + offset, cloth))
        row.append(row[0])
        grid.append(row)
    part.quad_grid(grid)
    orient_outward(part.bm, lambda c: Vector(points[int(np.argmin(np.linalg.norm(points - np.array(c), axis=1)))]))
    obj = part.build(target)
    return part.paint(obj)


def bracer(arm, target):
    rows, sides = 12, 26
    start = arm.elbow + arm.d2 * 0.035
    end = arm.wrist - arm.d2 * 0.004
    frames = transport(np.array([start, end]), np.array(arm.dorsal))
    tangent, normal, binormal = frames[0]
    part = Part(f'Bracer_{arm.side}')
    grid = []
    length = np.linalg.norm(end - start)
    for i in range(rows + 1):
        u = i / rows
        row = []
        for j in range(sides):
            theta = 2.0 * math.pi * j / sides
            point = max(0.0, math.cos(theta)) ** 5
            extend = 0.04 * point
            along = -extend + (length + extend) * u
            r = np.interp(along / length, (-0.6, 0.0, 0.35, 1.0), (0.066, 0.061, 0.055, 0.046)) + 0.003 * math.sin(math.pi * u)
            center = start + tangent * along
            p = center + normal * math.cos(theta) * r + binormal * math.sin(theta) * r * 0.92
            edge = min(u * (length + extend), (1.0 - u) * (length + extend))
            gold = edge < 0.011
            groove = abs(edge - 0.02) < 0.0025
            row.append(part.vert(p, GOLD_LOOK if gold or groove else LEATHER_LOOK))
        row.append(row[0])
        grid.append(row)
    part.quad_grid(grid)
    orient_outward(part.bm, lambda c: Vector(start + tangent * float(np.dot(np.array(c) - start, tangent))))
    obj = part.build(target)
    part.paint(obj)
    solidify(obj, 0.006, offset=-1.0)
    kit.bevel(obj, 0.0015, segments=1, angle=50.0, harden=False)
    paint_nearest(obj, part.points, part.values)
    obj.data.shade_smooth()
    return obj


def finger_path(knuckle, length, flex, x):
    points = [np.array(knuckle)]
    direction = np.array((0.0, 0.0, 1.0))
    pieces = (0.46, 0.3, 0.24)
    angle = 0.0
    for piece, bend in zip(pieces, flex):
        angle += bend
        direction = np.array((0.0, math.sin(angle), math.cos(angle)))
        steps = 3
        for k in range(steps):
            points.append(points[-1] + direction * (length * piece / steps))
    return points


def wrap_path(knuckle, length, x, arm):
    center = np.array((x, GRIP_CENTER[0], arm.knuckle_z(x) + GRIP_CENTER[1]))
    start = np.radians(35.0)
    entry = center + GRIP_RADIUS * np.array((0.0, -math.cos(start), math.sin(start)))
    points = [np.array(knuckle), entry]
    left = length - np.linalg.norm(entry - points[0])
    sweep = left / GRIP_RADIUS
    for k in range(1, 9):
        angle = start + sweep * k / 8
        points.append(center + GRIP_RADIUS * np.array((0.0, -math.cos(angle), math.sin(angle))))
    return points


def fist(arm, target, curl):
    shapes, tags = [], []
    hand = arm.hand

    def capsule(a, b, ra, rb, label):
        shapes.append(pk.capsule_bm(Vector(hand(a)), Vector(hand(b)), ra, rb, segments=12, rings=4))
        span = label if isinstance(label, tuple) else (label, label)
        tags.append((np.array(hand(a)), np.array(hand(b)), span))

    proper = arm.frame.copy()
    if arm.mirrored:
        proper[:, 0] = -proper[:, 0]
    shapes.append(pk.ellipsoid_bm(Vector(hand((0.0, 0.007, 0.052))), (0.043, 0.019, 0.05), rotation=Matrix(proper.tolist()).to_quaternion(), segments=24, rings=14))
    tags.append((hand((0.0, 0.007, 0.03)), hand((0.0, 0.007, 0.075)), (0.0, 0.0)))
    capsule((0.0, 0.004, -0.02), (0.0, 0.006, 0.03), 0.03, 0.033, 0.0)
    flex = (math.radians(24.0), math.radians(32.0), math.radians(18.0))
    for x, radius, length in zip(FINGER_X, FINGER_RADIUS, FINGER_LENGTH):
        knuckle = (x, 0.003, arm.knuckle_z(x))
        path = wrap_path(knuckle, length, x, arm) if curl > 0.5 else finger_path(knuckle, length, flex, x)
        total = 0.0
        for k, (a, b) in enumerate(zip(path, path[1:])):
            seg = np.linalg.norm(b - a)
            t0, t1 = total / length, (total + seg) / length
            total += seg
            capsule(a, b, radius * (1.0 - 0.18 * t0), radius * (1.0 - 0.18 * t1), (t0, t1))
        capsule((x * 0.8, 0.004, 0.05), knuckle, radius * 1.15, radius * 1.1, 0.0)
    if curl > 0.5:
        thumb = ((0.03, 0.008, 0.016), (0.052, 0.03, 0.04), (0.06, 0.05, 0.06), (0.06, 0.064, 0.078))
    else:
        thumb = ((0.03, 0.008, 0.016), (0.048, 0.024, 0.045), (0.046, 0.033, 0.074), (0.04, 0.036, 0.098))
    radii = (0.0165, 0.0145, 0.0128, 0.0115)
    for k in range(3):
        capsule(thumb[k], thumb[k + 1], radii[k], radii[k + 1], (0.2 + 0.3 * k, 0.5 + 0.3 * k))
    obj = pk.sculpt(shapes, target, None, f'Fist_{arm.side}', voxel=0.0028, smooth=4, smooth_factor=0.5, faces=2800)
    for name in list(obj.keys()):
        del obj[name]
    co = np.array([v.co[:] for v in obj.data.vertices])
    best = np.full(len(co), 9.0)
    label = np.zeros(len(co))
    for a, b, value in tags:
        ab = b - a
        t = np.clip(((co - a) @ ab) / max(ab @ ab, 1e-12), 0.0, 1.0)
        d = np.linalg.norm(co - (a + t[:, None] * ab), axis=1)
        closer = d < best
        best[closer] = d[closer]
        label[closer] = (value[0] + (value[1] - value[0]) * t)[closer]
    bare = smoothstep(0.4, 0.46, label)[:, None]
    for name in GLOVE_LOOK:
        mixed = np.array(GLOVE_LOOK[name])[None, :] * (1.0 - bare) + np.array(SKIN_LOOK[name])[None, :] * bare
        set_color(obj, name, mixed)
    obj.data.shade_smooth()
    return obj


def hand_plate(arm, fist_obj, target):
    bvh = BVHTree.FromObject(fist_obj, bpy_depsgraph())
    part = Part(f'HandPlate_{arm.side}')
    rows, cols = 16, 16
    grid = []
    for i in range(rows):
        z = 0.03 + (arm.knuckle_z(0.0) - 0.024 - 0.03) * i / (rows - 1)
        row = []
        for j in range(cols):
            x = -0.024 + 0.052 * j / (cols - 1)
            origin = arm.hand((x, -0.08, z))
            hit = bvh.ray_cast(Vector(origin), Vector(arm.palmar), 0.2)
            if hit[0] is None:
                row.append(None)
                continue
            p = np.array(hit[0]) - arm.palmar * 0.0025
            edge = min(i, rows - 1 - i, j, cols - 1 - j)
            row.append(part.vert(p, GOLD_LOOK if edge == 0 else look((0.05, 0.034, 0.026), 0.4, leather=1.0)))
        grid.append(row)
    part.quad_grid(grid)
    core = Vector(arm.hand((0.0, 0.02, 0.05)))
    orient_outward(part.bm, lambda c: core)
    obj = part.build(target)
    part.paint(obj)
    solidify(obj, 0.0022, offset=1.0)
    kit.bevel(obj, 0.0008, segments=1, angle=50.0, harden=False)
    paint_nearest(obj, part.points, part.values)
    obj.data.shade_smooth()
    return obj


def bpy_depsgraph():
    import bpy
    return bpy.context.evaluated_depsgraph_get()


def brooch(name, center, normal, up, wide, tall, target, depth=0.03, glow=1.0):
    normal = unit(normal)
    up = orth(up, normal)
    right = np.cross(up, normal)
    center = np.asarray(center, dtype=np.float64)
    part = Part(name)
    gem = look(GEM, 0.1, glow=glow)

    def ring(scale, height, value):
        return [part.vert(center + right * (a * wide * scale) + up * (b * tall * scale) + normal * height, value) for a, b in ((1.0, 0.0), (0.0, 1.0), (-1.0, 0.0), (0.0, -1.0))]

    base = ring(1.0, -depth, GOLD_LOOK)
    rim = ring(0.96, 0.006, GOLD_LOOK)
    lip = ring(0.66, 0.008, GOLD_LOOK)
    seat = ring(0.6, 0.004, gem)
    crown = ring(0.32, 0.018, gem)
    apex = part.vert(center + normal * 0.024, gem)
    bm = part.bm
    for k in range(4):
        n = (k + 1) % 4
        bm.faces.new((base[k], base[n], rim[n], rim[k]))
        bm.faces.new((rim[k], rim[n], lip[n], lip[k]))
        bm.faces.new((lip[k], lip[n], seat[n], seat[k]))
        bm.faces.new((seat[k], seat[n], crown[n], crown[k]))
        bm.faces.new((crown[k], crown[n], apex))
    bm.faces.new(tuple(reversed(base)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = part.build(target)
    part.paint(obj, smooth=False)
    return obj


def pauldron(arm, target, bvh):
    s = arm.s
    lateral = unit((s * math.cos(math.radians(10.0)), 0.0, -math.sin(math.radians(10.0))))
    up = orth((0.0, 0.0, 1.0), lateral)
    back = np.cross(lateral, up) * s
    origin = arm.shoulder - up * 0.004 - lateral * 0.012

    def outline(v):
        return 0.055 + 0.105 * (1.0 - abs(v)) ** 1.6 + 0.012 * (1.0 - v * v)

    def place(ell, v, extra):
        alpha = math.radians(98.0) * v
        out = max(ell, 0.0) / 0.2
        rho = 0.1 + 0.016 * out + 0.004 * math.cos(v * math.pi * 0.5) + extra
        lift = 1.0 - 0.45 * out
        p = origin + lateral * ell + up * (rho * math.cos(alpha) * lift) + back * (rho * math.sin(alpha) * (1.0 + 0.15 * out))
        p = p - up * (0.035 * out ** 2) + up * (0.01 * (1.0 - abs(v)) * out)
        return p + up * (0.008 * max(0.0, 1.0 - abs(v) / 0.14) * min(max((ell + 0.06) / 0.2, 0.0), 1.0))

    def shell(name, rows, cols, span, start, reach_scale, extra, thickness, rim, inset_at):
        part = Part(name)
        grid = []
        for i in range(rows + 1):
            u = i / rows
            row = []
            for j in range(cols + 1):
                v = span * (-1.0 + 2.0 * j / cols)
                reach = outline(v) * reach_scale
                ell = start + (reach - start) * u
                edge = min((1.0 - u) * (reach - start), (span - abs(v)) * 0.17, u * (reach - start))
                gold = edge < rim or (inset_at > 0.0 and abs(edge - inset_at) < 0.0028)
                row.append(part.vert(place(ell, v, extra), SILVER_LOOK if gold else STEEL_LOOK))
            grid.append(row)
        part.quad_grid(grid)
        orient_outward(part.bm, lambda c: Vector(origin + lateral * float(np.dot(np.array(c) - origin, lateral))))
        obj = part.build(target)
        part.paint(obj)
        solidify(obj, thickness, offset=-1.0)
        kit.bevel(obj, 0.0018, segments=1, angle=50.0, harden=False)
        paint_nearest(obj, part.points, part.values)
        obj.data.shade_smooth()
        return obj

    return shell(f'Pauldron_{arm.side}', 15, 30, 1.0, -0.06, 1.0, 0.0, 0.011, 0.01, 0.026)


def cape_hit(bvh, phi, z):
    direction = np.array((math.sin(phi), math.cos(phi), 0.0))
    center = np.array((AXIS[0], AXIS[1], z))
    origin = center + direction * 1.2
    hit = bvh.ray_cast(Vector(origin), Vector(-direction), 1.3)
    if hit[0] is None:
        return 0.08
    return max(1.2 - hit[3], 0.08)


def cape_end(phi):
    a = abs(phi)
    hem = 0.15 + 0.05 * abs(((a / 23.0) % 2.0) - 1.0)
    flap = 1.335 - 0.07 * max(0.0, 1.0 - abs(a - 90.0) / 11.0) - 0.045 * max(0.0, 1.0 - abs(a - 106.0) / 8.0)
    front = float(smoothstep(112.0, 124.0, a))
    flap = flap * (1.0 - front) + (1.365 - 0.055 * float(smoothstep(128.0, 176.0, a))) * front
    w = float(smoothstep(60.0, 72.0, a))
    return hem * (1.0 - w) + flap * w


def cape_top(phi):
    a = abs(phi)
    return 1.508 - 0.04 * float(smoothstep(20.0, 85.0, a)) + 0.03 * float(smoothstep(100.0, 140.0, a)) - 0.075 * float(smoothstep(140.0, 176.0, a))


def narrow(z):
    return 1.0 - CAPE_NARROW * float(smoothstep(1.2, 0.3, z))


def cape_surface(bvh, cols, rows):
    phis = np.radians(np.linspace(-CAPE_PHI, CAPE_PHI, cols))
    ts = np.linspace(0.0, 1.0, rows) ** 1.15
    radius = np.zeros((rows, cols))
    heights = np.zeros((rows, cols))
    floor = np.zeros((rows, cols))
    for j, phi in enumerate(phis):
        deg = math.degrees(phi)
        top, end = cape_top(deg), cape_end(deg)
        previous = None
        for i, t in enumerate(ts):
            z = top - (top - end) * t
            hit = cape_hit(bvh, phi * narrow(z), z) + 0.045 * float(smoothstep(1.3, 0.95, z))
            r = hit + 0.024
            floor[i, j] = hit + 0.012
            if previous is not None:
                drop = previous[1] - z
                r = max(r, previous[0] - 0.1 * drop)
                if z < 1.22:
                    r = min(r, previous[0] + 0.3 * drop)
                    floor[i, j] = min(floor[i, j], r - 0.012)
            radius[i, j] = r
            heights[i, j] = z
            previous = (r, z)
    radius += 0.035 * smoothstep(1.0, 0.15, heights)
    floor += 0.035 * smoothstep(1.0, 0.15, heights)
    for _ in range(40):
        padded = np.pad(radius, 1, mode='edge')
        relaxed = (padded[:-2, 1:-1] + padded[2:, 1:-1] + padded[1:-1, :-2] + padded[1:-1, 2:] + 2.0 * padded[1:-1, 1:-1]) / 6.0
        radius = np.maximum(relaxed, floor)
    return phis, ts, radius, heights


def cape_point(phi, r, z):
    phi = phi * narrow(z)
    return np.array((AXIS[0] + r * math.sin(phi), AXIS[1] + r * math.cos(phi), z))


FIELD = {}


def cape_field(bvh, cols, rows):
    key = id(bvh)
    if key not in FIELD:
        FIELD[key] = cape_surface(bvh, 181, 81)
    phis, ts, radius, heights = FIELD[key]
    new_phis = np.radians(np.linspace(-CAPE_PHI, CAPE_PHI, cols))
    new_ts = np.linspace(0.0, 1.0, rows) ** 1.15
    across = np.array([np.interp(new_phis, phis, radius[i]) for i in range(len(ts))])
    sampled = np.array([np.interp(new_ts, ts, across[:, j]) for j in range(cols)]).T
    tall = np.array([[cape_top(math.degrees(p)) - (cape_top(math.degrees(p)) - cape_end(math.degrees(p))) * t for p in new_phis] for t in new_ts])
    return new_phis, new_ts, sampled, tall


def cape(bvh, target, cols, rows, folds):
    phis, ts, radius, heights = cape_field(bvh, cols, rows)
    part = Part('Cape' if folds else 'CapeLow')
    cloth = look(CLOTH, 0.86, fabric=1.0, cape=1.0)
    grid = []
    rng = np.random.default_rng(5)
    waves = rng.uniform(0.0, 2.0 * math.pi, 4)
    for i in range(rows):
        row = []
        for j in range(cols):
            phi, z, r = phis[j], heights[i, j], radius[i, j]
            hang = float(smoothstep(1.25, 0.6, z))
            if folds:
                r += hang * (0.012 * math.sin(phi * 9.0 + waves[0]) + 0.006 * math.sin(phi * 17.0 + z * 3.0 + waves[1]))
            p = cape_point(phi, r, z)
            if z < 1.44:
                for _ in range(3):
                    near, _, _, gap = bvh.find_nearest(Vector(p))
                    if near is None or gap >= CAPE_CLEARANCE:
                        break
                    p = p + np.array((math.sin(phi * narrow(z)), math.cos(phi * narrow(z)), 0.0)) * (CAPE_CLEARANCE - gap + 0.001)
            deg = abs(math.degrees(phi))
            from_end = abs(z - cape_end(math.degrees(phi)))
            gold = from_end < 0.022 or CAPE_PHI - deg < 2.2 or abs(z - cape_top(math.degrees(phi))) < 0.004
            row.append(part.vert(p, GOLD_LOOK if gold else cloth))
        grid.append(row)
    part.quad_grid(grid, flip=True)
    orient_outward(part.bm, lambda c: Vector((AXIS[0], AXIS[1], c.z)))
    layer = part.bm.loops.layers.uv.new('UVMap')
    where = {v: (j / (cols - 1), i / (rows - 1)) for i, row in enumerate(grid) for j, v in enumerate(row)}
    for face in part.bm.faces:
        for loop in face.loops:
            u, v = where[loop.vert]
            loop[layer].uv = (0.004 + 0.992 * u, 0.502 + 0.494 * (1.0 - v))
    obj = part.build(target)
    part.paint(obj)
    count = len(obj.data.polygons)
    solidify(obj, 0.007, offset=-1.0)
    uv = obj.data.uv_layers['UVMap'].data
    for poly in obj.data.polygons[count:2 * count]:
        for index in poly.loop_indices:
            u, v = uv[index].uv
            uv[index].uv = (u, v - 0.5)
    lining = look(LINING, 0.8, fabric=0.8)

    def inner(vertex, value):
        radial = Vector((vertex.co.x - AXIS[0], vertex.co.y - AXIS[1], 0.0))
        if radial.length > 1e-6 and vertex.normal.dot(radial.normalized()) < -0.3 and value is not GOLD_LOOK:
            return lining
        return value

    paint_nearest(obj, part.points, part.values, override=inner)
    obj.data.shade_smooth()
    return obj


def cape_joints(bvh):
    phis, ts, radius, heights = cape_field(bvh, 93, 60)
    joints = []
    for k, deg in enumerate(CAPE_COLUMNS):
        j = int(np.argmin(np.abs(np.degrees(phis) - deg)))
        chain = []
        for z in CAPE_ROWS:
            r = float(np.interp(-z, -heights[:, j], radius[:, j]))
            chain.append(cape_point(phis[j], r - 0.012, z))
        joints.append((chr(ord('a') + k), chain))
    return joints


def cape_weights(co):
    x, y, z = co
    phi = math.degrees(math.atan2(x - AXIS[0], y - AXIS[1])) / narrow(z)
    a = abs(phi)
    side = 'l' if phi > 0.0 else 'r'
    if a > 66.0 or z > CAPE_ROWS[0] + 0.02:
        shoulder = float(smoothstep(40.0, 100.0, a)) * 0.5 * (1.0 - float(smoothstep(112.0, 140.0, a)))
        arm = 0.0
        weights = {'chest': 1.0 - shoulder}
        weights[f'shoulder.{side}'] = shoulder - arm
        if arm > 0.0:
            weights[f'upperarm.{side}'] = arm
        hang = float(smoothstep(72.0, 62.0, a)) * float(smoothstep(CAPE_ROWS[0] + 0.02, CAPE_ROWS[0] - 0.06, z))
        if hang <= 0.0:
            return weights
        rigid = weights
    else:
        rigid, hang = {'chest': 1.0}, 1.0
    columns = CAPE_COLUMNS
    across = {}
    if phi <= columns[0]:
        across[0] = 1.0
    elif phi >= columns[-1]:
        across[len(columns) - 1] = 1.0
    else:
        for k in range(len(columns) - 1):
            if columns[k] <= phi <= columns[k + 1]:
                t = (phi - columns[k]) / (columns[k + 1] - columns[k])
                across[k] = 1.0 - t
                across[k + 1] = t
                break
    centers = [0.5 * (a0 + a1) for a0, a1 in zip(CAPE_ROWS, CAPE_ROWS[1:])]
    along = {}
    if z >= centers[0]:
        along[1] = 1.0
    elif z <= centers[-1]:
        along[len(centers)] = 1.0
    else:
        for k in range(len(centers) - 1):
            if centers[k] >= z >= centers[k + 1]:
                t = (centers[k] - z) / (centers[k] - centers[k + 1])
                along[k + 1] = 1.0 - t
                along[k + 2] = t
                break
    attach = float(smoothstep(CAPE_ROWS[0] - 0.08, CAPE_ROWS[0] + 0.01, z))
    weights = {name: w * (attach + (1.0 - attach) * (1.0 - hang)) for name, w in rigid.items()}
    for k, a_w in across.items():
        for row, b_w in along.items():
            name = f'cape.{chr(ord("a") + k)}.{row}'
            weights[name] = weights.get(name, 0.0) + a_w * b_w * (1.0 - attach) * hang
    return weights


def leg_frame(joints):
    hip, knee, ankle, ball, toe = (np.array(p, dtype=np.float64) for p in joints)
    forward = ball - ankle
    forward[2] = 0.0
    return hip, knee, ankle, ball, toe, unit(forward)


def trousers(side, joints, target):
    hip, knee, ankle, ball, toe, forward = leg_frame(joints)
    end = ankle + (knee - ankle) * 0.45
    count = 28
    points = []
    for k in range(count):
        t = k / (count - 1)
        if t < 0.5:
            points.append(hip + (knee - hip) * (t / 0.5))
        else:
            points.append(knee + (end - knee) * ((t - 0.5) / 0.5))
    points = np.array(points)
    frames = transport(points, forward)
    part = Part(f'Trousers_{side}')
    grid = []
    sides = 18
    for k, (center, (tangent, normal, binormal)) in enumerate(zip(points, frames)):
        t = k / (count - 1)
        base = float(np.interp(t, (0.0, 0.3, 0.5, 1.0), (0.095, 0.082, 0.07, 0.06)))
        row = []
        for j in range(sides):
            theta = 2.0 * math.pi * j / sides
            r = base * (1.0 + 0.05 * math.sin(3.0 * theta + 9.0 * t) + 0.03 * math.sin(5.0 * theta - 13.0 * t))
            row.append(part.vert(center + normal * math.cos(theta) * r + binormal * math.sin(theta) * r, TROUSER_LOOK))
        row.append(row[0])
        grid.append(row)
    part.quad_grid(grid)
    orient_outward(part.bm, lambda c: Vector(points[int(np.argmin(np.linalg.norm(points - np.array(c), axis=1)))]))
    return part.paint(part.build(target))


def boot(side, joints, target):
    hip, knee, ankle, ball, toe, forward = leg_frame(joints)
    shin = unit(knee - ankle)
    top = ankle + shin * (np.linalg.norm(knee - ankle) - 0.035)
    heel = ankle - forward * 0.045
    heel[2] = 0.04
    instep = ankle + forward * 0.03
    instep[2] = 0.06
    ball_point = ball.copy()
    ball_point[2] = 0.04
    tip = toe + forward * 0.03
    tip[2] = 0.03
    shapes = [
        pk.capsule_bm(Vector(top), Vector(ankle + shin * 0.06), 0.069, 0.058, segments=20, rings=6),
        pk.capsule_bm(Vector(top - shin * 0.015), Vector(top + shin * 0.01), 0.075, 0.075, segments=24, rings=4),
        pk.capsule_bm(Vector(ankle + shin * 0.06), Vector(heel), 0.056, 0.045, segments=16, rings=5),
        pk.capsule_bm(Vector(heel), Vector(instep), 0.045, 0.05, segments=16, rings=5),
        pk.capsule_bm(Vector(instep), Vector(ball_point), 0.05, 0.044, segments=16, rings=5),
        pk.capsule_bm(Vector(ball_point), Vector(tip), 0.044, 0.026, segments=16, rings=5),
    ]
    floor = pk.box((1.0, 1.0, 0.2), center=(float(ankle[0]), float(ankle[1]), -0.1))
    obj = pk.sculpt(shapes, target, None, f'Boot_{side}', voxel=0.004, smooth=4, smooth_factor=0.5, faces=1700, carve=[floor])
    co = np.array([v.co[:] for v in obj.data.vertices])
    along = (co - ankle) @ shin
    reach = np.linalg.norm(knee - ankle) - 0.035
    cuff = smoothstep(reach - 0.014, reach - 0.008, along)
    sole = smoothstep(0.016, 0.01, co[:, 2])
    for name in BOOT_LOOK:
        value = np.array(BOOT_LOOK[name])[None, :] * (1.0 - cuff[:, None]) + np.array(SILVER_LOOK[name])[None, :] * cuff[:, None]
        value = value * (1.0 - sole[:, None]) + np.array(SOLE_LOOK[name])[None, :] * sole[:, None]
        set_color(obj, name, value)
    obj.data.shade_smooth()
    return obj


def shin_plate(side, joints, boot_obj, target):
    hip, knee, ankle, ball, toe, forward = leg_frame(joints)
    shin = unit(knee - ankle)
    across = unit(np.cross(shin, forward))
    front = unit(np.cross(across, shin))
    bvh = BVHTree.FromObject(boot_obj, bpy_depsgraph())
    part = Part(f'ShinPlate_{side}')
    rows, cols = 18, 12
    length = np.linalg.norm(knee - ankle)
    grid = []
    for i in range(rows):
        u = i / (rows - 1)
        along = 0.1 + (length - 0.17) * u
        half = 0.03 * (1.0 - 0.92 * smoothstep(0.6, 1.0, u)) * (0.55 + 0.45 * smoothstep(0.0, 0.25, u))
        row = []
        for j in range(cols):
            v = -1.0 + 2.0 * j / (cols - 1)
            origin = ankle + shin * along + across * (half * v) + front * 0.2
            hit = bvh.ray_cast(Vector(origin), Vector(-front), 0.4)
            if hit[0] is None:
                row.append(None)
                continue
            edge = min(i, rows - 1 - i, j, cols - 1 - j)
            ridge = 0.007 * (1.0 - abs(v)) ** 1.5
            row.append(part.vert(np.array(hit[0]) + front * (0.003 + ridge), SILVER_LOOK if edge == 0 else STEEL_LOOK))
        grid.append(row)
    part.quad_grid(grid)
    core = Vector(ankle + shin * (length * 0.5))
    orient_outward(part.bm, lambda c: Vector(np.array(core) + shin * float(np.dot(np.array(c) - np.array(core), shin))))
    obj = part.build(target)
    part.paint(obj)
    solidify(obj, 0.003, offset=1.0)
    kit.bevel(obj, 0.0008, segments=1, angle=50.0, harden=False)
    paint_nearest(obj, part.points, part.values)
    obj.data.shade_smooth()
    return obj


CHEST_SPAN = 64.0
CHEST_BOTTOM = 1.135
CHEST_V = (1.3, 1.475)
TABARD_LOOK = look((0.5, 0.42, 0.3), 0.8, fabric=0.8)


def chest_top(s):
    return CHEST_V[0] + (CHEST_V[1] - CHEST_V[0]) * float(smoothstep(0.0, 0.42, abs(s))) ** 0.8


def chest_panel(bvh, target, cols=42, rows=24):
    spans = np.linspace(-1.0, 1.0, cols)
    ts = np.linspace(0.0, 1.0, rows)
    radius = np.zeros((rows, cols))
    floor = np.zeros((rows, cols))
    heights = np.zeros((rows, cols))
    for j, s in enumerate(spans):
        phi = math.pi + math.radians(CHEST_SPAN) * s
        top = chest_top(s)
        for i, t in enumerate(ts):
            z = top - (top - CHEST_BOTTOM) * t
            hit = cape_hit(bvh, phi, z)
            floor[i, j] = hit + 0.009
            radius[i, j] = hit + 0.016
            heights[i, j] = z
    for _ in range(30):
        padded = np.pad(radius, 1, mode='edge')
        relaxed = (padded[:-2, 1:-1] + padded[2:, 1:-1] + padded[1:-1, :-2] + padded[1:-1, 2:] + 2.0 * padded[1:-1, 1:-1]) / 6.0
        radius = np.maximum(relaxed, floor)
    part = Part('ChestPanel')
    cloth = look(CLOTH, 0.86, fabric=1.0)
    grid = []
    for i, t in enumerate(ts):
        row = []
        for j, s in enumerate(spans):
            phi = math.pi + math.radians(CHEST_SPAN) * s
            z = heights[i, j]
            p = np.array((AXIS[0] + radius[i, j] * math.sin(phi), AXIS[1] + radius[i, j] * math.cos(phi), z))
            lapel = abs(z - chest_top(s)) < 0.013 and abs(s) < 0.97
            stripe = abs(s) < 0.17 and z < CHEST_V[0] + 0.01
            edge = abs(abs(s) - 0.17) < 0.022 and z < CHEST_V[0] + 0.01
            row.append(part.vert(p, GOLD_LOOK if lapel or edge else TABARD_LOOK if stripe else cloth))
        grid.append(row)
    part.quad_grid(grid, flip=True)
    orient_outward(part.bm, lambda c: Vector((AXIS[0], AXIS[1], c.z)))
    obj = part.build(target)
    part.paint(obj)
    solidify(obj, 0.005, offset=-1.0)
    paint_nearest(obj, part.points, part.values)
    obj.data.shade_smooth()
    return obj
