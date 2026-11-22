import bpy
import bmesh
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
NAME = 'HubCliffs'
PIECES = {
    'Cliff1': ('EdgeCliff1.glb', 1.0),
    'Cliff2': ('EdgeCliff2.glb', 1.0),
    'Cliff3': ('EdgeCliff3.glb', 1.0),
    'Cliff4': ('EdgeCliff4.glb', 1.0),
    'Pillars': ('BasaltPillars.glb', 1.0),
    'Ledge': ('Ledge1.glb', 0.6),
    'Islet': ('InvertedMountain.glb', 0.35),
    'Stalactite': ('FloatingStalactite.glb', 0.35),
    'Spike': ('Spike.glb', 0.35),
}
SCALE = 8.0
WELD = 1e-6
FACES = 36000
FAR_FACES = 8000
TEXTURE = 2048
CAGE = (0.05, 0.16)
AO_DISTANCE = 1.2
UV_PROXY = (20, 66.0)
TOP_NORMAL = 0.85
TOP_BAND = 0.4
BLOCK_SCALE = 0.8
CRACK_SCALE = 1.2
PALE_LUM = (0.36, 0.5)
PALE_WARM = (0.02, 0.09)
BASALT = ((0.1, 0.094, 0.09), (0.21, 0.196, 0.185))
LIMESTONE = ((0.4, 0.37, 0.33), (0.6, 0.56, 0.5))
CAP = ((0.27, 0.258, 0.245), (0.42, 0.4, 0.375))
GRIME = (0.1, 0.09, 0.085)


def srgb(color):
    return np.asarray(pt.srgb_to_linear(color), np.float32)


def smoothstep(low, high, value):
    t = np.clip((value - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def load(target, name):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(HERE, 'data', PIECES[name][0]))
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
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3) * SCALE
    me.vertices.foreach_set('co', co.ravel())
    me.update()
    centers, normals, areas, _ = mp.face_arrays(me)
    low, high = co.min(axis=0), co.max(axis=0)
    band = high[2] - TOP_BAND * (high[2] - low[2])
    top = (normals[:, 2] > TOP_NORMAL) & (centers[:, 2] > band)
    order = np.argsort(centers[top, 2])
    weights = np.cumsum(areas[top][order])
    level = float(centers[top, 2][order][np.searchsorted(weights, 0.6 * weights[-1])])
    shift = np.array((0.5 * (low[0] + high[0]), 0.5 * (low[1] + high[1]), level), np.float32)
    me.vertices.foreach_set('co', (co - shift).ravel())
    me.update()
    obj.name = f'{name}Source'
    obj['span'] = (float(low[2] - level), float(high[2] - level))
    print(f'{name}: {len(me.polygons)} faces, size {np.round(high - low, 2)}, top {level - low[2]:.2f} m above the bottom', flush=True)
    return obj


def high_material(obj):
    material = obj.data.materials[0]
    tree = material.node_tree
    bsdf = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')
    bsdf.name = 'BSDF'
    emission = tree.nodes.new('ShaderNodeEmission')
    emission.name = 'Emit'
    image = next(n.image for n in tree.nodes if n.type == 'TEX_IMAGE' and n.image and 'normal' not in n.image.name and 'metal' not in n.image.name)
    source = tree.nodes.new('ShaderNodeTexImage')
    source.image = image
    source.name = 'OUT_src'
    coords = tree.nodes.new('ShaderNodeTexCoord')
    geometry = tree.nodes.new('ShaderNodeNewGeometry')
    low, high = obj['span']

    def combine(name, values):
        node = tree.nodes.new('ShaderNodeCombineColor')
        node.name = name
        for socket, value in zip(node.inputs, values):
            tree.links.new(value, socket)

    def split(vector):
        node = tree.nodes.new('ShaderNodeSeparateXYZ')
        tree.links.new(vector, node.inputs['Vector'])
        return node.outputs

    def remap(value, a, b):
        node = tree.nodes.new('ShaderNodeMapRange')
        node.inputs['From Min'].default_value = a
        node.inputs['From Max'].default_value = b
        tree.links.new(value, node.inputs['Value'])
        return node.outputs['Result']

    def noise(scale, detail=3.0, offset=0.0):
        node = tree.nodes.new('ShaderNodeTexNoise')
        node.noise_dimensions = '4D'
        node.inputs['Scale'].default_value = scale
        node.inputs['Detail'].default_value = detail
        node.inputs['Roughness'].default_value = 0.55
        node.inputs[1].default_value = offset
        tree.links.new(coords.outputs['Object'], node.inputs['Vector'])
        return node.outputs['Fac']

    def voronoi(scale, feature, output):
        node = tree.nodes.new('ShaderNodeTexVoronoi')
        node.feature = feature
        node.inputs['Scale'].default_value = scale
        tree.links.new(coords.outputs['Object'], node.inputs['Vector'])
        return node.outputs[output]

    combine('OUT_shape', (geometry.outputs['Pointiness'], remap(split(geometry.outputs['Normal'])[2], -1.0, 1.0), remap(split(coords.outputs['Object'])[2], low, high)))
    combine('OUT_noise', (noise(0.12), noise(0.6, offset=3.0), noise(2.5, offset=7.0)))
    block = tree.nodes.new('ShaderNodeSeparateColor')
    tree.links.new(voronoi(BLOCK_SCALE, 'F1', 'Color'), block.inputs['Color'])
    combine('OUT_detail', (block.outputs['Red'], noise(0.05, 2.0, offset=11.0), voronoi(CRACK_SCALE, 'DISTANCE_TO_EDGE', 'Distance')))


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


def palette(pair, tone):
    dark, light = srgb(pair[0]), srgb(pair[1])
    return dark[None, None, :] + (light - dark)[None, None, :] * tone[..., None]


def compose(maps, ao, soot_scale):
    src = maps['src'][..., :3]
    covered = src.sum(axis=-1) > 0.0
    shown = kit.linear_to_srgb(np.clip(src, 0.0, 1.0))
    lum = pt.blur(shown @ np.array((0.2126, 0.7152, 0.0722), np.float32), 3)
    warm = pt.blur(shown[..., 0] - shown[..., 2], 3)
    pale = smoothstep(*PALE_LUM, lum) * smoothstep(*PALE_WARM, warm)
    shape = maps['shape']
    point = mp.normalized(shape[..., 0], 2.0, 98.0, covered)
    convex = smoothstep(0.55, 0.85, point)
    concave = 1.0 - smoothstep(0.15, 0.45, point)
    nz = shape[..., 1] * 2.0 - 1.0
    height = shape[..., 2]
    top = smoothstep(0.72, 0.92, nz)
    under = smoothstep(-0.15, -0.6, nz)
    large, medium, small = maps['noise'][..., 0], maps['noise'][..., 1], maps['noise'][..., 2]
    block, broad, crack = maps['detail'][..., 0], maps['detail'][..., 1], maps['detail'][..., 2]
    tone = np.clip(0.45 * block + 0.3 * large + 0.25 * medium, 0.0, 1.0)
    basalt = palette(BASALT, tone)
    lime = palette(LIMESTONE, np.clip(0.3 * block + 0.4 * medium + 0.3 * small, 0.0, 1.0))
    cap = palette(CAP, np.clip(0.55 * block + 0.25 * large + 0.2 * small, 0.0, 1.0))
    side = basalt * (1.0 - pale)[..., None] + lime * pale[..., None]
    cover = np.clip(top * (0.75 + 0.25 * smoothstep(0.4, 0.6, medium)), 0.0, 1.0)
    base = side * (1.0 - cover)[..., None] + cap * cover[..., None]
    hue = 1.0 + np.array((0.05, 0.0, -0.06), np.float32)[None, None, :] * (block - 0.5)[..., None] * 2.0
    base = base * hue
    wear = convex * smoothstep(0.35, 0.65, small * 0.6 + medium * 0.4)
    base = base * (1.0 + 0.45 * wear)[..., None]
    fissure = (1.0 - smoothstep(0.003, 0.012, crack)) * smoothstep(0.52, 0.7, broad) * (1.0 - 0.6 * under)
    grime = np.clip(0.9 * (1.0 - ao) + 0.4 * concave + 0.7 * fissure, 0.0, 1.0) * (0.7 + 0.3 * medium)
    base = base * (1.0 - 0.6 * grime)[..., None] + srgb(GRIME)[None, None, :] * (0.2 * grime)[..., None]
    soot = np.clip(0.55 * under + 0.35 * (1.0 - smoothstep(0.0, 0.45, height)), 0.0, 0.75) * soot_scale
    base = base * (1.0 - soot)[..., None]
    base = np.clip(base, 0.0, 1.0)
    rough = 0.78 + 0.1 * (medium - 0.5) + 0.08 * pale + 0.08 * grime - 0.16 * wear - 0.05 * cover
    orm = np.ones(base.shape[:2] + (3,), np.float32)
    orm[..., 0] = np.clip(0.25 + 0.75 * ao, 0.0, 1.0) * (1.0 - 0.25 * concave)
    orm[..., 1] = np.clip(rough, 0.45, 0.97)
    orm[..., 2] = 0.0
    return base, orm, pale


def write_maps(folder, slot, base, normal, orm):
    files = {}
    for key, data, color in (('albedo', base, True), ('normal', normal, False), ('orm', orm, False)):
        files[key] = os.path.join(folder, f'{NAME}_{slot}_{key}.png')
        kit.save_png(files[key], data, srgb=color)
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


def build_piece(target, name, folder):
    started = time.time()
    source = load(target, name)
    high_material(source)
    low = source.copy()
    low.data = source.data.copy()
    low.name = name
    target.objects.link(low)
    mp.decimate(low, FACES)
    kit.smooth_by_angle(low, 45.0)
    while low.data.uv_layers:
        low.data.uv_layers.remove(low.data.uv_layers[0])
    proxy_unwrap(low, target)
    print(f'  low ready in {time.time() - started:.0f} s', flush=True)
    maps = bake_from_high(source, low, ('src', 'shape', 'noise', 'detail', 'normal'), TEXTURE)
    span = tuple(source['span'])
    images = [n.image for n in source.data.materials[0].node_tree.nodes if n.type == 'TEX_IMAGE' and n.image]
    bpy.data.objects.remove(source, do_unlink=True)
    for image in set(images):
        if image.users == 0:
            bpy.data.images.remove(image)
    ao = bake_ao(low, TEXTURE)
    base, orm, pale = compose(maps, ao, PIECES[name][1])
    slot = f'{name}Stone'
    files = write_maps(folder, slot, base, kit.sanitize_normals(maps['normal'], max_tilt=50.0), orm)
    mp.assign(low, slot, files)
    low['span'] = span
    far = low.copy()
    far.data = low.data.copy()
    far.name = f'{name}Far'
    target.objects.link(far)
    mp.decimate(far, FAR_FACES)
    print(f'{name}: {len(low.data.polygons)} faces ({len(far.data.polygons)} far), pale {pale.mean():.2f}, built in {time.time() - started:.0f} s', flush=True)
    return low, far


def preview(objects, folder):
    kit.environment('forest.exr', 1.1, 1.2)
    bpy.context.scene.view_settings.view_transform = 'Standard'
    offset = 0.0
    for obj in objects:
        width = obj.dimensions.x
        obj.location = (offset + 0.5 * width, 0.0, 0.0)
        offset += width + 1.5
    kit.render(os.path.join(folder, 'cliffs_q.png'), objects, yaw=30.0, pitch=30.0, size=(1800, 800), samples=48, fill=0.75)
    kit.render(os.path.join(folder, 'cliffs_game.png'), objects, yaw=0.0, pitch=57.0, size=(1800, 800), samples=48, fill=0.75)
    kit.render(os.path.join(folder, 'cliffs_back.png'), objects, yaw=180.0, pitch=20.0, size=(1800, 800), samples=48, fill=0.75)
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
    built = [build_piece(target, name, folder) for name in pieces]
    objects = [low for low, _ in built]
    for obj in objects:
        low, high = obj['span']
        print(f'{obj.name}: x {obj.dimensions.x:.2f} y {obj.dimensions.y:.2f} z {low:.2f}..{high:.2f}', flush=True)
    if out:
        kit.export_gltf(objects + [far for _, far in built], os.path.join(out, NAME + '.gltf'), {})
        bpy.ops.outliner.orphans_purge(do_recursive=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, NAME + '.blend'))
    if preview_dir:
        for _, far in built:
            far.hide_render = True
        preview(objects, preview_dir)


if __name__ == '__main__':
    main()
