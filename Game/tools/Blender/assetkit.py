import bpy
import bmesh
import json
import math
import os
import numpy as np
from mathutils import Matrix, Vector


def enable_gpu():
    prefs = bpy.context.preferences.addons['cycles'].preferences
    for kind in ('OPTIX', 'CUDA'):
        try:
            prefs.compute_device_type = kind
        except TypeError:
            continue
        prefs.refresh_devices()
        found = False
        for device in prefs.devices:
            device.use = device.type == kind
            found = found or device.use
        if found:
            return True
    return False


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'GPU' if enable_gpu() else 'CPU'
    scene.world = bpy.data.worlds.new('World')
    scene.world.use_nodes = True
    return scene


def collection(name):
    found = bpy.data.collections.get(name)
    if not found:
        found = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(found)
    return found


def mesh_object(name, bm, target=None, material=None):
    mesh = bpy.data.meshes.new(name)
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    (target or bpy.context.scene.collection).objects.link(obj)
    if material:
        mesh.materials.append(material)
    return obj


def cut(bm, point, normal):
    geometry = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.bisect_plane(bm, geom=geometry, dist=1e-5, plane_co=point, plane_no=normal, clear_outer=True)
    boundary = [edge for edge in bm.edges if len(edge.link_faces) == 1]
    if boundary:
        bmesh.ops.holes_fill(bm, edges=boundary, sides=0)


def select_only(objects, active=None):
    for obj in bpy.context.view_layer.objects:
        obj.select_set(False)
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = active or objects[0]


def apply_modifiers(obj):
    with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
        for modifier in list(obj.modifiers):
            bpy.ops.object.modifier_apply(modifier=modifier.name)


def bevel(obj, width, segments=2, angle=30.0, harden=True, profile=0.5, apply=True):
    modifier = obj.modifiers.new('Bevel', 'BEVEL')
    modifier.width = width
    modifier.segments = segments
    modifier.limit_method = 'ANGLE'
    modifier.angle_limit = math.radians(angle)
    modifier.profile = profile
    modifier.harden_normals = harden
    modifier.miter_outer = 'MITER_ARC'
    if apply:
        obj.data.shade_smooth()
        apply_modifiers(obj)
    return modifier


def smooth_by_angle(obj, angle):
    obj.data.shade_smooth()
    obj.data.set_sharp_from_angle(angle=math.radians(angle))


def join(objects, name):
    select_only(objects, objects[0])
    with bpy.context.temp_override(active_object=objects[0], selected_editable_objects=objects, selected_objects=objects):
        bpy.ops.object.join()
    result = objects[0]
    result.name = name
    result.data.name = name
    return result


def unwrap(objects, angle=60.0, margin=0.004):
    select_only(objects)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(angle), island_margin=margin, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    bpy.ops.uv.select_all(action='SELECT')
    bpy.ops.uv.average_islands_scale()
    bpy.ops.uv.pack_islands(rotate=True, margin=margin, shape_method='CONCAVE')
    bpy.ops.object.mode_set(mode='OBJECT')


class Shader:
    def __init__(self, name):
        self.material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        self.material.use_nodes = True
        self.tree = self.material.node_tree
        self.tree.nodes.clear()
        self.count = 0

    def node(self, kind, **props):
        node = self.tree.nodes.new(kind)
        node.location = ((self.count % 12) * 220.0, -(self.count // 12) * 260.0)
        self.count += 1
        for key, value in props.items():
            setattr(node, key, value)
        return node

    def feed(self, socket, value):
        if isinstance(value, bpy.types.NodeSocket):
            self.tree.links.new(value, socket)
        elif value is not None:
            if isinstance(value, (int, float)) and hasattr(socket.default_value, '__len__'):
                size = len(socket.default_value)
                value = (float(value),) * (size if size == 3 else 3) + ((1.0,) if size == 4 else ())
            elif isinstance(value, tuple) and hasattr(socket.default_value, '__len__') and len(socket.default_value) == 4 and len(value) == 3:
                value = value + (1.0,)
            socket.default_value = value

    def coords(self, space='Object'):
        return self.node('ShaderNodeTexCoord').outputs[space]

    def geometry(self, output):
        return self.node('ShaderNodeNewGeometry').outputs[output]

    def math(self, operation, a, b=0.0, c=None, clamp=False):
        node = self.node('ShaderNodeMath', operation=operation, use_clamp=clamp)
        self.feed(node.inputs[0], a)
        self.feed(node.inputs[1], b)
        if c is not None:
            self.feed(node.inputs[2], c)
        return node.outputs[0]

    def add(self, a, b, clamp=False):
        return self.math('ADD', a, b, clamp=clamp)

    def sub(self, a, b, clamp=False):
        return self.math('SUBTRACT', a, b, clamp=clamp)

    def mul(self, a, b, clamp=False):
        return self.math('MULTIPLY', a, b, clamp=clamp)

    def madd(self, a, b, c, clamp=False):
        return self.math('MULTIPLY_ADD', a, b, c, clamp=clamp)

    def clamp01(self, a):
        return self.math('ADD', a, 0.0, clamp=True)

    def smooth(self, low, high, value):
        node = self.node('ShaderNodeMapRange', interpolation_type='SMOOTHSTEP', clamp=True)
        self.feed(node.inputs['Value'], value)
        self.feed(node.inputs['From Min'], low)
        self.feed(node.inputs['From Max'], high)
        self.feed(node.inputs['To Min'], 0.0)
        self.feed(node.inputs['To Max'], 1.0)
        return node.outputs['Result']

    def remap(self, value, low, high, to_low, to_high, clamp=True):
        node = self.node('ShaderNodeMapRange', interpolation_type='LINEAR', clamp=clamp)
        self.feed(node.inputs['Value'], value)
        self.feed(node.inputs['From Min'], low)
        self.feed(node.inputs['From Max'], high)
        self.feed(node.inputs['To Min'], to_low)
        self.feed(node.inputs['To Max'], to_high)
        return node.outputs['Result']

    def lerp(self, factor, a, b):
        node = self.node('ShaderNodeMix', data_type='FLOAT', clamp_factor=True)
        self.feed(node.inputs[0], factor)
        self.feed(node.inputs[2], a)
        self.feed(node.inputs[3], b)
        return node.outputs[0]

    def mix(self, factor, a, b, blend='MIX'):
        node = self.node('ShaderNodeMix', data_type='RGBA', blend_type=blend, clamp_factor=True)
        self.feed(node.inputs[0], factor)
        self.feed(node.inputs[6], a)
        self.feed(node.inputs[7], b)
        return node.outputs[2]

    def vector(self, operation, a, b=None, scale=None):
        node = self.node('ShaderNodeVectorMath', operation=operation)
        self.feed(node.inputs[0], a)
        if b is not None:
            self.feed(node.inputs[1], b)
        if scale is not None:
            self.feed(node.inputs['Scale'], scale)
        return node.outputs['Value'] if operation in ('LENGTH', 'DOT_PRODUCT', 'DISTANCE') else node.outputs['Vector']

    def stretch(self, vector, scale):
        return self.vector('MULTIPLY', vector, scale)

    def offset(self, vector, delta):
        return self.vector('ADD', vector, delta)

    def separate(self, vector):
        node = self.node('ShaderNodeSeparateXYZ')
        self.feed(node.inputs[0], vector)
        return node.outputs['X'], node.outputs['Y'], node.outputs['Z']

    def combine(self, x, y, z):
        node = self.node('ShaderNodeCombineXYZ')
        self.feed(node.inputs['X'], x)
        self.feed(node.inputs['Y'], y)
        self.feed(node.inputs['Z'], z)
        return node.outputs['Vector']

    def torus(self, period):
        u, v, _ = self.separate(self.coords('UV'))
        radius = period / (2.0 * math.pi)
        around = self.mul(u, 2.0 * math.pi)
        along = self.mul(v, 2.0 * math.pi)
        vector = self.combine(self.mul(self.math('COSINE', around), radius), self.mul(self.math('SINE', around), radius), self.mul(self.math('COSINE', along), radius))
        return vector, self.mul(self.math('SINE', along), radius)

    def noise(self, vector, scale, detail=2.0, roughness=0.5, distortion=0.0, lacunarity=2.0, w=None):
        node = self.node('ShaderNodeTexNoise', noise_dimensions='3D' if w is None else '4D')
        self.feed(node.inputs['Vector'], vector)
        if w is not None:
            self.feed(node.inputs['W'], w)
        self.feed(node.inputs['Scale'], scale)
        self.feed(node.inputs['Detail'], detail)
        self.feed(node.inputs['Roughness'], roughness)
        self.feed(node.inputs['Distortion'], distortion)
        self.feed(node.inputs['Lacunarity'], lacunarity)
        return node.outputs['Fac']

    def noise_color(self, vector, scale, detail=2.0, roughness=0.5, distortion=0.0, w=None):
        node = self.node('ShaderNodeTexNoise', noise_dimensions='3D' if w is None else '4D')
        self.feed(node.inputs['Vector'], vector)
        if w is not None:
            self.feed(node.inputs['W'], w)
        self.feed(node.inputs['Scale'], scale)
        self.feed(node.inputs['Detail'], detail)
        self.feed(node.inputs['Roughness'], roughness)
        self.feed(node.inputs['Distortion'], distortion)
        return node.outputs['Color']

    def voronoi(self, vector, scale, feature='F1', randomness=1.0, output='Distance', distance='EUCLIDEAN', w=None):
        node = self.node('ShaderNodeTexVoronoi', voronoi_dimensions='3D' if w is None else '4D', feature=feature, distance=distance)
        self.feed(node.inputs['Vector'], vector)
        if w is not None:
            self.feed(node.inputs['W'], w)
        self.feed(node.inputs['Scale'], scale)
        self.feed(node.inputs['Randomness'], randomness)
        return node.outputs[output]

    def wave(self, vector, scale, kind='BANDS', direction='Z', distortion=0.0, detail=2.0, detail_scale=1.0, profile='SIN', phase=0.0):
        node = self.node('ShaderNodeTexWave', wave_type=kind, wave_profile=profile)
        if kind == 'BANDS':
            node.bands_direction = direction
        else:
            node.rings_direction = direction
        self.feed(node.inputs['Vector'], vector)
        self.feed(node.inputs['Scale'], scale)
        self.feed(node.inputs['Distortion'], distortion)
        self.feed(node.inputs['Detail'], detail)
        self.feed(node.inputs['Detail Scale'], detail_scale)
        self.feed(node.inputs['Phase Offset'], phase)
        return node.outputs['Fac']

    def occlusion(self, distance, samples=16, inside=False, only_local=True):
        node = self.node('ShaderNodeAmbientOcclusion', samples=samples, inside=inside, only_local=only_local)
        self.feed(node.inputs['Distance'], distance)
        return node.outputs['AO']

    def finish(self, base, roughness, height=0.0, bump=0.0, bump_distance=0.01, metallic=0.0, emission=None, emission_color=(1.0, 0.4, 0.1), emission_strength=1.0, bake_size=1024, specular=0.5, alpha=None, cutoff=0.5):
        outputs = {}
        channels = [('base', base), ('rough', roughness), ('metal', metallic), ('height', height), ('emit', emission if emission is not None else 0.0)]
        if alpha is not None:
            channels.append(('alpha', alpha))
        for key, value in channels:
            reroute = self.node('NodeReroute')
            reroute.name = 'OUT_' + key
            reroute.label = reroute.name
            if isinstance(value, bpy.types.NodeSocket):
                self.tree.links.new(value, reroute.inputs[0])
            else:
                constant = self.node('ShaderNodeValue') if not isinstance(value, tuple) else self.node('ShaderNodeRGB')
                constant.outputs[0].default_value = value if not isinstance(value, tuple) else value + (1.0,)
                self.tree.links.new(constant.outputs[0], reroute.inputs[0])
            outputs[key] = reroute.outputs[0]
        bsdf = self.node('ShaderNodeBsdfPrincipled')
        bsdf.name = 'Principled BSDF'
        self.feed(bsdf.inputs['Base Color'], outputs['base'])
        self.feed(bsdf.inputs['Roughness'], outputs['rough'])
        self.feed(bsdf.inputs['Metallic'], outputs['metal'])
        self.feed(bsdf.inputs['Specular IOR Level'], specular)
        bump_node = self.node('ShaderNodeBump')
        bump_node.name = 'Bump'
        self.feed(bump_node.inputs['Height'], outputs['height'])
        self.feed(bump_node.inputs['Strength'], bump)
        self.feed(bump_node.inputs['Distance'], bump_distance)
        self.feed(bsdf.inputs['Normal'], bump_node.outputs['Normal'])
        tint = self.mix(outputs['emit'], (0.0, 0.0, 0.0), emission_color)
        self.feed(bsdf.inputs['Emission Color'], tint)
        self.feed(bsdf.inputs['Emission Strength'], emission_strength)
        if alpha is not None:
            self.feed(bsdf.inputs['Alpha'], outputs['alpha'])
            self.material['alpha_cutoff'] = cutoff
        output = self.node('ShaderNodeOutputMaterial', target='ALL')
        output.name = 'Material Output'
        self.feed(output.inputs['Surface'], bsdf.outputs['BSDF'])
        self.material['bake_size'] = bake_size
        self.material['emit_color'] = list(emission_color)
        self.material['emit_strength'] = emission_strength if emission is not None else 0.0
        return self.material


def image_pixels(image):
    width, height = image.size
    data = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(data)
    return data.reshape(height, width, 4)


def fill_outside(image, covered):
    height, width = covered.shape
    color = image[..., :3] * covered[..., None]
    weight = covered.astype(np.float32)
    levels = []
    while color.shape[0] > 1 and color.shape[1] > 1 and color.shape[0] % 2 == 0 and color.shape[1] % 2 == 0:
        levels.append((color, weight))
        h, w = color.shape[0] // 2, color.shape[1] // 2
        color = color.reshape(h, 2, w, 2, 3).sum(axis=(1, 3))
        weight = weight.reshape(h, 2, w, 2).sum(axis=(1, 3))
    filled = color / np.maximum(weight, 1e-6)[..., None]
    for color, weight in reversed(levels):
        average = color / np.maximum(weight, 1e-6)[..., None]
        coarse = np.repeat(np.repeat(filled, 2, axis=0), 2, axis=1)
        filled = np.where(weight[..., None] > 0.0, average, coarse)
    result = image.copy()
    result[..., :3] = np.where(covered[..., None], image[..., :3], filled)
    return result


def sanitize_normals(image, max_tilt=35.0):
    normal = image[..., :3] * 2.0 - 1.0
    planar = normal[..., :2].copy()
    planar[(normal[..., 2] < 0.5) | (np.linalg.norm(normal, axis=2) < 0.85)] = 0.0
    radius = np.linalg.norm(planar, axis=2)
    limit = math.sin(math.radians(max_tilt))
    planar *= np.where(radius > limit, limit / np.maximum(radius, 1e-6), 1.0)[..., None]
    result = image.copy()
    result[..., :2] = planar * 0.5 + 0.5
    result[..., 2] = np.sqrt(np.clip(1.0 - (planar ** 2).sum(axis=2), 0.0, 1.0)) * 0.5 + 0.5
    return result


def linear_to_srgb(values):
    values = np.clip(values, 0.0, 1.0)
    return np.where(values <= 0.0031308, values * 12.92, 1.055 * np.power(values, 1.0 / 2.4) - 0.055)


def save_png(path, rgb, srgb=False, alpha=None):
    height, width = rgb.shape[:2]
    data = np.ones((height, width, 4), dtype=np.float32)
    data[..., :3] = linear_to_srgb(rgb[..., :3]) if srgb else np.clip(rgb[..., :3], 0.0, 1.0)
    if alpha is not None:
        data[..., 3] = np.clip(alpha, 0.0, 1.0)
    name = os.path.basename(path)
    image = bpy.data.images.get(name)
    if image:
        bpy.data.images.remove(image)
    image = bpy.data.images.new(name, width, height, alpha=alpha is not None, float_buffer=False)
    image.colorspace_settings.name = 'Non-Color'
    image.pixels.foreach_set(data.ravel())
    image.filepath_raw = path
    image.file_format = 'PNG'
    image.save()
    bpy.data.images.remove(image)


def bake_material(material, objects, folder, prefix, ao_distance=0.5, ao_samples=128, margin=16):
    scene = bpy.context.scene
    size = int(material.get('bake_size', 1024))
    tree = material.node_tree
    output = tree.nodes['Material Output']
    bsdf = tree.nodes['Principled BSDF']
    emission = tree.nodes.new('ShaderNodeEmission')
    target = tree.nodes.new('ShaderNodeTexImage')
    tree.nodes.active = target
    scratch = bpy.data.images.new(f'{prefix}_scratch', 16, 16, alpha=False, float_buffer=True)
    others = []
    for obj in objects:
        for slot in obj.material_slots:
            other = slot.material
            if other and other != material and other not in [entry[0] for entry in others]:
                node = other.node_tree.nodes.new('ShaderNodeTexImage')
                node.image = scratch
                others.append((other, node, other.node_tree.nodes.active))
                other.node_tree.nodes.active = node
    select_only(objects)
    baked = {}

    def bake(kind, channel, samples, edge=margin, **options):
        image = bpy.data.images.new(f'{prefix}_{material.name}_{channel}_bake', size, size, alpha=False, float_buffer=True)
        target.image = image
        scene.cycles.samples = samples
        bpy.ops.object.bake(type=kind, margin=edge, margin_type='EXTEND', use_clear=True, **options)
        baked[channel] = image_pixels(image)
        bpy.data.images.remove(image)

    white = tree.nodes.new('ShaderNodeValue')
    white.outputs[0].default_value = 1.0
    tree.links.new(white.outputs[0], emission.inputs['Color'])
    tree.links.new(emission.outputs[0], output.inputs['Surface'])
    bake('EMIT', 'coverage', 1, edge=0)
    tree.nodes.remove(white)
    for channel in ('base', 'rough', 'metal', 'emit') + (('alpha',) if material.get('alpha_cutoff') else ()):
        tree.links.new(tree.nodes['OUT_' + channel].outputs[0], emission.inputs['Color'])
        emission.inputs['Strength'].default_value = 1.0
        tree.links.new(emission.outputs[0], output.inputs['Surface'])
        bake('EMIT', channel, 16)
    tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    bake('NORMAL', 'normal', 16, normal_space='TANGENT', normal_r='POS_X', normal_g='POS_Y', normal_b='POS_Z')
    if material.get('no_ao'):
        baked['ao'] = np.ones_like(baked['base'])
    else:
        scene.world.light_settings.distance = ao_distance
        bake('AO', 'ao', ao_samples)
    tree.nodes.remove(emission)
    tree.nodes.remove(target)
    for other, node, active in others:
        other.node_tree.nodes.remove(node)
        other.node_tree.nodes.active = active
    bpy.data.images.remove(scratch)
    covered = baked.pop('coverage')[..., 0] > 0.5
    for channel in baked:
        baked[channel] = fill_outside(baked[channel], covered)
    baked['normal'] = sanitize_normals(baked['normal'])

    files = {}
    base_name = f'{prefix}_{material.name}'
    files['albedo'] = os.path.join(folder, base_name + '_albedo.png')
    save_png(files['albedo'], baked['base'], srgb=True, alpha=baked['alpha'][..., 0] * covered if 'alpha' in baked else None)
    files['normal'] = os.path.join(folder, base_name + '_normal.png')
    save_png(files['normal'], baked['normal'])
    orm = np.zeros_like(baked['base'])
    orm[..., 0] = baked['ao'][..., 0]
    orm[..., 1] = baked['rough'][..., 0]
    orm[..., 2] = baked['metal'][..., 0]
    files['orm'] = os.path.join(folder, base_name + '_orm.png')
    save_png(files['orm'], orm)
    if material.get('emit_strength', 0.0) > 0.0:
        mask = np.repeat(baked['emit'][..., :1], 3, axis=2)
        files['emit'] = os.path.join(folder, base_name + '_emit.png')
        save_png(files['emit'], mask)
    return files


def baked_material(material, files):
    result = bpy.data.materials.new(material.name + '_Baked')
    result.use_backface_culling = True
    result.use_nodes = True
    tree = result.node_tree
    tree.nodes.clear()
    bsdf = tree.nodes.new('ShaderNodeBsdfPrincipled')
    output = tree.nodes.new('ShaderNodeOutputMaterial')
    tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

    def image(path, color):
        node = tree.nodes.new('ShaderNodeTexImage')
        node.image = bpy.data.images.load(path, check_existing=True)
        node.image.colorspace_settings.name = 'sRGB' if color else 'Non-Color'
        return node

    tree.links.new(image(files['albedo'], True).outputs['Color'], bsdf.inputs['Base Color'])
    orm = image(files['orm'], False)
    split = tree.nodes.new('ShaderNodeSeparateColor')
    tree.links.new(orm.outputs['Color'], split.inputs['Color'])
    tree.links.new(split.outputs['Green'], bsdf.inputs['Roughness'])
    tree.links.new(split.outputs['Blue'], bsdf.inputs['Metallic'])
    normal_map = tree.nodes.new('ShaderNodeNormalMap')
    tree.links.new(image(files['normal'], False).outputs['Color'], normal_map.inputs['Color'])
    tree.links.new(normal_map.outputs['Normal'], bsdf.inputs['Normal'])
    if 'emit' in files:
        tree.links.new(image(files['emit'], False).outputs['Color'], bsdf.inputs['Emission Color'])
        bsdf.inputs['Emission Strength'].default_value = 1.0
    result['source'] = material.name
    return result


def export_gltf(objects, path, emissive, masked=None, armature=None, animations=False):
    select_only(objects + ([armature] if armature else []))
    bpy.ops.export_scene.gltf(
        filepath=path,
        export_format='GLTF_SEPARATE',
        use_selection=True,
        export_apply=True,
        export_texcoords=True,
        export_normals=True,
        export_tangents=False,
        export_materials='EXPORT',
        export_image_format='AUTO',
        export_keep_originals=True,
        export_yup=True,
        export_extras=False,
        export_cameras=False,
        export_lights=False,
        export_animations=animations,
        export_animation_mode='ACTIONS',
        export_force_sampling=True,
        export_anim_single_armature=True,
        export_reset_pose_bones=True,
        export_skins=armature is not None,
        export_morph=False,
    )
    with open(path, 'r', encoding='utf-8') as file:
        document = json.load(file)
    used = set(document.get('extensionsUsed', []))
    for material in document.get('materials', []):
        pbr = material.setdefault('pbrMetallicRoughness', {})
        surface = pbr.get('metallicRoughnessTexture')
        if surface:
            material['occlusionTexture'] = {'index': surface['index']}
        pbr['roughnessFactor'] = 1.0
        pbr['metallicFactor'] = 1.0
        source = material['name'][:-len('_Baked')] if material['name'].endswith('_Baked') else material['name']
        material['name'] = source
        cutoff = (masked or {}).get(source)
        if cutoff:
            material['alphaMode'] = 'MASK'
            material['alphaCutoff'] = cutoff
        glow = emissive.get(source)
        extensions = material.setdefault('extensions', {})
        extensions.pop('KHR_materials_emissive_strength', None)
        if glow and 'emissiveTexture' in material:
            material['emissiveFactor'] = [round(c, 5) for c in glow[0]]
            extensions['KHR_materials_emissive_strength'] = {'emissiveStrength': glow[1]}
            used.add('KHR_materials_emissive_strength')
        else:
            material.pop('emissiveFactor', None)
            material.pop('emissiveTexture', None)
        if not extensions:
            material.pop('extensions', None)
    if used:
        document['extensionsUsed'] = sorted(used)
    with open(path, 'w', encoding='utf-8', newline='\n') as file:
        json.dump(document, file, indent=1)


def bake_and_export(objects, folder, name, ao_distance=0.5, armature=None, animations=False):
    os.makedirs(folder, exist_ok=True)
    materials = []
    for obj in objects:
        for slot in obj.material_slots:
            if slot.material and slot.material not in materials:
                materials.append(slot.material)
    baked = {}
    emissive = {}
    masked = {}
    for material in materials:
        if material.get('alpha_cutoff'):
            masked[material.name] = float(material['alpha_cutoff'])
        users = [obj for obj in objects if material.name in obj.data.materials]
        files = bake_material(material, users, folder, name, ao_distance=ao_distance)
        baked[material.name] = baked_material(material, files)
        if 'emit' in files:
            emissive[material.name] = (tuple(material['emit_color']), float(material['emit_strength']))
    originals = {obj.name: [slot.material for slot in obj.material_slots] for obj in objects}
    for obj in objects:
        for slot in obj.material_slots:
            slot.material = baked[slot.material.name]
    export_gltf(objects, os.path.join(folder, name + '.gltf'), emissive, masked, armature, animations)
    return originals, baked


def restore_materials(objects, originals):
    for obj in objects:
        for slot, material in zip(obj.material_slots, originals[obj.name]):
            slot.material = material


def environment(path_name='forest.exr', strength=1.0, rotation=0.0):
    world = bpy.context.scene.world
    tree = world.node_tree
    tree.nodes.clear()
    folder = bpy.utils.system_resource('DATAFILES', path='studiolights/world')
    texture = tree.nodes.new('ShaderNodeTexEnvironment')
    texture.image = bpy.data.images.load(os.path.join(folder, path_name), check_existing=True)
    mapping = tree.nodes.new('ShaderNodeMapping')
    mapping.inputs['Rotation'].default_value = (0.0, 0.0, rotation)
    coords = tree.nodes.new('ShaderNodeTexCoord')
    tree.links.new(coords.outputs['Generated'], mapping.inputs['Vector'])
    tree.links.new(mapping.outputs['Vector'], texture.inputs['Vector'])
    background = tree.nodes.new('ShaderNodeBackground')
    background.inputs['Strength'].default_value = strength
    tree.links.new(texture.outputs['Color'], background.inputs['Color'])
    output = tree.nodes.new('ShaderNodeOutputWorld')
    tree.links.new(background.outputs['Background'], output.inputs['Surface'])


def render(path, targets, yaw=35.0, pitch=57.0, fov=40.0, size=(1280, 720), samples=96, fill=1.15, look_offset=(0.0, 0.0, 0.0)):
    scene = bpy.context.scene
    low = Vector((1e9, 1e9, 1e9))
    high = Vector((-1e9, -1e9, -1e9))
    for obj in targets:
        for corner in obj.bound_box:
            world = obj.matrix_world @ Vector(corner)
            low = Vector(map(min, low, world))
            high = Vector(map(max, high, world))
    center = (low + high) * 0.5 + Vector(look_offset)
    radius = (high - low).length * 0.5
    camera_data = bpy.data.cameras.new('PreviewCamera')
    camera_data.angle = math.radians(fov)
    camera_data.sensor_fit = 'VERTICAL'
    camera = bpy.data.objects.new('PreviewCamera', camera_data)
    scene.collection.objects.link(camera)
    yaw_r = math.radians(yaw)
    pitch_r = math.radians(pitch)
    direction = Vector((math.cos(pitch_r) * math.sin(yaw_r), -math.cos(pitch_r) * math.cos(yaw_r), math.sin(pitch_r)))
    distance = radius * fill / math.sin(math.radians(fov) * 0.5)
    camera.location = center + direction * distance
    camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = camera
    scene.render.resolution_x, scene.render.resolution_y = size
    scene.render.resolution_percentage = 100
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera)


def tileable_plane():
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=0.5)
    layer = bm.loops.layers.uv.new('UVMap')
    for face in bm.faces:
        for loop in face.loops:
            loop[layer].uv = (loop.vert.co.x + 0.5, loop.vert.co.y + 0.5)
    return mesh_object('BakePlane', bm)


def bake_emit(material, obj, channel, size, samples=8):
    scene = bpy.context.scene
    obj.data.materials.clear()
    obj.data.materials.append(material)
    tree = material.node_tree
    emission = tree.nodes.new('ShaderNodeEmission')
    target = tree.nodes.new('ShaderNodeTexImage')
    image = bpy.data.images.new(f'{material.name}_{channel}', size, size, alpha=False, float_buffer=True)
    target.image = image
    tree.nodes.active = target
    tree.links.new(tree.nodes['OUT_' + channel].outputs[0], emission.inputs['Color'])
    tree.links.new(emission.outputs[0], tree.nodes['Material Output'].inputs['Surface'])
    select_only([obj])
    scene.cycles.samples = samples
    bpy.ops.object.bake(type='EMIT', margin=0, use_clear=True)
    pixels = image_pixels(image)
    tree.links.new(tree.nodes['Principled BSDF'].outputs['BSDF'], tree.nodes['Material Output'].inputs['Surface'])
    tree.nodes.remove(emission)
    tree.nodes.remove(target)
    bpy.data.images.remove(image)
    return pixels


def periodic_blur(values, passes, radius):
    result = values
    for _ in range(passes):
        for axis in (0, 1):
            total = np.zeros_like(result)
            for offset in range(-radius, radius + 1):
                total += np.roll(result, offset, axis=axis)
            result = total / (2 * radius + 1)
    return result


def periodic_normals(height, depth, period):
    texel = period / height.shape[0]
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * depth / texel
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * depth / texel
    normal = np.stack([-dx, -dy, np.ones_like(height)], axis=2)
    normal /= np.linalg.norm(normal, axis=2, keepdims=True)
    return normal * 0.5 + 0.5


def export_tileable(material, obj, folder, period, depth, cavity, size=1024, preview=None):
    os.makedirs(folder, exist_ok=True)
    name = material.name
    base = bake_emit(material, obj, 'base', size)
    rough = bake_emit(material, obj, 'rough', size)[..., 0]
    height = bake_emit(material, obj, 'height', size)[..., 0]
    height = (height - height.min()) / max(height.max() - height.min(), 1e-6)
    occlusion = np.clip(1.0 - cavity * np.maximum(periodic_blur(height, 2, 6) - height, 0.0), 0.55, 1.0)
    normal = periodic_normals(height, depth, period)
    save_png(os.path.join(folder, f'{name}_diff.png'), base, srgb=True)
    save_png(os.path.join(folder, f'{name}_nor_gl.png'), normal)
    save_png(os.path.join(folder, f'{name}_arm.png'), np.stack([occlusion, rough, np.zeros_like(rough)], axis=2))
    save_png(os.path.join(folder, f'{name}_disp.png'), np.repeat(height[..., None], 3, axis=2))
    if preview:
        os.makedirs(preview, exist_ok=True)
        light = np.array([0.45, 0.55, 0.7])
        lambert = np.clip(((normal * 2.0 - 1.0) * (light / np.linalg.norm(light))).sum(axis=2), 0.0, 1.0)
        shaded = base[..., :3] * (0.35 + 0.9 * lambert[..., None]) * occlusion[..., None] * 2.2
        save_png(os.path.join(preview, f'{name}_tiled.png'), np.tile(shaded, (2, 2, 1)), srgb=True)
