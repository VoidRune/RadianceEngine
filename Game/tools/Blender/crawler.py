import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector, noise
from mathutils.bvhtree import BVHTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

NAME = 'Crawler'
SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'assets', 'Source', 'CrimsonShardSpider.obj')
SCALE = 0.75
CENTRE = Vector((0.0, -0.1, 0.0))
SOCKET = Vector((0.0, 0.05, 0.38))
LEG_RADIUS = 0.11
FEMUR_OVERLAP = 0.03
FACET_ANGLE = 12.0
LEGS = {
    'front': ((0.19, -0.495, 0.36), (0.355, -0.69, 0.53), (0.48, -0.83, 0.38), (0.53, -0.89, 0.02)),
    'middle': ((0.25, -0.27, 0.35), (0.44, -0.27, 0.51), (0.61, -0.27, 0.36), (0.68, -0.27, 0.02)),
    'back': ((0.225, 0.02, 0.37), (0.4, 0.17, 0.5), (0.56, 0.31, 0.32), (0.62, 0.36, 0.02)),
}
EYES = [(-0.075, 0.501), (-0.041, 0.479), (0.041, 0.479), (0.073, 0.501), (-0.08, 0.4525), (0.0, 0.45), (0.077, 0.4525), (-0.0525, 0.4225), (0.045, 0.4225)]
EYE_RADIUS = 0.021
BUDGET = {'Shell': 2600, 'Crystal': 1400, 'Femur': 130, 'Tibia': 210, 'Eyes': 260, 'Chunk': 140}
CHUNKS = 9
BUDS = [((0.1, -0.09), 38.0, -35.0, 0.03, 0.16), ((-0.1, -0.11), 36.0, 35.0, 0.028, 0.14), ((0.0, -0.17), 24.0, 0.0, 0.025, 0.12)]


def attribute(s, name):
    return s.node('ShaderNodeAttribute', attribute_name=name, attribute_type='GEOMETRY').outputs['Fac']


def chitin_material(name, tip=None, seam=False):
    s = kit.Shader(name)
    p = pk.local(s)
    _, y, _ = s.separate(s.coords('Object'))
    tone = s.noise(p, 3.0, detail=3.0)
    mottle = s.noise(p, 10.0, detail=2.0)
    base = s.mix(s.smooth(0.3, 0.78, tone), (0.11, 0.105, 0.115), (0.16, 0.152, 0.165))
    base = s.mix(s.mul(s.smooth(0.45, 0.7, mottle), 0.35), base, (0.19, 0.18, 0.195))
    cavity = pk.cavity(s, 0.04)
    base = s.mix(s.mul(cavity, 0.8), base, (0.03, 0.028, 0.032))
    rim = s.mul(pk.wear(s, 0.012), s.add(0.55, s.mul(s.smooth(0.35, 0.65, s.noise(p, 7.0, detail=2.0)), 0.45)))
    base = s.mix(s.mul(rim, 0.85), base, (0.46, 0.44, 0.48))
    emission = None
    if seam:
        near = s.sub(1.0, s.smooth(0.0, 0.2, attribute(s, 'Seam')))
        base = s.mix(s.smooth(-0.42, 0.06, y), s.mul(base, 0.72), s.mul(base, 1.3))
        base = s.mix(s.mul(near, 0.8), base, s.mul(base, 1.9))
        cracks = s.sub(1.0, s.smooth(0.0, 0.01, s.voronoi(p, 16.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
        veins = s.mul(s.mul(cracks, s.smooth(0.42, 0.62, s.noise(p, 5.0, detail=2.0))), s.sub(1.0, s.smooth(0.0, 0.17, attribute(s, 'Seam'))))
        emission = s.mul(veins, s.sub(1.0, cavity))
    if tip is not None:
        start, end, color = tip
        base = s.mix(s.smooth(start, end, s.mul(y, -1.0)), base, color)
        band = s.sub(1.0, s.smooth(0.0, 0.045, s.math('ABSOLUTE', s.add(y, 0.01))))
        base = s.mix(s.mul(band, 0.75), base, (0.03, 0.028, 0.03))
    rough = s.remap(rim, 0.0, 1.0, 0.5, 0.32)
    rough = s.add(rough, s.mul(cavity, 0.15))
    height = s.add(s.mul(mottle, 0.25), s.mul(s.noise(p, 22.0, detail=1.0), 0.05))
    return s.finish(base, rough, height=height, bump=0.05, bump_distance=0.002, metallic=s.mul(rim, 0.15), specular=0.6, bake_size=1024, emission=emission, emission_color=(1.0, 1.0, 1.0), emission_strength=1.0)


def crust_material(name):
    s = kit.Shader(name)
    p = pk.local(s)
    large = s.noise(p, 6.0, detail=4.0, roughness=0.55)
    grains = s.sub(1.0, s.smooth(0.0, 0.12, s.voronoi(p, 70.0, 'F1', 1.0, 'Distance')))
    sparkle = s.mul(grains, s.smooth(0.55, 0.75, s.noise(p, 11.0, detail=2.0)))
    base = s.mix(s.smooth(0.3, 0.75, large), (0.025, 0.022, 0.026), (0.07, 0.062, 0.068))
    base = s.mix(s.mul(pk.cavity(s, 0.03), 0.7), base, (0.012, 0.01, 0.012))
    base = s.mix(sparkle, base, (0.55, 0.58, 0.62))
    rough = s.remap(sparkle, 0.0, 1.0, 0.8, 0.15)
    height = s.add(s.mul(large, 0.6), s.mul(grains, 0.15))
    return s.finish(base, rough, height=height, bump=0.25, bump_distance=0.004, bake_size=512)


def eye_material(name):
    s = kit.Shader(name)
    return s.finish((0.92, 0.95, 1.0), 0.06, specular=0.9, bake_size=64)


def crystal_material(name):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(s.coords('Object'))
    depth = s.smooth(0.2, 0.85, s.occlusion(0.12, samples=24, inside=True))
    planes = s.mul(s.sub(1.0, s.smooth(0.0, 0.02, s.voronoi(s.stretch(p, (1.0, 1.0, 0.45)), 2.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.5, 0.66, s.noise(p, 2.0, detail=2.0)))
    hairline = s.mul(s.sub(1.0, s.smooth(0.0, 0.006, s.voronoi(p, 9.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.6, 0.72, s.noise(p, 4.0, detail=2.0)))
    frost = s.sub(1.0, s.smooth(0.0, 0.11, attribute(s, 'Seam')))
    heart = s.mul(s.sub(1.0, s.smooth(0.0, 0.24, z)), 0.5)
    edge = pk.wear(s, 0.01)
    base = s.mix(s.sub(1.0, depth), (0.82, 0.86, 0.92), (0.95, 0.97, 1.0))
    base = s.mix(s.mul(edge, 0.7), base, (1.0, 1.0, 1.0))
    rough = s.add(s.add(0.02, s.mul(hairline, 0.1)), s.mul(frost, 0.6))
    s.material['preview_transmission'] = 1.0
    return s.finish(base, rough, height=s.add(s.mul(planes, 0.2), s.mul(hairline, 0.1)), bump=0.03, bump_distance=0.002, emission=s.add(s.add(s.mul(planes, 0.8), heart), s.mul(frost, 0.35)), emission_color=(1.0, 1.0, 1.0), emission_strength=0.8, specular=0.9, bake_size=512, transmission=1.0, ior=1.62, attenuation=((0.96, 0.97, 0.99), 0.6))


def load_source(path):
    bpy.ops.wm.obj_import(filepath=path)
    obj = bpy.context.selected_objects[0]
    obj.data.transform(obj.matrix_world)
    obj.matrix_world = Matrix.Identity(4)
    low = min(v.co.z for v in obj.data.vertices)
    obj.data.transform(Matrix.Translation((0.0, 0.0, -low)))
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.to_mesh(obj.data)
    bm.free()
    return obj


def leg_chains():
    chains = []
    for pair, (name, points) in enumerate(LEGS.items()):
        for side in (1.0, -1.0):
            chains.append((pair * 2 + (0 if side < 0.0 else 1), [Vector((x * side, y, z)) for x, y, z in points]))
    return chains


def segment_distance(point, a, b):
    span = b - a
    t = (point - a).dot(span) / span.length_squared
    clamped = min(max(t, 0.0), 1.0)
    return (point - (a + span * clamped)).length, t


def eye_points(source):
    tree = BVHTree.FromObject(source, bpy.context.evaluated_depsgraph_get())
    points = []
    for x, z in EYES:
        hit, _, _, _ = tree.ray_cast(Vector((x, -3.0, z)), Vector((0.0, 1.0, 0.0)))
        if hit is not None:
            points.append(hit)
    return points


def crystal_score(centre):
    rear = (centre.y + 0.32) / 0.32
    lift = (centre.z - 0.52) / 0.16
    score = max(0.0, min(rear, 1.0)) + 0.9 * max(0.0, min(lift, 1.0)) - 0.35 + 0.22 * noise.noise(centre * 7.0)
    return 1.0 if centre.y > 0.04 else score


def classify(bm, eyes):
    chains = leg_chains()
    labels = {}
    open_faces = []
    for face in bm.faces:
        centre = face.calc_center_median()
        if any((centre - eye).length < EYE_RADIUS for eye in eyes):
            labels[face.index] = 'Eyes'
            continue
        best = LEG_RADIUS
        for index, chain in chains:
            hip_distance, hip_t = segment_distance(centre, chain[0], chain[1])
            if hip_t < 0.0:
                continue
            lower = min(segment_distance(centre, chain[1], chain[2])[0], segment_distance(centre, chain[2], chain[3])[0])
            distance = min(hip_distance, lower)
            if distance < best:
                best = distance
                labels[face.index] = ('Femur' if hip_distance <= lower else 'Tibia', index)
        if face.index not in labels:
            open_faces.append(face)
    seen = set()
    limit = math.cos(math.radians(FACET_ANGLE))
    for face in open_faces:
        if face.index in seen:
            continue
        region = [face]
        seen.add(face.index)
        for current in region:
            for edge in current.edges:
                for other in edge.link_faces:
                    if other.index not in seen and other.index not in labels and other.normal.dot(current.normal) > limit:
                        seen.add(other.index)
                        region.append(other)
        area = sum(f.calc_area() for f in region)
        score = sum(crystal_score(f.calc_center_median()) * f.calc_area() for f in region) / max(area, 1e-9)
        for f in region:
            labels[f.index] = 'Crystal' if score > 0.5 else 'Shell'
    return labels


def extract(source, labels, keep, name, target, material):
    bm = bmesh.new()
    bm.from_mesh(source.data)
    bm.faces.ensure_lookup_table()
    doomed = [face for face in bm.faces if not keep(labels[face.index])]
    bmesh.ops.delete(bm, geom=doomed, context='FACES')
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context='VERTS')
    return kit.mesh_object(name, bm, target, material)


def seam_lip(source, labels, target, material, rng):
    bm = bmesh.new()
    bm.from_mesh(source.data)
    bm.faces.ensure_lookup_table()
    points = []
    for edge in bm.edges:
        faces = edge.link_faces
        if len(faces) != 2:
            continue
        kinds = {labels[faces[0].index], labels[faces[1].index]}
        if kinds != {'Crystal', 'Shell'}:
            continue
        middle = (edge.verts[0].co + edge.verts[1].co) * 0.5
        if middle.y < -0.12 or middle.z < 0.28:
            continue
        shell = faces[0] if labels[faces[0].index] == 'Shell' else faces[1]
        if all((middle - p).length > 0.075 for p, _ in points):
            points.append((middle.copy(), shell.normal.copy()))
    bm.free()
    plates = []
    for point, normal in points:
        direction = (normal * 0.7 + Vector((0.0, 0.35, 0.8))).normalized()
        plates.append(pk.cone_bm(point - normal * 0.015, point + direction * rng.uniform(0.06, 0.09), rng.uniform(0.035, 0.05), sides=4, flatten=0.45, twist=rng.uniform(0.0, 3.0)))
    return pk.part('Lip', pk.merge_bm(plates), target, material, rng=rng, smooth=10.0) if plates else None


def simplify(obj, budget, angle=4.0):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.holes_fill(bm, edges=[edge for edge in bm.edges if edge.is_boundary], sides=0)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()
    faces = len(obj.data.polygons)
    if faces > budget:
        modifier = obj.modifiers.new('Decimate', 'DECIMATE')
        modifier.ratio = budget / faces
        modifier.use_collapse_triangulate = True
        kit.apply_modifiers(obj)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.index_update()
    seen = set()
    stray = []
    for face in bm.faces:
        if face.index in seen:
            continue
        island = [face]
        seen.add(face.index)
        for current in island:
            for edge in current.edges:
                for other in edge.link_faces:
                    if other.index not in seen:
                        seen.add(other.index)
                        island.append(other)
        if len(island) < 24:
            stray += island
    bmesh.ops.delete(bm, geom=stray, context='FACES')
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(angle), verts=bm.verts, edges=bm.edges)
    bmesh.ops.triangulate(bm, faces=bm.faces, quad_method='BEAUTY', ngon_method='BEAUTY')
    bm.to_mesh(obj.data)
    bm.free()
    pk.tag(obj, rng=random.Random(len(obj.name)))
    return obj


def crisp(obj):
    kit.smooth_by_angle(obj, 24.0)
    modifier = obj.modifiers.new('Weighted', 'WEIGHTED_NORMAL')
    modifier.keep_sharp = True
    modifier.weight = 60
    kit.apply_modifiers(obj)
    return obj


def segment_frame(start, end):
    forward = (end - start).normalized()
    up = Vector((0.0, 0.0, 1.0))
    up = (up - forward * up.dot(forward)).normalized()
    side = up.cross(forward)
    return Matrix(((side.x, side.y, side.z, 0.0), (-forward.x, -forward.y, -forward.z, 0.0), (up.x, up.y, up.z, 0.0), (0.0, 0.0, 0.0, 1.0))) @ Matrix.Translation(-start)


def scaled(matrix):
    return Matrix.Diagonal((SCALE, SCALE, SCALE, 1.0)) @ matrix


def trim_segment(obj, length):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    doomed = [face for face in bm.faces if -face.calc_center_median().y > length or -face.calc_center_median().y < -FEMUR_OVERLAP]
    bmesh.ops.delete(bm, geom=doomed, context='FACES')
    bm.to_mesh(obj.data)
    bm.free()


def store_seam(obj, other):
    tree = BVHTree.FromObject(other, bpy.context.evaluated_depsgraph_get())
    layer = obj.data.attributes.get('Seam') or obj.data.attributes.new('Seam', 'FLOAT', 'POINT')
    values = []
    for vertex in obj.data.vertices:
        nearest = tree.find_nearest(vertex.co)
        values.append(nearest[3] if nearest[0] is not None else 1.0)
    layer.data.foreach_set('value', values)


def raw_shard(rng, radius_x, radius_y, height, sides=6):
    angles = sorted(2.0 * math.pi * (i + rng.uniform(-0.3, 0.3)) / sides for i in range(sides))
    spokes = [rng.uniform(0.8, 1.15) for _ in range(sides)]
    bm = bmesh.new()
    rings = []
    for z, scale in ((-0.05, 0.9), (height * 0.3, 1.0), (height * 0.62, 0.72)):
        rings.append([bm.verts.new((math.cos(a) * radius_x * scale * spoke, math.sin(a) * radius_y * scale * spoke, z)) for a, spoke in zip(angles, spokes)])
    apex = bm.verts.new((rng.uniform(-0.2, 0.2) * radius_x, rng.uniform(-0.2, 0.2) * radius_y, height))
    bm.faces.new(list(reversed(rings[0])))
    for lower, upper in zip(rings, rings[1:]):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((lower[i], lower[j], upper[j], upper[i]))
    for i in range(sides):
        bm.faces.new((rings[-1][i], rings[-1][(i + 1) % sides], apex))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    angle = rng.uniform(0.0, 2.0 * math.pi)
    kit.cut(bm, Vector((math.cos(angle) * radius_x * 0.3, math.sin(angle) * radius_y * 0.3, height * 0.78)), Vector((math.cos(angle), math.sin(angle), 1.3)).normalized())
    return bm


def buds(shell, target, material, rng):
    tree = BVHTree.FromObject(shell, bpy.context.evaluated_depsgraph_get())
    socket = (SOCKET - CENTRE) * SCALE
    objects = []
    for index, ((x, y), lean, heading, radius, height) in enumerate(BUDS):
        hit, normal, _, _ = tree.ray_cast(Vector((x, y, 3.0)), Vector((0.0, 0.0, -1.0)))
        base = (hit if hit is not None else Vector((x, y, 0.3))) - socket - Vector((0.0, 0.0, 0.02))
        bm = raw_shard(rng, radius, radius * 0.9, height)
        bmesh.ops.transform(bm, matrix=Matrix.Translation(base) @ Matrix.Rotation(math.radians(heading), 4, 'Z') @ Matrix.Rotation(math.radians(-lean), 4, 'X'), verts=bm.verts)
        bud = pk.part(f'Bud{index}', bm, target, material, rng=rng)
        kit.bevel(bud, 0.004, segments=1, angle=25.0, harden=True)
        objects.append(bud)
    return objects


def chunks(crystal, target, material, rng):
    points = [v.co.copy() for v in crystal.data.vertices]
    centre = sum(points, Vector()) / len(points)
    seeds = [centre + (rng.choice(points) - centre) * rng.uniform(0.35, 0.8) for _ in range(CHUNKS)]
    pieces = []
    for index, seed in enumerate(seeds):
        bm = bmesh.new()
        bm.from_mesh(crystal.data)
        for other in seeds:
            if other is seed or (other - seed).length < 1e-4:
                continue
            middle = (seed + other) * 0.5
            geometry = bm.verts[:] + bm.edges[:] + bm.faces[:]
            bmesh.ops.bisect_plane(bm, geom=geometry, plane_co=middle, plane_no=(other - seed).normalized(), clear_outer=True)
        bmesh.ops.holes_fill(bm, edges=[edge for edge in bm.edges if edge.is_boundary], sides=0)
        bmesh.ops.triangulate(bm, faces=bm.faces)
        if len(bm.faces) < 4:
            bm.free()
            continue
        piece = simplify(kit.mesh_object(f'Chunk{len(pieces)}', bm, target, material), BUDGET['Chunk'], angle=8.0)
        kit.smooth_by_angle(piece, 20.0)
        pieces.append(piece)
    return pieces


def meshy_parts(m, target, source_path, rng):
    source = load_source(source_path)
    eyes = eye_points(source)
    bm = bmesh.new()
    bm.from_mesh(source.data)
    labels = classify(bm, eyes)
    bm.free()
    lip = seam_lip(source, labels, target, m['chitin'], rng)
    shell = simplify(extract(source, labels, lambda label: label == 'Shell', 'Shell', target, m['chitin']), BUDGET['Shell'])
    if lip is not None:
        shell = kit.join([shell, lip], 'Shell')
    crystal = simplify(extract(source, labels, lambda label: label == 'Crystal', 'Crystal', target, m['crystal']), BUDGET['Crystal'], angle=6.0)
    eye = simplify(extract(source, labels, lambda label: label == 'Eyes', 'Eyes', target, m['eye']), BUDGET['Eyes'], angle=1.0)
    legs = []
    for index, chain in leg_chains():
        femur = simplify(extract(source, labels, lambda label, i=index: label == ('Femur', i), f'Femur{index}', target, m['leg']), BUDGET['Femur'])
        tibia = simplify(extract(source, labels, lambda label, i=index: label == ('Tibia', i), f'Tibia{index}', target, m['claw']), BUDGET['Tibia'])
        femur.data.transform(scaled(segment_frame(chain[0], chain[1])))
        tibia.data.transform(scaled(segment_frame(chain[1], chain[3])))
        trim_segment(femur, (chain[1] - chain[0]).length * SCALE + FEMUR_OVERLAP)
        trim_segment(tibia, 1e9)
        legs += [femur, tibia]
    bpy.data.objects.remove(source, do_unlink=True)
    shell.data.transform(scaled(Matrix.Translation(-CENTRE)))
    eye.data.transform(scaled(Matrix.Translation(-CENTRE)))
    crystal.data.transform(scaled(Matrix.Translation(-CENTRE)))
    store_seam(shell, crystal)
    store_seam(crystal, shell)
    crystal.data.transform(Matrix.Translation(-(SOCKET - CENTRE) * SCALE))
    for obj in [shell, eye] + legs:
        crisp(obj)
    extras = buds(shell, target, m['crystal'], rng) + chunks(crystal, target, m['crystal'], rng)
    for obj in [shell, crystal, eye] + legs + extras:
        obj.data.update()
    return [shell, eye, crystal] + legs + extras


def gem(radius, height, rng, sides=6, tip=0.3, sink=0.05, taper=0.9, chips=2):
    bm = bmesh.new()
    angles = [2.0 * math.pi * i / sides + rng.uniform(-0.06, 0.06) for i in range(sides)]
    radii = [radius * rng.uniform(0.9, 1.06) for _ in range(sides)]
    shoulder = height * (1.0 - tip)
    bottom = [bm.verts.new((math.cos(a) * r, math.sin(a) * r, -sink)) for a, r in zip(angles, radii)]
    top = [bm.verts.new((math.cos(a) * r * taper, math.sin(a) * r * taper, shoulder + rng.uniform(-0.04, 0.04) * height)) for a, r in zip(angles, radii)]
    apex = bm.verts.new((0.0, 0.0, height))
    bm.faces.new(list(reversed(bottom)))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
        bm.faces.new((top[i], top[j], apex))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    for _ in range(chips):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        normal = Vector((math.cos(angle), math.sin(angle), rng.uniform(0.6, 1.4))).normalized()
        kit.cut(bm, Vector((math.cos(angle) * radius * 0.62, math.sin(angle) * radius * 0.62, shoulder + height * tip * 0.2)), normal)
    return bm


def leaning(bm, angle, lean, distance):
    out = Vector((math.cos(angle), math.sin(angle), 0.0))
    axis = Vector((0.0, 0.0, 1.0)).cross(out).normalized()
    bmesh.ops.transform(bm, matrix=Matrix.Translation(out * distance) @ Matrix.Rotation(math.radians(lean), 4, axis) @ Matrix.Rotation(angle, 4, 'Z'), verts=bm.verts)
    return bm


def egg(m, target, rng):
    shell = pk.sculpt([
        pk.ellipsoid_bm((0.0, 0.0, 0.1), (0.12, 0.11, 0.13)),
        pk.ellipsoid_bm((0.02, -0.01, 0.03), (0.13, 0.12, 0.05)),
    ] + [pk.ellipsoid_bm((math.cos(a) * 0.1, math.sin(a) * 0.1, rng.uniform(0.0, 0.05)), (0.05, 0.05, 0.04)) for a in (0.4, 2.2, 3.9, 5.3)], target, m['crust'], 'EggShell', voxel=0.008, smooth=4, faces=900, rng=rng, carve=[pk.ellipsoid_bm((0.0, 0.0, 0.23), (0.08, 0.08, 0.07))])
    gems = [gem(0.04, 0.2, rng, chips=1)]
    for angle in (30.0, 150.0, 270.0):
        gems.append(leaning(gem(0.026, 0.13, rng, sides=5, chips=0), math.radians(angle), 28.0, 0.035))
    crystal = pk.part('EggGem', pk.merge_bm(gems), target, m['crystal'], rng=rng)
    crystal.data.transform(Matrix.Translation((0.0, 0.0, 0.13)))
    kit.bevel(crystal, 0.004, segments=1, angle=25.0, harden=True)
    return [shell, crystal]


def shard(m, target, rng):
    bm = bmesh.new()
    ring = [bm.verts.new((math.cos(2.0 * math.pi * i / 5) * 0.034, -0.06, math.sin(2.0 * math.pi * i / 5) * 0.03)) for i in range(5)]
    front = bm.verts.new((0.0, -0.2, 0.0))
    back = bm.verts.new((0.0, 0.05, 0.0))
    for i in range(5):
        j = (i + 1) % 5
        bm.faces.new((ring[i], ring[j], front))
        bm.faces.new((ring[j], ring[i], back))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    obj = pk.part('Shard', bm, target, m['crystal'], rng=rng)
    kit.bevel(obj, 0.003, segments=1, angle=25.0, harden=True)
    return obj


def build(target, source_path):
    rng = random.Random(77)
    m = {
        'chitin': chitin_material('CrawlerChitin', seam=True),
        'leg': chitin_material('CrawlerLeg'),
        'claw': chitin_material('CrawlerClaw', tip=(0.3, 0.4, (0.03, 0.028, 0.032))),
        'crust': crust_material('CrawlerCrust'),
        'eye': eye_material('CrawlerEye'),
        'crystal': crystal_material('CrawlerCrystal'),
    }
    m['eye']['no_ao'] = True
    objects = meshy_parts(m, target, source_path, rng)
    objects += egg(m, target, rng)
    objects.append(shard(m, target, rng))
    return objects


def place_segment(by, shown, name, start, end):
    copy = by[name].copy()
    copy.data = by[name].data
    bpy.context.scene.collection.objects.link(copy)
    forward = (end - start).normalized()
    lift = Vector((0.0, 0.0, 1.0))
    lift = (lift - forward * lift.dot(forward)).normalized()
    side = lift.cross(forward)
    copy.matrix_world = Matrix(((side.x, -forward.x, lift.x, start.x), (side.y, -forward.y, lift.y, start.y), (side.z, -forward.z, lift.z, start.z), (0.0, 0.0, 0.0, 1.0)))
    shown.append(copy)


def pose_preview(objects, offset, extras):
    by = {obj.name: obj for obj in objects}
    shift = Matrix.Translation(offset)
    shown = []
    socket = Matrix.Translation((SOCKET - CENTRE) * SCALE)
    for name in ['Shell', 'Eyes', 'Crystal'] + extras:
        copy = by[name].copy()
        copy.data = by[name].data
        bpy.context.scene.collection.objects.link(copy)
        copy.matrix_world = shift @ (socket if name.startswith(('Crystal', 'Bud', 'Chunk')) else Matrix.Identity(4))
        shown.append(copy)
    for index, chain in leg_chains():
        femur = max(-v.co.y for v in by[f'Femur{index}'].data.vertices) - FEMUR_OVERLAP
        tibia = max(-v.co.y for v in by[f'Tibia{index}'].data.vertices)
        hip = shift @ ((chain[0] - CENTRE) * SCALE)
        foot = shift @ ((chain[3] - CENTRE) * SCALE)
        reach = (foot - hip).length
        cos_hip = max(-1.0, min(1.0, (femur * femur + reach * reach - tibia * tibia) / (2.0 * femur * reach)))
        down = (foot - hip).normalized()
        up = Vector((0.0, 0.0, 1.0))
        up = (up - down * up.dot(down)).normalized()
        knee = hip + (down * cos_hip + up * math.sin(math.acos(cos_hip))) * femur
        place_segment(by, shown, f'Femur{index}', hip, knee)
        place_segment(by, shown, f'Tibia{index}', knee, foot)
    return shown


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target, args.get('--source') or SOURCE)
    for obj in objects:
        print(f'{obj.name}: {len(obj.data.polygons)} faces')
    pk.unwrap_all(objects, {'CrawlerChitin': 0.006, 'CrawlerLeg': 0.008, 'CrawlerClaw': 0.008, 'CrawlerCrystal': 0.008, 'CrawlerCrust': 0.008, 'CrawlerEye': 0.01})
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.12)
    if args.get('--preview'):
        pk.ground_preview(6.0)
        for material in bpy.data.materials:
            if material.get('preview_transmission') and material.node_tree:
                bsdf = material.node_tree.nodes.get('Principled BSDF')
                bsdf.inputs['Transmission Weight'].default_value = 1.0
                bsdf.inputs['IOR'].default_value = 1.62
                bsdf.inputs['Base Color'].default_value = (1.0, 0.25, 0.2, 1.0)
                material.node_tree.links.remove(bsdf.inputs['Base Color'].links[0])
        shown = pose_preview(objects, (0.0, 0.0, 0.0), ['Bud0', 'Bud1', 'Bud2'])
        for obj in objects:
            obj.hide_render = True
        pk.preview(shown, args['--preview'], 'crawler', fill=1.1)


if __name__ == '__main__':
    main()
