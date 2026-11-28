import bpy
import bmesh
import math
import os
import sys
import time
import numpy as np
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import mainportal as mp
import portalterrace as pt

NAME = 'EarthSpike'
LENGTH = 3.5
DEPTH = 0.9
BASE_RADIUS = 0.76
VOXEL = 0.014
TEXTURE = 1024
CAGE = (0.03, 0.09)
AO_DISTANCE = 0.6
PIECE_FACES = 1150
SLAB_FACES = 260
FRAG_FACES = 240
VARIANTS = (
    {'seed': 11, 'bend': -0.16, 'twist': 0.35, 'squash': 0.86},
    {'seed': 23, 'bend': -0.04, 'twist': -0.45, 'squash': 0.8},
    {'seed': 37, 'bend': -0.1, 'twist': 0.6, 'squash': 0.92},
)
LETTERS = 'ABC'
SLABS = 5
FRAGS = 4
FRAG_SPREAD = 2.2
WEATHERED = ((0.37, 0.35, 0.32), (0.54, 0.51, 0.46))
FRESH = ((0.58, 0.545, 0.48), (0.71, 0.67, 0.59))
SOIL = ((0.22, 0.175, 0.13), (0.33, 0.265, 0.2))
GRIME = (0.07, 0.06, 0.05)


def smoothstep(low, high, value):
    t = np.clip((value - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def srgb(color):
    return np.asarray(pt.srgb_to_linear(color), np.float32)


def palette(pair, tone):
    dark, light = srgb(pair[0]), srgb(pair[1])
    return dark[None, None, :] + (light - dark)[None, None, :] * tone[..., None]


def radius(z):
    u = np.clip((np.asarray(z, np.float64) + DEPTH) / (LENGTH + DEPTH), 0.0, 1.0)
    return BASE_RADIUS * (1.0 - u) ** (0.55 + 0.4 * u)


def axis_x(z, bend):
    u = np.clip((np.asarray(z, np.float64) + DEPTH) / (LENGTH + DEPTH), 0.0, 1.0)
    return bend * u * u


def solid(rng, shape):
    bm = bmesh.new()
    sides = int(rng.integers(6, 8))
    angles = (np.arange(sides) + rng.uniform(-0.28, 0.28, sides)) / sides * 2.0 * math.pi
    radii = rng.uniform(0.84, 1.16, sides)
    rings = []
    for z in np.linspace(-DEPTH, LENGTH - 0.24, 24):
        u = (z + DEPTH) / (LENGTH + DEPTH)
        r = float(radius(z))
        a = angles + shape['twist'] * u
        cx = float(axis_x(z, shape['bend']))
        rings.append([bm.verts.new((cx + math.cos(a[i]) * radii[i] * r, math.sin(a[i]) * radii[i] * r * shape['squash'], z)) for i in range(sides)])
    tip = bm.verts.new((float(axis_x(LENGTH, shape['bend'])) + rng.uniform(0.02, 0.06), rng.uniform(-0.03, 0.03), LENGTH))
    for lower, upper in zip(rings, rings[1:]):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((lower[i], lower[j], upper[j], upper[i]))
    for i in range(sides):
        bm.faces.new((rings[-1][i], rings[-1][(i + 1) % sides], tip))
    bm.faces.new(list(reversed(rings[0])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    for _ in range(2):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        normal = Vector((math.cos(angle), math.sin(angle), 0.55)).normalized()
        z = rng.uniform(2.9, 3.15)
        center = Vector((float(axis_x(z, shape['bend'])), 0.0, z))
        kit.cut(bm, center + normal * (float(radius(z)) * 0.55), normal)
    return bm


def spike_chisels(rng, shape, count):
    result = []
    for _ in range(count):
        z = rng.uniform(-0.3, 2.9)
        angle = rng.uniform(0.0, 2.0 * math.pi)
        tilt = rng.uniform(-0.4, 0.4)
        normal = np.array((math.cos(angle) * math.cos(tilt), math.sin(angle) * math.cos(tilt), math.sin(tilt)))
        r = float(radius(z))
        center = np.array((float(axis_x(z, shape['bend'])), 0.0, z))
        result.append((center + normal * r * (1.0 - rng.uniform(0.12, 0.26)), normal, r * rng.uniform(1.0, 1.8)))
    return result


def flatten(co, chisels):
    for point, normal, reach in chisels:
        offset = co - point[None, :]
        depth = offset @ normal
        lateral = np.linalg.norm(offset - depth[:, None] * normal[None, :], axis=1)
        weight = 1.0 - smoothstep(0.55 * reach, reach, lateral)
        co = co - (np.maximum(depth, 0.0) * weight)[:, None] * normal[None, :]
    return co


def split_planes(rng, shape):
    def plane(z, tilt):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        normal = Vector((math.sin(tilt) * math.cos(angle), math.sin(tilt) * math.sin(angle), math.cos(tilt)))
        return Vector((float(axis_x(z, shape['bend'])), 0.0, z)), normal

    low = plane(rng.uniform(0.15, 0.35), rng.uniform(0.15, 0.35))
    mid = plane(rng.uniform(1.05, 1.35), rng.uniform(0.2, 0.45))
    high = plane(rng.uniform(2.05, 2.35), rng.uniform(0.25, 0.5))
    angle = rng.uniform(0.0, math.pi)
    split = (Vector((float(axis_x(0.75, shape['bend'])), 0.0, 0.75)), Vector((math.cos(angle), math.sin(angle), rng.uniform(-0.15, 0.15))).normalized())
    return [
        [(low, -1)],
        [(low, 1), (mid, -1), (split, -1)],
        [(low, 1), (mid, -1), (split, 1)],
        [(mid, 1), (high, -1)],
        [(high, 1)],
    ]


def carve(bm, halves):
    piece = bm.copy()
    for (point, normal), side in halves:
        kit.cut(piece, point, -normal if side > 0 else normal)
    return piece


def remesh(obj, voxel):
    modifier = obj.modifiers.new('Remesh', 'REMESH')
    modifier.mode = 'VOXEL'
    modifier.voxel_size = voxel
    modifier.adaptivity = 0.0
    kit.apply_modifiers(obj)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    seen = set()
    parts = []
    for start in bm.verts:
        if start.index in seen:
            continue
        stack = [start]
        part = []
        seen.add(start.index)
        while stack:
            v = stack.pop()
            part.append(v)
            for edge in v.link_edges:
                other = edge.other_vert(v)
                if other.index not in seen:
                    seen.add(other.index)
                    stack.append(other)
        parts.append(part)
    parts.sort(key=len, reverse=True)
    for part in parts[1:]:
        bmesh.ops.delete(bm, geom=part, context='VERTS')
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def arrays(obj):
    me = obj.data
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get('co', co)
    normals = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get('normal', normals)
    return co.reshape(-1, 3).astype(np.float64), normals.reshape(-1, 3).astype(np.float64)


class Field:
    def __init__(self, rng, low, high, chip_count, chip_depth, crack_count):
        self.strata = Vector((rng.uniform(-0.45, 0.45), rng.uniform(-0.45, 0.45), 1.0)).normalized()
        self.band_scale = rng.uniform(4.0, 5.5)
        self.band_offsets = rng.uniform(-1.0, 1.0, 512)
        self.band_tones = rng.uniform(0.0, 1.0, 512)
        self.waves = [(rng.normal(size=(6, 3)) * f, rng.uniform(0.0, 2.0 * math.pi, 6)) for f in (2.2, 5.5, 13.0)]
        self.chip_depth = chip_depth
        self.chips = rng.uniform(low, high, (chip_count, 3))
        self.chip_cut = rng.uniform(0.0, 1.0, chip_count)
        self.chip_tilt = rng.normal(size=(chip_count, 3)) * 0.09
        self.chip_tree = KDTree(chip_count)
        for i, p in enumerate(self.chips):
            self.chip_tree.insert(Vector(p), i)
        self.chip_tree.balance()
        cracks = rng.uniform(low, high, (crack_count, 3))
        self.crack_tree = KDTree(crack_count)
        for i, p in enumerate(cracks):
            self.crack_tree.insert(Vector(p), i)
        self.crack_tree.balance()
        self.crack_keep = rng.uniform(0.0, 1.0, crack_count)

    def wave(self, co, octave):
        w, phase = self.waves[octave]
        return np.sin(co @ w.T + phase[None, :]).mean(axis=1) * 1.6

    def evaluate(self, co):
        strata = co @ np.asarray(self.strata) * self.band_scale + 0.35 * self.wave(co, 0)
        band = np.floor(strata).astype(np.int64)
        frac = strata - band
        ledge = smoothstep(0.8, 1.0, frac)
        index = band % len(self.band_offsets)
        step = self.band_offsets[index] * (1.0 - ledge) + self.band_offsets[(index + 1) % len(self.band_offsets)] * ledge
        tone = self.band_tones[index] * (1.0 - ledge) + self.band_tones[(index + 1) % len(self.band_tones)] * ledge
        chip = np.zeros(len(co))
        crack = np.zeros(len(co))
        for i, p in enumerate(co):
            v = Vector(p)
            pair = self.chip_tree.find_n(v, 2)
            cell = pair[0][1]
            depth = self.chip_cut[cell]
            if depth > 0.6:
                rim = smoothstep(0.0, 0.08, pair[1][2] - pair[0][2])
                chip[i] = rim * ((depth - 0.6) / 0.4 * 0.7 + 0.3 + float(np.dot(p - self.chips[cell], self.chip_tilt[cell])))
            near = self.crack_tree.find_n(v, 2)
            if self.crack_keep[near[0][1]] > 0.72:
                crack[i] = 1.0 - smoothstep(0.0, 0.045, near[1][2] - near[0][2])
        chip = np.clip(chip, 0.0, 1.0)
        return {
            'step': step,
            'tone': tone,
            'chip': chip,
            'crack': crack,
            'lump': self.wave(co, 1),
            'grain': self.wave(co, 2),
            'broad': self.wave(co, 0),
        }


def displace(obj, field, planes, soil_top, amplitude, chisels=()):
    co, normals = arrays(obj)
    if chisels:
        co = flatten(co, chisels)
        obj.data.vertices.foreach_set('co', co.astype(np.float32).ravel())
        obj.data.update()
        co, normals = arrays(obj)
    f = field.evaluate(co)
    scale = amplitude(co)
    cap = np.zeros(len(co))
    shift = np.zeros_like(co)
    for (point, normal), _ in planes:
        n = np.asarray(normal)
        dist = np.abs((co - np.asarray(point)) @ n)
        facing = np.abs(normals @ n)
        on = smoothstep(2.2 * VOXEL, 0.8 * VOXEL, dist) * smoothstep(0.55, 0.8, facing)
        rough = 0.6 * f['grain'] + 0.4 * f['lump']
        shift += (on * 0.014 * rough)[:, None] * n[None, :]
        cap = np.maximum(cap, on)
    soil = 1.0 - smoothstep(soil_top - 0.45, soil_top, co[:, 2] + 0.18 * f['broad'])
    rock = 0.02 * f['lump'] + 0.02 * f['step'] - field.chip_depth * f['chip'] - 0.009 * f['crack'] + 0.003 * f['grain']
    crust = 0.014 * np.abs(f['grain']) + 0.01 * f['lump'] + 0.006
    outer = (rock * (1.0 - soil) + crust * soil) * scale * (1.0 - cap)
    co = co + normals * outer[:, None] + shift
    obj.data.vertices.foreach_set('co', co.astype(np.float32).ravel())
    obj.data.update()
    tone = np.clip(0.55 * f['tone'] + 0.25 * (f['broad'] * 0.5 + 0.5) + 0.2 * (f['grain'] * 0.5 + 0.5), 0.0, 1.0)
    write_color(obj, 'Masks', np.stack((cap, soil, tone), axis=1))
    write_color(obj, 'Detail', np.stack((f['chip'], f['crack'], f['lump'] * 0.5 + 0.5), axis=1))


def write_color(obj, name, values):
    me = obj.data
    layer = me.color_attributes.get(name) or me.color_attributes.new(name, 'FLOAT_COLOR', 'POINT')
    rgba = np.ones((len(values), 4), np.float32)
    rgba[:, :3] = values
    layer.data.foreach_set('color', rgba.ravel())


def mesh_from(bm, name, target):
    return kit.mesh_object(name, bm, target)


def high_material(objects, height):
    material = bpy.data.materials.new('EarthHigh')
    material.use_nodes = True
    tree = material.node_tree
    tree.nodes.clear()
    output = tree.nodes.new('ShaderNodeOutputMaterial')
    emission = tree.nodes.new('ShaderNodeEmission')
    emission.name = 'Emit'
    bsdf = tree.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.name = 'BSDF'
    tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    geometry = tree.nodes.new('ShaderNodeNewGeometry')
    coords = tree.nodes.new('ShaderNodeTexCoord')
    for name in ('Masks', 'Detail'):
        attribute = tree.nodes.new('ShaderNodeAttribute')
        attribute.attribute_name = name
        attribute.name = 'OUT_' + name
    combine = tree.nodes.new('ShaderNodeCombineColor')
    combine.name = 'OUT_shape'
    tree.links.new(geometry.outputs['Pointiness'], combine.inputs[0])
    nz = tree.nodes.new('ShaderNodeSeparateXYZ')
    tree.links.new(geometry.outputs['Normal'], nz.inputs['Vector'])
    remap = tree.nodes.new('ShaderNodeMapRange')
    remap.inputs['From Min'].default_value = -1.0
    tree.links.new(nz.outputs['Z'], remap.inputs['Value'])
    tree.links.new(remap.outputs['Result'], combine.inputs[1])
    pz = tree.nodes.new('ShaderNodeSeparateXYZ')
    tree.links.new(coords.outputs['Object'], pz.inputs['Vector'])
    remap = tree.nodes.new('ShaderNodeMapRange')
    remap.inputs['From Min'].default_value = height[0]
    remap.inputs['From Max'].default_value = height[1]
    tree.links.new(pz.outputs['Z'], remap.inputs['Value'])
    tree.links.new(remap.outputs['Result'], combine.inputs[2])
    for obj in objects:
        obj.data.materials.clear()
        obj.data.materials.append(material)
    return material


def bake(highs, low, material, size):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 4
    tree = material.node_tree
    output = next(n for n in tree.nodes if n.type == 'OUTPUT_MATERIAL')
    emission = tree.nodes['Emit']
    holder = mp.hooded.target_image(low, low.name + 'Bake')
    maps = {}
    for channel in ('Masks', 'Detail', 'shape', 'normal'):
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
        kit.select_only(highs + [low], active=low)
        started = time.time()
        bpy.ops.object.bake(use_selected_to_active=True, cage_extrusion=CAGE[0], max_ray_distance=CAGE[1], margin=16, **options)
        print(f'  baked {low.name} {channel} in {time.time() - started:.1f} s', flush=True)
        maps[channel] = kit.image_pixels(image)
        bpy.data.images.remove(image)
    return maps


def bake_ao(node, size):
    scene = bpy.context.scene
    scene.cycles.samples = 64
    scene.world.light_settings.distance = AO_DISTANCE
    holder = mp.hooded.target_image(node, node.name + 'AO')
    image = bpy.data.images.new(f'{node.name}_ao', size, size, alpha=False, float_buffer=True)
    holder.image = image
    hidden = [o for o in scene.objects if o.type == 'MESH' and o is not node and not o.hide_render]
    for obj in hidden:
        obj.hide_render = True
    kit.select_only([node])
    bpy.ops.object.bake(type='AO', margin=16)
    for obj in hidden:
        obj.hide_render = False
    ao = kit.image_pixels(image)[..., 0]
    bpy.data.images.remove(image)
    return ao


def compose(maps, ao):
    masks, detail, shape = maps['Masks'], maps['Detail'], maps['shape']
    covered = (shape[..., :3].sum(axis=-1) > 0.0)
    cap = smoothstep(0.3, 0.7, masks[..., 0])
    soil = smoothstep(0.2, 0.8, masks[..., 1])
    tone = masks[..., 2]
    chip, crack, lump = detail[..., 0], detail[..., 1], detail[..., 2]
    point = mp.normalized(shape[..., 0], 2.0, 98.0, covered)
    convex = smoothstep(0.55, 0.85, point)
    concave = 1.0 - smoothstep(0.15, 0.45, point)
    nz = shape[..., 1] * 2.0 - 1.0
    under = smoothstep(-0.1, -0.6, nz)
    weathered = palette(WEATHERED, tone)
    fresh = palette(FRESH, np.clip(0.6 * tone + 0.4 * lump, 0.0, 1.0))
    freshness = np.clip(cap + 0.45 * smoothstep(0.15, 0.6, chip) + 0.25 * convex, 0.0, 1.0)
    base = weathered * (1.0 - freshness)[..., None] + fresh * freshness[..., None]
    hue = 1.0 + np.array((0.05, 0.0, -0.05), np.float32)[None, None, :] * (lump - 0.5)[..., None] * 2.0
    base = base * hue
    layer = np.array((1.07, 1.0, 0.9), np.float32)[None, None, :] * (1.0 - tone)[..., None] + np.array((0.94, 0.97, 1.02), np.float32)[None, None, :] * tone[..., None]
    base = base * (layer * (1.0 - cap)[..., None] + cap[..., None])
    dirt = palette(SOIL, np.clip(lump, 0.0, 1.0))
    seep = np.clip(soil + 0.55 * concave * smoothstep(0.55, 0.9, lump) + 0.35 * under, 0.0, 1.0) * (1.0 - cap)
    base = base * (1.0 - seep)[..., None] + dirt * seep[..., None]
    grime = np.clip(0.85 * (1.0 - ao) + 0.35 * concave + 0.8 * crack * (1.0 - cap), 0.0, 1.0)
    base = base * (1.0 - 0.42 * grime)[..., None] + srgb(GRIME)[None, None, :] * (0.15 * grime)[..., None]
    base = np.clip(base, 0.0, 1.0)
    rough = 0.82 - 0.1 * freshness + 0.12 * seep + 0.05 * grime - 0.06 * convex * (1.0 - seep)
    orm = np.ones(base.shape[:2] + (3,), np.float32)
    orm[..., 0] = np.clip(0.25 + 0.75 * ao, 0.0, 1.0) * (1.0 - 0.2 * concave)
    orm[..., 1] = np.clip(rough, 0.5, 0.97)
    orm[..., 2] = 0.0
    return base, orm


def finish(low, highs, folder, slot, size):
    mp.unwrap(low)
    material = highs[0].data.materials[0]
    maps = bake(highs, low, material, size)
    ao = bake_ao(low, size)
    base, orm = compose(maps, ao)
    files = {}
    for key, data, color in (('albedo', base, True), ('normal', kit.sanitize_normals(maps['normal'], max_tilt=50.0), False), ('orm', orm, False)):
        files[key] = os.path.join(folder, f'{NAME}_{slot}_{key}.png')
        kit.save_png(files[key], data, srgb=color)
    mp.assign(low, slot, files)


def separate(obj):
    bpy.context.view_layer.update()
    kit.select_only([obj])
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.separate(type='LOOSE')
    bpy.ops.object.mode_set(mode='OBJECT')
    return [o for o in bpy.context.selected_objects]


def centroid(obj):
    co, _ = arrays(obj)
    return co.mean(axis=0)


def build_spike(target, letter, shape, folder):
    started = time.time()
    rng = np.random.default_rng(shape['seed'])
    body = solid(rng, shape)
    splits = split_planes(rng, shape)
    chisels = spike_chisels(rng, shape, 18)
    field = Field(rng, (-1.0, -1.0, -DEPTH - 0.1), (1.0, 1.0, LENGTH + 0.1), 380, 0.035, 60)
    highs = []
    lows = []
    for index, halves in enumerate(splits):
        high = mesh_from(carve(body, halves), f'Spike{letter}{index}High', target)
        remesh(high, VOXEL)
        displace(high, field, halves, 0.3, lambda co: np.clip(radius(co[:, 2]) / 0.42, 0.22, 1.0), chisels)
        highs.append(high)
        low = high.copy()
        low.data = high.data.copy()
        low.name = f'Spike{letter}{index}'
        target.objects.link(low)
        volume = max(0.15, float(np.ptp(arrays(high)[0][:, 2])) / (LENGTH + DEPTH))
        mp.decimate(low, int(PIECE_FACES * (0.6 + volume)))
        wrap = low.modifiers.new('Wrap', 'SHRINKWRAP')
        wrap.wrap_method = 'NEAREST_SURFACEPOINT'
        wrap.target = high
        kit.apply_modifiers(low)
        lows.append(low)
    body.free()
    centers = [centroid(low) for low in lows]
    high_material(highs, (-DEPTH, LENGTH))
    joined = kit.join(lows, f'Spike{letter}')
    kit.smooth_by_angle(joined, 40.0)
    finish(joined, highs, folder, f'Spike{letter}', TEXTURE)
    for high in highs:
        bpy.data.objects.remove(high, do_unlink=True)
    parts = separate(joined)
    named = []
    for index, center in enumerate(centers):
        part = min(parts, key=lambda p: float(np.linalg.norm(centroid(p) - center)))
        parts.remove(part)
        part.name = f'Spike{letter}{index}'
        part.data.name = part.name
        named.append(part)
    for extra in parts:
        bpy.data.objects.remove(extra, do_unlink=True)
    print(f'Spike{letter}: {sum(len(p.data.polygons) for p in named)} faces in {len(named)} pieces, built in {time.time() - started:.0f} s', flush=True)
    return named


def prism(rng, sides, size, thickness):
    bm = bmesh.new()
    angles = (np.arange(sides) + rng.uniform(-0.3, 0.3, sides)) / sides * 2.0 * math.pi
    radii = size * rng.uniform(0.7, 1.15, sides) * np.array([1.0 if i % 2 == 0 else 0.86 for i in range(sides)])
    bottom = [bm.verts.new((math.cos(a) * r, math.sin(a) * r * 0.8, -thickness * 0.5)) for a, r in zip(angles, radii)]
    top = [bm.verts.new((math.cos(a) * r * 0.94, math.sin(a) * r * 0.8 * 0.94, thickness * 0.5)) for a, r in zip(angles, radii)]
    bm.faces.new(list(reversed(bottom)))
    bm.faces.new(top)
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    for _ in range(3):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        normal = Vector((math.cos(angle), math.sin(angle), rng.uniform(-0.6, 0.6))).normalized()
        kit.cut(bm, normal * (size * rng.uniform(0.6, 0.85)), normal)
    return bm


def chunk(rng, size):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=1, radius=size * 0.5)
    stretch = rng.uniform((0.85, 0.6, 0.5), (1.05, 0.8, 0.7))
    for v in bm.verts:
        v.co = Vector((v.co.x * stretch[0], v.co.y * stretch[1], v.co.z * stretch[2]))
    for _ in range(7):
        normal = Vector(rng.normal(size=3)).normalized()
        kit.cut(bm, normal * (size * 0.5 * rng.uniform(0.45, 0.75) * float(np.abs(np.asarray(normal) * stretch).sum() / np.abs(np.asarray(normal)).sum())), normal)
    return bm


def place(obj, matrix):
    co, _ = arrays(obj)
    co = co @ np.asarray(matrix.to_3x3()).T + np.asarray(matrix.translation)
    obj.data.vertices.foreach_set('co', co.astype(np.float32).ravel())
    obj.data.update()


def build_rubble(target, folder):
    started = time.time()
    rng = np.random.default_rng(71)
    field = Field(rng, (-2.0, -2.0, -0.6), (FRAG_SPREAD * (FRAGS + 1), 2.0, 0.6), 700, 0.02, 40)
    highs = []
    lows = []
    kinds = []
    start = rng.uniform(0.0, 2.0 * math.pi)
    for k in range(SLABS):
        theta = start + 2.0 * math.pi * (k + rng.uniform(-0.18, 0.18)) / SLABS
        size = rng.uniform(0.24, 0.34)
        thickness = rng.uniform(0.11, 0.16)
        high = mesh_from(prism(rng, int(rng.integers(6, 9)), size, thickness), f'Slab{k}High', target)
        remesh(high, VOXEL * 0.85)
        lift = math.radians(rng.uniform(16.0, 30.0))
        reach = math.hypot(1.08 * math.cos(theta), 0.86 * math.sin(theta)) + rng.uniform(-0.04, 0.06)
        matrix = Matrix.Translation((math.cos(theta) * reach, math.sin(theta) * reach, size * math.sin(lift) * 0.35 - 0.05)) @ Matrix.Rotation(theta, 4, 'Z') @ Matrix.Rotation(lift, 4, 'Y') @ Matrix.Rotation(rng.uniform(0.0, 2.0 * math.pi), 4, 'Z')
        place(high, matrix)
        highs.append(high)
        kinds.append(('slab', k))
    for k in range(FRAGS):
        high = mesh_from(chunk(rng, 1.0), f'Frag{k}High', target)
        remesh(high, VOXEL * 1.2)
        place(high, Matrix.Translation((FRAG_SPREAD * (k + 2), 0.0, 0.0)))
        highs.append(high)
        kinds.append(('frag', k))
    for high, (kind, k) in zip(highs, kinds):
        co, normals = arrays(high)
        if kind == 'slab':
            displace(high, field, [], -10.0, lambda c: np.full(len(c), 0.55))
            co, normals = arrays(high)
            write_color(high, 'Masks', np.stack((np.zeros(len(co)), np.clip(smoothstep(0.1, -0.5, normals[:, 2]) + smoothstep(0.02, -0.12, co[:, 2]), 0.0, 1.0), np.full(len(co), 0.5)), axis=1))
        else:
            displace(high, field, [], -10.0, lambda c: np.full(len(c), 1.0))
            co, _ = arrays(high)
            masks = np.zeros((len(co), 3))
            masks[:, 0] = 0.3 + 0.3 * (field.wave(co, 1) * 0.5 + 0.5)
            masks[:, 1] = smoothstep(0.75, 1.0, field.wave(co, 0) * 0.5 + 0.5)
            masks[:, 2] = 0.5
            write_color(high, 'Masks', masks)
        low = high.copy()
        low.data = high.data.copy()
        low.name = high.name[:-4]
        target.objects.link(low)
        mp.decimate(low, SLAB_FACES if kind == 'slab' else FRAG_FACES)
        lows.append(low)
    high_material(highs, (-0.3, 0.3))
    centers = [centroid(low) for low in lows]
    joined = kit.join(lows, 'Rubble')
    kit.smooth_by_angle(joined, 40.0)
    finish(joined, highs, folder, 'Rubble', TEXTURE)
    for high in highs:
        bpy.data.objects.remove(high, do_unlink=True)
    parts = separate(joined)
    slabs = []
    frags = []
    for (kind, k), center in zip(kinds, centers):
        part = min(parts, key=lambda p: float(np.linalg.norm(centroid(p) - center)))
        parts.remove(part)
        if kind == 'slab':
            slabs.append(part)
        else:
            co, _ = arrays(part)
            co -= co.mean(axis=0)
            part.data.vertices.foreach_set('co', co.astype(np.float32).ravel())
            part.data.update()
            part.name = f'Frag{k}'
            part.data.name = part.name
            frags.append(part)
    for extra in parts:
        bpy.data.objects.remove(extra, do_unlink=True)
    collar = kit.join(slabs, 'Collar')
    print(f'Rubble: collar {len(collar.data.polygons)} faces, {len(frags)} fragments, built in {time.time() - started:.0f} s', flush=True)
    return [collar] + frags


def preview(spikes, rubble, folder):
    kit.environment('forest.exr', 1.1, 1.2)
    bpy.context.scene.view_settings.view_transform = 'Standard'
    collar = rubble[0]
    shown = []
    for i, parts in enumerate(spikes):
        for part in parts:
            part.location = (i * 3.0, 0.0, 0.0)
            part.rotation_euler = (0.0, 0.75, 0.0)
            shown.append(part)
        copy = collar.copy()
        copy.location = (i * 3.0, 0.0, 0.0)
        bpy.context.scene.collection.objects.link(copy)
        shown.append(copy)
    for frag in rubble[1:]:
        frag.hide_render = True
    collar.hide_render = True
    kit.render(os.path.join(folder, 'spike_game.png'), shown, yaw=0.0, pitch=57.0, size=(1600, 900), samples=48, fill=0.7)
    kit.render(os.path.join(folder, 'spike_side.png'), shown, yaw=90.0, pitch=10.0, size=(1600, 900), samples=48, fill=0.7)
    for part in shown:
        part.rotation_euler = (0.0, 0.0, 0.0)
    exploded = []
    for parts in spikes[:1]:
        for part in parts:
            center = centroid(part)
            part.location = Vector(center) * 0.25
            exploded.append(part)
    for i, frag in enumerate(rubble[1:]):
        frag.hide_render = False
        frag.location = (2.0 + i * 1.3, -1.5, 0.5)
        exploded.append(frag)
    kit.render(os.path.join(folder, 'spike_parts.png'), exploded, yaw=30.0, pitch=20.0, size=(1600, 900), samples=48, fill=0.7)
    for obj in exploded + shown:
        obj.location = (0.0, 0.0, 0.0)
        obj.rotation_euler = (0.0, 0.0, 0.0)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    out = None
    preview_dir = None
    count = len(VARIANTS)
    for i, arg in enumerate(argv):
        if arg == '--out':
            out = os.path.abspath(argv[i + 1])
        if arg == '--preview':
            preview_dir = os.path.abspath(argv[i + 1])
        if arg == '--variants':
            count = int(argv[i + 1])
    folder = out or preview_dir
    os.makedirs(folder, exist_ok=True)
    kit.reset()
    kit.enable_gpu()
    bpy.context.preferences.filepaths.save_version = 0
    target = kit.collection(NAME)
    spikes = [build_spike(target, LETTERS[i], VARIANTS[i], folder) for i in range(count)]
    rubble = build_rubble(target, folder)
    objects = [p for parts in spikes for p in parts] + rubble
    if out:
        kit.export_gltf(objects, os.path.join(out, NAME + '.gltf'), {})
        bpy.ops.outliner.orphans_purge(do_recursive=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, NAME + '.blend'))
    if preview_dir:
        preview(spikes, rubble, preview_dir)


if __name__ == '__main__':
    main()
