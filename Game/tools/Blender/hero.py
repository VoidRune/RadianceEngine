import bpy
import bmesh
import heapq
import math
import os
import random
import sys
from mathutils import Matrix, Vector, interpolate
from mathutils.bvhtree import BVHTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import heroanim

NAME = 'Hero'
SPIRIT = (0.45, 0.85, 1.0)
SIDES = (('l', 1.0), ('r', -1.0))
ARM_Z = 1.465
GRIP = Vector((0.835, 0.0, 1.43))
CAPE = ((1.46, 0.15), (1.1, 0.175), (0.7, 0.2), (0.3, 0.215))
CAPE_BONES = tuple(f'cape.{index + 1}' for index in range(len(CAPE) - 1))
DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'HumanHead.blend')
HEAD_FRAME = Matrix.Translation((0.0, 0.006, 1.434)) @ Matrix.Scale(0.92, 4)
SKULL = Vector((0.0, -0.01, 0.315))
HAIR_TONE = (0.032, 0.017, 0.009)


def smoothstep(low, high, value):
    t = min(max((value - low) / (high - low), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def wrap(angle):
    return math.atan2(math.sin(angle), math.cos(angle))


def joint_layout():
    joints = [
        ('root', None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.12), (0.0, -1.0, 0.0)),
        ('hips', 'root', (0.0, 0.0, 0.99), (0.0, 0.0, 1.12), (0.0, -1.0, 0.0)),
        ('spine', 'hips', (0.0, 0.0, 1.12), (0.0, 0.0, 1.3), (0.0, -1.0, 0.0)),
        ('chest', 'spine', (0.0, 0.0, 1.3), (0.0, 0.0, 1.5), (0.0, -1.0, 0.0)),
        ('neck', 'chest', (0.0, 0.008, 1.5), (0.0, 0.0, 1.6), (0.0, -1.0, 0.0)),
        ('head', 'neck', (0.0, 0.0, 1.6), (0.0, 0.0, 1.835), (0.0, -1.0, 0.0)),
    ]
    parent = 'chest'
    for name, (z0, y0), (z1, y1) in zip(CAPE_BONES, CAPE, CAPE[1:]):
        joints.append((name, parent, (0.0, y0, z0), (0.0, y1, z1), (0.0, -1.0, 0.0)))
        parent = name
    for side, s in SIDES:
        joints += [
            (f'shoulder.{side}', 'chest', (s * 0.035, 0.0, 1.475), (s * 0.2, 0.01, ARM_Z), (0.0, 0.0, 1.0)),
            (f'upperarm.{side}', f'shoulder.{side}', (s * 0.2, 0.01, ARM_Z), (s * 0.49, 0.01, ARM_Z), (0.0, 0.0, 1.0)),
            (f'lowerarm.{side}', f'upperarm.{side}', (s * 0.49, 0.01, ARM_Z), (s * 0.75, 0.0, ARM_Z), (0.0, 0.0, 1.0)),
            (f'wrist.{side}', f'lowerarm.{side}', (s * 0.75, 0.0, ARM_Z), (s * 0.78, 0.0, ARM_Z), (0.0, 0.0, 1.0)),
            (f'hand.{side}', f'wrist.{side}', (s * 0.78, 0.0, ARM_Z), (s * 0.875, 0.0, ARM_Z), (0.0, 0.0, 1.0)),
            (f'upperleg.{side}', 'hips', (s * 0.1, 0.0, 0.95), (s * 0.105, -0.005, 0.515), (0.0, 1.0, 0.0)),
            (f'lowerleg.{side}', f'upperleg.{side}', (s * 0.105, -0.005, 0.515), (s * 0.105, 0.025, 0.09), (0.0, 1.0, 0.0)),
            (f'foot.{side}', f'lowerleg.{side}', (s * 0.105, 0.025, 0.09), (s * 0.105, -0.09, 0.03), (0.0, 0.0, 1.0)),
            (f'toes.{side}', f'foot.{side}', (s * 0.105, -0.09, 0.03), (s * 0.105, -0.17, 0.025), (0.0, 0.0, 1.0)),
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
    for side, s in SIDES:
        slot = data.edit_bones.new(f'handslot.{side}')
        slot.tail = (0.0, 0.06, 0.0)
        slot.matrix = Matrix(((-1.0, 0.0, 0.0, s * GRIP.x), (0.0, -1.0, 0.0, GRIP.y), (0.0, 0.0, 1.0, GRIP.z), (0.0, 0.0, 0.0, 1.0)))
        slot.parent = bones[f'hand.{side}']
        slot.use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')
    return rig


def loft(rings, closed=True, data=None, flip=False, merge=0.0):
    bm = bmesh.new()
    layer = bm.verts.layers.float_vector.new('Cloth')
    mark = bm.verts.layers.float.new('Mark')
    grid = []
    for i, ring in enumerate(rings):
        row = []
        for j, point in enumerate(ring):
            vertex = bm.verts.new(point)
            if data:
                entry = data[i][j]
                vertex[layer] = entry[:3]
                vertex[mark] = entry[3] if len(entry) > 3 else 0.0
            else:
                vertex[layer] = (1.0, 0.0, 1.0)
            row.append(vertex)
        grid.append(row)
    count = len(rings[0])
    for i in range(len(rings) - 1):
        for j in range(count if closed else count - 1):
            k = (j + 1) % count
            quad = (grid[i][j], grid[i][k], grid[i + 1][k], grid[i + 1][j])
            bm.faces.new(tuple(reversed(quad)) if flip else quad)
    if merge > 0.0:
        bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=merge)
    return bm


def solidify(obj, thickness, offset=-1.0):
    modifier = obj.modifiers.new('Solidify', 'SOLIDIFY')
    modifier.thickness = thickness
    modifier.offset = offset
    modifier.use_rim = True
    modifier.use_even_offset = False
    modifier.material_offset = 1
    modifier.material_offset_rim = 0
    kit.apply_modifiers(obj)
    return obj


def cloth_object(name, bm, target, materials, rng, thickness=0.012, offset=-1.0):
    obj = kit.mesh_object(name, bm, target, materials[0])
    for material in materials[1:]:
        obj.data.materials.append(material)
    pk.tag(obj, rng=rng)
    if thickness:
        solidify(obj, thickness, offset)
    obj.data.shade_smooth()
    return obj


def ensure_layers(obj):
    attributes = obj.data.attributes
    if 'Cloth' not in attributes:
        attributes.new('Cloth', 'FLOAT_VECTOR', 'POINT').data.foreach_set('vector', [1.0, 0.0, 1.0] * len(obj.data.vertices))
    if 'Mark' not in attributes:
        attributes.new('Mark', 'FLOAT', 'POINT')
    if 'Glow' not in attributes:
        attributes.new('Glow', 'FLOAT', 'POINT')
    return obj


def join(parts, name):
    for part in parts:
        ensure_layers(part)
    return kit.join(parts, name)


def place_part(name, bm, target, material, rng, matrix=None, bevel=None, smooth=None):
    if matrix is not None:
        bmesh.ops.transform(bm, matrix=matrix, verts=bm.verts)
    return pk.part(name, bm, target, material, rng=rng, bevel=bevel, smooth=smooth)


def ring_x(x_center, radius, minor, s, steps=24, sides=8):
    bm = pk.torus(radius, minor, steps, sides)
    bmesh.ops.transform(bm, matrix=Matrix.Translation((s * x_center, 0.004, ARM_Z)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Y'), verts=bm.verts)
    return bm


TORSO = [
    (0.98, 0.183, 0.132, 0.0), (1.06, 0.176, 0.127, 0.0), (1.16, 0.184, 0.131, -0.004), (1.26, 0.205, 0.141, -0.01),
    (1.34, 0.226, 0.149, -0.014), (1.41, 0.236, 0.144, -0.01), (1.46, 0.226, 0.133, -0.004), (1.5, 0.168, 0.115, 0.0),
    (1.53, 0.106, 0.093, 0.005), (1.56, 0.086, 0.081, 0.006),
]


def torso_section(z):
    z = min(max(z, TORSO[0][0]), TORSO[-1][0])
    for (z0, rx0, ry0, y0), (z1, rx1, ry1, y1) in zip(TORSO, TORSO[1:]):
        if z <= z1:
            t = (z - z0) / (z1 - z0)
            return lerp(rx0, rx1, t), lerp(ry0, ry1, t), lerp(y0, y1, t)
    return TORSO[-1][1:]


def torso_point(angle, z, gap):
    rx, ry, yc = torso_section(z)
    point = Vector((math.cos(angle) * (rx + gap), yc + math.sin(angle) * (ry + gap), z))
    front = max(0.0, -math.sin(angle))
    pec = math.exp(-((abs(point.x) - 0.085) / 0.07) ** 2 - ((z - 1.36) / 0.06) ** 2)
    point.y -= 0.016 * pec * front * front
    return point


def body(m, target, rng):
    shapes = [
        pk.ellipsoid_bm((0.0, 0.0, 1.0), (0.168, 0.112, 0.12)),
        pk.ellipsoid_bm((0.0, 0.004, 1.15), (0.155, 0.105, 0.13)),
        pk.ellipsoid_bm((0.0, 0.0, 1.335), (0.205, 0.13, 0.16)),
        pk.capsule_bm((0.0, 0.012, 1.44), (0.0, 0.008, 1.62), 0.066, 0.06),
    ]
    for _, s in SIDES:
        shapes += [
            pk.ellipsoid_bm((s * 0.085, -0.075, 1.37), (0.09, 0.05, 0.068)),
            pk.capsule_bm((s * 0.04, 0.012, 1.5), (s * 0.17, 0.008, 1.485), 0.058, 0.052),
            pk.ellipsoid_bm((s * 0.205, 0.0, ARM_Z + 0.008), (0.078, 0.074, 0.074)),
            pk.capsule_bm((s * 0.2, 0.01, ARM_Z), (s * 0.49, 0.01, ARM_Z), 0.064, 0.05),
            pk.ellipsoid_bm((s * 0.34, -0.008, ARM_Z + 0.006), (0.1, 0.06, 0.062)),
            pk.capsule_bm((s * 0.49, 0.01, ARM_Z), (s * 0.75, 0.0, ARM_Z), 0.052, 0.038),
            pk.ellipsoid_bm((s * 0.57, 0.0, ARM_Z + 0.004), (0.075, 0.054, 0.052)),
            pk.capsule_bm((s * 0.1, 0.0, 0.96), (s * 0.105, -0.005, 0.515), 0.094, 0.064),
            pk.capsule_bm((s * 0.105, -0.005, 0.515), (s * 0.105, 0.025, 0.09), 0.062, 0.042),
            pk.ellipsoid_bm((s * 0.105, 0.03, 0.37), (0.058, 0.062, 0.1)),
            pk.ellipsoid_bm((s * 0.105, -0.035, 0.045), (0.043, 0.1, 0.044)),
        ]
    return pk.sculpt(shapes, target, m['shade'], 'Hero_Body', voxel=0.01, smooth=6, smooth_factor=0.5, faces=8000, rng=rng)


def sleeve(m, target, rng, side, s):
    stations = [(0.15, 0.088, 0.084), (0.22, 0.084, 0.08), (0.3, 0.078, 0.075), (0.38, 0.071, 0.069), (0.45, 0.063, 0.062), (0.5, 0.059, 0.058), (0.56, 0.061, 0.059), (0.625, 0.056, 0.054)]
    cols = 28
    rings, data = [], []
    for x, ry, rz in stations:
        ring, values = [], []
        elbow = math.exp(-((x - 0.5) / 0.05) ** 2)
        for j in range(cols):
            angle = 2.0 * math.pi * j / cols
            wrinkle = 0.004 * math.sin(4.0 * angle + x * 41.0) * (0.4 + elbow)
            ring.append(Vector((s * x, 0.008 + math.cos(angle) * (ry + wrinkle), ARM_Z + math.sin(angle) * (rz + wrinkle))))
            values.append((1.0, angle * 0.06, 1.0, 0.0))
        rings.append(ring)
        data.append(values)
    return cloth_object(f'Wizard_Sleeve_{side.upper()}', loft(rings, data=data, flip=s < 0.0), target, [m['cloth'], m['cloth']], rng, thickness=0.0)


def cuirass(m, target, rng):
    cols, rows = 80, 24
    rings, data = [], []
    for i in range(rows + 1):
        t = i / rows
        ring, values = [], []
        for j in range(cols):
            angle = 2.0 * math.pi * j / cols
            back = 0.5 + 0.5 * math.sin(angle)
            top = lerp(1.488, 1.535, back ** 0.7)
            z = lerp(0.99, top, t)
            point = torso_point(angle, z, 0.006)
            seams = [abs(wrap(angle)) * 0.2, abs(wrap(angle - math.pi)) * 0.2]
            if point.y < -0.03:
                seams.append(abs(point.x))
                if abs(point.x) < 0.17:
                    seams += [abs(z - 1.29), abs(z - 1.2), abs(z - 1.115)]
            ring.append(point)
            values.append((min(z - 0.99, top - z), angle * 0.19, min(seams), 0.0))
        rings.append(ring)
        data.append(values)
    return cloth_object('Wizard_Cuirass', loft(rings, data=data), target, [m['armor'], m['armor']], rng, thickness=0.006, offset=1.0)


def collar(m, target, rng):
    cols = 48
    rings, data = [], []
    for z, radius in ((1.468, 0.09), (1.505, 0.084), (1.54, 0.078), (1.578, 0.075)):
        ring, values = [], []
        for j in range(cols):
            angle = 2.0 * math.pi * j / cols
            ring.append(Vector((math.cos(angle) * radius, 0.012 + math.sin(angle) * radius * 0.95, z + 0.022 * max(0.0, math.sin(angle)) ** 1.5)))
            values.append((1.578 - z + 0.002, angle * 0.08, 1.0, 0.0))
        rings.append(ring)
        data.append(values)
    return cloth_object('Wizard_Collar', loft(rings, data=data), target, [m['under'], m['under']], rng, thickness=0.004, offset=1.0)


def baldric(m, target, rng):
    rows, columns = 44, 3
    width = 0.046
    centers = []
    for i in range(rows + 1):
        t = i / rows
        z = lerp(1.49, 1.06, t)
        rx, _, _ = torso_section(z)
        x = lerp(-0.168, 0.165, t)
        angle = -math.acos(max(-1.0, min(1.0, x / (rx + 0.014))))
        centers.append((torso_point(angle, z, 0.014), angle, z))
    rings, data = [], []
    for i, (point, angle, z) in enumerate(centers):
        ahead = centers[min(i + 1, rows)][0] - centers[max(i - 1, 0)][0]
        rx, ry, _ = torso_section(z)
        normal = Vector((math.cos(angle) / rx, math.sin(angle) / ry, 0.0)).normalized()
        across = normal.cross(ahead).normalized()
        ring, values = [], []
        for k in range(columns):
            w = (k / (columns - 1) - 0.5) * width
            ring.append(point + across * w)
            values.append((0.5 * width - abs(w) + 0.0005, i / rows * 0.6, 1.0, 0.0))
        rings.append(ring)
        data.append(values)
    strap = cloth_object('Wizard_Baldric', loft(rings, closed=False, data=data, flip=True), target, [m['armor'], m['armor']], rng, thickness=0.005, offset=1.0)
    fitting_point, fitting_angle, fitting_z = centers[rows // 2]
    plate = pk.box((0.034, 0.012, 0.05))
    frame = Matrix.Translation(fitting_point + Vector((0.0, -0.008, 0.0))) @ Matrix.Rotation(-0.75, 4, 'Y')
    return [strap, place_part('Wizard_BaldricPlate', plate, target, m['bronze'], rng, matrix=frame, bevel=(0.004, 2, 30.0))]


def belt(m, target, rng):
    cols = 80
    rings, data = [], []
    for z in (0.995, 1.031, 1.068):
        ring, values = [], []
        for j in range(cols):
            angle = 2.0 * math.pi * j / cols
            ring.append(Vector((math.cos(angle) * 0.205, math.sin(angle) * 0.155, z)))
            values.append((min(z - 0.995, 1.068 - z), angle * 0.18, 1.0, 0.0))
        rings.append(ring)
        data.append(values)
    parts = [cloth_object('Wizard_BeltBand', loft(rings, data=data), target, [m['armor'], m['armor']], rng, thickness=0.012, offset=1.0)]
    buckle = pk.lathe([(0.0, 0.0), (0.03, 0.0), (0.044, 0.002), (0.048, 0.007), (0.046, 0.012), (0.04, 0.0135), (0.033, 0.011), (0.027, 0.012), (0.019, 0.0155), (0.012, 0.017), (0.0, 0.018)], 36)
    face = Matrix.Translation((0.0, -0.166, 1.031)) @ Matrix.Rotation(0.5 * math.pi, 4, 'X')
    parts.append(place_part('Wizard_Buckle', buckle, target, m['bronze'], rng, matrix=face, smooth=35.0))
    parts.append(place_part('Wizard_BuckleGem', pk.ellipsoid_bm((0.0, 0.0, 0.0), (0.012, 0.012, 0.007), segments=16, rings=8), target, m['gem'], rng, matrix=Matrix.Translation((0.0, -0.183, 1.031)) @ Matrix.Rotation(0.5 * math.pi, 4, 'X'), smooth=60.0))
    for _, s in SIDES:
        loop = pk.torus(0.011, 0.003, 16, 6)
        parts.append(place_part('Wizard_BeltLoop', loop, target, m['bronze'], rng, matrix=Matrix.Translation((s * 0.19, 0.075, 0.99)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Y') @ Matrix.Rotation(s * 0.4, 4, 'X'), smooth=50.0))
    return parts


def skirt_section(z):
    t = smoothstep(1.0, 0.45, z) ** 0.8
    return lerp(0.2, 0.236, t), lerp(0.152, 0.178, t)


def skirt_panel(m, target, rng, name, start, end, pointed):
    span = end - start
    center = 0.5 * (start + end)
    cols = max(12, int(span * 26))
    rows = 18
    top = 1.005
    drop = 0.09
    slope = drop / (0.5 * span * 0.226)
    slant = 1.0 / math.sqrt(1.0 + slope * slope) if pointed else 1.0

    def hem(angle):
        if pointed:
            return 0.452 + drop * abs(angle - center) / (0.5 * span)
        return 0.505 + 0.01 * math.sin(3.1 * angle + 0.6)

    rings, data = [], []
    for i in range(rows + 1):
        v = i / rows
        ring, values = [], []
        for j in range(cols + 1):
            u = j / cols
            angle = lerp(start, end, u)
            bottom = hem(angle)
            z = lerp(top, bottom, v)
            rx, ry = skirt_section(z)
            fold = 0.0 if pointed else 0.6 * math.sin((angle - start) * 9.0 + 0.4) + 0.4 * math.sin((angle - start) * 15.0 + 1.9)
            depth = 1.0 + 0.045 * fold * v ** 1.4
            point = Vector((math.cos(angle) * rx * depth, math.sin(angle) * ry * depth + 0.01 * v * v, z))
            if pointed:
                point.y -= 0.014 * v
            arc = rx * span
            edge = min(min(u, 1.0 - u) * arc, (z - bottom) * slant)
            along = (u - 0.5) * arc / slant if pointed else angle * rx
            ring.append(point)
            values.append((edge, along, (u - 0.5) * arc if pointed else 1.0, 1.0 if z - bottom < 0.1 else 0.0))
        rings.append(ring)
        data.append(values)
    return cloth_object(name, loft(rings, closed=False, data=data, flip=True), target, [m['cloth'], m['lining']], rng, thickness=0.008)


def skirt(m, target, rng):
    front = -0.5 * math.pi
    half, gap, vent = 0.33, 0.07, 0.035
    return [
        skirt_panel(m, target, rng, 'Wizard_Tabard', front - half, front + half, True),
        skirt_panel(m, target, rng, 'Wizard_Skirt_L', front + half + gap, 0.5 * math.pi - vent, False),
        skirt_panel(m, target, rng, 'Wizard_Skirt_R', 0.5 * math.pi + vent, 1.5 * math.pi - half - gap, False),
    ]


def leg_tube(points, radii, cols=20):
    rings = []
    for i, (point, radius) in enumerate(zip(points, radii)):
        a = Vector(points[max(i - 1, 0)])
        b = Vector(points[min(i + 1, len(points) - 1)])
        frame = (b - a).normalized().to_track_quat('Z', 'Y').to_matrix()
        rings.append([Vector(point) + frame @ Vector((math.cos(2.0 * math.pi * j / cols) * radius, math.sin(2.0 * math.pi * j / cols) * radius, 0.0)) for j in range(cols)])
    return rings


def legs(m, target, rng):
    parts = []
    for side, s in SIDES:
        rings = leg_tube([(s * 0.095, 0.0, 1.0), (s * 0.104, -0.003, 0.76), (s * 0.105, -0.005, 0.515), (s * 0.105, 0.01, 0.44), (s * 0.105, 0.015, 0.39)], [0.096, 0.09, 0.074, 0.068, 0.066])
        parts.append(cloth_object(f'Wizard_Trousers_{side.upper()}', loft(rings), target, [m['under'], m['under']], rng, thickness=0.0))
        x = s * 0.105
        boot = pk.sculpt([
            pk.capsule_bm((x, 0.02, 0.46), (x, 0.022, 0.1), 0.07, 0.054),
            pk.ellipsoid_bm((x, 0.032, 0.34), (0.066, 0.07, 0.1)),
            pk.ellipsoid_bm((x, -0.035, 0.05), (0.05, 0.11, 0.052)),
            pk.ellipsoid_bm((x, 0.035, 0.045), (0.046, 0.052, 0.047)),
            pk.capsule_bm((x, -0.1, 0.045), (x + s * 0.004, -0.175, 0.052), 0.036, 0.026),
        ], target, m['leather'], f'Wizard_Boot_{side.upper()}', voxel=0.005, smooth=6, smooth_factor=0.5, faces=1800, rng=rng)
        bm = bmesh.new()
        bm.from_mesh(boot.data)
        kit.cut(bm, Vector((0.0, 0.0, 0.004)), Vector((0.0, 0.0, -1.0)))
        bm.to_mesh(boot.data)
        bm.free()
        parts.append(boot)
        cuff = pk.torus(0.074, 0.018, 28, 8)
        parts.append(place_part(f'Wizard_BootCuff_{side.upper()}', cuff, target, m['leather'], rng, matrix=Matrix.Translation((x, 0.021, 0.452)), smooth=40.0))
        strap = pk.torus(0.066, 0.006, 28, 6)
        parts.append(place_part(f'Wizard_BootStrap_{side.upper()}', strap, target, m['leather'], rng, matrix=Matrix.Translation((x, 0.024, 0.27)), smooth=40.0))
        parts.append(place_part(f'Wizard_BootBuckle_{side.upper()}', pk.box((0.026, 0.008, 0.02)), target, m['bronze'], rng, matrix=Matrix.Translation((x, -0.043, 0.27)), bevel=(0.003, 1, 30.0)))
    return parts


def bracer(m, target, rng, side, s, material):
    rows, cols = 10, 28
    x0, x1 = 0.6, 0.778
    rings, data = [], []
    for i in range(rows + 1):
        t = i / rows
        x = lerp(x0, x1, t)
        r = lerp(0.061, 0.05, t) + 0.003 * math.sin(math.pi * t)
        ring, values = [], []
        for j in range(cols):
            angle = 2.0 * math.pi * j / cols
            ring.append(Vector((s * x, 0.004 + math.cos(angle) * r, ARM_Z + math.sin(angle) * r * 0.96)))
            values.append((min(x - x0, x1 - x), angle * r, 1.0, 1.0))
        rings.append(ring)
        data.append(values)
    parts = [cloth_object(f'Wizard_Bracer_{side.upper()}', loft(rings, data=data, flip=s < 0.0), target, [material, material], rng, thickness=0.005, offset=1.0)]
    for x, radius in ((x0 + 0.005, 0.066), (x1 - 0.005, 0.0555)):
        parts.append(pk.part(f'Wizard_BracerBand_{side.upper()}', ring_x(x, radius, 0.0042, s, 28, 8), target, m['bronze'], rng=rng, smooth=50.0))
    return parts


def fist(target, material, name, rng, s, inflate=0.0):
    pivot = Vector((0.8, 0.0, 1.41))
    shift = GRIP - pivot

    def at(x, y, z):
        point = pivot + (Vector((x, y, z)) - pivot) * 1.08 + shift
        return Vector((s * point.x, point.y, point.z))

    shapes = [
        pk.ellipsoid_bm(at(0.775, 0.0, 1.437), (0.054 + inflate, 0.051 + inflate, 0.026 + inflate)),
        pk.capsule_bm(at(0.705, 0.0, 1.445), at(0.745, 0.0, 1.442), 0.035 + inflate, 0.038 + inflate),
    ]
    for y, radius in ((-0.03, 0.0146), (-0.01, 0.0157), (0.01, 0.0151), (0.028, 0.0135)):
        path = [at(0.818, y, 1.437), at(0.842, y, 1.425), at(0.84, y, 1.398), at(0.818, y, 1.385), at(0.795, y, 1.39)]
        for a, b in zip(path, path[1:]):
            shapes.append(pk.capsule_bm(a, b, radius + inflate, radius * 0.95 + inflate, segments=10, rings=3))
    thumb = [at(0.752, -0.04, 1.43), at(0.78, -0.052, 1.405), at(0.805, -0.045, 1.39), at(0.815, -0.03, 1.39)]
    for a, b in zip(thumb, thumb[1:]):
        shapes.append(pk.capsule_bm(a, b, 0.0167 + inflate, 0.0157 + inflate, segments=10, rings=3))
    return pk.sculpt(shapes, target, material, name, voxel=0.0035, smooth=4, smooth_factor=0.5, faces=1500, rng=rng)


def book(m, target, rng):
    frame = Matrix.Translation((0.238, 0.052, 0.86)) @ Matrix.Rotation(0.32, 4, 'Z') @ Matrix.Rotation(-0.08, 4, 'Y')
    parts = [
        place_part('Wizard_Book', pk.box((0.046, 0.136, 0.18)), target, m['leather'], rng, matrix=frame, bevel=(0.006, 2, 30.0)),
        place_part('Wizard_BookPages', pk.box((0.036, 0.126, 0.168), (0.0, -0.008, 0.0)), target, m['paper'], rng, matrix=frame),
        place_part('Wizard_BookClasp', pk.box((0.054, 0.024, 0.032), (0.0, -0.062, 0.0)), target, m['bronze'], rng, matrix=frame, bevel=(0.004, 1, 30.0)),
        place_part('Wizard_BookBoss', pk.ellipsoid_bm((0.024, 0.0, 0.0), (0.006, 0.026, 0.026), segments=20, rings=10), target, m['bronze'], rng, matrix=frame, smooth=50.0),
        place_part('Wizard_BookStrap', pk.box((0.012, 0.03, 0.09), (0.0, 0.04, 0.12)), target, m['leather'], rng, matrix=frame, bevel=(0.003, 1, 30.0)),
    ]
    for z in (-0.088, 0.088):
        parts.append(place_part('Wizard_BookCorner', pk.box((0.05, 0.016, 0.016), (0.0, -0.066, z)), target, m['bronze'], rng, matrix=frame, bevel=(0.003, 1, 30.0)))
    return parts


def scroll_case(m, target, rng):
    frame = Matrix.Translation((-0.24, 0.056, 0.76)) @ Matrix.Rotation(-0.1, 4, 'X') @ Matrix.Translation((0.0, 0.0, -0.8))
    parts = [
        place_part('Wizard_ScrollBody', pk.lathe([(0.0285, 0.69), (0.0285, 0.935)], 28), target, m['leather'], rng, matrix=frame, smooth=50.0),
        place_part('Wizard_ScrollCap', pk.lathe([(0.0305, 0.928), (0.0338, 0.932), (0.0352, 0.94), (0.0338, 0.948), (0.0302, 0.952), (0.03, 0.962), (0.024, 0.97), (0.014, 0.976), (0.0085, 0.978), (0.007, 0.986), (0.0105, 0.992), (0.007, 0.998), (0.0, 1.0)], 28), target, m['bronze'], rng, matrix=frame, smooth=35.0),
        place_part('Wizard_ScrollFoot', pk.lathe([(0.0, 0.632), (0.006, 0.638), (0.01, 0.648), (0.018, 0.656), (0.026, 0.664), (0.03, 0.673), (0.0338, 0.677), (0.0352, 0.685), (0.0338, 0.693), (0.0305, 0.697)], 28), target, m['bronze'], rng, matrix=frame, smooth=35.0),
    ]
    for z in (0.772, 0.858):
        parts.append(place_part('Wizard_ScrollBand', pk.lathe([(0.0282, z - 0.008), (0.0322, z - 0.006), (0.0334, z), (0.0322, z + 0.006), (0.0282, z + 0.008)], 28), target, m['bronze'], rng, matrix=frame, smooth=35.0))
    for k in range(8):
        angle = 2.0 * math.pi * (k + 0.5) / 8
        for z0, z1 in ((0.7, 0.764), (0.78, 0.85), (0.866, 0.926)):
            rod = pk.rod((math.cos(angle) * 0.0296, math.sin(angle) * 0.0296, z0), (math.cos(angle + 0.25) * 0.0296, math.sin(angle + 0.25) * 0.0296, z1), 0.0019, steps=6)
            parts.append(place_part('Wizard_ScrollFiligree', rod, target, m['bronze'], rng, matrix=frame, smooth=50.0))
    parts.append(place_part('Wizard_ScrollGem', pk.ellipsoid_bm((0.0, -0.032, 0.94), (0.0075, 0.005, 0.0075), segments=12, rings=8), target, m['gem'], rng, matrix=frame, smooth=60.0))
    hanger = pk.torus(0.011, 0.0028, 16, 6)
    parts.append(place_part('Wizard_ScrollRing', hanger, target, m['bronze'], rng, matrix=frame @ Matrix.Translation((0.0, 0.0, 1.011)) @ Matrix.Rotation(0.5 * math.pi, 4, 'X'), smooth=50.0))
    parts.append(place_part('Wizard_ScrollStrap', pk.box((0.012, 0.006, 0.05), (-0.235, 0.07, 1.0)), target, m['leather'], rng, bevel=(0.002, 1, 30.0)))
    return parts


def lame(name, s, x0, x1, r0, r1, spread0, spread1, rows, cols, material, target, rng, flare=0.007):
    rings, data = [], []
    for i in range(rows + 1):
        t = i / rows
        x = lerp(x0, x1, t)
        spread = lerp(spread0, spread1, smoothstep(0.0, 1.0, t))
        radius = lerp(r0, r1, t) + flare * smoothstep(0.65, 1.0, t)
        zc = ARM_Z + 0.004 - 0.07 * (x - 0.2)
        ring, values = [], []
        for j in range(cols + 1):
            u = j / cols
            theta = lerp(-spread, spread, u)
            rr = radius + 0.6 * flare * smoothstep(0.8, 1.0, abs(2.0 * u - 1.0))
            ring.append(Vector((s * x, 0.005 + math.sin(theta) * rr, zc + math.cos(theta) * rr)))
            values.append((min(x - x0, x1 - x, (spread - abs(theta)) * rr), theta * rr, 1.0, 0.0))
        rings.append(ring)
        data.append(values)
    return cloth_object(name, loft(rings, closed=False, data=data, flip=s > 0.0), target, [material, material], rng, thickness=0.006, offset=1.0)


def pauldrons(m, target, rng):
    parts = []
    for side, s in SIDES:
        parts.append(lame(f'Wizard_PauldronCap_{side.upper()}', s, 0.1, 0.245, 0.098, 0.106, 1.2, 2.05, 12, 32, m['plate'], target, rng))
        parts.append(lame(f'Wizard_PauldronLame1_{side.upper()}', s, 0.225, 0.295, 0.1, 0.112, 2.05, 2.0, 6, 32, m['plate'], target, rng))
        parts.append(lame(f'Wizard_PauldronLame2_{side.upper()}', s, 0.275, 0.345, 0.106, 0.118, 2.0, 1.9, 6, 32, m['plate'], target, rng))
    return parts


def chest_plate(m, target, rng):
    cols, rows = 44, 16
    span = 0.8
    rings, data = [], []
    for i in range(rows + 1):
        t = i / rows
        ring, values = [], []
        for j in range(cols + 1):
            u = j / cols
            side = abs(2.0 * u - 1.0)
            angle = -0.5 * math.pi + (2.0 * u - 1.0) * span
            bottom = 1.322 + 0.07 * side ** 1.4
            top = 1.462 + 0.045 * smoothstep(0.15, 0.6, side)
            z = lerp(bottom, top, t)
            rx = torso_section(z)[0]
            height = top - bottom
            ring.append(torso_point(angle, z, 0.017))
            values.append((min(t * height, (1.0 - t) * height, min(u, 1.0 - u) * 2.0 * span * rx), (2.0 * u - 1.0) * span * rx, 1.0, 0.0))
        rings.append(ring)
        data.append(values)
    parts = [cloth_object('Wizard_ChestPlate', loft(rings, closed=False, data=data), target, [m['plate'], m['plate']], rng, thickness=0.007, offset=1.0)]
    center = torso_point(-0.5 * math.pi, 1.415, 0.031)
    facing = Matrix.Translation(center) @ Matrix.Rotation(0.5 * math.pi, 4, 'X')
    parts.append(place_part('Wizard_ChestGem', pk.ellipsoid_bm((0.0, 0.0, 0.0), (0.016, 0.019, 0.009), segments=16, rings=8), target, m['gem'], rng, matrix=facing, smooth=60.0))
    parts.append(place_part('Wizard_ChestBezel', pk.torus(0.02, 0.0036, 24, 8), target, m['bronze'], rng, matrix=facing @ Matrix.Diagonal((1.0, 1.15, 1.0, 1.0)), smooth=50.0))
    return parts


def cape(m, target, rng):
    rows, cols = 30, 48
    top, bottom = 1.495, 0.27
    rings, data = [], []
    for i in range(rows + 1):
        v = i / rows
        line = lerp(top, bottom, v)
        a0 = lerp(0.42, -0.05, smoothstep(0.0, 0.75, v))
        a1 = math.pi - a0
        ring, values = [], []
        for j in range(cols + 1):
            u = j / cols
            angle = lerp(a0, a1, u)
            hem = 0.035 * (1.0 - math.sin(angle)) + 0.012 * math.sin(angle * 5.0 + 0.7)
            z = line + hem * v ** 6
            body_point = torso_point(angle, min(max(z, 1.02), 1.5), 0.026)
            rx = lerp(0.24, 0.34, smoothstep(1.32, 0.35, z))
            ry = lerp(0.168, 0.255, smoothstep(1.36, 0.42, z))
            hang = Vector((math.cos(angle) * rx, 0.015 + math.sin(angle) * ry, z))
            point = body_point.lerp(hang, smoothstep(1.47, 1.28, z))
            point.z = z
            fold = (0.55 * math.sin(angle * 7.0 + 0.3) + 0.3 * math.sin(angle * 12.0 + 1.7) + 0.15 * math.sin(angle * 19.0 + 0.4)) * smoothstep(1.4, 0.5, z)
            point += Vector((math.cos(angle), math.sin(angle), 0.0)) * 0.036 * fold
            ring.append(point)
            values.append((min(min(u, 1.0 - u) * (a1 - a0) * rx, z - bottom - hem + 0.001), angle * rx, 1.0, 0.0))
        rings.append(ring)
        data.append(values)
    return cloth_object('Wizard_Cape', loft(rings, closed=False, data=data, flip=True), target, [m['cloth'], m['lining']], rng, thickness=0.01)


HOOD_CAP = (0.116, 0.132, 0.122, 0.145)
HOOD_DRAPE = (0.205, 0.17, 0.18, 1.48, 1.585)


def ellipse_radius(azimuth, side, front, back):
    depth = back if math.sin(azimuth) > 0.0 else front
    return 1.0 / math.sqrt((math.cos(azimuth) / side) ** 2 + (math.sin(azimuth) / depth) ** 2)


def hood_cap(azimuth, elevation):
    side, front, back, top = HOOD_CAP
    flat = ellipse_radius(azimuth, side, front, back)
    rise = top if elevation > 0.0 else flat
    return 1.0 / math.sqrt((math.cos(elevation) / flat) ** 2 + (math.sin(elevation) / rise) ** 2)


def hood_meridian(azimuth, center):
    side, front, back, low, high = HOOD_DRAPE
    shoulder = (ellipse_radius(azimuth, side, front, back), lerp(low, high, math.cos(azimuth) ** 2) - center.z)
    best, tangent = 9.0, None
    for k in range(97):
        elevation = lerp(-0.4, 0.5 * math.pi, k / 96)
        radius = hood_cap(azimuth, elevation)
        point = (radius * math.cos(elevation), radius * math.sin(elevation))
        angle = math.atan2(point[1] - shoulder[1], point[0] - shoulder[0])
        if angle < best:
            best, tangent = angle, (elevation, point)
    return tangent, shoulder


def hood_radius(azimuth, elevation, tangent, shoulder):
    split, top = tangent
    if elevation >= split:
        return hood_cap(azimuth, elevation), 0.0
    ray = (math.cos(elevation), math.sin(elevation))
    along = (shoulder[0] - top[0], shoulder[1] - top[1])
    radius = (top[0] * along[1] - top[1] * along[0]) / (ray[0] * along[1] - ray[1] * along[0])
    point = (ray[0] * radius, ray[1] * radius)
    return radius, min(1.0, math.hypot(point[0] - top[0], point[1] - top[1]) / math.hypot(*along))


def hood(m, target, rng):
    center = HEAD_FRAME @ SKULL + Vector((0.0, 0.006, 0.0))
    rows, cols = 34, 64
    apex, hem = 0.72, 0.035
    rings, data = [], []
    for i in range(rows + 1):
        v = i / rows
        facing = lerp(-0.95, 0.5 * math.pi, v)
        gap = 0.82 * min(1.0, (apex - facing) / 0.9) ** 0.85 if facing < apex else 0.0
        gap = lerp(gap, 0.5, smoothstep(-0.35, -0.95, facing))
        start, end = -0.5 * math.pi + gap, 1.5 * math.pi - gap
        ring, values = [], []
        for j in range(cols + 1):
            u = j / cols
            azimuth = lerp(start, end, u)
            tangent, shoulder = hood_meridian(azimuth, center)
            bottom = math.atan2(shoulder[1], shoulder[0]) - hem
            elevation = lerp(bottom, 0.5 * math.pi, v)
            radius, slope = hood_radius(azimuth, elevation, tangent, shoulder)
            folds = 0.007 * math.sin(6.0 * azimuth + 0.8) + 0.004 * math.sin(11.0 * azimuth + 2.1 + 3.0 * slope)
            radius += (folds - 0.009 * math.sin(math.pi * min(slope, 1.0))) * slope
            direction = Vector((math.cos(elevation) * math.cos(azimuth), math.cos(elevation) * math.sin(azimuth), math.sin(elevation)))
            point = center + direction * radius
            seam = math.exp(-(point.x / 0.022) ** 2) * smoothstep(0.1, 0.7, elevation)
            point += direction * 0.006 * seam
            rim = min(u, 1.0 - u) * (end - start) if gap > 0.0 else 9.0
            near = 1.0 - smoothstep(0.0, 0.5, rim)
            point.y -= 0.03 * near * smoothstep(-0.85, -0.35, elevation)
            crown = math.exp(-((elevation - apex) / 0.22) ** 2) * math.exp(-(wrap(azimuth + 0.5 * math.pi) / 0.45) ** 2)
            point.y -= 0.045 * crown
            point.z -= 0.01 * crown
            ring.append(point)
            reach = radius * max(math.cos(elevation), 0.2)
            values.append((min(rim * reach, (elevation - bottom) * radius + 0.002), azimuth * 0.2, 1.0, 0.0))
        rings.append(ring)
        data.append(values)
    bm = loft(rings, closed=False, data=data, merge=0.0006)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges[:], dist=1e-6)
    rim = [vertex for vertex in bm.verts if vertex.is_boundary and vertex.co.z > 1.56]
    for _ in range(3):
        bmesh.ops.smooth_vert(bm, verts=rim, factor=0.35, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    return cloth_object('Wizard_Hood', bm, target, [m['hood'], m['hood_lining']], rng, thickness=0.01)


def cowl(m, target, rng):
    around, rows = 96, 16
    rings, data = [], []
    for i in range(rows + 1):
        v = i / rows
        ring, values = [], []
        for j in range(around):
            azimuth = 2.0 * math.pi * j / around
            back = max(0.0, math.sin(azimuth))
            pocket = back ** 2.5
            neck = Vector((math.cos(azimuth) * 0.106, 0.016 + math.sin(azimuth) * 0.102, 1.532 + 0.012 * back))
            rest = torso_point(azimuth, 1.47, 0.03)
            drop = lerp(1.47, 1.29, pocket) - 0.035 * pocket * math.cos(wrap(azimuth - 0.5 * math.pi) * 3.0)
            outer = Vector((lerp(rest.x, rest.x * 0.55, pocket), lerp(rest.y, 0.235, pocket), drop))
            roll = Vector((math.cos(azimuth) * 0.17, 0.016 + math.sin(azimuth) * 0.16, 1.575 + 0.02 * back))
            point = neck * (1.0 - v) ** 2 + roll * 2.0 * (1.0 - v) * v + outer * v * v
            fold = 0.6 * math.sin(azimuth * 7.0 + 0.8) + 0.4 * math.sin(azimuth * 12.0 + 2.1)
            point += Vector((math.cos(azimuth), math.sin(azimuth), 0.25)) * 0.014 * fold * math.sin(math.pi * v) * (0.5 + pocket)
            ring.append(point)
            values.append(((1.0 - v) * 0.16 + 0.002, azimuth * 0.15, 1.0, 0.0))
        rings.append(ring)
        data.append(values)
    return cloth_object('Wizard_Cowl', loft(rings, data=data, flip=True), target, [m['hood'], m['hood_lining']], rng, thickness=0.01)


def falloff(point, center, radius):
    return math.exp(-((point - Vector(center)).length / radius) ** 2)


def curve_at(x, points):
    if x <= points[0][0]:
        return points[0][1]
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        if x <= x1:
            return lerp(y0, y1, (x - x0) / (x1 - x0))
    return points[-1][1]


def masculine(p):
    q = p.copy()
    jaw = math.exp(-((p.z - 0.205) / 0.05) ** 2) * smoothstep(0.09, 0.05, p.y)
    chin = falloff(p, (0.0, -0.128, 0.183), 0.035)
    nose = falloff(p, (0.0, -0.152, 0.27), 0.025)
    neck = smoothstep(0.21, 0.13, p.z) * smoothstep(-0.12, -0.085, p.y)
    q.x *= (1.0 + 0.2 * jaw) * (1.0 + 0.3 * chin) * (1.0 + 0.08 * nose) * (1.0 + 0.12 * neck) * 1.03
    q.y = lerp(q.y - 0.006 * chin - 0.004 * nose, 0.015 + (q.y - 0.015) * 1.1, neck)
    q.z -= 0.005 * chin
    for s in (1.0, -1.0):
        brow = falloff(p, (s * 0.035, -0.108, 0.338), 0.024)
        q.y -= 0.008 * brow
        q.z -= 0.0025 * brow
        q.x += s * 0.002 * falloff(p, (s * 0.056, -0.085, 0.29), 0.025)
    return q


def head_angles(p):
    d = p - SKULL
    return abs(wrap(math.atan2(d.y, d.x) + 0.5 * math.pi)), math.atan2(d.z, math.hypot(d.x, d.y))


HAIRLINE = ((0.0, 0.62), (0.5, 0.56), (0.85, 0.4), (1.15, 0.06), (1.57, 0.12), (2.0, -0.42), (2.5, -0.9), (math.pi, -1.0))


def scalp_cover(p, fade=0.0):
    front, elevation = head_angles(p)
    reach = 0.06 + fade * smoothstep(0.8, 1.4, front)
    return smoothstep(-reach, 0.06, elevation - curve_at(front, HAIRLINE))


def hair_amount(p, region):
    return 0.0 if region in (3, 4) else scalp_cover(p)


class Scalar:
    @staticmethod
    def smooth(low, high, value):
        return smoothstep(low, high, value)

    @staticmethod
    def lerp(factor, a, b):
        return lerp(a, b, min(1.0, max(0.0, factor)))

    @staticmethod
    def add(a, b):
        return a + b

    @staticmethod
    def sub(a, b):
        return a - b

    @staticmethod
    def mul(a, b):
        return a * b

    @staticmethod
    def math(operation, a, b=0.0):
        return abs(a) if operation == 'ABSOLUTE' else max(a, b) if operation == 'MAXIMUM' else min(a, b)


def beard_field(o, x, y, z, grow=0.0):
    ax = o.math('ABSOLUTE', x)
    diagonal = o.add(o.mul(o.add(y, 0.075), 1.24), 0.3)
    cheek = o.lerp(o.smooth(-0.084, -0.068, y), diagonal, 0.339)
    top = o.lerp(o.smooth(0.034, 0.022, ax), cheek, 0.25)
    upper = o.smooth(-grow, 0.006 - grow, o.sub(top, z))
    neck = o.smooth(0.232 - grow, 0.248 - grow, o.sub(z, y))
    back = o.smooth(-grow, 0.007 - grow, o.sub(o.lerp(o.smooth(0.235, 0.26, z), -0.02, -0.046), y))
    lx, lz = o.mul(ax, 1.0 / 0.027), o.mul(o.sub(z, 0.2285), 1.0 / 0.012)
    lips = o.smooth(0.7, 1.15, o.add(o.mul(lx, lx), o.mul(lz, lz)))
    return o.mul(o.mul(upper, neck), o.mul(back, lips))


def load_head():
    names = ['Head', 'Sclera.L', 'Sclera.R', 'Iris.L', 'Iris.R']
    with bpy.data.libraries.load(DATA, link=False) as (source, target):
        target.objects = list(names)
    return dict(zip(names, target.objects))


def shell(source, surface, faces, low, high, reach, thickness=None):
    keep = {face.index for face in faces}
    bm = source.copy()
    bm.faces.index_update()
    bmesh.ops.delete(bm, geom=[face for face in bm.faces if face.index not in keep], context='FACES')
    bmesh.ops.delete(bm, geom=[vertex for vertex in bm.verts if not vertex.link_faces], context='VERTS')
    boundary = [vertex for vertex in bm.verts if vertex.is_boundary]
    for _ in range(14):
        moved = {}
        for vertex in boundary:
            neighbors = [edge.other_vert(vertex) for edge in vertex.link_edges if edge.is_boundary]
            if len(neighbors) == 2:
                moved[vertex] = vertex.co * 0.5 + (neighbors[0].co + neighbors[1].co) * 0.25
        for vertex, co in moved.items():
            location = surface.find_nearest(co)[0]
            vertex.co = location if location is not None else co
    inner = list({edge.other_vert(vertex) for vertex in boundary for edge in vertex.link_edges if not edge.other_vert(vertex).is_boundary})
    for _ in range(2):
        bmesh.ops.smooth_vert(bm, verts=inner, factor=0.4, use_axis_x=True, use_axis_y=True, use_axis_z=True)
        for vertex in inner:
            location = surface.find_nearest(vertex.co)[0]
            if location is not None:
                vertex.co = location
    distance = {vertex: 0.0 for vertex in boundary}
    queue = [(0.0, index, vertex) for index, vertex in enumerate(distance)]
    heapq.heapify(queue)
    counter = len(queue)
    while queue:
        d, _, vertex = heapq.heappop(queue)
        if d > distance.get(vertex, 1e9):
            continue
        for edge in vertex.link_edges:
            other = edge.other_vert(vertex)
            step = d + edge.calc_length()
            if step < distance.get(other, 1e9):
                distance[other] = step
                counter += 1
                heapq.heappush(queue, (step, counter, other))
    bm.normal_update()
    base = bm.verts.layers.float_vector.get('HeadPos')
    cloth = bm.verts.layers.float_vector.new('Cloth')
    offsets = []
    for vertex in bm.verts:
        d = distance.get(vertex, reach)
        amount = lerp(low, high, smoothstep(0.0, reach, d)) * (thickness(vertex[base]) if thickness else 1.0)
        offsets.append(vertex.normal * amount)
        front, elevation = head_angles(vertex[base])
        vertex[cloth] = (1.0, -elevation * 0.11, front * 0.1)
    for vertex, offset in zip(bm.verts, offsets):
        vertex.co += offset
    return bm


WHORL = Vector((-0.014, 0.03, 0.43))
LENS = ((-1.0, 0.0), (-0.5, 0.8), (0.5, 0.8), (1.0, 0.0), (0.5, -0.4), (-0.5, -0.4))


def cap_thickness(p):
    return 0.0015 + 0.0065 * smoothstep(0.0, 1.1, head_angles(p)[1])


def hair_flow(point, normal):
    d = point - SKULL
    elevation = math.atan2(d.z, math.hypot(d.x, d.y))
    front = -d.y / max(math.hypot(d.x, d.y), 1e-6)
    top = smoothstep(0.45, 0.85, elevation) * smoothstep(-0.2, 0.5, front)
    fall = smoothstep(0.85, 0.25, elevation) * (1.0 - 0.6 * smoothstep(0.3, 0.9, front))
    away = (point - WHORL).normalized()
    swirl = normal.cross(away) * (0.6 * math.exp(-((point - WHORL).length / 0.03) ** 2))
    flow = (away + swirl) * (1.0 + 0.5 * (1.0 - top)) + Vector((-0.85, -1.0, -0.1)).normalized() * (1.6 * top) + Vector((0.0, 0.3, -1.0)).normalized() * (1.4 * fall)
    flow -= normal * flow.dot(normal)
    return flow.normalized()


def trace(bvh, start, length, curl=0.0, stop=True, step=0.003):
    point, normal, _, _ = bvh.find_nearest(start)
    points, normals = [point], [normal]
    travelled = 0.0
    while travelled < length:
        flow = hair_flow(point, normal)
        flow = (flow + normal.cross(flow) * (curl * travelled / max(length, 1e-6))).normalized()
        point, normal, _, _ = bvh.find_nearest(point + flow * step)
        if stop and scalp_cover(point) < 0.35:
            break
        points.append(point)
        normals.append(normal)
        travelled += step
    return points, normals


def resample(points, normals, spacing):
    lengths = [0.0]
    for a, b in zip(points, points[1:]):
        lengths.append(lengths[-1] + (b - a).length)
    count = max(2, int(lengths[-1] / spacing) + 1)
    result, k = [], 0
    for i in range(count):
        s = lengths[-1] * i / (count - 1)
        while k < len(points) - 2 and lengths[k + 1] < s:
            k += 1
        f = min(1.0, max(0.0, (s - lengths[k]) / max(lengths[k + 1] - lengths[k], 1e-9)))
        normal = sum((normals[j] for j in range(max(0, k - 2), min(len(normals), k + 4))), Vector()).normalized()
        result.append((points[k].lerp(points[k + 1], f), normal, s))
    return result


def strip(samples, halves, heights):
    centers = [point + normal * height for (point, normal, _), height in zip(samples, heights)]
    bm = bmesh.new()
    cloth = bm.verts.layers.float_vector.new('Cloth')
    rings = []
    last = len(samples) - 1
    for i, ((point, normal, s), center, half) in enumerate(zip(samples, centers, halves)):
        ahead = (centers[min(i + 1, last)] - centers[max(i - 1, 0)]).normalized()
        side = ahead.cross(normal).normalized()
        up = side.cross(ahead).normalized()
        ring = []
        for cx, cy in LENS:
            vertex = bm.verts.new(center + side * (cx * half) + up * (cy * min(half * 0.28, 0.0032)))
            vertex[cloth] = (cx, s, (cx * 0.5 + 0.5) * half * 2.0)
            ring.append(vertex)
        rings.append(ring)
    sides = len(LENS)
    for a, b in zip(rings, rings[1:]):
        for j in range(sides):
            bm.faces.new((a[j], a[(j + 1) % sides], b[(j + 1) % sides], b[j]))
    bm.faces.new(list(reversed(rings[0])))
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges[:], dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def hair_locks(bvh, rng, count=28, spacing=0.007):
    origin, normal, _, _ = bvh.find_nearest(WHORL)
    tangent = normal.cross(Vector((0.0, 0.0, 1.0))).normalized()
    bitangent = normal.cross(tangent).normalized()
    paths, fringes = [], []
    for index in range(count):
        angle = 2.0 * math.pi * (index + rng.uniform(-0.15, 0.15)) / count
        points, normals = trace(bvh, origin + (tangent * math.cos(angle) + bitangent * math.sin(angle)) * 0.002, 0.24)
        end = points[-1] - SKULL
        fringe = -end.y / max(math.hypot(end.x, end.y), 1e-6) > 0.55
        if fringe:
            more, extra = trace(bvh, points[-1], rng.uniform(0.008, 0.02), curl=-0.5, stop=False)
            points, normals = points + more[1:], normals + extra[1:]
        else:
            keep = max(6, int(len(points) * rng.uniform(0.8, 1.0)))
            points, normals = points[:keep], normals[:keep]
        paths.append(resample(points, normals, spacing))
        fringes.append(fringe)
    locks = []
    for index, (samples, fringe) in enumerate(zip(paths, fringes)):
        total = samples[-1][2]
        halves, heights = [], []
        for i, (point, _, s) in enumerate(samples):
            gaps = [(point - other[i][0]).length for other in (paths[index - 1], paths[(index + 1) % count]) if i < len(other)]
            spread = max(gaps) if gaps else 0.012
            tip = 1.0 - smoothstep(total - 0.04, total, s)
            halves.append(min(0.016, max(0.004, 0.62 * spread)) * smoothstep(0.0, 0.025, s) * tip ** 0.7)
            beyond = 1.0 - scalp_cover(point)
            layer = (0.0016 if index % 2 else 0.0032) * (0.25 + 0.75 * smoothstep(-0.3, 0.8, head_angles(point)[1]))
            edge = 1.0 if fringe else smoothstep(0.3, 0.8, scalp_cover(point))
            heights.append((cap_thickness(point) * (1.0 - beyond) + layer) * edge + 0.006 * beyond * smoothstep(total - 0.05, total, s))
        locks.append(strip(samples, halves, heights))
    return locks


def eye(source, side, m, target, rng):
    iris = source[f'Iris.{side}'].data.copy()
    iris.transform(HEAD_FRAME)
    middle = sum((vertex.co for vertex in iris.vertices), Vector()) / len(iris.vertices)
    radius = max((vertex.co - middle).length for vertex in iris.vertices)
    sclera = source[f'Sclera.{side}'].data.copy()
    sclera.transform(HEAD_FRAME)
    bm = bmesh.new()
    bm.from_mesh(sclera)
    cornea = [face for face in bm.faces if face.calc_center_median().y < middle.y + 0.0015 and math.hypot(face.calc_center_median().x - middle.x, face.calc_center_median().z - middle.z) < radius * 1.04]
    bmesh.ops.delete(bm, geom=cornea, context='FACES')
    bm.to_mesh(sclera)
    bm.free()
    attribute = iris.attributes.new('EyePos', 'FLOAT_VECTOR', 'POINT')
    attribute.data.foreach_set('vector', [c for vertex in iris.vertices for c in (vertex.co - middle)])
    parts = []
    for name, mesh, material, ratio in ((f'Hero_Sclera_{side}', sclera, m['sclera'], 0.3), (f'Hero_Iris_{side}', iris, m['iris'], 0.3)):
        mesh.materials.clear()
        mesh.materials.append(material)
        obj = bpy.data.objects.new(name, mesh)
        target.objects.link(obj)
        modifier = obj.modifiers.new('Decimate', 'DECIMATE')
        modifier.ratio = ratio
        kit.apply_modifiers(obj)
        obj.data.shade_smooth()
        pk.tag(obj, rng=rng)
        parts.append(obj)
    glow = pk.part(f'Hero_EyeGlow_{side}', pk.ellipsoid_bm(middle + Vector((0.0, -0.0009, 0.0)), (radius * 0.8, 0.0011, radius * 0.52), segments=16, rings=8), target, m['eyes'], rng=rng, smooth=80.0)
    return parts, glow


def head(m, target, rng):
    source = load_head()
    bm = bmesh.new()
    bm.from_mesh(source['Head'].data)
    region = bm.faces.layers.int.get('Region')
    bmesh.ops.delete(bm, geom=[face for face in bm.faces if face[region] == 36 or face.calc_center_median().z < 0.1 or (face[region] == 35 and math.hypot(face.calc_center_median().x, face.calc_center_median().y - 0.012) > 0.072)], context='FACES')
    bmesh.ops.delete(bm, geom=[vertex for vertex in bm.verts if not vertex.link_faces], context='VERTS')
    base = bm.verts.layers.float_vector.new('HeadPos')
    scalp = bm.verts.layers.float.new('Scalp')
    for vertex in bm.verts:
        vertex[base] = vertex.co.copy()
        vertex[scalp] = 0.0 if any(face[region] in (3, 4) for face in vertex.link_faces) else scalp_cover(vertex.co, fade=0.3)
    bm.normal_update()
    bm.faces.index_update()
    bvh = BVHTree.FromBMesh(bm)

    def origin(face):
        return sum((vertex[base] for vertex in face.verts), Vector()) / len(face.verts)

    cap = shell(bm, bvh, [face for face in bm.faces if hair_amount(origin(face), face[region]) > 0.5], 0.1, 1.0, 0.016, thickness=cap_thickness)
    locks = hair_locks(bvh, rng)
    for vertex in bm.verts:
        vertex.co += vertex.normal * (0.0035 * beard_field(Scalar, *vertex[base]))
    for mesh in [bm, cap] + locks:
        for vertex in mesh.verts:
            vertex.co = HEAD_FRAME @ masculine(vertex.co)
        mesh.normal_update()
    face = kit.mesh_object('Hero_Face', bm, target, m['skin'])
    pk.tag(face, rng=rng)
    face.data.shade_smooth()
    parts = [face]
    hair = [kit.mesh_object('Hero_HairCap', cap, target, m['hair'])]
    for index, lock in enumerate(locks):
        hair.append(kit.mesh_object(f'Hero_HairLock{index}', lock, target, m['hair']))
    for obj in hair:
        pk.tag(obj, rng=rng)
        obj.data.shade_smooth()
    glows = []
    for side in ('L', 'R'):
        pieces, glow = eye(source, side, m, target, rng)
        parts += pieces
        glows.append(glow)
    for obj in source.values():
        bpy.data.objects.remove(obj, do_unlink=True)
    return parts, glows, hair


def hat_hair(hair, target, cut):
    trimmed = []
    for obj in hair:
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1e-5, plane_co=(0.0, 0.0, cut), plane_no=(0.0, 0.0, 1.0), clear_outer=True)
        bmesh.ops.delete(bm, geom=[vertex for vertex in bm.verts if not vertex.link_faces], context='VERTS')
        if not bm.faces:
            bm.free()
            continue
        mesh = bpy.data.meshes.new(obj.name.replace('Hero_Hair', 'Hero_HatHair'))
        bm.to_mesh(mesh)
        bm.free()
        for material in obj.data.materials:
            mesh.materials.append(material)
        copy = bpy.data.objects.new(mesh.name, mesh)
        target.objects.link(copy)
        trimmed.append(copy)
    return trimmed


def hat(m, target, rng):
    parts = []
    base_z = 1.778
    scale = 1.08
    cols = 72
    brim_rings, brim_data = [], []
    for i in range(8):
        t = i / 7
        r = lerp(0.098, 0.3, t) * scale
        ring, values = [], []
        for j in range(cols):
            angle = 2.0 * math.pi * j / cols
            front = max(0.0, -math.sin(angle))
            back = max(0.0, math.sin(angle))
            droop = (-0.062 * t ** 1.5 + 0.016 * math.sin(3.0 * angle + 0.7) * t * t + 0.03 * t * t * front - 0.012 * t * t * back) * scale
            ring.append(Vector((math.cos(angle) * r, math.sin(angle) * r * 0.97 + 0.012, base_z + droop)))
            values.append(((1.0 - t) * 0.22 + 0.002, angle * 0.3, 1.0, 0.0))
        brim_rings.append(ring)
        brim_data.append(values)
    parts.append(cloth_object('Wizard_HatBrim', loft(brim_rings, data=brim_data, flip=True), target, [m['felt'], m['felt']], rng, thickness=0.012, offset=0.0))
    p0, p1, p2, p3 = Vector((0.0, 0.012, base_z)), Vector((0.0, 0.0, base_z + 0.28)), Vector((-0.013, 0.038, base_z + 0.485)), Vector((-0.032, 0.235, base_z + 0.505))

    def center(t):
        return p0 * (1 - t) ** 3 + p1 * 3 * (1 - t) ** 2 * t + p2 * 3 * (1 - t) * t * t + p3 * t ** 3

    rings, data = [], []
    steps, around = 30, 44
    for i in range(steps + 1):
        t = i / steps
        c = center(t)
        frame = (center(min(t + 0.01, 1.0)) - center(max(t - 0.01, 0.0))).normalized().to_track_quat('Z', 'Y').to_matrix()
        radius = (0.103 * (1.0 - t) ** 1.45 + 0.003) * scale
        crease = math.exp(-((t - 0.56) / 0.07) ** 2)
        ring, values = [], []
        for j in range(around):
            angle = 2.0 * math.pi * j / around
            wrinkle = 1.0 + 0.12 * crease * math.sin(4.0 * angle + 1.0) + 0.03 * math.sin(7.0 * angle + t * 9.0) * smoothstep(0.08, 0.3, t) * (1.0 - t)
            ring.append(c + frame @ Vector((math.cos(angle) * radius * wrinkle, math.sin(angle) * radius * wrinkle * 0.97, 0.0)))
            values.append((1.0, angle * 0.2, 1.0, 0.0))
        rings.append(ring)
        data.append(values)
    parts.append(cloth_object('Wizard_HatCrown', loft(rings, data=data), target, [m['felt'], m['felt']], rng, thickness=0.0))
    band_rings = []
    for z in (base_z + 0.006, base_z + 0.046):
        band_rings.append([Vector((math.cos(2.0 * math.pi * j / cols) * 0.1065 * scale, math.sin(2.0 * math.pi * j / cols) * 0.1035 * scale + 0.012, z)) for j in range(cols)])
    parts.append(cloth_object('Wizard_HatBand', loft(band_rings), target, [m['leather'], m['leather']], rng, thickness=0.006, offset=1.0))
    buckle_center = Vector((0.049, -0.108, base_z + 0.026))
    buckle = pk.box((0.044, 0.01, 0.046))
    bmesh.ops.rotate(buckle, verts=buckle.verts, cent=(0.0, 0.0, 0.0), matrix=Matrix.Rotation(-0.42, 3, 'Z'))
    bmesh.ops.translate(buckle, verts=buckle.verts, vec=buckle_center)
    parts.append(pk.part('Wizard_HatBuckle', buckle, target, m['bronze'], rng=rng, bevel=(0.004, 2, 30.0)))
    parts.append(pk.part('Wizard_HatGem', pk.ellipsoid_bm(buckle_center + Vector((0.003, -0.007, 0.0)), (0.012, 0.006, 0.014), segments=12, rings=8), target, m['gem'], rng=rng, smooth=60.0))
    for part in parts:
        part.data.transform(Matrix.Translation((0.0, -0.014, 0.006)))
    return parts


def cloth_material(name, tone, trims=True, ornate=False, bake_size=1024, gold=(0.36, 0.23, 0.08), inset=0.0):
    s = kit.Shader(name)
    p = pk.local(s)
    edge, along, seam = s.separate(pk.local(s, 'Cloth'))
    mark = s.node('ShaderNodeAttribute', attribute_name='Mark', attribute_type='GEOMETRY').outputs['Fac']
    weave = s.mul(s.wave(p, 220.0, kind='BANDS', direction='X', distortion=0.5), s.wave(p, 220.0, kind='BANDS', direction='Z', distortion=0.5))
    mottle = s.noise(p, 6.0, detail=4.0)
    base = s.mix(s.smooth(0.3, 0.75, mottle), (tone[0] * 0.8, tone[1] * 0.8, tone[2] * 0.82), (tone[0] * 1.15, tone[1] * 1.12, tone[2] * 1.1))
    sheen = s.smooth(0.5, 0.85, s.noise(s.stretch(p, (1.0, 1.0, 0.35)), 3.0, detail=3.0))
    base = s.mix(s.mul(sheen, 0.25), base, (tone[0] * 1.6, tone[1] * 1.55, tone[2] * 1.6))
    dirt = pk.cavity(s, 0.06)
    base = s.mix(s.mul(dirt, 0.55), base, (tone[0] * 0.4, tone[1] * 0.38, tone[2] * 0.42))
    rough = s.remap(weave, 0.0, 1.0, 0.72, 0.86)
    height = s.add(s.mul(weave, 0.3), s.mul(mottle, 0.25))
    metal = 0.0
    if trims:
        def line(distance, width):
            return s.sub(1.0, s.smooth(width * 0.55, width, s.math('ABSOLUTE', s.sub(edge, distance))))

        lines = s.math('MAXIMUM', line(0.008 + inset, 0.0028), line(0.019 + inset, 0.0021))
        if ornate:
            field = s.mul(mark, s.mul(s.smooth(0.024, 0.027, edge), s.sub(1.0, s.smooth(0.066, 0.069, edge))))
            cell = s.mul(s.sub(s.math('FRACT', s.mul(along, 1.0 / 0.052)), 0.5), 0.052)
            du = s.math('ABSOLUTE', cell)
            dv = s.math('ABSOLUTE', s.sub(edge, 0.0465))
            diamond = s.add(du, s.mul(dv, 1.2))
            outline = s.sub(1.0, s.smooth(0.0016, 0.003, s.math('ABSOLUTE', s.sub(diamond, 0.0175))))
            dot = s.sub(1.0, s.smooth(0.0035, 0.005, s.math('SQRT', s.add(s.mul(du, du), s.mul(dv, dv)))))
            spark = s.mul(s.sub(1.0, s.smooth(0.001, 0.0022, s.math('ABSOLUTE', s.sub(du, 0.026)))), s.sub(1.0, s.smooth(0.004, 0.008, dv)))
            motif = s.mul(field, s.math('MAXIMUM', s.math('MAXIMUM', outline, dot), spark))
            border = s.mul(mark, line(0.072, 0.0024))
            piping = s.sub(1.0, s.smooth(0.0011, 0.0022, s.math('ABSOLUTE', s.sub(s.math('ABSOLUTE', seam), 0.024))))
            base = s.mix(s.mul(field, 0.6), base, (tone[0] * 1.9, tone[1] * 1.8, tone[2] * 1.7))
            lines = s.math('MAXIMUM', lines, s.math('MAXIMUM', s.math('MAXIMUM', motif, border), piping))
        base = s.mix(lines, base, gold)
        rough = s.lerp(lines, rough, 0.42)
        metal = s.mul(lines, 0.75)
        height = s.add(height, s.mul(lines, 0.35))
    return s.finish(base, rough, metallic=metal, height=height, bump=0.1, bump_distance=0.002, bake_size=bake_size)


def armor_leather_material(name, tone, runes=False, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    edge, along, seam = s.separate(pk.local(s, 'Cloth'))
    grain = s.voronoi(p, 150.0, 'F1', 1.0, 'Distance')
    blotch = s.noise(p, 5.0, detail=4.0)
    base = s.mix(s.smooth(0.35, 0.7, blotch), (tone[0] * 0.62, tone[1] * 0.58, tone[2] * 0.55), tone)
    worn = pk.wear(s, 0.01)
    base = s.mix(s.mul(worn, 0.45), base, (tone[0] * 1.7, tone[1] * 1.55, tone[2] * 1.4))
    rim = s.sub(1.0, s.smooth(0.0015, 0.006, edge))
    base = s.mix(s.mul(rim, 0.6), base, (tone[0] * 0.45, tone[1] * 0.4, tone[2] * 0.38))
    dashes = s.smooth(0.25, 0.6, s.math('ABSOLUTE', s.math('SINE', s.mul(along, 2.0 * math.pi / 0.008))))
    stitch = s.mul(s.sub(1.0, s.smooth(0.0007, 0.0016, s.math('ABSOLUTE', s.sub(edge, 0.0095)))), dashes)
    seam = s.math('ABSOLUTE', seam)
    groove = s.sub(1.0, s.smooth(0.0012, 0.0032, seam))
    seam_dashes = s.smooth(0.25, 0.6, s.math('ABSOLUTE', s.math('SINE', s.mul(s.add(s.separate(p)[0], s.separate(p)[2]), 2.0 * math.pi / 0.008))))
    seam_stitch = s.mul(s.sub(1.0, s.smooth(0.0006, 0.0014, s.math('ABSOLUTE', s.sub(seam, 0.0058)))), seam_dashes)
    stitch = s.math('MAXIMUM', stitch, seam_stitch)
    base = s.mix(s.mul(groove, 0.8), base, (tone[0] * 0.3, tone[1] * 0.28, tone[2] * 0.26))
    base = s.mix(s.mul(stitch, 0.85), base, (0.42, 0.33, 0.21))
    dirt = pk.cavity(s, 0.03)
    base = s.mix(s.mul(dirt, 0.5), base, (0.03, 0.018, 0.012))
    rough = s.remap(blotch, 0.3, 0.7, 0.48, 0.66)
    rough = s.lerp(s.mul(worn, 0.5), rough, 0.38)
    height = s.add(s.mul(grain, 0.25), s.sub(s.mul(stitch, 0.4), s.add(s.mul(groove, 0.9), s.mul(rim, 0.3))))
    glow = None
    if runes:
        period, middle, half, width = 0.026, 0.044, 0.011, 0.0009

        def stroke(distance):
            return s.sub(1.0, s.smooth(width, width * 2.2, s.math('ABSOLUTE', distance)))

        def hashed(value, seed):
            return s.math('FRACT', s.mul(s.math('SINE', s.mul(value, seed)), 43758.5453))

        index = s.math('FLOOR', s.mul(along, 1.0 / period))
        du = s.mul(s.sub(s.math('FRACT', s.mul(along, 1.0 / period)), 0.5), period)
        dv = s.sub(edge, middle)
        inside = s.mul(s.sub(1.0, s.smooth(half * 0.85, half, s.math('ABSOLUTE', dv))), s.sub(1.0, s.smooth(period * 0.36, period * 0.42, s.math('ABSOLUTE', du))))
        slope = s.mul(s.sub(hashed(index, 12.9898), 0.5), 2.6)
        tick = s.mul(s.sub(hashed(index, 78.233), 0.5), half * 1.4)
        side = s.smooth(-0.0004, 0.0004, s.mul(du, s.sub(hashed(index, 37.719), 0.5)))
        stem = stroke(s.add(du, s.mul(s.sub(hashed(index, 4.1414), 0.5), period * 0.3)))
        diagonal = stroke(s.sub(dv, s.mul(du, slope)))
        branch = s.mul(stroke(s.sub(dv, tick)), side)
        glyph = s.mul(inside, s.math('MAXIMUM', s.math('MAXIMUM', stem, diagonal), branch))
        rails = s.math('MAXIMUM', stroke(s.sub(edge, middle - half - 0.006)), stroke(s.sub(edge, middle + half + 0.006)))
        mark = s.math('MAXIMUM', glyph, rails)
        base = s.mix(mark, base, (0.18, 0.42, 0.5))
        glow = s.mul(mark, 0.8)
    return s.finish(base, rough, height=height, bump=0.15, bump_distance=0.002, emission=glow, emission_color=SPIRIT, emission_strength=2.4, bake_size=bake_size)


def plate_material(name, tone, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    edge, along, _ = s.separate(pk.local(s, 'Cloth'))
    grain = s.noise(p, 14.0, detail=3.0)
    base = s.mix(grain, (tone[0] * 0.82, tone[1] * 0.8, tone[2] * 0.78), tone)
    pits = pk.cavity(s, 0.03)
    base = s.mix(s.mul(pits, 0.7), base, (0.07, 0.06, 0.035))
    worn = pk.wear(s, 0.008)
    base = s.mix(s.mul(worn, 0.55), base, (min(1.0, tone[0] * 1.35), min(1.0, tone[1] * 1.3), min(1.0, tone[2] * 1.25)))
    rim = s.sub(1.0, s.smooth(0.0035, 0.0055, edge))
    groove = s.math('MAXIMUM', s.sub(1.0, s.smooth(0.0006, 0.0013, s.math('ABSOLUTE', s.sub(edge, 0.0082)))), s.sub(1.0, s.smooth(0.0006, 0.0013, s.math('ABSOLUTE', s.sub(edge, 0.0205)))))
    cell = s.mul(s.sub(s.math('FRACT', s.mul(along, 1.0 / 0.028)), 0.5), 0.028)
    rivet = s.sub(1.0, s.smooth(0.0022, 0.0034, s.math('SQRT', s.add(s.mul(cell, cell), s.mul(s.sub(edge, 0.0142), s.sub(edge, 0.0142))))))
    scroll = s.mul(s.smooth(0.026, 0.032, edge), s.sub(1.0, s.smooth(0.0, 0.035, s.math('ABSOLUTE', s.sub(s.noise(s.stretch(p, (1.0, 1.0, 1.0)), 24.0, detail=2.0, distortion=1.2), 0.5)))))
    engraving = s.math('MAXIMUM', groove, s.mul(scroll, 0.85))
    base = s.mix(s.mul(engraving, 0.75), base, (0.05, 0.035, 0.018))
    base = s.mix(s.mul(rivet, 0.6), base, (min(1.0, tone[0] * 1.4), min(1.0, tone[1] * 1.35), min(1.0, tone[2] * 1.3)))
    rough = s.remap(grain, 0.3, 0.7, 0.3, 0.44)
    rough = s.lerp(engraving, rough, 0.6)
    metal = s.lerp(s.math('MAXIMUM', pits, engraving), 1.0, 0.45)
    height = s.add(s.sub(s.mul(rim, 0.6), s.mul(engraving, 0.8)), s.add(s.mul(rivet, 0.9), s.mul(grain, 0.1)))
    return s.finish(base, rough, height=height, bump=0.25, bump_distance=0.0015, metallic=metal, bake_size=bake_size)


def skin_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    hx, hy, hz = s.separate(pk.local(s, 'HeadPos'))
    ax = s.math('ABSOLUTE', hx)

    def spot(cx, cy, cz, radius):
        return s.sub(1.0, s.smooth(0.0, radius, s.vector('LENGTH', s.combine(s.sub(ax, cx), s.sub(hy, cy), s.sub(hz, cz)))))

    mottle = s.noise(p, 40.0, detail=4.0)
    blotch = s.noise(p, 9.0, detail=3.0)
    base = s.mix(s.smooth(0.3, 0.7, s.add(s.mul(mottle, 0.5), s.mul(blotch, 0.5))), (0.45, 0.28, 0.2), (0.56, 0.37, 0.28))
    red = s.math('MAXIMUM', s.math('MAXIMUM', spot(0.052, -0.09, 0.272, 0.032), spot(0.0, -0.158, 0.266, 0.022)), spot(0.083, -0.02, 0.29, 0.03))
    base = s.mix(s.mul(red, 0.45), base, (0.55, 0.25, 0.19))
    lip_shape = s.vector('LENGTH', s.combine(s.mul(ax, 1.0 / 0.026), 0.0, s.mul(s.sub(hz, 0.2285), 1.0 / 0.0115)))
    lips = s.mul(s.sub(1.0, s.smooth(0.75, 1.0, lip_shape)), s.sub(1.0, s.smooth(-0.13, -0.11, hy)))
    base = s.mix(s.mul(lips, 0.8), base, (0.4, 0.17, 0.14))
    grain = s.noise(s.combine(s.mul(hx, 900.0), s.mul(hy, 900.0), s.mul(hz, 220.0)), 1.0, detail=2.0)
    arch = s.sub(0.3395, s.mul(s.mul(s.sub(ax, 0.032), s.sub(ax, 0.032)), 3.2))
    thick = s.remap(ax, 0.012, 0.062, 0.0085, 0.0042)
    band = s.sub(1.0, s.smooth(0.4, 1.0, s.math('DIVIDE', s.math('ABSOLUTE', s.sub(hz, arch)), thick)))
    span = s.mul(s.smooth(0.008, 0.017, ax), s.sub(1.0, s.smooth(0.054, 0.066, ax)))
    strokes = s.smooth(0.3, 0.7, s.noise(s.combine(s.mul(s.add(ax, s.mul(hz, 0.4)), 700.0), s.mul(hz, 160.0), 0.0), 2.0, detail=2.0))
    brows = s.mul(s.mul(band, span), s.mul(s.sub(1.0, s.smooth(-0.075, -0.06, hy)), s.add(0.62, s.mul(strokes, 0.38))))
    base = s.mix(brows, base, (HAIR_TONE[0] * 0.6, HAIR_TONE[1] * 0.6, HAIR_TONE[2] * 0.6))
    scalp = s.node('ShaderNodeAttribute', attribute_name='Scalp', attribute_type='GEOMETRY').outputs['Fac']
    shadow = s.math('MAXIMUM', beard_field(s, hx, hy, hz, grow=0.004), s.mul(s.smooth(0.05, 0.9, scalp), 1.6))
    base = s.mix(s.mul(shadow, s.add(0.12, s.mul(grain, 0.14))), base, (0.15, 0.12, 0.11))
    beard = beard_field(s, hx, hy, hz)
    coverage = s.smooth(0.22, 0.62, s.mul(beard, s.add(0.42, s.mul(grain, 0.8))))
    tone = s.mix(s.smooth(0.3, 0.75, grain), (HAIR_TONE[0] * 0.55, HAIR_TONE[1] * 0.55, HAIR_TONE[2] * 0.55), (HAIR_TONE[0] * 1.5, HAIR_TONE[1] * 1.45, HAIR_TONE[2] * 1.4))
    base = s.mix(coverage, base, tone)
    dirt = pk.cavity(s, 0.012)
    base = s.mix(s.mul(dirt, 0.45), base, (0.2, 0.1, 0.08))
    rough = s.lerp(coverage, s.lerp(lips, s.remap(mottle, 0.3, 0.7, 0.42, 0.56), 0.36), 0.62)
    height = s.add(s.add(s.mul(s.voronoi(p, 900.0, 'F1', 1.0, 'Distance'), 0.3), s.mul(mottle, 0.2)), s.mul(s.add(coverage, s.mul(grain, beard)), 0.5))
    return s.finish(base, rough, height=height, bump=0.06, bump_distance=0.0005, specular=0.45, bake_size=bake_size)


def sclera_material(name):
    s = kit.Shader(name)
    veins = s.smooth(0.62, 0.75, s.noise(pk.local(s), 300.0, detail=3.0))
    return s.finish(s.mix(s.mul(veins, 0.25), (0.7, 0.66, 0.62), (0.6, 0.3, 0.28)), 0.12, bake_size=128)


def iris_material(name, color=(0.1, 0.15, 0.2)):
    s = kit.Shader(name)
    x, _, z = s.separate(pk.local(s, 'EyePos'))
    radius = s.vector('LENGTH', s.combine(x, 0.0, z))
    streaks = s.noise(s.combine(s.mul(s.math('ARCTAN2', z, x), 6.0), s.mul(radius, 300.0), 0.0), 3.0, detail=3.0)
    base = s.mix(s.smooth(0.3, 0.7, streaks), (color[0] * 0.5, color[1] * 0.5, color[2] * 0.5), (color[0] * 1.6, color[1] * 1.6, color[2] * 1.6))
    base = s.mix(s.mul(s.smooth(0.0062, 0.0076, radius), 0.8), base, (0.01, 0.012, 0.015))
    base = s.mix(s.sub(1.0, s.smooth(0.0026, 0.0031, radius)), base, (0.004, 0.004, 0.004))
    return s.finish(base, 0.08, bake_size=128)


def hair_material(name, tone, bake_size=1024):
    s = kit.Shader(name)
    seed = s.separate(pk.local(s))[0]
    edge, along, across = s.separate(pk.local(s, 'Cloth'))
    strands = s.add(s.mul(s.noise(s.combine(s.mul(across, 260.0), s.mul(along, 12.0), 0.0), 2.0, detail=4.0, w=seed), 0.7), s.mul(s.noise(s.combine(s.mul(across, 900.0), s.mul(along, 30.0), 0.0), 2.0, detail=2.0, w=seed), 0.3))
    base = s.mix(s.smooth(0.3, 0.7, strands), (tone[0] * 0.55, tone[1] * 0.55, tone[2] * 0.55), (tone[0] * 1.6, tone[1] * 1.55, tone[2] * 1.5))
    ridge = s.sub(1.0, s.smooth(0.2, 0.9, s.math('ABSOLUTE', edge)))
    base = s.mix(s.mul(ridge, 0.35), base, (tone[0] * 2.4, tone[1] * 2.3, tone[2] * 2.2))
    base = s.mix(s.mul(s.sub(1.0, s.smooth(0.0, 0.018, along)), 0.45), base, (tone[0] * 0.45, tone[1] * 0.45, tone[2] * 0.45))
    base = s.mix(s.mul(pk.cavity(s, 0.015), 0.6), base, (tone[0] * 0.3, tone[1] * 0.3, tone[2] * 0.3))
    rough = s.remap(strands, 0.0, 1.0, 0.38, 0.6)
    return s.finish(base, rough, height=strands, bump=0.3, bump_distance=0.0012, bake_size=bake_size)


def shade_material(name):
    s = kit.Shader(name)
    p = pk.local(s)
    mottle = s.noise(p, 9.0, detail=4.0)
    glow = s.node('ShaderNodeAttribute', attribute_name='Glow', attribute_type='GEOMETRY').outputs['Fac']
    base = s.mix(s.smooth(0.3, 0.7, mottle), (0.004, 0.003, 0.008), (0.012, 0.008, 0.022))
    base = s.mix(s.mul(glow, 0.5), base, (0.12, 0.3, 0.38))
    rough = s.remap(mottle, 0.3, 0.7, 0.55, 0.75)
    return s.finish(base, rough, height=mottle, bump=0.05, bump_distance=0.003, emission=s.mul(glow, 0.7), emission_color=SPIRIT, emission_strength=1.8, bake_size=512)


def eye_glow_material(name):
    s = kit.Shader(name)
    p = pk.local(s)
    glow = s.remap(s.noise(p, 60.0, detail=2.0), 0.3, 0.7, 0.85, 1.0)
    return s.finish((0.75, 0.95, 1.0), 0.3, emission=glow, emission_color=SPIRIT, emission_strength=9.0, bake_size=64)


def materials():
    tan = (0.13, 0.086, 0.044)
    leather = (0.075, 0.043, 0.024)
    bronze = (0.34, 0.225, 0.1)
    return {
        'cloth': cloth_material('Garb', (0.017, 0.018, 0.028), ornate=True, bake_size=2048),
        'lining': cloth_material('Lining', (0.07, 0.012, 0.01), trims=False, bake_size=512),
        'under': cloth_material('Underlayer', (0.016, 0.014, 0.018), trims=False, bake_size=256),
        'hood': cloth_material('HoodCloth', tan, bake_size=1024, gold=(0.42, 0.29, 0.12), inset=0.008),
        'hood_lining': cloth_material('HoodLining', (0.012, 0.009, 0.008), trims=False, bake_size=256),
        'felt': cloth_material('Felt', (0.018, 0.016, 0.028), bake_size=512),
        'armor': armor_leather_material('ArmorLeather', leather, bake_size=1024),
        'runes': armor_leather_material('RuneLeather', leather, runes=True, bake_size=256),
        'leather': pk.leather_material('HeroLeather', tone=leather, bake_size=512),
        'plate': plate_material('Bronze', bronze, bake_size=1024),
        'bronze': pk.brass_material('HeroBronze', tone=bronze, bake_size=512),
        'gem': pk.glow_material('SpiritGem', color=SPIRIT, strength=4.0, base=(0.5, 0.85, 1.0), bake_size=64),
        'paper': pk.paper_material('HeroPaper', bake_size=256),
        'skin': skin_material('Skin'),
        'sclera': sclera_material('Sclera'),
        'iris': iris_material('Iris'),
        'hair': hair_material('Hair', HAIR_TONE),
        'shade': shade_material('Shade'),
        'eyes': eye_glow_material('SpiritEyes'),
    }


def bone_heat(obj, rig, exclude=()):
    bones = rig.data.bones
    for name in exclude:
        bones[name].use_deform = False
    kit.select_only([obj, rig], active=rig)
    with bpy.context.temp_override(active_object=rig, object=rig, selected_objects=[obj, rig], selected_editable_objects=[obj, rig]):
        bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    for name in exclude:
        bones[name].use_deform = True
    names = {group.index: group.name for group in obj.vertex_groups}
    return [{names[g.group]: g.weight for g in vertex.groups if g.weight > 0.0} for vertex in obj.data.vertices]


class WeightSource:
    def __init__(self, obj, table):
        mesh = obj.data
        self.verts = [vertex.co.copy() for vertex in mesh.vertices]
        self.polys = [tuple(poly.vertices) for poly in mesh.polygons]
        self.bvh = BVHTree.FromPolygons(self.verts, self.polys)
        self.table = table

    def sample(self, co):
        location, _, index, _ = self.bvh.find_nearest(co)
        poly = self.polys[index]
        result = {}
        for k, factor in zip(poly, interpolate.poly_3d_calc([self.verts[k] for k in poly], location)):
            for name, weight in self.table[k].items():
                result[name] = result.get(name, 0.0) + weight * factor
        return result


def assign(obj, rig, weights):
    obj.vertex_groups.clear()
    groups = {}
    for index, entry in enumerate(weights):
        items = [(name, weight) for name, weight in sorted(entry.items(), key=lambda item: -item[1])[:4] if weight > 0.015]
        total = sum(weight for _, weight in items)
        if total <= 0.0:
            items, total = [('hips', 1.0)], 1.0
        for name, weight in items:
            if name not in groups:
                groups[name] = obj.vertex_groups.new(name=name)
            groups[name].add([index], weight / total, 'REPLACE')
    for modifier in [modifier for modifier in obj.modifiers if modifier.type == 'ARMATURE']:
        obj.modifiers.remove(modifier)
    obj.parent = rig
    obj.matrix_parent_inverse = Matrix.Identity(4)
    modifier = obj.modifiers.new('Armature', 'ARMATURE')
    modifier.object = rig


def skirt_weight(center):
    def rule(co):
        leg = 0.72 * smoothstep(1.02, 0.5, co.z)
        shin = 0.3 * smoothstep(0.62, 0.42, co.z)
        left = 0.5 if center else 0.5 + 0.5 * max(-1.0, min(1.0, co.x / 0.12))
        weights = {'hips': 1.0 - leg, 'spine': 0.25 * smoothstep(1.0, 1.06, co.z)}
        for side, share in (('l', left), ('r', 1.0 - left)):
            weights[f'upperleg.{side}'] = leg * share * (1.0 - shin)
            weights[f'lowerleg.{side}'] = leg * share * shin
        return weights
    return rule


def cape_weight(source):
    centers = [0.5 * (a[0] + b[0]) for a, b in zip(CAPE, CAPE[1:])]

    def rule(co):
        chain = {}
        if co.z >= centers[0]:
            chain[CAPE_BONES[0]] = 1.0
        elif co.z <= centers[-1]:
            chain[CAPE_BONES[-1]] = 1.0
        else:
            for k in range(len(centers) - 1):
                if centers[k] >= co.z >= centers[k + 1]:
                    t = (centers[k] - co.z) / (centers[k] - centers[k + 1])
                    chain[CAPE_BONES[k]] = 1.0 - t
                    chain[CAPE_BONES[k + 1]] = t
        attach = smoothstep(1.3, 1.45, co.z)
        weights = {name: weight * (1.0 - attach) for name, weight in chain.items()}
        for name, weight in shoulder_bias(0.0)(source.sample(co)).items():
            weights[name] = weights.get(name, 0.0) + weight * attach
        return weights
    return rule


def rigid(bone, blend=None):
    def rule(co):
        weights = {bone: 1.0}
        if blend:
            weights.update(blend(co))
        return weights
    return rule


def fixed(weights):
    return lambda co: dict(weights)


def shoulder_bias(keep):
    def bias(weights):
        for side, _ in SIDES:
            arm = weights.pop(f'upperarm.{side}', 0.0)
            if arm * keep > 0.0:
                weights[f'upperarm.{side}'] = arm * keep
            weights[f'shoulder.{side}'] = weights.get(f'shoulder.{side}', 0.0) + arm * (1.0 - keep)
        return weights
    return bias


def skin(rig, parts):
    body_obj = next(obj for obj in parts if obj.name == 'Hero_Body')
    table = bone_heat(body_obj, rig, exclude=CAPE_BONES)
    source = WeightSource(body_obj, table)
    assign(body_obj, rig, table)

    def neck_blend(co):
        return {'neck': 1.6 * (1.0 - smoothstep(1.55, 1.64, co.z)), 'chest': 1.8 * (1.0 - smoothstep(1.48, 1.55, co.z))}

    def hand_rule(side):
        def rule(co):
            grip = smoothstep(0.735, 0.78, abs(co.x))
            return {f'hand.{side}': grip, f'lowerarm.{side}': 1.0 - grip}
        return rule

    def hood_rule(co):
        cover = smoothstep(1.56, 1.65, co.z)
        weights = {name: weight * (1.0 - cover) for name, weight in shoulder_bias(0.0)(source.sample(co)).items()}
        weights['head'] = weights.get('head', 0.0) + cover
        return weights

    def cowl_rule(co):
        neck = 0.4 * smoothstep(1.49, 1.57, co.z)
        return {'neck': neck, 'chest': 1.0 - neck}

    def hip_rule(co):
        return {'hips': 1.0, 'spine': 0.3 * smoothstep(1.0, 1.06, co.z)}

    rules = {
        'Wizard_Tabard': skirt_weight(True), 'Wizard_Skirt_L': skirt_weight(False), 'Wizard_Skirt_R': skirt_weight(False),
        'Wizard_Cape': cape_weight(source), 'Wizard_Hood': hood_rule, 'Wizard_Cowl': cowl_rule, 'Wizard_Collar': cowl_rule,
        'Hero_Face': rigid('head', neck_blend), 'Wizard_Hat': rigid('head'),
        'Wizard_ChestPlate': rigid('chest'), 'Wizard_ChestGem': rigid('chest'), 'Wizard_ChestBezel': rigid('chest'),
    }
    for side, _ in SIDES:
        for part in ('Sclera', 'Iris', 'EyeGlow'):
            rules[f'Hero_{part}_{side.upper()}'] = rigid('head')
        rules[f'Hero_Hand_{side.upper()}'] = hand_rule(side)
        rules[f'Wizard_Glove_{side.upper()}'] = hand_rule(side)
        rules[f'Wizard_Bracer_{side.upper()}'] = rigid(f'lowerarm.{side}')
        rules[f'Wizard_BracerBand_{side.upper()}'] = rigid(f'lowerarm.{side}')
        rules[f'Wizard_PauldronCap_{side.upper()}'] = fixed({f'shoulder.{side}': 0.85, f'upperarm.{side}': 0.15})
        rules[f'Wizard_PauldronLame1_{side.upper()}'] = fixed({f'shoulder.{side}': 0.45, f'upperarm.{side}': 0.55})
        rules[f'Wizard_PauldronLame2_{side.upper()}'] = fixed({f'shoulder.{side}': 0.2, f'upperarm.{side}': 0.8})
    post = {'Wizard_Cuirass': shoulder_bias(0.0), 'Wizard_Baldric': shoulder_bias(0.0), 'Wizard_BaldricPlate': shoulder_bias(0.0)}
    for obj in parts:
        if obj is body_obj:
            continue
        name = obj.name.split('.')[0]
        rule = rules.get(name)
        if rule is None and (name.startswith('Wizard_Book') or name.startswith('Wizard_Scroll') or name.startswith('Wizard_Belt') or name.startswith('Wizard_Buckle')):
            rule = hip_rule
        if rule is None and (name.startswith('Wizard_Hat') or name.startswith('Hero_Hair') or name.startswith('Hero_HatHair')):
            rule = rigid('head')
        weights = []
        for vertex in obj.data.vertices:
            co = obj.matrix_world @ vertex.co
            entry = rule(co) if rule else source.sample(co)
            weights.append(post[name](entry) if name in post else entry)
        assign(obj, rig, weights)


def relax(rig):
    pose = rig.pose.bones
    for side, s in SIDES:
        for name, rotation in ((f'upperarm.{side}', (-1.25, 0.0, 0.0)), (f'lowerarm.{side}', (-0.1, 0.0, -s * 0.4))):
            bone = pose[name]
            bone.rotation_mode = 'XYZ'
            bone.rotation_euler = rotation
    bpy.context.view_layer.update()


def build(target):
    rng = random.Random(61)
    m = materials()
    rig = build_rig(target)
    garb = [cuirass(m, target, rng), collar(m, target, rng)]
    garb += baldric(m, target, rng)
    garb += belt(m, target, rng)
    garb += skirt(m, target, rng)
    garb += legs(m, target, rng)
    garb += book(m, target, rng)
    garb += scroll_case(m, target, rng)
    for side, s in SIDES:
        garb.append(sleeve(m, target, rng, side, s))
        garb += bracer(m, target, rng, side, s, m['runes'] if s > 0.0 else m['armor'])
        garb.append(fist(target, m['leather'], f'Wizard_Glove_{side.upper()}', rng, s, inflate=0.003))
    armor = pauldrons(m, target, rng) + chest_plate(m, target, rng)
    mantle = [cape(m, target, rng)]
    face, glows, hair = head(m, target, rng)
    capped = hat_hair(hair, target, 1.798)
    hands = [fist(target, m['shade'], f'Hero_Hand_{side.upper()}', rng, s) for side, s in SIDES]
    shadow = body(m, target, rng)
    hooded = [hood(m, target, rng)]
    lowered = [cowl(m, target, rng)]
    hatted = hat(m, target, rng)
    skin(rig, [shadow] + face + glows + hair + capped + hands + garb + armor + mantle + hooded + lowered + hatted)
    slots = {
        'Hero_Body': shadow,
        'Hero_Head': join(face, 'Hero_Head'),
        'Hero_EyeGlow': join(glows, 'Hero_EyeGlow'),
        'Hero_Hair': join(hair, 'Hero_Hair'),
        'Hero_HatHair': join(capped, 'Hero_HatHair'),
        'Hero_Hands': join(hands, 'Hero_Hands'),
        'Wizard_Garb': join(garb, 'Wizard_Garb'),
        'Wizard_Armor': join(armor, 'Wizard_Armor'),
        'Wizard_Cape': join(mantle, 'Wizard_Cape'),
        'Wizard_Hood': join(hooded, 'Wizard_Hood'),
        'Wizard_Cowl': join(lowered, 'Wizard_Cowl'),
        'Wizard_Hat': join(hatted, 'Wizard_Hat'),
    }
    return rig, slots


def shot(path, center, yaw, pitch, height, size=(720, 960), samples=48, fov=30.0):
    scene = bpy.context.scene
    camera_data = bpy.data.cameras.new('PreviewCamera')
    camera_data.angle = math.radians(fov)
    camera_data.sensor_fit = 'VERTICAL'
    camera = bpy.data.objects.new('PreviewCamera', camera_data)
    scene.collection.objects.link(camera)
    yaw_r, pitch_r = math.radians(yaw), math.radians(pitch)
    direction = Vector((math.cos(pitch_r) * math.sin(yaw_r), -math.cos(pitch_r) * math.cos(yaw_r), math.sin(pitch_r)))
    camera.location = Vector(center) + direction * (height * 0.5 / math.tan(math.radians(fov) * 0.5))
    camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = camera
    bpy.context.view_layer.update()
    scene.render.resolution_x, scene.render.resolution_y = size
    scene.render.resolution_percentage = 100
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera)


def preview(folder, prefix='hero'):
    os.makedirs(folder, exist_ok=True)
    scene = bpy.context.scene
    scene.view_settings.view_transform = 'Standard'
    kit.environment('forest.exr', strength=1.0, rotation=1.2)
    for name, yaw, pitch in (('front', 0.0, 6.0), ('three', -35.0, 12.0), ('side', -90.0, 4.0), ('backthree', 145.0, 10.0)):
        shot(os.path.join(folder, f'{prefix}_{name}.png'), (0.0, 0.0, 1.12), yaw, pitch, 2.45)
    for name, yaw, pitch in (('face', -20.0, 6.0), ('facefront', 0.0, 2.0), ('faceside', -90.0, 2.0), ('faceback', 150.0, 8.0)):
        shot(os.path.join(folder, f'{prefix}_{name}.png'), (0.0, -0.01, 1.7), yaw, pitch, 0.5, size=(640, 720))
    shot(os.path.join(folder, f'{prefix}_game.png'), (0.0, 0.0, 0.95), 0.0, 57.0, 2.6)


def export(rig, slots, folder):
    objects = list(slots.values())
    pk.unwrap_all(objects, {'SpiritEyes': 0.02, 'SpiritGem': 0.02, 'HeroBronze': 0.01, 'HeroPaper': 0.01, 'Underlayer': 0.01, 'RuneLeather': 0.01, 'HoodLining': 0.01})
    heroanim.build_actions(rig)
    os.makedirs(folder, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, NAME + '.blend'), compress=True)
    kit.bake_and_export(objects, folder, NAME, ao_distance=0.3, armature=rig, animations=True)


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    rig, slots = build(target)
    if args.get('--out'):
        export(rig, slots, args['--out'])
    if args.get('--preview'):
        headwear = args.get('--headwear', 'hood')
        extra = {'hood': {'Wizard_Hood', 'Hero_EyeGlow', 'Hero_Hair'}, 'hat': {'Wizard_Cowl', 'Wizard_Hat', 'Hero_HatHair'}, 'bare': {'Wizard_Cowl', 'Hero_Hair'}}[headwear]
        shown = {'Hero_Head', 'Wizard_Garb', 'Wizard_Armor', 'Wizard_Cape'} | extra
        for name, obj in slots.items():
            obj.hide_render = name not in shown
        pk.ground_preview(4.0)
        if args.get('--pose', 'relaxed') == 'relaxed':
            relax(rig)
        preview(args['--preview'], headwear)


if __name__ == '__main__':
    main()
