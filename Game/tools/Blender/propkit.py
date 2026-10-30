import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit


def options():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    return dict(zip(argv[::2], argv[1::2]))


def local(shader, name='Local'):
    return shader.node('ShaderNodeAttribute', attribute_name=name, attribute_type='GEOMETRY').outputs['Vector']


def tag(obj, frame=None, offset=None, rng=None):
    mesh = obj.data
    attribute = mesh.attributes.get('Local') or mesh.attributes.new('Local', 'FLOAT_VECTOR', 'POINT')
    inverse = frame.inverted() if frame else Matrix.Identity(4)
    shift = Vector(offset) if offset else (Vector((rng.uniform(-40.0, 40.0), rng.uniform(-40.0, 40.0), rng.uniform(-40.0, 40.0))) if rng else Vector())
    values = []
    for vertex in mesh.vertices:
        values.extend(inverse @ vertex.co + shift)
    attribute.data.foreach_set('vector', values)
    return obj


def part(name, bm, target, material, matrix=None, frame=None, rng=None, bevel=None, smooth=None):
    obj = kit.mesh_object(name, bm, target, material)
    tag(obj, frame, rng=rng)
    if bevel:
        width, segments, angle = bevel
        kit.bevel(obj, width, segments=segments, angle=angle)
    elif smooth:
        kit.smooth_by_angle(obj, smooth)
    if matrix is not None:
        obj.matrix_world = matrix
    return obj


def box(size, center=(0.0, 0.0, 0.0)):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for vertex in bm.verts:
        vertex.co = Vector((vertex.co.x * size[0] + center[0], vertex.co.y * size[1] + center[1], vertex.co.z * size[2] + center[2]))
    return bm


def chipped(size, rng, chips=2, depth=0.02, center=(0.0, 0.0, 0.0)):
    bm = box(size, center)
    hx, hy, hz = size[0] * 0.5, size[1] * 0.5, size[2] * 0.5
    for _ in range(chips):
        corner = Vector((rng.choice((-1.0, 1.0)) * hx, rng.choice((-1.0, 1.0)) * hy, rng.choice((-1.0, 1.0)) * hz)) + Vector(center)
        normal = Vector((math.copysign(rng.uniform(0.4, 1.0), corner.x - center[0]), math.copysign(rng.uniform(0.4, 1.0), corner.y - center[1]), math.copysign(rng.uniform(0.4, 1.0), corner.z - center[2]))).normalized()
        kit.cut(bm, corner - normal * depth * rng.uniform(0.6, 1.3), normal)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def lathe(profile, steps, phase=0.0):
    bm = bmesh.new()
    poles = {index: bm.verts.new((0.0, 0.0, z)) for index, (r, z) in enumerate(profile) if r <= 1e-6}
    rings = []
    for i in range(steps):
        angle = phase + 2.0 * math.pi * i / steps
        c, s = math.cos(angle), math.sin(angle)
        rings.append([poles[j] if j in poles else bm.verts.new((r * c, r * s, z)) for j, (r, z) in enumerate(profile)])
    for i in range(steps):
        a, b = rings[i], rings[(i + 1) % steps]
        for j in range(len(profile) - 1):
            if j in poles:
                bm.faces.new((a[j], b[j + 1], a[j + 1]))
            elif j + 1 in poles:
                bm.faces.new((a[j], b[j], a[j + 1]))
            else:
                bm.faces.new((a[j], b[j], b[j + 1], a[j + 1]))
    first, last = profile[0], profile[-1]
    if first[0] > 1e-6:
        bm.faces.new([ring[0] for ring in reversed(rings)])
    if last[0] > 1e-6:
        bm.faces.new([ring[-1] for ring in rings])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def cylinder(radius, height, steps=12, z=0.0, top=None):
    return lathe([(radius, z), (top if top is not None else radius, z + height)], steps)


def rod(start, end, radius, steps=8, end_radius=None):
    a, b = Vector(start), Vector(end)
    axis = b - a
    bm = lathe([(radius, 0.0), (end_radius if end_radius is not None else radius, axis.length)], steps)
    rotation = axis.normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation(a) @ rotation, verts=bm.verts)
    return bm


def torus(major, minor, steps=24, sides=8):
    bm = bmesh.new()
    rings = []
    for i in range(steps):
        a = 2.0 * math.pi * i / steps
        ring = []
        for j in range(sides):
            b = 2.0 * math.pi * j / sides
            r = major + minor * math.cos(b)
            ring.append(bm.verts.new((r * math.cos(a), r * math.sin(a), minor * math.sin(b))))
        rings.append(ring)
    for i in range(steps):
        for j in range(sides):
            bm.faces.new((rings[i][j], rings[(i + 1) % steps][j], rings[(i + 1) % steps][(j + 1) % sides], rings[i][(j + 1) % sides]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def tube(points, radius, target, material, name, resolution=4, sides=6, radii=None, closed=False, rng=None):
    curve = bpy.data.curves.new(name, 'CURVE')
    curve.dimensions = '3D'
    curve.resolution_u = resolution
    curve.bevel_depth = radius
    curve.bevel_resolution = max(1, sides // 2 - 1)
    curve.use_fill_caps = True
    spline = curve.splines.new('POLY' if resolution <= 1 else 'BEZIER')
    if spline.type == 'BEZIER':
        spline.bezier_points.add(len(points) - 1)
        for index, (point, co) in enumerate(zip(spline.bezier_points, points)):
            point.co = Vector(co)
            point.handle_left_type = 'AUTO'
            point.handle_right_type = 'AUTO'
            point.radius = radii[index] if radii else 1.0
    else:
        spline.points.add(len(points) - 1)
        for index, (point, co) in enumerate(zip(spline.points, points)):
            point.co = Vector(co).to_4d()
            point.radius = radii[index] if radii else 1.0
    spline.use_cyclic_u = closed
    obj = bpy.data.objects.new(name, curve)
    target.objects.link(obj)
    kit.select_only([obj])
    with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
        bpy.ops.object.convert(target='MESH')
    mesh_obj = bpy.context.view_layer.objects.active
    mesh_obj.name = name
    mesh_obj.data.materials.clear()
    mesh_obj.data.materials.append(material)
    tag(mesh_obj, rng=rng)
    mesh_obj.data.shade_smooth()
    return mesh_obj


def blob(elements, target, material, name, resolution=0.012, threshold=0.6, faces=None, smooth=True, rng=None):
    data = bpy.data.metaballs.new(name)
    data.resolution = resolution
    data.render_resolution = resolution
    data.threshold = threshold
    for kind, center, radius, size, rotation, stiffness in elements:
        element = data.elements.new(type=kind)
        element.co = Vector(center)
        element.radius = radius
        element.stiffness = stiffness
        if size is not None:
            element.size_x, element.size_y, element.size_z = size
        if rotation is not None:
            element.rotation = rotation
    obj = bpy.data.objects.new(name, data)
    target.objects.link(obj)
    bpy.context.view_layer.update()
    kit.select_only([obj])
    with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
        bpy.ops.object.convert(target='MESH')
    mesh_obj = bpy.context.view_layer.objects.active
    mesh_obj.name = name
    if faces:
        modifier = mesh_obj.modifiers.new('Decimate', 'DECIMATE')
        modifier.ratio = min(1.0, faces / max(len(mesh_obj.data.polygons), 1))
        kit.apply_modifiers(mesh_obj)
    mesh_obj.data.materials.clear()
    mesh_obj.data.materials.append(material)
    tag(mesh_obj, rng=rng)
    if smooth:
        mesh_obj.data.shade_smooth()
    return mesh_obj


def ellipsoid(center, size, radius=1.0, stiffness=2.0, rotation=None):
    return ('ELLIPSOID', center, radius, size, rotation, stiffness)


def ball(center, radius, stiffness=2.0):
    return ('BALL', center, radius, None, None, stiffness)


def capsule(start, end, radius, stiffness=2.0):
    a, b = Vector(start), Vector(end)
    axis = b - a
    center = (a + b) * 0.5
    rotation = Vector((1.0, 0.0, 0.0)).rotation_difference(axis.normalized()) if axis.length > 1e-6 else None
    return ('CAPSULE', tuple(center), radius, (axis.length * 0.5, 1.0, 1.0), rotation, stiffness)


def ellipsoid_bm(center, radii, rotation=None, segments=20, rings=12):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=1.0)
    matrix = Matrix.Translation(center) @ (rotation.to_matrix().to_4x4() if rotation is not None else Matrix.Identity(4)) @ Matrix.Diagonal((radii[0], radii[1], radii[2], 1.0))
    bmesh.ops.transform(bm, matrix=matrix, verts=bm.verts)
    return bm


def capsule_bm(start, end, start_radius, end_radius=None, segments=16, rings=6):
    end_radius = start_radius if end_radius is None else end_radius
    a, b = Vector(start), Vector(end)
    length = (b - a).length
    profile = [(0.0, -start_radius)]
    for i in range(1, rings):
        angle = -0.5 * math.pi + 0.5 * math.pi * i / rings
        profile.append((math.cos(angle) * start_radius, math.sin(angle) * start_radius))
    for i in range(rings + 1):
        angle = 0.5 * math.pi * i / rings
        profile.append((math.cos(angle) * end_radius, length + math.sin(angle) * end_radius))
    profile[-1] = (0.0, length + end_radius)
    bm = lathe(profile, segments)
    rotation = (b - a).normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4() if length > 1e-6 else Matrix.Identity(4)
    bmesh.ops.transform(bm, matrix=Matrix.Translation(a) @ rotation, verts=bm.verts)
    return bm


def cone_bm(base_center, tip, radius, sides=8, flatten=1.0, twist=0.0):
    a, b = Vector(base_center), Vector(tip)
    bm = bmesh.new()
    ring = [bm.verts.new((math.cos(twist + 2.0 * math.pi * i / sides) * radius, math.sin(twist + 2.0 * math.pi * i / sides) * radius * flatten, 0.0)) for i in range(sides)]
    apex = bm.verts.new((0.0, 0.0, (b - a).length))
    bm.faces.new(list(reversed(ring)))
    for i in range(sides):
        bm.faces.new((ring[i], ring[(i + 1) % sides], apex))
    rotation = (b - a).normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation(a) @ rotation, verts=bm.verts)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def merge_bm(parts):
    bm = bmesh.new()
    mesh = bpy.data.meshes.new('MergeScratch')
    for piece in parts:
        piece.to_mesh(mesh)
        bm.from_mesh(mesh)
        piece.free()
    bpy.data.meshes.remove(mesh)
    return bm


def sculpt(shapes, target, material, name, voxel, smooth=8, smooth_factor=0.6, faces=None, rng=None, carve=None):
    obj = kit.mesh_object(name, merge_bm(shapes), target, material)
    remesh_modifier = obj.modifiers.new('Remesh', 'REMESH')
    remesh_modifier.mode = 'VOXEL'
    remesh_modifier.voxel_size = voxel
    if carve:
        kit.apply_modifiers(obj)
        cutter = kit.mesh_object(name + '_Cutter', merge_bm(carve), target)
        boolean = obj.modifiers.new('Carve', 'BOOLEAN')
        boolean.operation = 'DIFFERENCE'
        boolean.solver = 'EXACT'
        boolean.object = cutter
        kit.apply_modifiers(obj)
        bpy.data.objects.remove(cutter, do_unlink=True)
        retopo = obj.modifiers.new('Remesh', 'REMESH')
        retopo.mode = 'VOXEL'
        retopo.voxel_size = voxel
    if smooth > 0:
        smoother = obj.modifiers.new('Smooth', 'SMOOTH')
        smoother.factor = smooth_factor
        smoother.iterations = smooth
    kit.apply_modifiers(obj)
    if faces:
        decimate = obj.modifiers.new('Decimate', 'DECIMATE')
        decimate.ratio = min(1.0, faces / max(len(obj.data.polygons), 1))
        kit.apply_modifiers(obj)
    tag(obj, rng=rng)
    obj.data.shade_smooth()
    return obj


def remesh(obj, voxel, faces=None):
    modifier = obj.modifiers.new('Remesh', 'REMESH')
    modifier.mode = 'VOXEL'
    modifier.voxel_size = voxel
    modifier.use_smooth_shade = True
    kit.apply_modifiers(obj)
    if faces:
        decimate = obj.modifiers.new('Decimate', 'DECIMATE')
        decimate.ratio = min(1.0, faces / max(len(obj.data.polygons), 1))
        kit.apply_modifiers(obj)
    obj.data.shade_smooth()
    return obj


def subdivide(obj, levels=1):
    modifier = obj.modifiers.new('Subsurf', 'SUBSURF')
    modifier.levels = levels
    modifier.render_levels = levels
    kit.apply_modifiers(obj)
    return obj


def place(obj, location=(0.0, 0.0, 0.0), yaw=0.0, pitch=0.0, roll=0.0, scale=1.0):
    obj.matrix_world = Matrix.Translation(location) @ Matrix.Rotation(yaw, 4, 'Z') @ Matrix.Rotation(pitch, 4, 'X') @ Matrix.Rotation(roll, 4, 'Y') @ Matrix.Scale(scale, 4)
    return obj


def bake_parts(parts):
    groups = {}
    for obj in parts:
        groups.setdefault(obj.data.materials[0].name, []).append(obj)
    return groups


def unwrap_all(parts, margins=None, angle=55.0):
    names = []
    for obj in parts:
        for slot in obj.material_slots:
            if slot.material and slot.material.name not in names and not slot.material.get('keep_uv'):
                names.append(slot.material.name)
    for name in names:
        users = [obj for obj in parts if any(slot.material and slot.material.name == name for slot in obj.material_slots)]
        margin = (margins or {}).get(name, 0.004)
        kit.select_only(users)
        bpy.ops.object.mode_set(mode='EDIT')
        for obj in users:
            index = [slot.material.name if slot.material else '' for slot in obj.material_slots].index(name)
            bm = bmesh.from_edit_mesh(obj.data)
            for face in bm.faces:
                face.select = face.material_index == index
            bm.select_flush_mode()
            bmesh.update_edit_mesh(obj.data)
        bpy.ops.uv.smart_project(angle_limit=math.radians(angle), island_margin=margin, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
        bpy.ops.uv.select_all(action='SELECT')
        bpy.ops.uv.average_islands_scale()
        bpy.ops.uv.pack_islands(rotate=True, margin=margin, shape_method='CONCAVE')
        bpy.ops.object.mode_set(mode='OBJECT')


def ground_preview(size=6.0):
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=size)
    shader = kit.Shader('PreviewGround')
    return kit.mesh_object('PreviewGround', bm, kit.collection('Preview'), shader.finish((0.16, 0.15, 0.14), 0.9))


def preview(objects, folder, name, close=True, environment='forest.exr', fill=1.25):
    os.makedirs(folder, exist_ok=True)
    scene = bpy.context.scene
    scene.view_settings.view_transform = 'Standard'
    kit.environment(environment, strength=1.0, rotation=1.2)
    kit.render(os.path.join(folder, f'{name}_game.png'), objects, yaw=0.0, pitch=57.0, fill=fill, size=(960, 640), samples=64)
    if close:
        kit.render(os.path.join(folder, f'{name}_close.png'), objects, yaw=-28.0, pitch=18.0, fill=0.9, size=(960, 640), samples=64)


def export(objects, folder, name, ao_distance=0.4):
    os.makedirs(folder, exist_ok=True)
    bpy.context.scene['ao_distance'] = ao_distance
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, name + '.blend'), compress=True)
    kit.bake_and_export(objects, folder, name, ao_distance=ao_distance)


def wear(s, width=0.02):
    edges = s.occlusion(width, samples=16, inside=True)
    return s.sub(1.0, s.smooth(0.5, 0.92, edges))


def cavity(s, distance=0.08):
    return s.sub(1.0, s.smooth(0.35, 0.95, s.occlusion(distance, samples=16)))


def wood_material(name, light=(0.3, 0.19, 0.1), dark=(0.13, 0.08, 0.045), grain=1.0, weathered=0.3, bake_size=1024):
    s = kit.Shader(name)
    p = local(s)
    island = s.geometry('Random Per Island')
    rings = s.wave(s.stretch(p, (0.08 * grain, 1.0, 1.0)), 7.0 * grain, kind='RINGS', direction='X', distortion=6.0, detail=3.0, detail_scale=1.2)
    fibre = s.noise(s.stretch(p, (0.7, 14.0, 14.0)), 2.2 * grain, detail=6.0, roughness=0.6)
    blotch = s.noise(p, 1.8, detail=3.0)
    tone = s.clamp01(s.add(s.mul(rings, 0.22), s.add(s.mul(fibre, 0.5), s.mul(island, 0.3))))
    base = s.mix(tone, dark, light)
    base = s.mix(s.mul(s.smooth(0.55, 0.8, blotch), 0.35), base, (dark[0] * 0.8, dark[1] * 0.8, dark[2] * 0.8))
    grey = (0.2, 0.19, 0.17)
    base = s.mix(s.mul(s.smooth(0.4, 0.75, s.noise(p, 3.5, detail=4.0)), weathered), base, grey)
    edge = wear(s, 0.015)
    base = s.mix(s.mul(edge, 0.35), base, (light[0] * 1.35, light[1] * 1.3, light[2] * 1.25))
    dirt = cavity(s, 0.06)
    base = s.mix(s.mul(dirt, 0.6), base, (0.035, 0.028, 0.02))
    rough = s.remap(fibre, 0.3, 0.7, 0.72, 0.9)
    rough = s.lerp(s.mul(edge, 0.5), rough, 0.6)
    height = s.add(s.mul(rings, 0.5), s.mul(fibre, 0.5))
    return s.finish(base, rough, height=height, bump=0.25, bump_distance=0.006, bake_size=bake_size)


def iron_material(name, tone=(0.05, 0.048, 0.046), rust=0.6, bake_size=1024):
    s = kit.Shader(name)
    p = local(s)
    noise = s.noise(p, 9.0, detail=4.0)
    hammer = s.voronoi(p, 26.0, 'F1', 1.0, 'Distance')
    base = s.mix(noise, tone, (tone[0] * 1.6, tone[1] * 1.6, tone[2] * 1.6))
    edge = wear(s, 0.012)
    base = s.mix(s.mul(edge, 0.7), base, (0.32, 0.31, 0.3))
    pits = cavity(s, 0.05)
    corrosion = s.mul(s.mul(pits, s.smooth(0.4, 0.7, s.noise(p, 6.0, detail=5.0))), rust)
    base = s.mix(corrosion, base, (0.14, 0.06, 0.025))
    metal = s.lerp(corrosion, 0.9, 0.25)
    rough = s.remap(noise, 0.3, 0.7, 0.48, 0.66)
    rough = s.lerp(s.mul(edge, 0.6), rough, 0.32)
    rough = s.lerp(corrosion, rough, 0.88)
    height = s.add(s.mul(hammer, 0.6), s.mul(noise, 0.4))
    return s.finish(base, rough, height=height, bump=0.2, bump_distance=0.004, metallic=metal, bake_size=bake_size)


def brass_material(name, tone=(0.8, 0.55, 0.24), bake_size=1024):
    s = kit.Shader(name)
    p = local(s)
    noise = s.noise(p, 12.0, detail=3.0)
    base = s.mix(noise, (tone[0] * 0.85, tone[1] * 0.85, tone[2] * 0.85), tone)
    pits = cavity(s, 0.04)
    base = s.mix(s.mul(pits, 0.75), base, (0.12, 0.13, 0.08))
    edge = wear(s, 0.01)
    base = s.mix(s.mul(edge, 0.5), base, (min(1.0, tone[0] * 1.15), min(1.0, tone[1] * 1.15), min(1.0, tone[2] * 1.1)))
    rough = s.remap(noise, 0.3, 0.7, 0.28, 0.42)
    rough = s.lerp(pits, rough, 0.75)
    metal = s.lerp(pits, 1.0, 0.4)
    return s.finish(base, rough, height=noise, bump=0.05, bump_distance=0.002, metallic=metal, bake_size=bake_size)


def stone_material(name, light=(0.38, 0.355, 0.31), dark=(0.2, 0.185, 0.155), moss=0.0, bake_size=1024):
    s = kit.Shader(name)
    p = local(s)
    _, _, z = s.separate(s.coords('Object'))
    large = s.noise(p, 1.1, detail=3.0)
    mid = s.noise(p, 4.5, detail=5.0, roughness=0.55)
    fine = s.noise(p, 38.0, detail=2.0)
    specks = s.sub(1.0, s.smooth(0.0, 0.14, s.voronoi(p, 85.0, 'F1', 1.0, 'Distance')))
    island = s.geometry('Random Per Island')
    tone = s.madd(s.smooth(0.3, 0.7, large), 0.45, s.add(s.mul(mid, 0.3), s.mul(island, 0.25)))
    base = s.mix(tone, dark, light)
    base = s.mix(s.mul(s.smooth(0.55, 0.75, s.noise(p, 2.6, detail=3.0)), 0.3), base, (light[0] * 0.9, light[1] * 0.75, light[2] * 0.6))
    base = s.mix(s.mul(specks, 0.3), base, (0.07, 0.066, 0.062))
    dirt = cavity(s, 0.12)
    base = s.mix(s.mul(dirt, 0.7), base, (0.05, 0.045, 0.036))
    streaks = s.smooth(0.5, 0.72, s.noise(s.stretch(p, (14.0, 14.0, 0.8)), 1.0, detail=3.0, roughness=0.6))
    base = s.mix(s.mul(streaks, 0.3), base, (0.07, 0.068, 0.058))
    grime = s.mul(s.sub(1.0, s.smooth(0.03, 0.35, z)), s.smooth(0.3, 0.6, s.noise(p, 3.5, detail=4.0)))
    base = s.mix(s.mul(grime, 0.55), base, (0.045, 0.05, 0.035))
    if moss > 0.0:
        up = s.smooth(0.55, 0.85, s.separate(s.geometry('Normal'))[2])
        patches = s.smooth(0.45, 0.65, s.noise(p, 2.2, detail=5.0, roughness=0.6))
        base = s.mix(s.mul(s.mul(up, patches), moss), base, (0.07, 0.1, 0.035))
    edge = wear(s, 0.025)
    base = s.mix(s.mul(edge, 0.3), base, (light[0] * 0.95, light[1] * 0.95, light[2] * 0.95))
    rough = s.remap(fine, 0.3, 0.7, 0.74, 0.9)
    rough = s.lerp(s.mul(edge, 0.4), rough, 0.92)
    chisel = s.wave(s.stretch(p, (1.0, 1.0, 0.4)), 14.0, kind='BANDS', direction='X', distortion=4.0, detail=2.0)
    pits = s.smooth(0.02, 0.18, s.voronoi(p, 30.0, 'F1', 1.0, 'Distance'))
    height = s.add(s.mul(mid, 0.8), s.add(s.mul(chisel, 0.1), s.add(s.mul(pits, 0.05), s.mul(fine, 0.03))))
    return s.finish(base, rough, height=height, bump=0.22, bump_distance=0.012, bake_size=bake_size)


def masonry_material(name, light=(0.36, 0.33, 0.29), dark=(0.17, 0.155, 0.135), brick=(0.32, 0.16), mortar=(0.11, 0.1, 0.085), soot=0.0, bake_size=2048, radius=None):
    s = kit.Shader(name)
    p = local(s)
    x, y, z = s.separate(p)
    node = s.node('ShaderNodeTexBrick', offset=0.5, offset_frequency=2, squash=1.0, squash_frequency=2)
    across = s.mul(s.math('ARCTAN2', s.mul(x, -1.0), s.mul(y, -1.0)), radius) if radius else s.add(x, y)
    s.feed(node.inputs['Vector'], s.combine(across, z, 0.0))
    s.feed(node.inputs['Color1'], (1.0, 1.0, 1.0))
    s.feed(node.inputs['Color2'], (0.0, 0.0, 0.0))
    s.feed(node.inputs['Mortar'], (0.0, 0.0, 0.0))
    s.feed(node.inputs['Scale'], 1.0)
    s.feed(node.inputs['Mortar Size'], 0.018)
    s.feed(node.inputs['Mortar Smooth'], 0.4)
    s.feed(node.inputs['Bias'], 0.0)
    s.feed(node.inputs['Brick Width'], brick[0])
    s.feed(node.inputs['Row Height'], brick[1])
    joints = node.outputs['Fac']
    per_brick = node.outputs['Color']
    large = s.noise(p, 1.4, detail=3.0)
    mid = s.noise(p, 6.0, detail=5.0, roughness=0.55)
    tone = s.clamp01(s.add(s.mul(s.separate(per_brick)[0], 0.5), s.add(s.mul(large, 0.35), s.mul(mid, 0.25))))
    base = s.mix(tone, dark, light)
    base = s.mix(s.smooth(0.2, 0.6, joints), base, mortar)
    dirt = cavity(s, 0.1)
    base = s.mix(s.mul(dirt, 0.6), base, (0.04, 0.035, 0.03))
    if soot > 0.0:
        sooty = s.mul(s.smooth(0.4, 0.75, s.noise(s.stretch(p, (3.0, 3.0, 0.8)), 1.6, detail=4.0)), soot)
        base = s.mix(sooty, base, (0.03, 0.026, 0.024))
    edge = wear(s, 0.025)
    base = s.mix(s.mul(edge, 0.25), base, (light[0], light[1], light[2]))
    rough = s.remap(mid, 0.3, 0.7, 0.78, 0.92)
    height = s.add(s.mul(s.sub(1.0, s.smooth(0.2, 0.6, joints)), -0.6), s.mul(mid, 0.4))
    return s.finish(base, rough, height=height, bump=0.35, bump_distance=0.012, bake_size=bake_size)


def leather_material(name, tone=(0.22, 0.09, 0.05), bake_size=512):
    s = kit.Shader(name)
    p = local(s)
    grain = s.voronoi(p, 120.0, 'F1', 1.0, 'Distance')
    blotch = s.noise(p, 6.0, detail=4.0)
    base = s.mix(s.smooth(0.35, 0.7, blotch), (tone[0] * 0.7, tone[1] * 0.65, tone[2] * 0.6), tone)
    edge = wear(s, 0.01)
    base = s.mix(s.mul(edge, 0.5), base, (tone[0] * 1.6, tone[1] * 1.5, tone[2] * 1.4))
    dirt = cavity(s, 0.03)
    base = s.mix(s.mul(dirt, 0.5), base, (0.04, 0.02, 0.012))
    rough = s.remap(blotch, 0.3, 0.7, 0.5, 0.7)
    rough = s.lerp(s.mul(edge, 0.5), rough, 0.4)
    return s.finish(base, rough, height=grain, bump=0.15, bump_distance=0.002, bake_size=bake_size)


def bark_material(name, tone=(0.16, 0.12, 0.085), rings=(0.42, 0.32, 0.2), bake_size=1024):
    s = kit.Shader(name)
    p = local(s)
    x, y, z = s.separate(p)
    radial = s.vector('LENGTH', s.stretch(p, (1.0, 1.0, 0.0)))
    ridges = s.noise(s.stretch(p, (14.0, 14.0, 1.5)), 1.0, detail=5.0, roughness=0.6, distortion=1.2)
    bark = s.mix(s.smooth(0.35, 0.7, ridges), (tone[0] * 0.5, tone[1] * 0.5, tone[2] * 0.5), tone)
    up = s.smooth(0.6, 0.85, s.separate(s.geometry('Normal'))[2])
    growth = s.wave(s.stretch(p, (1.0, 1.0, 0.0)), 10.0, kind='RINGS', direction='SPHERICAL', distortion=2.0, detail=2.0)
    cut = s.mix(s.mul(growth, 0.45), rings, (rings[0] * 0.65, rings[1] * 0.6, rings[2] * 0.55))
    cracks = s.sub(1.0, s.smooth(0.0, 0.02, s.voronoi(s.stretch(p, (2.0, 2.0, 0.0)), 6.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
    cut = s.mix(s.mul(cracks, 0.7), cut, (0.05, 0.035, 0.02))
    base = s.mix(up, bark, cut)
    dirt = cavity(s, 0.06)
    base = s.mix(s.mul(dirt, 0.6), base, (0.03, 0.022, 0.015))
    rough = s.lerp(up, 0.9, 0.78)
    height = s.lerp(up, ridges, s.mul(cracks, -1.0))
    return s.finish(base, rough, height=height, bump=0.45, bump_distance=0.01, bake_size=bake_size)


def map_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = local(s)
    x, y, z = s.separate(p)
    land = s.noise(s.combine(x, z, 0.0), 2.6, detail=6.0, roughness=0.55)
    coast = s.sub(1.0, s.smooth(0.0, 0.012, s.math('ABSOLUTE', s.sub(land, 0.5))))
    sea = s.smooth(0.5, 0.48, land)
    base = s.mix(sea, (0.74, 0.66, 0.5), (0.55, 0.6, 0.56))
    stains = s.smooth(0.45, 0.8, s.noise(p, 3.0, detail=4.0))
    base = s.mix(s.mul(stains, 0.35), base, (0.5, 0.4, 0.26))
    base = s.mix(s.mul(coast, 0.85), base, (0.12, 0.08, 0.05))
    route = s.sub(1.0, s.smooth(0.004, 0.008, s.math('ABSOLUTE', s.sub(z, s.add(s.mul(s.math('SINE', s.mul(x, 9.0)), 0.06), s.mul(x, 0.2))))))
    base = s.mix(s.mul(route, 0.9), base, (0.55, 0.05, 0.04))
    edge = cavity(s, 0.02)
    base = s.mix(s.mul(edge, 0.4), base, (0.3, 0.22, 0.12))
    return s.finish(base, 0.85, height=stains, bump=0.05, bump_distance=0.002, bake_size=bake_size)


def rune_page_material(name, glow=(0.62, 0.42, 1.0), strength=2.2, bake_size=512):
    s = kit.Shader(name)
    p = local(s)
    x, y, z = s.separate(p)
    tone = (0.78, 0.71, 0.56)
    stains = s.smooth(0.45, 0.8, s.noise(p, 4.0, detail=4.0))
    base = s.mix(s.mul(stains, 0.35), tone, (tone[0] * 0.78, tone[1] * 0.68, tone[2] * 0.48))
    rows = s.wave(p, 42.0, kind='BANDS', direction='Y', distortion=0.4, detail=1.0)
    gaps = s.smooth(0.3, 0.55, s.noise(s.stretch(p, (60.0, 3.0, 60.0)), 1.0, detail=2.0))
    scribble = s.mul(s.sub(1.0, s.smooth(0.08, 0.28, rows)), gaps)
    circle = s.vector('LENGTH', s.stretch(p, (1.0, 0.0, 1.0)))
    rune = s.mul(s.sub(1.0, s.smooth(0.004, 0.009, s.math('ABSOLUTE', s.sub(s.math('MODULO', s.mul(circle, 18.0), 1.0), 0.5)))), s.sub(1.0, s.smooth(0.07, 0.09, circle)))
    ink = s.math('MAXIMUM', s.mul(scribble, 0.8), rune)
    base = s.mix(ink, base, (0.25, 0.12, 0.42))
    return s.finish(base, 0.82, height=stains, bump=0.04, bump_distance=0.002, emission=s.mul(ink, 0.9), emission_color=glow, emission_strength=strength, bake_size=bake_size)


def wax_material(name, tone=(0.78, 0.72, 0.6), bake_size=512):
    s = kit.Shader(name)
    p = local(s)
    drip = s.noise(s.stretch(p, (18.0, 18.0, 2.0)), 1.0, detail=3.0)
    base = s.mix(s.smooth(0.4, 0.7, drip), (tone[0] * 0.9, tone[1] * 0.88, tone[2] * 0.82), tone)
    dirt = cavity(s, 0.02)
    base = s.mix(s.mul(dirt, 0.4), base, (0.35, 0.28, 0.18))
    return s.finish(base, 0.48, height=drip, bump=0.1, bump_distance=0.003, bake_size=bake_size)


def ember_material(name, bake_size=512, strength=4.0):
    s = kit.Shader(name)
    p = local(s)
    warped = s.offset(p, s.mul(s.noise_color(p, 4.0, detail=2.0), 0.08))
    cells = s.voronoi(warped, 11.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    cracks = s.sub(1.0, s.smooth(0.0, 0.035, cells))
    heat = s.smooth(0.42, 0.72, s.noise(p, 3.2, detail=4.0))
    glow = s.clamp01(s.add(s.mul(cracks, heat), s.mul(heat, 0.18)))
    ash = s.mul(s.smooth(0.55, 0.8, s.noise(p, 7.0, detail=3.0)), s.sub(1.0, heat))
    base = s.mix(ash, (0.022, 0.019, 0.017), (0.13, 0.12, 0.11))
    base = s.mix(glow, base, (0.5, 0.12, 0.03))
    rough = s.lerp(glow, 0.92, 0.5)
    return s.finish(base, rough, height=s.smooth(0.0, 0.2, cells), bump=0.3, bump_distance=0.006, emission=glow, emission_color=(1.0, 0.36, 0.08), emission_strength=strength, bake_size=bake_size)


def glow_material(name, color=(1.0, 0.62, 0.28), strength=3.0, base=(0.9, 0.62, 0.32), bake_size=256):
    s = kit.Shader(name)
    p = local(s)
    mottled = s.noise(p, 20.0, detail=3.0)
    glow = s.remap(mottled, 0.2, 0.8, 0.75, 1.0)
    return s.finish(base, 0.25, height=mottled, bump=0.05, bump_distance=0.002, emission=glow, emission_color=color, emission_strength=strength, bake_size=bake_size)


def paper_material(name, tone=(0.72, 0.64, 0.48), ink=(0.08, 0.06, 0.05), lines=True, bake_size=512):
    s = kit.Shader(name)
    p = local(s)
    x, y, z = s.separate(p)
    stains = s.smooth(0.45, 0.8, s.noise(p, 3.0, detail=4.0))
    base = s.mix(s.mul(stains, 0.4), tone, (tone[0] * 0.75, tone[1] * 0.62, tone[2] * 0.42))
    if lines:
        rows = s.wave(p, 28.0, kind='BANDS', direction='Z', distortion=0.5, detail=1.0)
        gaps = s.smooth(0.35, 0.6, s.noise(s.stretch(p, (5.0, 5.0, 40.0)), 1.0, detail=2.0))
        scribble = s.mul(s.sub(1.0, s.smooth(0.08, 0.3, rows)), gaps)
        base = s.mix(s.mul(scribble, 0.7), base, ink)
    edge = cavity(s, 0.02)
    base = s.mix(s.mul(edge, 0.4), base, (0.3, 0.22, 0.12))
    return s.finish(base, 0.85, height=stains, bump=0.05, bump_distance=0.002, bake_size=bake_size)


def burlap_material(name, tone=(0.55, 0.43, 0.27), bake_size=1024, target=False):
    s = kit.Shader(name)
    p = local(s)
    x, y, z = s.separate(p)
    warp = s.wave(p, 120.0, kind='BANDS', direction='X', distortion=1.5, detail=1.0)
    weft = s.wave(p, 120.0, kind='BANDS', direction='Z', distortion=1.5, detail=1.0)
    weave = s.mul(warp, weft)
    blotch = s.noise(p, 4.0, detail=4.0)
    base = s.mix(s.add(s.mul(weave, 0.35), s.mul(blotch, 0.5)), (tone[0] * 0.7, tone[1] * 0.66, tone[2] * 0.6), tone)
    dirt = cavity(s, 0.05)
    base = s.mix(s.mul(dirt, 0.6), base, (0.12, 0.09, 0.05))
    if target:
        radial = s.vector('LENGTH', s.stretch(s.offset(p, (0.0, 0.0, -1.08)), (1.0, 0.0, 1.0)))
        front = s.smooth(-0.05, -0.15, y)
        ring = s.math('MODULO', s.mul(radial, 1.0 / 0.035), 2.0)
        red = s.mul(s.mul(s.smooth(0.8, 1.2, ring), s.sub(1.0, s.smooth(0.15, 0.17, radial))), front)
        paint_tone = s.mix(s.smooth(0.3, 0.7, s.noise(p, 30.0, detail=2.0)), (0.42, 0.04, 0.03), (0.6, 0.07, 0.04))
        base = s.mix(s.mul(red, 0.92), base, paint_tone)
        face = s.mul(s.smooth(-0.08, -0.14, y), s.mul(s.smooth(1.45, 1.47, z), s.sub(1.0, s.smooth(1.72, 1.74, z))))
        stitches = 0.0
        for cx in (-0.075, 0.075):
            dx = s.sub(x, cx)
            dz = s.sub(z, 1.625)
            inside = s.mul(s.sub(1.0, s.smooth(0.026, 0.03, s.math('ABSOLUTE', dx))), s.sub(1.0, s.smooth(0.026, 0.03, s.math('ABSOLUTE', dz))))
            cross = s.math('MINIMUM', s.math('ABSOLUTE', s.sub(dx, dz)), s.math('ABSOLUTE', s.add(dx, dz)))
            mark = s.mul(s.sub(1.0, s.smooth(0.004, 0.008, cross)), inside)
            stitches = mark if stitches == 0.0 else s.math('MAXIMUM', stitches, mark)
        mouth_line = s.sub(1.0, s.smooth(0.003, 0.006, s.math('ABSOLUTE', s.sub(z, 1.525))))
        mouth_span = s.sub(1.0, s.smooth(0.075, 0.08, s.math('ABSOLUTE', x)))
        dashes = s.smooth(0.35, 0.45, s.math('FRACT', s.mul(x, 45.0)))
        mouth = s.mul(s.mul(mouth_line, mouth_span), dashes)
        stitches = s.mul(s.math('MAXIMUM', stitches, mouth), face)
        base = s.mix(stitches, base, (0.05, 0.035, 0.025))
    return s.finish(base, 0.93, height=weave, bump=0.35, bump_distance=0.003, bake_size=bake_size)


def straw_material(name, tone=(0.72, 0.58, 0.3), bake_size=512):
    s = kit.Shader(name)
    p = local(s)
    strands = s.noise(s.stretch(p, (60.0, 60.0, 2.5)), 1.0, detail=4.0)
    base = s.mix(s.smooth(0.3, 0.7, strands), (tone[0] * 0.6, tone[1] * 0.55, tone[2] * 0.45), tone)
    dirt = cavity(s, 0.03)
    base = s.mix(s.mul(dirt, 0.6), base, (0.15, 0.11, 0.05))
    return s.finish(base, 0.88, height=strands, bump=0.4, bump_distance=0.003, bake_size=bake_size)


def rope_material(name, tone=(0.36, 0.27, 0.16), bake_size=512):
    s = kit.Shader(name)
    p = local(s)
    twist = s.wave(p, 140.0, kind='BANDS', direction='DIAGONAL', distortion=3.0, detail=3.0, detail_scale=2.0)
    fibres = s.noise(p, 90.0, detail=3.0)
    twist = s.add(s.mul(twist, 0.7), s.mul(fibres, 0.3))
    base = s.mix(twist, (tone[0] * 0.55, tone[1] * 0.52, tone[2] * 0.5), tone)
    dirt = cavity(s, 0.02)
    base = s.mix(s.mul(dirt, 0.5), base, (0.1, 0.08, 0.05))
    return s.finish(base, 0.9, height=twist, bump=0.4, bump_distance=0.003, bake_size=bake_size)


def fur_material(name, tone=(0.025, 0.023, 0.028), sheen=(0.09, 0.085, 0.1), bake_size=512):
    s = kit.Shader(name)
    p = local(s)
    strands = s.noise(s.stretch(p, (40.0, 40.0, 8.0)), 1.0, detail=5.0)
    patches = s.noise(p, 6.0, detail=3.0)
    base = s.mix(s.mul(s.smooth(0.4, 0.75, patches), 0.6), tone, sheen)
    base = s.mix(s.mul(s.smooth(0.55, 0.8, strands), 0.3), base, sheen)
    return s.finish(base, s.remap(strands, 0.3, 0.7, 0.62, 0.8), height=strands, bump=0.3, bump_distance=0.002, bake_size=bake_size)


def rubber_material(name, tone=(0.95, 0.68, 0.06), bake_size=512, roughness=0.32):
    s = kit.Shader(name)
    p = local(s)
    mottled = s.noise(p, 25.0, detail=2.0)
    base = s.mix(s.mul(mottled, 0.15), tone, (tone[0] * 0.85, tone[1] * 0.82, tone[2] * 0.8))
    dirt = cavity(s, 0.03)
    base = s.mix(s.mul(dirt, 0.35), base, (tone[0] * 0.4, tone[1] * 0.35, tone[2] * 0.3))
    return s.finish(base, s.remap(mottled, 0.3, 0.7, roughness - 0.04, roughness + 0.06), height=mottled, bump=0.02, bump_distance=0.001, specular=0.6, bake_size=bake_size)


def crystal_material(name, core=(0.55, 0.3, 1.0), shell=(0.08, 0.05, 0.16), strength=3.0, bake_size=512, veins=0.35, cells=9.0, thickness=0.12, translucency=0.0):
    s = kit.Shader(name)
    p = local(s)
    _, _, z = s.separate(s.coords('Object'))
    inner = s.occlusion(thickness, samples=24, inside=True)
    depth = s.smooth(0.25, 0.9, inner)
    edges = s.sub(1.0, s.smooth(0.0, 0.05, s.voronoi(p, cells, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
    glow = s.clamp01(s.add(s.mul(s.sub(1.0, depth), 0.55), s.mul(edges, veins)))
    glow = s.mul(glow, s.add(0.55, s.mul(s.smooth(0.0, 1.2, z), 0.45)))
    base = s.mix(glow, shell, core)
    return s.finish(base, 0.12, height=edges, bump=0.08 * veins / 0.35, bump_distance=0.003, emission=glow, emission_color=core, emission_strength=strength, specular=0.7, bake_size=bake_size, translucency=translucency, thickness=thickness * 2.0)


def eye_material(name, color=(0.75, 0.85, 0.2), strength=3.0):
    s = kit.Shader(name)
    p = local(s)
    radial = s.vector('LENGTH', s.stretch(p, (1.0, 0.0, 1.0)))
    pupil = s.sub(1.0, s.smooth(0.004, 0.008, s.math('ABSOLUTE', s.separate(p)[0])))
    glow = s.mul(s.sub(1.0, pupil), s.sub(1.0, s.smooth(0.01, 0.03, radial)))
    base = s.mix(pupil, color, (0.01, 0.01, 0.01))
    return s.finish(base, 0.15, emission=s.clamp01(s.add(glow, 0.4)), emission_color=color, emission_strength=strength, bake_size=128)
