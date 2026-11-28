import bpy
import bmesh
import colorsys
import math
import os
import sys
import time
import numpy as np
from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import mainportal as mp
import portalterrace as pt

HERE = os.path.dirname(os.path.abspath(__file__))
NAME = 'HubDecor'
PIECES = {
    'Bench': {'glb': 'Bench.glb', 'scale': 1.85, 'faces': 18000},
    'Lantern': {'glb': 'Lantern.glb', 'scale': 2.6, 'faces': 22000, 'clutter': True},
    'FenceRail': {'glb': 'FenceMiddle.glb', 'scale': 2.0, 'faces': 9000},
    'PillarEnd': {'glb': 'PillarEnd.glb', 'scale': 1.25, 'faces': 14000, 'clutter': True},
    'PillarTall': {'glb': 'PillarTall.glb', 'scale': 1.3, 'faces': 12000, 'clutter': True},
}
WELD = 1e-5
TEXTURE = 2048
CAGE = (0.02, 0.06)
AO_DISTANCE = 0.35
UV_PROXY = (6, 60.0)
CLUTTER_Z = 0.16
LOOSE_SHARE = 0.03
FENCE_TRIM = 0.03
DELIGHT = 1.0
DELIGHT_CLAMP = (0.55, 1.8)
GLOW = ((1.0, 0.62, 0.28), 3.0)
STONE = 0, 'stone'
WOOD = 1, 'wood'
MOSS = 2, 'moss'
IRON = 3, 'iron'
BRONZE = 4, 'bronze'
SILVER = 5, 'silver'
GLASS = 6, 'glass'
CLASSES = (STONE, WOOD, MOSS, IRON, BRONZE, SILVER, GLASS)
ROUGH = {0: 0.8, 1: 0.64, 2: 0.9, 3: 0.48, 4: 0.4, 5: 0.32, 6: 0.18}
METAL = {0: 0.0, 1: 0.0, 2: 0.0, 3: 1.0, 4: 1.0, 5: 1.0, 6: 0.0}
IRON_TONE = (0.36, 0.35, 0.36)
BRONZE_TONE = (0.62, 0.42, 0.27)
SILVER_TONE = (0.66, 0.66, 0.68)


def smoothstep(low, high, value):
    t = np.clip((value - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def linear(color):
    return np.asarray(pt.srgb_to_linear(color), np.float32)


def hsv(srgb):
    high = srgb.max(axis=-1)
    low = srgb.min(axis=-1)
    span = np.maximum(high - low, 1e-6)
    r, g, b = srgb[..., 0], srgb[..., 1], srgb[..., 2]
    hue = np.where(high == r, ((g - b) / span) % 6.0, np.where(high == g, (b - r) / span + 2.0, (r - g) / span + 4.0)) * 60.0
    hue = np.where(high - low < 1e-6, 0.0, hue)
    return hue, (high - low) / np.maximum(high, 1e-6), high


def mesh_coords(me):
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get('co', co)
    return co.reshape(-1, 3)


def source_image(obj):
    return next(n.image for n in obj.data.materials[0].node_tree.nodes if n.type == 'TEX_IMAGE' and n.image and 'normal' not in n.image.name and 'metal' not in n.image.name)


def face_colors(obj):
    me = obj.data
    image = source_image(obj)
    width, height = image.size
    pixels = kit.image_pixels(image)[..., :3]
    uv = np.empty(len(me.loops) * 2, np.float32)
    me.uv_layers[0].data.foreach_get('uv', uv)
    uv = uv.reshape(-1, 2)
    starts = np.empty(len(me.polygons), np.int32)
    totals = np.empty(len(me.polygons), np.int32)
    me.polygons.foreach_get('loop_start', starts)
    me.polygons.foreach_get('loop_total', totals)
    face = np.repeat(np.arange(len(me.polygons)), totals)
    mean = np.zeros((len(me.polygons), 2), np.float32)
    np.add.at(mean, face, uv)
    mean /= totals[:, None]
    x = np.clip((mean[:, 0] % 1.0) * width, 0, width - 1).astype(np.int32)
    y = np.clip((mean[:, 1] % 1.0) * height, 0, height - 1).astype(np.int32)
    return pixels[y, x]


def delete_faces(obj, doomed):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in np.nonzero(doomed)[0]], context='FACES')
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context='VERTS')
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def loose_parts(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    label = np.full(len(bm.verts), -1, np.int32)
    parts = 0
    for vert in bm.verts:
        if label[vert.index] >= 0:
            continue
        stack = [vert]
        label[vert.index] = parts
        while stack:
            current = stack.pop()
            for edge in current.link_edges:
                other = edge.other_vert(current)
                if label[other.index] < 0:
                    label[other.index] = parts
                    stack.append(other)
        parts += 1
    bm.free()
    return label, parts


def remove_clutter(obj):
    me = obj.data
    co = mesh_coords(me)
    label, parts = loose_parts(obj)
    sizes = np.bincount(label, minlength=parts)
    top = np.full(parts, -1e9, np.float32)
    np.maximum.at(top, label, co[:, 2])
    doomed_parts = (sizes < LOOSE_SHARE * sizes.max()) & (top < CLUTTER_Z)
    starts = np.empty(len(me.polygons), np.int32)
    me.polygons.foreach_get('loop_start', starts)
    loops = np.empty(len(me.loops), np.int32)
    me.loops.foreach_get('vertex_index', loops)
    first = loops[starts]
    centers, normals, _, _ = mp.face_arrays(me)
    hue, sat, _ = hsv(face_colors(obj))
    green = (hue > 38.0) & (hue < 130.0) & (sat > 0.45)
    low = centers[:, 2] < CLUTTER_Z
    plinth = low & ~green & (sat < 0.4)
    if plinth.sum() < 50:
        plinth = low
    lo =np.percentile(centers[plinth, :2], 2.0, axis=0) - 0.015
    hi = np.percentile(centers[plinth, :2], 98.0, axis=0) + 0.015
    outside = (centers[:, 0] < lo[0]) | (centers[:, 0] > hi[0]) | (centers[:, 1] < lo[1]) | (centers[:, 1] > hi[1])
    doomed = doomed_parts[label[first]] | (low & green & outside)
    delete_faces(obj, doomed)
    print(f'  clutter: {int(doomed_parts.sum())} loose parts, {int(doomed.sum())} faces removed, plinth {np.round(lo, 2)}..{np.round(hi, 2)}', flush=True)


def fence_frame(co):
    mid = np.abs(co[:, 0]) < 0.25 * (co[:, 0].max() - co[:, 0].min())
    z = co[mid, 2]
    hist, edges = np.histogram(z, bins=60)
    empty = np.nonzero(hist == 0)[0]
    gap_rows = empty[(empty > 5) & (empty < 55)]
    gap = (edges[gap_rows.min()], edges[gap_rows.max() + 1])
    in_gap = (co[:, 2] > gap[0] + 0.01) & (co[:, 2] < gap[1] - 0.01)
    xs = co[in_gap, 0]
    middle = 0.5 * (co[:, 0].min() + co[:, 0].max())
    left_inner = xs[xs < middle].max()
    right_inner = xs[xs > middle].min()
    left_outer = co[:, 0].min()
    right_outer = co[:, 0].max()
    return gap, (left_outer, left_inner), (right_inner, right_outer)


def load(target, name):
    spec = PIECES[name]
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(HERE, 'data', spec['glb']))
    added = [o for o in bpy.data.objects if o not in before]
    obj = next(o for o in added if o.type == 'MESH')
    world = obj.matrix_world.copy()
    for other in added:
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
    me = obj.data
    co = mesh_coords(me) * spec['scale']
    co[:, 2] -= co[:, 2].min()
    if spec.get('turn'):
        co[:, :2] *= -1.0
    info = {}
    if name == 'FenceRail':
        gap, left, right = fence_frame(co)
        left_center = 0.5 * (left[0] + left[1])
        right_center = 0.5 * (right[0] + right[1])
        cuts = (left[1] + FENCE_TRIM, right[0] - FENCE_TRIM)
        shift_x = 0.5 * (cuts[0] + cuts[1])
        info['span'] = float(right_center - left_center)
        info['rails'] = (float(cuts[0] - shift_x), float(cuts[1] - shift_x))
        info['gap'] = (float(gap[0]), float(gap[1]))
        wood = (co[:, 0] > cuts[0]) & (co[:, 0] < cuts[1])
        shift = np.array((shift_x, np.median(co[wood, 1]), 0.0), np.float32)
    elif name == 'Lantern':
        height = co[:, 2].max()
        shaft = (co[:, 2] > 0.25 * height) & (co[:, 2] < 0.45 * height)
        low, high = co[shaft].min(axis=0), co[shaft].max(axis=0)
        shift = np.array((0.5 * (low[0] + high[0]), 0.5 * (low[1] + high[1]), 0.0), np.float32)
        info['column'] = (float(0.5 * (high[0] - low[0])), float(0.5 * (high[1] - low[1])))
    else:
        low, high = np.percentile(co, 1.0, axis=0), np.percentile(co, 99.0, axis=0)
        shift = np.array((0.5 * (low[0] + high[0]), 0.5 * (low[1] + high[1]), 0.0), np.float32)
    co -= shift
    me.vertices.foreach_set('co', co.ravel())
    me.update()
    if name == 'FenceRail':
        bm = bmesh.new()
        bm.from_mesh(me)
        for cut, side in zip(info['rails'], (-1.0, 1.0)):
            bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(cut, 0.0, 0.0), plane_no=(side, 0.0, 0.0), clear_outer=True)
        bm.to_mesh(me)
        bm.free()
        me.update()
    if spec.get('clutter'):
        remove_clutter(obj)
    co = mesh_coords(me)
    info['low'] = co.min(axis=0)
    info['high'] = co.max(axis=0)
    obj.name = f'{name}Source'
    print(f'{name}: {len(me.polygons)} faces, size {np.round(info["high"] - info["low"], 3)}, {", ".join(f"{k} {np.round(v, 3) if not isinstance(v, float) else round(v, 3)}" for k, v in info.items() if k not in ("low", "high"))}', flush=True)
    return obj, info


def high_material(obj, info):
    material = obj.data.materials[0]
    tree = material.node_tree
    bsdf = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')
    bsdf.name = 'BSDF'
    emission = tree.nodes.new('ShaderNodeEmission')
    emission.name = 'Emit'
    source = tree.nodes.new('ShaderNodeTexImage')
    source.image = source_image(obj)
    source.name = 'OUT_src'
    geometry = tree.nodes.new('ShaderNodeNewGeometry')
    coords = tree.nodes.new('ShaderNodeTexCoord')

    def vector_math(operation, a, b, c=None):
        node = tree.nodes.new('ShaderNodeVectorMath')
        node.operation = operation
        for index, value in enumerate((a, b, c)):
            if value is None:
                continue
            if isinstance(value, tuple):
                node.inputs[index].default_value = value
            else:
                tree.links.new(value, node.inputs[index])
        node.name = f'{operation}{len(tree.nodes)}'
        return node.outputs['Vector']

    geo = vector_math('MULTIPLY_ADD', geometry.outputs['Normal'], (0.5, 0.5, 0.5), (0.5, 0.5, 0.5))
    tree.nodes[geo.node.name].name = 'OUT_geo'
    low, high = info['low'], info['high']
    span = tuple(float(max(h - l, 1e-4)) for l, h in zip(low, high))
    offset = vector_math('SUBTRACT', coords.outputs['Object'], tuple(float(v) for v in low))
    pos = vector_math('DIVIDE', offset, span)
    tree.nodes[pos.node.name].name = 'OUT_pos'


def bake_from_high(high, low, channels, size):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 4
    tree = high.data.materials[0].node_tree
    output = next(n for n in tree.nodes if n.type == 'OUTPUT_MATERIAL')
    emission = tree.nodes['Emit']
    surface = output.inputs['Surface'].links[0].from_socket if output.inputs['Surface'].links else tree.nodes['BSDF'].outputs['BSDF']
    holder = mp.hooded.target_image(low, low.name + 'Bake')
    maps = {}
    for channel in channels:
        image = bpy.data.images.new(f'{low.name}_{channel}', size, size, alpha=False, float_buffer=True)
        holder.image = image
        if channel == 'normal':
            tree.links.new(surface, output.inputs['Surface'])
            options = {'type': 'NORMAL', 'normal_space': 'TANGENT'}
        else:
            tree.links.new(tree.nodes['OUT_' + channel].outputs[0], emission.inputs['Color'])
            tree.links.new(emission.outputs['Emission'], output.inputs['Surface'])
            options = {'type': 'EMIT'}
        kit.select_only([high, low], active=low)
        started = time.time()
        bpy.ops.object.bake(use_selected_to_active=True, cage_extrusion=CAGE[0], max_ray_distance=CAGE[1], margin=16, **options)
        print(f'  baked {low.name} {channel} in {time.time() - started:.1f} s', flush=True)
        maps[channel] = kit.image_pixels(image)
        bpy.data.images.remove(image)
    tree.links.new(surface, output.inputs['Surface'])
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


def classify(name, info, hue, sat, val, pos, normal):
    cls = np.full(hue.shape, STONE[0], np.int8)
    moss = (hue >= 38.0) & (hue <= 130.0) & (sat >= 0.5)
    warm = (hue >= 8.0) & (hue < 40.0)
    if name == 'Bench':
        cls[:] = IRON[0]
        cls[warm & (sat >= 0.38)] = WOOD[0]
        cls[moss] = MOSS[0]
    elif name == 'FenceRail':
        cls[warm & (sat >= 0.42)] = WOOD[0]
        rails = (pos[..., 0] > info['rails'][0] + 0.01) & (pos[..., 0] < info['rails'][1] - 0.01)
        cls[rails & (sat < 0.22) & (val > 0.4)] = SILVER[0]
        cls[moss] = MOSS[0]
    elif name == 'Lantern':
        hx, hy = info['column']
        height = info['high'][2]
        margin = 0.03
        out = ((np.abs(pos[..., 0]) > hx + margin) | (np.abs(pos[..., 1]) > hy + margin)) & (pos[..., 2] > 0.3 * height)
        glass = out & (pos[..., 0] > hx + 0.35) & (val > 0.62) & (sat > 0.35) & (hue >= 18.0) & (hue <= 58.0)
        cls[out] = IRON[0]
        cls[out & warm & (sat > 0.3)] = BRONZE[0]
        cls[moss & ~glass] = MOSS[0]
        cls[glass] = GLASS[0]
    else:
        cls[moss] = MOSS[0]
    return cls


def delight(color, normal, mask, label):
    count = int(mask.sum())
    if count < 2000:
        return color
    rng = np.random.default_rng(7)
    pick = rng.choice(count, min(count, 60000), replace=False)
    n = normal[mask][pick]
    design = np.concatenate((np.ones((len(n), 1), np.float32), n), axis=1)
    target = np.log(color[mask][pick] + 0.01)
    coef, *_ = np.linalg.lstsq(design, target, rcond=None)
    residual = target - design @ coef
    keep = (np.abs(residual) < 2.0 * residual.std(axis=0)).all(axis=1)
    coef, *_ = np.linalg.lstsq(design[keep], target[keep], rcond=None)
    tilt = normal @ coef[1:]
    factor = np.clip(np.exp(-DELIGHT * tilt), *DELIGHT_CLAMP)
    result = color.copy()
    result[mask] = (color * factor)[mask]
    print(f'    delight {label}: {count} texels, light dir lum {np.round(coef[1:] @ np.array((0.2126, 0.7152, 0.0722)), 2)}', flush=True)
    return result


def compose(name, info, maps, ao):
    src = maps['src'][..., :3]
    covered = src.sum(axis=-1) > 0.0
    shown = kit.linear_to_srgb(np.clip(src, 0.0, 1.0))
    hue, sat, val = hsv(shown)
    normal = maps['geo'][..., :3] * 2.0 - 1.0
    normal /= np.maximum(np.linalg.norm(normal, axis=-1, keepdims=True), 1e-4)
    low, high = info['low'], info['high']
    pos = low[None, None, :] + maps['pos'][..., :3] * (high - low)[None, None, :]
    cls = classify(name, info, hue, sat, val, pos, normal)
    base = src.copy()
    for index, label in CLASSES:
        base = delight(base, normal, covered & (cls == index), label)
    lum = base @ np.array((0.2126, 0.7152, 0.0722), np.float32)
    for index, tone in ((IRON[0], IRON_TONE), (BRONZE[0], BRONZE_TONE), (SILVER[0], SILVER_TONE)):
        mask = cls == index
        if not mask.any():
            continue
        reference = max(float(np.median(lum[mask & covered])) if (mask & covered).any() else 0.1, 1e-3)
        shade = np.clip(pt.blur(np.clip(lum / reference, 0.5, 1.25), 2), 0.5, 1.2)
        base[mask] = (linear(tone)[None, :] * shade[..., None])[mask]
    stone = cls == STONE[0]
    grey = lum[..., None]
    base[stone] = (grey + 0.85 * (base - grey))[stone]
    base = np.clip(base, 0.0, 1.0)
    detail = pt.blur(lum, 2)
    detail = np.clip((lum - detail) * 4.0, -0.5, 0.5)
    rough = np.array([ROUGH[i] for i in range(len(CLASSES))], np.float32)[cls]
    metal = np.array([METAL[i] for i in range(len(CLASSES))], np.float32)[cls]
    rough = rough - 0.12 * detail + 0.06 * (1.0 - ao)
    orm = np.ones(base.shape[:2] + (3,), np.float32)
    orm[..., 0] = np.clip(0.3 + 0.7 * ao, 0.0, 1.0)
    orm[..., 1] = np.clip(rough, 0.12, 0.97)
    orm[..., 2] = metal
    emit = None
    glass = cls == GLASS[0]
    if glass.any():
        emit = np.clip(pt.blur(glass.astype(np.float32), 1) * smoothstep(0.3, 0.7, val), 0.0, 1.0)
        centre = pos[glass & covered].mean(axis=0)
        info['glow'] = centre
    shares = {label: float((covered & (cls == index)).sum()) / max(float(covered.sum()), 1.0) for index, label in CLASSES}
    print('  classes ' + ', '.join(f'{k} {v:.2f}' for k, v in shares.items() if v > 0.0), flush=True)
    return base, orm, emit, cls


def class_preview(cls, covered):
    colors = np.array(((0.6, 0.6, 0.55), (0.55, 0.3, 0.1), (0.2, 0.6, 0.1), (0.25, 0.25, 0.3), (0.8, 0.45, 0.15), (0.9, 0.9, 0.95), (1.0, 0.9, 0.2)), np.float32)
    image = colors[np.clip(cls, 0, len(colors) - 1)]
    image[~covered] = 0.0
    return image


def write_maps(folder, slot, base, normal, orm, emit=None):
    files = {}
    for key, data, color in (('albedo', base, True), ('normal', normal, False), ('orm', orm, False)):
        files[key] = os.path.join(folder, f'{NAME}_{slot}_{key}.png')
        kit.save_png(files[key], data, srgb=color)
    if emit is not None:
        files['emit'] = os.path.join(folder, f'{NAME}_{slot}_emit.png')
        kit.save_png(files['emit'], np.repeat(emit[..., None], 3, axis=2))
    return files


def proxy_unwrap(obj, target):
    proxy = obj.copy()
    proxy.data = obj.data.copy()
    target.objects.link(proxy)
    modifier = proxy.modifiers.new('Smooth', 'SMOOTH')
    modifier.factor = 1.0
    modifier.iterations = UV_PROXY[0]
    kit.apply_modifiers(proxy)
    mp.UV_ANGLE = UV_PROXY[1]
    mp.unwrap(proxy)
    uv = np.empty(len(proxy.data.loops) * 2, np.float32)
    proxy.data.uv_layers[0].data.foreach_get('uv', uv)
    obj.data.uv_layers.new(name='UVMap').data.foreach_set('uv', uv)
    bpy.data.objects.remove(proxy, do_unlink=True)


def engine(p):
    return (round(float(p[0]), 3), round(float(p[2]), 3), round(float(p[1]), 3))


def build_piece(target, name, folder, debug):
    started = time.time()
    source, info = load(target, name)
    high_material(source, info)
    low = source.copy()
    low.data = source.data.copy()
    low.name = name
    target.objects.link(low)
    mp.decimate(low, PIECES[name]['faces'])
    kit.smooth_by_angle(low, 40.0)
    while low.data.uv_layers:
        low.data.uv_layers.remove(low.data.uv_layers[0])
    proxy_unwrap(low, target)
    print(f'  low ready in {time.time() - started:.0f} s', flush=True)
    maps = bake_from_high(source, low, ('src', 'geo', 'pos', 'normal'), TEXTURE)
    images = [n.image for n in source.data.materials[0].node_tree.nodes if n.type == 'TEX_IMAGE' and n.image]
    bpy.data.objects.remove(source, do_unlink=True)
    for image in set(images):
        if image.users == 0:
            bpy.data.images.remove(image)
    ao = bake_ao(low, TEXTURE)
    base, orm, emit, cls = compose(name, info, maps, ao)
    slot = f'{name}Surface'
    files = write_maps(folder, slot, base, kit.sanitize_normals(maps['normal'], max_tilt=50.0), orm, emit)
    if debug:
        kit.save_png(os.path.join(debug, f'{name}_classes.png'), class_preview(cls, maps['src'][..., :3].sum(axis=-1) > 0.0))
        kit.save_png(os.path.join(debug, f'{name}_source.png'), maps['src'][..., :3], srgb=True)
    mp.assign(low, slot, files)
    if emit is not None:
        low.data.materials[0]['emit'] = True
    co = mesh_coords(low.data)
    foot = co[co[:, 2] < 0.3]
    report = f'{name}: {len(low.data.polygons)} faces, x {co[:, 0].min():.3f}..{co[:, 0].max():.3f} y {co[:, 1].min():.3f}..{co[:, 1].max():.3f} z ..{co[:, 2].max():.3f}, foot x {foot[:, 0].min():.3f}..{foot[:, 0].max():.3f} y {foot[:, 1].min():.3f}..{foot[:, 1].max():.3f}'
    if 'glow' in info:
        report += f', glow engine {engine(info["glow"])}'
    if 'span' in info:
        report += f', span {info["span"]:.3f} wood {info["rails"][0]:.3f}..{info["rails"][1]:.3f}'
    print(report + f', built in {time.time() - started:.0f} s', flush=True)
    return low


def preview(objects, folder):
    kit.environment('forest.exr', 1.1, 1.2)
    bpy.context.scene.view_settings.view_transform = 'Standard'
    offset = 0.0
    for obj in objects:
        width = obj.dimensions.x
        obj.location = (offset + 0.5 * width, 0.0, 0.0)
        offset += width + 0.6
    bpy.context.view_layer.update()
    kit.render(os.path.join(folder, 'decor_q.png'), objects, yaw=30.0, pitch=25.0, size=(1800, 800), samples=48, fill=0.75)
    kit.render(os.path.join(folder, 'decor_game.png'), objects, yaw=0.0, pitch=57.0, size=(1800, 800), samples=48, fill=0.75)
    kit.render(os.path.join(folder, 'decor_back.png'), objects, yaw=200.0, pitch=20.0, size=(1800, 800), samples=48, fill=0.75)
    for obj in objects:
        obj.location = (0.0, 0.0, 0.0)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    out = None
    preview_dir = None
    pieces = list(PIECES)
    for i, arg in enumerate(argv):
        if arg == '--out':
            out = os.path.abspath(argv[i + 1])
        if arg == '--preview':
            preview_dir = os.path.abspath(argv[i + 1])
        if arg == '--pieces':
            pieces = argv[i + 1].split(',')
    folder = out or preview_dir
    os.makedirs(folder, exist_ok=True)
    kit.reset()
    kit.enable_gpu()
    bpy.context.preferences.filepaths.save_version = 0
    target = kit.collection(NAME)
    objects = [build_piece(target, name, folder, preview_dir) for name in pieces]
    if out:
        emissive = {o.data.materials[0].get('source', o.data.materials[0].name): GLOW for o in objects if o.data.materials[0].get('emit')}
        kit.export_gltf(objects, os.path.join(out, NAME + '.gltf'), emissive)
        bpy.ops.outliner.orphans_purge(do_recursive=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, NAME + '.blend'))
    if preview_dir:
        preview(objects, preview_dir)


if __name__ == '__main__':
    main()
