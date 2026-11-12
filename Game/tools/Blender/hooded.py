import bpy
import bmesh
import math
import os
import sys
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import heroanim

NAME = 'Hero'
SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'HoodedMage.glb')
HEIGHT = 1.87
FACES = 60000
WELD = 0.0002
TEXTURE = 2048
SIDES = (('l', 1.0), ('r', -1.0))
AXIS = (0.0, 0.02)
HEAD_AXIS = (0.0, 0.01)
ARM_LEFT = ((0.06, 0.02, 1.5), (0.19, 0.025, 1.46), (0.33, 0.03, 1.27), (0.44, 0.005, 1.15), (0.48, -0.045, 1.03), (0.545, -0.04, 0.93))
LEG_LEFT = ((0.09, 0.02, 0.95), (0.15, 0.02, 0.52), (0.19, 0.03, 0.1), (0.215, -0.1, 0.03), (0.225, -0.18, 0.02))
ARMS = {side: [np.array((s * p[0], p[1], p[2])) for p in ARM_LEFT] for side, s in SIDES}
LEGS = {side: [np.array((s * p[0], p[1], p[2])) for p in LEG_LEFT] for side, s in SIDES}
ROBE_CHAINS = 8
ROBE_ROWS = (0.98, 0.62, 0.27)
ROBE_JOINTS = []
FACE_BOX = ((-0.085, 0.085), (-0.2, -0.04), (1.58, 1.8))
CAPE_SIDE = 100.0
CAPE_CREASE = 32.0
CAPE_Z = (0.15, 1.36)
CAPE_TOP = CAPE_Z[1]
LEG_REACH = ((0.0, 0.15, 0.25, 0.36, 0.5), (0.17, 0.15, 0.1, 0.085, 0.075))
LEG_TOP = (0.44, 0.52)
LEG_FOLLOW = (0.5, 0.12, 0.35)
LEG_FOLLOW_TOP = (0.85, 1.03)
LEG_FOLLOW_SPLIT = 0.14
MASKS = ('Hand',)
EMISSIVE = {}


def configure(**values):
    globals().update(values)
    globals()['ARMS'] = {side: [np.array((s * p[0], p[1], p[2])) for p in ARM_LEFT] for side, s in SIDES}
    globals()['LEGS'] = {side: [np.array((s * p[0], p[1], p[2])) for p in LEG_LEFT] for side, s in SIDES}
    globals()['CAPE_TOP'] = CAPE_Z[1]


def smoothstep(low, high, value):
    t = np.clip((np.asarray(value, dtype=np.float64) - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def unit(v):
    v = np.asarray(v, dtype=np.float64)
    return v / max(np.linalg.norm(v), 1e-12)


def orth(v, axis):
    v = np.asarray(v, dtype=np.float64)
    return unit(v - axis * float(np.dot(v, axis)))


def blend(a, b, factor):
    result = {name: weight * (1.0 - factor) for name, weight in a.items()}
    for name, weight in b.items():
        result[name] = result.get(name, 0.0) + weight * factor
    return result


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
    co = np.zeros(len(obj.data.vertices) * 3)
    obj.data.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    low, high = co.min(axis=0), co.max(axis=0)
    scale = HEIGHT / (high[2] - low[2])
    co = (co - np.array(((low[0] + high[0]) * 0.5, 0.0, low[2]))) * scale
    obj.data.vertices.foreach_set('co', co.ravel())
    obj.data.update()
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    before = len(bm.verts)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=WELD)
    print("welded", before - len(bm.verts), "seam vertices")
    bm.to_mesh(obj.data)
    bm.free()
    obj.name = 'HoodedSource'
    print('source', len(obj.data.polygons), 'faces, scale', round(float(scale), 4))
    return obj


def decimate(obj, faces):
    co = np.zeros(len(obj.data.vertices) * 3)
    obj.data.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    (x0, x1), (y0, y1), (z0, z1) = FACE_BOX
    face = (co[:, 0] > x0) & (co[:, 0] < x1) & (co[:, 1] > y0 - 0.03) & (co[:, 1] < y1) & (co[:, 2] > z0) & (co[:, 2] < z1)
    hands = (np.abs(co[:, 0]) > 0.4) & (co[:, 2] < 1.16) & (co[:, 2] > 0.85)
    group = obj.vertex_groups.new(name='Keep')
    for index, weight in enumerate(np.where(face | hands, 0.25, 1.0).tolist()):
        group.add([index], weight, 'REPLACE')
    modifier = obj.modifiers.new('Decimate', 'DECIMATE')
    modifier.ratio = min(1.0, faces / max(len(obj.data.polygons), 1))
    modifier.vertex_group = 'Keep'
    modifier.vertex_group_factor = 1.0
    modifier.use_symmetry = True
    modifier.symmetry_axis = 'X'
    kit.apply_modifiers(obj)
    obj.vertex_groups.clear()
    obj.data.shade_smooth()
    print('decimated to', len(obj.data.polygons), 'faces')


def chain(point, joints, names, length=0.05):
    best = None
    for index in range(len(names)):
        a, b = joints[index], joints[index + 1]
        ab = b - a
        span = float(np.linalg.norm(ab))
        t = float(np.clip((point - a) @ ab / (span * span), 0.0, 1.0))
        distance = float(np.linalg.norm(point - (a + ab * t)))
        if best is None or distance < best[0] - 1e-9:
            best = (distance, index, t * span, span)
    distance, index, along, span = best
    weights = {names[index]: 1.0}
    if along < length and index > 0:
        share = 0.5 * (1.0 - along / length)
        weights = {names[index]: 1.0 - share, names[index - 1]: share}
    elif span - along < length and index + 1 < len(names):
        share = 0.5 * (1.0 - (span - along) / length)
        weights = {names[index]: 1.0 - share, names[index + 1]: share}
    return weights, index, along, distance


def torso_weights(point):
    x, y, z = point
    hips = 1.0 - float(smoothstep(0.98, 1.26, z))
    chest = float(smoothstep(1.14, 1.46, z))
    return {'hips': hips, 'spine': max(0.0, 1.0 - hips - chest), 'chest': chest}


def upper_weights(point):
    x, y, z = point
    reach = math.hypot(x - HEAD_AXIS[0], y - HEAD_AXIS[1])
    core = float(smoothstep(1.58, 1.62, z)) * float(smoothstep(0.14, 0.11, reach))
    head = max(core, float(smoothstep(1.5, 1.68, z)) * float(smoothstep(0.3, 0.22, reach)))
    neck = 0.5 * (1.0 - head) * float(smoothstep(1.44, 1.56, z)) * float(smoothstep(0.32, 0.24, reach))
    return {'head': head, 'neck': neck, 'chest': max(0.0, 1.0 - head - neck)}


def robe_weights(point):
    x, y, z = point
    angle = math.atan2(x - AXIS[0], -(y - AXIS[1])) % (2.0 * math.pi)
    position = angle / (2.0 * math.pi) * ROBE_CHAINS
    first = int(position) % ROBE_CHAINS
    t = position - int(position)
    lower = float(smoothstep(ROBE_ROWS[1] + 0.06, ROBE_ROWS[1] - 0.06, z))
    top = float(smoothstep(0.93, 1.03, z))
    weights = {'hips': top}
    for index, amount in ((first, 1.0 - t), ((first + 1) % ROBE_CHAINS, t)):
        weights[f'robe.{index}.1'] = weights.get(f'robe.{index}.1', 0.0) + amount * (1.0 - lower) * (1.0 - top)
        weights[f'robe.{index}.2'] = weights.get(f'robe.{index}.2', 0.0) + amount * lower * (1.0 - top)
    return weights


def arm_weights(point, side):
    joints = ARMS[side]
    names = [f'{name}.{side}' for name in ('upperarm', 'lowerarm', 'wrist', 'hand')]
    weights, index, along, distance = chain(point, joints[1:], names)
    if index == 0:
        keep = float(smoothstep(0.03, 0.14, along))
        weights = {name: weight * keep for name, weight in weights.items()}
        weights[f'shoulder.{side}'] = weights.get(f'shoulder.{side}', 0.0) + 0.75 * (1.0 - keep)
        weights['chest'] = weights.get('chest', 0.0) + 0.25 * (1.0 - keep)
    return weights, index, distance


def leg_weights(point, side):
    hip, knee, ankle, ball, toe = LEGS[side]
    names = [f'{name}.{side}' for name in ('lowerleg', 'foot', 'toes')]
    weights, index, along, distance = chain(point, (hip, knee, ankle, ball, toe), [f'upperleg.{side}'] + names, length=0.05)
    if index == 0:
        weights = blend(weights, {'hips': 1.0}, 0.6 * float(smoothstep(0.12, 0.0, along)))
    return weights, distance


def field(point):
    x, y, z = point
    entry = robe_weights(point) if z < 1.03 else torso_weights(point)
    if z > 1.44:
        entry = blend(entry, upper_weights(point), float(smoothstep(1.44, 1.5, z)))
    for side, s in SIDES:
        weights, index, distance = arm_weights(point, side)
        radius = 0.13 if index == 0 else 0.1
        member = float(smoothstep(radius + 0.03, radius, distance)) * float(smoothstep(0.14, 0.2, s * x))
        if member > 0.0:
            entry = blend(entry, weights, member)
    legs = {side: leg_weights(point, side) for side, _ in SIDES}
    near = min(legs, key=lambda side: legs[side][1])
    weights, distance = legs[near]
    other = legs["r" if near == "l" else "l"][1]
    share, inner, outer = LEG_FOLLOW
    follow = share * float(smoothstep(outer, inner, distance)) * float(smoothstep(LEG_FOLLOW_TOP[1], LEG_FOLLOW_TOP[0], z)) * float(smoothstep(0.0, LEG_FOLLOW_SPLIT, other - distance))
    if follow > 0.0:
        entry = blend(entry, weights, follow)
    reach = float(np.interp(z, LEG_REACH[0], LEG_REACH[1]))
    member = float(smoothstep(LEG_TOP[1], LEG_TOP[0], z)) * float(smoothstep(reach + 0.03, reach, distance)) * float(smoothstep(0.0, 0.04, other - distance))
    if member > 0.0:
        entry = blend(entry, weights, member)
    return entry


def skin(obj, rig):
    mesh = obj.data
    count = len(mesh.vertices)
    co = np.zeros(count * 3)
    mesh.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    entries = [field(p) for p in co]
    names = sorted({name for entry in entries for name in entry})
    column = {name: k for k, name in enumerate(names)}
    weights = np.zeros((count, len(names)))
    for row, entry in enumerate(entries):
        for name, weight in entry.items():
            weights[row, column[name]] = max(weight, 0.0)
    edges = np.zeros(len(mesh.edges) * 2, dtype=np.int64)
    mesh.edges.foreach_get('vertices', edges)
    edges = edges.reshape(-1, 2)
    degree = np.maximum(np.bincount(edges.ravel(), minlength=count), 1)[:, None]
    for _ in range(3):
        total = np.zeros_like(weights)
        np.add.at(total, edges[:, 0], weights[edges[:, 1]])
        np.add.at(total, edges[:, 1], weights[edges[:, 0]])
        weights = 0.5 * weights + 0.5 * total / degree
    obj.vertex_groups.clear()
    groups = [obj.vertex_groups.new(name=name) for name in names]
    for row in range(count):
        order = np.argsort(-weights[row])[:4]
        chosen = [(int(k), float(weights[row, k])) for k in order if weights[row, k] > 0.015]
        total = sum(w for _, w in chosen) or 1.0
        for k, w in chosen:
            groups[k].add([row], w / total, 'REPLACE')
    obj.parent = rig
    obj.matrix_parent_inverse = Matrix.Identity(4)
    obj.modifiers.new('Armature', 'ARMATURE').object = rig


def robe_joints(obj):
    bvh = BVHTree.FromObject(obj, bpy.context.evaluated_depsgraph_get())
    ROBE_JOINTS.clear()
    for k in range(ROBE_CHAINS):
        angle = 2.0 * math.pi * k / ROBE_CHAINS
        direction = Vector((math.sin(angle), -math.cos(angle), 0.0))
        chain_points = []
        for z in ROBE_ROWS:
            origin = Vector((AXIS[0], AXIS[1], z)) + direction * 1.0
            hit = bvh.ray_cast(origin, -direction, 1.0)
            radius = (1.0 - hit[3]) if hit[0] is not None else 0.2
            chain_points.append(np.array((AXIS[0], AXIS[1], z)) + np.array(direction) * max(radius - 0.02, 0.08))
        ROBE_JOINTS.append(chain_points)


def dorsal(side):
    s = 1.0 if side == 'l' else -1.0
    d1 = unit(ARMS[side][2] - ARMS[side][1])
    a = math.atan2(abs(d1[0]), -d1[2])
    return tuple(orth((s * math.cos(a), 0.0, math.sin(a)), d1))


def grip(side):
    s = 1.0 if side == 'l' else -1.0
    wrist, palm, tip = ARMS[side][3], ARMS[side][4], ARMS[side][5]
    d3 = unit(tip - wrist)
    palmar = orth((-s, 0.0, 0.0), d3)
    radial = orth(orth((0.0, -1.0, 0.0), d3) - palmar * 0.0, palmar)
    center = palm + palmar * 0.03 + d3 * 0.015
    axis = unit(radial - 0.2 * d3)
    across = orth((s, 0.0, 0.0), axis)
    x = np.cross(axis, across)
    m = Matrix.Identity(4)
    for row in range(3):
        m[row][0], m[row][1], m[row][2], m[row][3] = x[row], axis[row], across[row], center[row]
    return m


def joint_layout():
    joints = [
        ('root', None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.12), (0.0, -1.0, 0.0)),
        ('hips', 'root', (0.0, 0.02, 0.98), (0.0, 0.02, 1.12), (0.0, -1.0, 0.0)),
        ('spine', 'hips', (0.0, 0.02, 1.12), (0.0, 0.02, 1.3), (0.0, -1.0, 0.0)),
        ('chest', 'spine', (0.0, 0.02, 1.3), (0.0, 0.02, 1.48), (0.0, -1.0, 0.0)),
        ('neck', 'chest', (0.0, 0.02, 1.5), (0.0, 0.0, 1.6), (0.0, -1.0, 0.0)),
        ('head', 'neck', (0.0, 0.0, 1.6), (0.0, 0.0, 1.84), (0.0, -1.0, 0.0)),
    ]
    for k, points in enumerate(ROBE_JOINTS):
        parent = 'hips'
        outward = unit((points[0][0] - AXIS[0], points[0][1] - AXIS[1], 0.0))
        for index in range(len(points) - 1):
            name = f'robe.{k}.{index + 1}'
            joints.append((name, parent, tuple(points[index]), tuple(points[index + 1]), tuple(outward)))
            parent = name
    for side, _ in SIDES:
        clavicle, shoulder, elbow, wrist, palm, tip = (tuple(p) for p in ARMS[side])
        hip, knee, ankle, ball, toe = (tuple(p) for p in LEGS[side])
        up = dorsal(side)
        joints += [
            (f'shoulder.{side}', 'chest', clavicle, shoulder, (0.0, 0.0, 1.0)),
            (f'upperarm.{side}', f'shoulder.{side}', shoulder, elbow, up),
            (f'lowerarm.{side}', f'upperarm.{side}', elbow, wrist, up),
            (f'wrist.{side}', f'lowerarm.{side}', wrist, palm, up),
            (f'hand.{side}', f'wrist.{side}', palm, tip, up),
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
        slot.matrix = grip(side)
        slot.parent = bones[f'hand.{side}']
        slot.use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')
    for side, _ in SIDES:
        for name in ('upperarm', 'lowerarm', 'wrist', 'hand'):
            data.bones[f'{name}.{side}']['dorsal'] = dorsal(side)
    return rig


def around(center):
    return math.degrees(math.atan2(abs(center[0] - AXIS[0]), -(center[1] - AXIS[1])))


def face_kinds(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.ensure_lookup_table()
    bm.normal_update()
    kinds = np.zeros(len(bm.faces), dtype=np.int32)
    (x0, x1), (y0, y1), (z0, z1) = FACE_BOX
    centers = [face.calc_center_median() for face in bm.faces]
    for face, c in zip(bm.faces, centers):
        if x0 < c.x < x1 and y0 < c.y < y1 and z0 < c.z < z1:
            kinds[face.index] = 2

    def inside(face):
        c = centers[face.index]
        return CAPE_Z[0] < c.z < CAPE_TOP and around(c) > CAPE_SIDE and kinds[face.index] == 0

    seeds = [face for face in bm.faces if inside(face) and around(centers[face.index]) > 110.0 and 0.4 < centers[face.index].z < 1.15 and face.normal.y > 0.5]
    limit = math.cos(math.radians(CAPE_CREASE))
    cape = set(face.index for face in seeds)
    stack = list(seeds)
    while stack:
        face = stack.pop()
        for edge in face.edges:
            for other in edge.link_faces:
                if other.index in cape or not inside(other) or face.normal.dot(other.normal) < limit:
                    continue
                cape.add(other.index)
                stack.append(other)
    kinds[list(cape)] = 1
    bm.free()
    return kinds.tolist()


SLOTS = (('Wizard_Garb', 'Coat', 2048), ('Wizard_Cape', 'Cape', 2048), ('Hero_Head', 'Face', 1024))
UV_ANGLE = 85.0
UV_OVERLAP_PASSES = 5
METAL_TONE = (0.86, 0.88, 0.92)
METAL_EDGE = (0.3, 0.6)
METAL_TIDY = (2, 1)
METAL_LEVEL = (0.72, 0.3, 0.66)
CREAM_EVEN = (0.7, 0.25)
ROUGHNESS = {"gold": 0.3, "cream": 0.42, "cloth": 0.85, "leather": 0.6, "skin": 0.55}
SATURATION = 1.12
AO_DISTANCE = 0.12
AO_STRENGTH = 0.85
CAPE_CONTRAST = 0.45


def unwrap(nodes):
    for node in nodes:
        kit.select_only([node])
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=math.radians(UV_ANGLE), island_margin=0.002, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
        separate_overlaps(node)
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.select_all(action='SELECT')
        bpy.ops.uv.average_islands_scale()
        bpy.ops.uv.pack_islands(rotate=True, margin=0.003, shape_method='CONCAVE')
        bpy.ops.object.mode_set(mode='OBJECT')


def separate_overlaps(node, passes=UV_OVERLAP_PASSES):
    bpy.context.scene.tool_settings.use_uv_select_sync = True
    bpy.ops.uv.select_all(action='SELECT')
    bpy.ops.uv.seams_from_islands()
    for _ in range(passes):
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.uv.select_overlap()
        bpy.ops.object.mode_set(mode='OBJECT')
        overlapping = sum(face.select for face in node.data.polygons)
        bpy.ops.object.mode_set(mode='EDIT')
        if overlapping == 0:
            return
        bpy.ops.mesh.region_to_loop()
        bpy.ops.mesh.mark_seam(clear=False)
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.unwrap(method='ANGLE_BASED', margin=0.003)
    print(f'{node.name}: UV overlaps left after {passes} passes')


def source_sockets(material):
    tree = material.node_tree
    bsdf = next(node for node in tree.nodes if node.type == 'BSDF_PRINCIPLED')
    base = bsdf.inputs['Base Color'].links[0].from_socket
    rough = bsdf.inputs['Roughness'].links[0].from_socket
    metal = bsdf.inputs['Metallic'].links[0].from_socket
    output = next(node for node in tree.nodes if node.type == 'OUTPUT_MATERIAL')
    return tree, bsdf, output, {'base': base, 'rough': rough, 'metal': metal}


def bake_node(high, node, slot, size):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 4
    tree, bsdf, output, sockets = source_sockets(high.data.materials[0])
    emission = tree.nodes.new('ShaderNodeEmission')
    holder = target_image(node, slot)
    maps = {}
    for channel in ('base', 'normal'):
        image = bpy.data.images.new(f'{slot}_{channel}', size, size, alpha=False, float_buffer=True)
        holder.image = image
        if channel == 'normal':
            tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
            options = {'type': 'NORMAL', 'normal_space': 'TANGENT'}
        else:
            tree.links.new(sockets[channel], emission.inputs['Color'])
            tree.links.new(emission.outputs['Emission'], output.inputs['Surface'])
            options = {'type': 'EMIT'}
        kit.select_only([high, node], active=node)
        bpy.ops.object.bake(use_selected_to_active=True, cage_extrusion=0.008, max_ray_distance=0.04, margin=16, **options)
        maps[channel] = kit.image_pixels(image)
        bpy.data.images.remove(image)
    tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    tree.nodes.remove(emission)
    for name in MASKS:
        maps[name.lower()] = bake_attribute(node, holder, name, size)
    return maps


def bake_attribute(node, holder, name, size):
    tree = holder.id_data
    attribute = tree.nodes.new('ShaderNodeAttribute')
    attribute.attribute_name = name
    attribute.attribute_type = 'GEOMETRY'
    glow = tree.nodes.new('ShaderNodeEmission')
    output = next(item for item in tree.nodes if item.type == 'OUTPUT_MATERIAL')
    tree.links.new(attribute.outputs['Fac'], glow.inputs['Color'])
    tree.links.new(glow.outputs['Emission'], output.inputs['Surface'])
    tree.nodes.active = holder
    image = bpy.data.images.new(name, size, size, alpha=False, float_buffer=True)
    holder.image = image
    kit.select_only([node])
    bpy.ops.object.bake(type='EMIT', margin=16)
    mask = kit.image_pixels(image)[..., 0]
    bpy.data.images.remove(image)
    return mask


def target_image(node, slot):
    target = bpy.data.materials.new(slot + 'Target')
    target.use_nodes = True
    holder = target.node_tree.nodes.new('ShaderNodeTexImage')
    target.node_tree.nodes.active = holder
    node.data.materials.clear()
    node.data.materials.append(target)
    return holder


def bake_occlusion(node, slot, size):
    scene = bpy.context.scene
    scene.cycles.samples = 64
    scene.world.light_settings.distance = AO_DISTANCE
    holder = target_image(node, slot)
    image = bpy.data.images.new(f'{slot}_ao', size, size, alpha=False, float_buffer=True)
    holder.image = image
    kit.select_only([node])
    bpy.ops.object.bake(type='AO', margin=16)
    ao = kit.image_pixels(image)[..., 0]
    bpy.data.images.remove(image)
    return ao


def soften(mask, passes=1):
    for _ in range(passes):
        padded = np.pad(mask, 1, mode='edge')
        mask = (padded[:-2, 1:-1] + padded[2:, 1:-1] + padded[1:-1, :-2] + padded[1:-1, 2:] + 4.0 * mask) / 8.0
    return mask


def surface_classes(base, face, hand):
    srgb = kit.linear_to_srgb(base[..., :3])
    top, bottom = srgb.max(axis=-1), srgb.min(axis=-1)
    value = top
    saturation = np.where(top > 1e-4, (top - bottom) / np.maximum(top, 1e-4), 0.0)
    r, g, b = srgb[..., 0], srgb[..., 1], srgb[..., 2]
    hue = np.degrees(np.arctan2(math.sqrt(3.0) * (g - b), 2.0 * r - g - b)) % 360.0
    warm = smoothstep(4.0, 12.0, hue) * (1.0 - smoothstep(52.0, 62.0, hue))
    flesh = warm * smoothstep(0.18, 0.28, saturation) * smoothstep(0.3, 0.42, value)
    region = np.ones_like(value) if face else np.clip(hand, 0.0, 1.0)
    skin = flesh * region
    gold = warm * smoothstep(0.26, 0.38, saturation) * smoothstep(0.38, 0.52, value) * (1.0 - region)
    cream = (1.0 - smoothstep(0.18, 0.3, saturation)) * smoothstep(0.46, 0.6, value) * (1.0 - gold)
    cloth = (1.0 - smoothstep(0.2, 0.32, value)) * (1.0 - gold) * (1.0 - skin)
    leather = np.clip(1.0 - gold - cream - cloth - skin, 0.0, 1.0)
    classes = {'gold': gold, 'cream': cream, 'cloth': cloth, 'leather': leather, 'skin': skin}
    return {name: soften(mask) for name, mask in classes.items()}, value


def morph(mask, radius, pick):
    for _ in range(radius):
        padded = np.pad(mask, 1, mode='edge')
        h, w = mask.shape
        mask = pick.reduce([padded[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] for dy in (-1, 0, 1) for dx in (-1, 0, 1)])
    return mask


def tidy(mask, close, open_):
    mask = morph(morph(mask, close, np.maximum), close, np.minimum)
    mask = morph(morph(mask, open_, np.minimum), open_, np.maximum)
    padded = np.pad(mask, 1, mode='edge')
    h, w = mask.shape
    return sum(padded[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] for dy in (-1, 0, 1) for dx in (-1, 0, 1)) / 9.0


def finish_maps(maps, ao, face, even=False):
    base = maps["base"][..., :3].copy()
    if even:
        tone = base @ np.array((0.2126, 0.7152, 0.0722))
        valid = tone > 0.002
        middle = np.median(tone[valid]) if valid.any() else 0.05
        target = middle + (tone - middle) * CAPE_CONTRAST
        base = base * (np.clip(target, 0.0, 1.0) / np.maximum(tone, 1e-4))[..., None]
    classes, value = surface_classes(base, face, maps['hand'])
    luminance = base @ np.array((0.2126, 0.7152, 0.0722))
    grey = luminance[..., None]
    base = np.clip(grey + (base - grey) * SATURATION, 0.0, 1.0)
    gold = smoothstep(METAL_EDGE[0], METAL_EDGE[1], tidy(classes['gold'], *METAL_TIDY))
    if even:
        cloth = gold < 0.1
        fill = np.median(base[cloth], axis=0) if cloth.any() else np.array((0.05, 0.05, 0.05))
        base = base * (1.0 - gold[..., None]) + fill[None, None, :] * gold[..., None]
        classes['cloth'] = classes['cloth'] + gold * (1.0 - classes['cloth'])
        gold = np.zeros_like(gold)
    elif gold.max() > 0.0:
        bright = np.percentile(luminance[gold > 0.5], 95) if (gold > 0.5).any() else 1.0
        level = np.clip(METAL_LEVEL[0] + METAL_LEVEL[1] * luminance / max(bright, 1e-4), METAL_LEVEL[2], 1.0)
        metal_tone = np.array(METAL_TONE)[None, None, :] * level[..., None]
        base = base * (1.0 - gold[..., None]) + metal_tone * gold[..., None]
    classes['gold'] = gold
    cream = classes['cream'] * (1.0 - gold)
    if (cream > 0.5).any():
        middle = np.median(base[cream > 0.5], axis=0)
        ratio = np.clip(1.0 + CREAM_EVEN[1] * (luminance / max(float(middle @ np.array((0.2126, 0.7152, 0.0722))), 1e-4) - 1.0), 0.8, 1.08)
        base = base * (1.0 - CREAM_EVEN[0] * cream[..., None]) + (middle[None, None, :] * ratio[..., None]) * (CREAM_EVEN[0] * cream[..., None])
    rough = sum(classes[name] * ROUGHNESS[name] for name in classes)
    dark = 1.0 - smoothstep(0.0, 0.25, value)
    rough = rough + classes['gold'] * dark * 0.15 + classes['skin'] * (1.0 - smoothstep(0.15, 0.3, value)) * 0.15
    metal = classes['gold']
    orm = np.ones_like(maps['base'])
    orm[..., 0] = 1.0 - AO_STRENGTH * (1.0 - ao)
    orm[..., 1] = np.clip(rough, 0.05, 1.0)
    orm[..., 2] = np.clip(metal, 0.0, 1.0)
    return base, orm, None


def write_material(node, folder, slot, base, normal, orm, emit=None):
    files = {}
    if emit is not None:
        files['emit'] = os.path.join(folder, f'{NAME}_{slot}_emit.png')
        kit.save_png(files['emit'], np.repeat(emit[..., None], 3, axis=2))
    files['albedo'] = os.path.join(folder, f'{NAME}_{slot}_albedo.png')
    kit.save_png(files['albedo'], base, srgb=True)
    files['normal'] = os.path.join(folder, f'{NAME}_{slot}_normal.png')
    kit.save_png(files['normal'], kit.sanitize_normals(normal, max_tilt=60.0))
    files['orm'] = os.path.join(folder, f'{NAME}_{slot}_orm.png')
    kit.save_png(files['orm'], orm)
    placeholder = bpy.data.materials.new(slot)
    node.data.materials.clear()
    node.data.materials.append(kit.baked_material(placeholder, files))


def mark_hands(obj):
    mesh = obj.data
    co = np.zeros(len(mesh.vertices) * 3)
    mesh.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    mask = np.zeros(len(co))
    for side, _ in SIDES:
        wrist, tip = ARMS[side][3], ARMS[side][5]
        tip = tip + unit(tip - wrist) * 0.08
        ab = tip - wrist
        t = np.clip((co - wrist) @ ab / (ab @ ab), 0.0, 1.0)
        distance = np.linalg.norm(co - (wrist + t[:, None] * ab), axis=1)
        along = (co - wrist) @ unit(ab)
        mask = np.maximum(mask, smoothstep(0.12, 0.09, distance) * smoothstep(0.04, 0.07, along))
    layer = mesh.color_attributes.new('Hand', 'FLOAT_COLOR', 'POINT')
    data = np.ones((len(co), 4), dtype=np.float32)
    data[:, :3] = mask[:, None]
    layer.data.foreach_set('color', data.ravel())


def mark_regions(obj):
    mark_hands(obj)


def split(obj, target):
    mesh = obj.data
    kinds = face_kinds(obj)
    print('faces per node', np.bincount(kinds, minlength=3).tolist())
    nodes = []
    for index, (name, _, _) in enumerate(SLOTS):
        part = obj.copy()
        part.data = mesh.copy()
        part.name = name
        part.data.name = name
        target.objects.link(part)
        bm = bmesh.new()
        bm.from_mesh(part.data)
        bm.faces.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[face for face in bm.faces if kinds[face.index] != index], context='FACES')
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
        bm.to_mesh(part.data)
        bm.free()
        nodes.append(part)
    bpy.data.objects.remove(obj, do_unlink=True)
    bpy.context.view_layer.update()
    return nodes


def build(target, folder):
    source = load(target)
    high = source.copy()
    high.data = source.data.copy()
    target.objects.link(high)
    decimate(source, FACES)
    robe_joints(source)
    rig = build_rig(target)
    skin(source, rig)
    mark_regions(source)
    nodes = split(source, target)
    unwrap(nodes)
    baked = [bake_node(high, node, slot, size) for node, (_, slot, size) in zip(nodes, SLOTS)]
    bpy.data.objects.remove(high, do_unlink=True)
    bpy.context.view_layer.update()
    occlusion = [bake_occlusion(node, slot, size) for node, (_, slot, size) in zip(nodes, SLOTS)]
    for node, (_, slot, _), maps, ao in zip(nodes, SLOTS, baked, occlusion):
        base, orm, emit = finish_maps(maps, ao, slot == "Face", even=slot == "Cape")
        write_material(node, folder, slot, base, maps['normal'], orm, emit)
    return rig, nodes


SHOTS = {
    'front': ((0.0, 0.0, 0.95), 0.0, 4.0, 5.2), 'q': ((0.0, 0.0, 0.95), 25.0, 8.0, 5.2), 'back': ((0.0, 0.0, 0.95), 180.0, 4.0, 5.2),
    'side': ((0.0, 0.0, 0.95), 90.0, 4.0, 5.2), 'bust': ((0.0, -0.02, 1.45), 15.0, 6.0, 1.9), 'game': ((0.0, 0.0, 0.9), 0.0, 57.0, 5.6), 'hands': ((0.0, -0.05, 1.05), 15.0, 5.0, 2.0), 'hand': ((-0.5, -0.04, 1.02), 330.0, 10.0, 0.8),
}


def preview(folder, prefix, views, samples):
    kit.environment('forest.exr', 1.0)
    pk.ground_preview(4.0)
    scene = bpy.context.scene
    for label in views:
        center, yaw, pitch, distance = SHOTS[label]
        data = bpy.data.cameras.new('Shot')
        data.angle = math.radians(30.0)
        camera = bpy.data.objects.new('Shot', data)
        scene.collection.objects.link(camera)
        yaw_r, pitch_r = math.radians(yaw), math.radians(pitch)
        direction = Vector((math.cos(pitch_r) * math.sin(yaw_r), -math.cos(pitch_r) * math.cos(yaw_r), math.sin(pitch_r)))
        camera.location = Vector(center) + direction * distance
        camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
        scene.camera = camera
        scene.render.resolution_x, scene.render.resolution_y = 700, 1000
        scene.cycles.samples = samples
        scene.render.filepath = os.path.join(folder, f'{prefix}_{label}.png')
        bpy.ops.render.render(write_still=True)
        bpy.data.objects.remove(camera)


def main():
    args = pk.options()
    kit.reset()
    kit.enable_gpu()
    target = kit.collection('Export')
    folder = os.path.abspath(args.get('--out') or args.get('--rigtest') or args.get('--preview'))
    os.makedirs(folder, exist_ok=True)
    rig, nodes = build(target, folder)
    if args.get('--rigtest') or args.get('--out'):
        heroanim.build_actions(rig)
        bpy.ops.outliner.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, NAME + '.blend'), compress=bool(args.get('--out')))
    if args.get('--out'):
        for node in nodes:
            for name in [attribute.name for attribute in node.data.color_attributes]:
                node.data.color_attributes.remove(node.data.color_attributes[name])
        kit.export_gltf(nodes, os.path.join(folder, NAME + '.gltf'), EMISSIVE, armature=rig, animations=True)
    if args.get('--preview'):
        views = args['--views'].split(',') if args.get('--views') else ['front', 'q', 'back', 'bust', 'game']
        preview(folder, args.get('--prefix', NAME.lower()), views, int(args.get('--samples', '32')))


if __name__ == '__main__':
    main()
