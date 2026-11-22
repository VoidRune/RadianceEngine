import bpy
import bmesh
import math
import os
import random
import sys
import time
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import hooded
import mainportal as mp
import nature

NAME = 'AutumnTree'
SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'OrangeTree.glb')
HEIGHT = 12.0
WELD = 1e-5
CUT = 0.006
CROWN_Z = 0.33
CAP_SCORE = ((0.6, 0.7), (0.5, 0.62), 12.0)
CAP_HUE = 40.0
CAP_SMOOTH = 3
CAP_THICK = (0.008, 0.035, 0.09)
CAP_SPECK = (0.0004, 0.0012)
CAP_LIMB = (0.03, 2.5)
GRASS = ((38.0, 75.0), 0.35, 0.12)
STONE = (0.09, 0.2, 0.2)
STONE_GROW = (14, 0.2, 0.45)
HOLE_SIDES = 24
STUB = (0.65, 1.3)
INSET = 0.04
ROOT_TIPS = ((2.08, 0.31), (2.01, -0.36), (2.37, 1.66), (3.29, 1.79), (2.87, 0.42), (3.39, 2.40), (0.51, 2.32))
ROOT_TIP_RADIUS = 0.3
TOADSTOOL = (0.012, 0.012, 0.17, 0.16, 0.04)
SPREAD = (0.28, 0.075, 0.008, 5)
STONE_MIN = 0.0006
STONE_FACES = 1200
BOULDER = (12, 18, 0.05, 1.08, (0.15, 0.55))
STONE_SINK = 0.08
ROCK = (0.3, 0.2, 0.55, 2, 0.5, (0.0004, 0.003), 200, 1.5)
FRAGMENT = 300
BARK_FACES = 36000
BARK_TEXTURE = 4096
LEAF_TEXTURE = 2048
STONE_TEXTURE = 2048
UV_ANGLE = 55.0
CAGE = (0.03, 0.09)
AO_DISTANCE = 0.9
BARK = ((0.33, 0.2, 0.12), (0.52, 0.36, 0.23), (0.055, 0.035, 0.028))
GRASS_CUT = (0.035, 0.3, 0.012)
GROUND = (0.025, 0.45, 0.01)
TUCK = (0.75, 0.8, -0.06, 0.15)
PLATES = ((7.0, 7.0, 0.9), 1.25, 0.07)
MOSS = ((0.24, 0.31, 0.09), (0.36, 0.42, 0.14))
LICHEN = (0.58, 0.6, 0.48)
LEAF_COLORS = ((0.72, 0.06, 0.04), (0.88, 0.14, 0.05), (0.96, 0.3, 0.06), (1.0, 0.5, 0.1), (0.98, 0.68, 0.2))
TWIG = (0.42, 0.29, 0.2)
SLOTS = 4
SPRAYS = 12
CARDS = 1000
CARD_SIZE = (1.0, 1.65)
CARD_INNER = (0.3, 0.9, 0.22)
LEAF_FACING = (0.48, 0.62, 0.24, 0.6)
CLUSTERS = 60
LITTER = (560, 0.9, 5.4, (0.2, 0.32))
STONE_MAX = 1.0
UV_PROXY = (40, 72.0)
TRUNK_JOINT = 2.6
CROWN_SPAN = (4.2, 7.2)
TWIG_REACH = 1.6
LEAF_TRANSLUCENCY = 0.7
LEAF_CUTOFF = 0.45


def smoothstep(low, high, value):
    t = np.clip((np.asarray(value, dtype=np.float32) - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def srgb(color):
    return mp.srgb_to_linear(color).astype(np.float32)


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
    islands = mp.FaceGraph(obj.data).components(np.ones(len(obj.data.polygons), bool))
    obj.data.attributes.new('Island', 'INT', 'FACE').data.foreach_set('value', islands.astype(np.int32))
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=WELD)
    bm.to_mesh(obj.data)
    bm.free()
    obj.name = 'TreeSource'
    print('source', len(obj.data.polygons), 'faces')
    return obj


def exposure(obj, centers, normals):
    me = obj.data
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get('co', co)
    tree = BVHTree.FromPolygons([Vector(v) for v in co.reshape(-1, 3)], [p.vertices[:] for p in me.polygons])
    result = np.zeros(len(centers), bool)
    cast = tree.ray_cast
    for i in np.nonzero(centers[:, 2] > CROWN_Z)[0]:
        direction = Vector(normals[i])
        hit = cast(Vector(centers[i]) + direction * 2e-4, direction, 1.0)
        result[i] = hit[0] is None
    return result


def toadstools(graph, centers, thick, ground):
    radial = np.hypot(centers[:, 0], centers[:, 1])
    z = centers[:, 2]
    thin = (thick < TOADSTOOL[0]) & (z > TOADSTOOL[1]) & (z < TOADSTOOL[2]) & (radial > TOADSTOOL[3]) & ~ground
    thin = graph.smooth(thin.astype(np.float64), 2) > 0.4
    labels = graph.components(thin)
    found = np.zeros(len(centers), bool)
    for label in np.unique(labels[thin]):
        members = np.nonzero(labels == label)[0]
        if np.ptp(z[members]) > TOADSTOOL[4]:
            found[members] = True
    found = graph.smooth(found.astype(np.float64), 3) > 0.2
    print('toadstool faces', int(found.sum()))
    return found


def classify(obj):
    me = obj.data
    hue, saturation, value = mp.source_colors(obj)
    centers, normals, areas, _, _ = mp.face_arrays(me)
    graph = mp.FaceGraph(me)
    z = centers[:, 2]
    (v0, v1), (s0, s1), dull = CAP_SCORE
    bright = mp.smoothstep(v0, v1, value) * ((hue < CAP_HUE) | (hue > 330.0))
    salmon = ((hue < dull) | (hue > 330.0)) * mp.smoothstep(s0, s1, saturation)
    score = np.maximum(bright, salmon) * (z > CROWN_Z)
    painted = graph.smooth(score.astype(np.float64), CAP_SMOOTH) > 0.5
    thick = mp.thickness(obj)
    outside = exposure(obj, centers, normals)
    shell = ((thick < CAP_THICK[0]) | outside) & (thick < CAP_THICK[2])
    cap = graph.smooth((painted & shell).astype(np.float64), CAP_SMOOTH) > 0.45
    cap = graph.despeckle(cap, areas, *CAP_SPECK)
    labels = graph.components(cap)
    for label in np.unique(labels[cap]):
        members = np.nonzero(labels == label)[0]
        if areas[members].sum() > CAP_LIMB[0]:
            continue
        spread = np.sort(np.linalg.eigvalsh(np.cov(centers[members].T)))
        if math.sqrt(spread[2] / max(spread[1], 1e-12)) > CAP_LIMB[1]:
            cap[members] = False
    (h0, h1), gs, gz = GRASS
    grass = (hue > h0) & (hue < h1) & (saturation > gs) & (z < gz)
    grass = graph.smooth(grass.astype(np.float64), 2) > 0.5
    seeds = ((saturation < STONE[0]) | ((hue > 180.0) & (saturation < STONE[2]))) & (z < STONE[1])
    seeds = graph.smooth(seeds.astype(np.float64), 2) > 0.5
    stone = (graph.smooth(seeds.astype(np.float64), STONE_GROW[0]) > STONE_GROW[1]) & (saturation < STONE_GROW[2]) & (z < STONE[1])
    stone = graph.smooth(stone.astype(np.float64), 2) > 0.5
    stone &= ~grass
    kind = np.zeros(len(centers), np.int8)
    kind[toadstools(graph, centers, thick, stone | grass)] = 4
    kind[stone] = 2
    kind[grass] = 3
    kind[cap] = 1
    print('faces: cap', int(cap.sum()), 'stone', int(stone.sum()), 'grass', int(grass.sum()), 'bark', int((kind == 0).sum()))
    return kind, graph, centers, normals, areas, saturation


def stone_clusters(kind, graph, centers, areas, saturation):
    stone = kind == 2
    labels = graph.components(stone)
    pale = np.nonzero((kind == 0) & (saturation < SPREAD[0]) & (centers[:, 2] < SPREAD[1]))[0]
    tree = KDTree(len(pale))
    for j, index in enumerate(pale):
        tree.insert(Vector(centers[index]), j)
    tree.balance()
    clusters = []
    for label in np.unique(labels[stone]):
        members = np.nonzero(labels == label)[0]
        if areas[members].sum() < STONE_MIN:
            continue
        taken = set(members.tolist())
        frontier = members
        for _ in range(SPREAD[3]):
            found = set()
            for index in frontier[:: max(1, len(frontier) // 400)]:
                for _, j, _ in tree.find_range(Vector(centers[index]), SPREAD[2]):
                    if pale[j] not in taken:
                        found.add(pale[j])
            if not found:
                break
            taken |= found
            frontier = np.array(sorted(found))
        chosen = np.array(sorted(taken))
        clusters.append(chosen)
    print('stone clusters', len(clusters))
    return clusters


def delete_faces(obj, mask):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in np.nonzero(mask)[0]], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(obj.data)
    bm.free()


def ground_faces(graph, centers, normals, areas):
    low = (centers[:, 2] < GROUND[0]) & (normals[:, 2] > GROUND[1])
    labels = graph.components(low)
    size = np.bincount(labels, weights=areas * low)
    ground = low & (size[labels] > GROUND[2])
    print('ground faces', int(ground.sum()))
    return ground


def cap_stubs(bm, limits):
    capped = 0
    for loop in boundary_loops(bm):
        co = np.array([v.co[:] for e in loop for v in e.verts])
        if co[:, 2].min() <= limits[0] or np.ptp(co, axis=0).max() >= limits[1]:
            continue
        hub = bm.verts.new(Vector(co.mean(axis=0)))
        for edge in loop:
            corner = edge.link_loops[0]
            bm.faces.new((corner.link_loop_next.vert, corner.vert, hub))
        capped += 1
    bm.normal_update()
    print('capped', capped, 'stubs')


def boundary_loops(bm):
    rims = {e for e in bm.edges if e.is_boundary}
    loops = []
    while rims:
        stack = [rims.pop()]
        loop = []
        while stack:
            edge = stack.pop()
            loop.append(edge)
            for vertex in edge.verts:
                for other in vertex.link_edges:
                    if other in rims:
                        rims.remove(other)
                        stack.append(other)
        loops.append(loop)
    return loops


def strip(obj, kind, centers, normals, covered, scale):
    ground = (kind == 3) & (centers[:, 2] < GRASS_CUT[0]) & ((normals[:, 2] > GRASS_CUT[1]) | (centers[:, 2] < GRASS_CUT[2]))
    doomed = (kind == 1) | ground | (kind == 4) | covered
    delete_faces(obj, doomed)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1e-6, plane_co=(0.0, 0.0, CUT), plane_no=(0.0, 0.0, 1.0), clear_inner=True)
    bm.to_mesh(obj.data)
    bm.free()
    graph = mp.FaceGraph(obj.data)
    labels = graph.components(np.ones(len(obj.data.polygons), bool))
    sizes = np.bincount(labels)
    print('bark components', np.sort(sizes)[::-1][:25].tolist())
    loose = sizes[labels] < FRAGMENT
    delete_faces(obj, loose)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    rims = [e for e in bm.edges if e.is_boundary]
    filled = bmesh.ops.holes_fill(bm, edges=rims, sides=HOLE_SIDES)['faces']
    bmesh.ops.triangulate(bm, faces=filled, quad_method='BEAUTY', ngon_method='BEAUTY')
    cap_stubs(bm, (CUT + STUB[0] / scale, STUB[1] / scale))
    left = sum(1 for e in bm.edges if e.is_boundary)
    bm.to_mesh(obj.data)
    bm.free()
    print('capped', len(filled), 'holes,', left, 'open edges left')
    print('bark kept', len(obj.data.polygons), 'faces, dropped', int(loose.sum()), 'loose')


def tuck(obj, stones):
    me = obj.data
    co = np.empty(len(me.vertices) * 3, np.float64)
    me.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    bm = bmesh.new()
    bm.from_mesh(me)
    hem = np.zeros(len(co), bool)
    for edge in bm.edges:
        if edge.is_boundary:
            hem[edge.verts[0].index] = hem[edge.verts[1].index] = True
    bm.free()
    hem &= (co[:, 2] < TUCK[0]) & (co[:, 2] > TUCK[2] + 0.02)
    if stones is not None:
        tree = BVHTree.FromObject(stones, bpy.context.evaluated_depsgraph_get())
        for index in np.nonzero(hem)[0]:
            if tree.find_nearest(Vector(co[index]), TUCK[3])[0] is not None:
                hem[index] = False
    points = co[hem]
    if not len(points):
        return
    kd = KDTree(len(points))
    for j, point in enumerate(points):
        kd.insert(Vector(point), j)
    kd.balance()
    near = np.nonzero(co[:, 2] < TUCK[0] + TUCK[1])[0]
    moved = 0
    for index in near:
        _, _, distance = kd.find(Vector(co[index]))
        weight = max(0.0, 1.0 - distance / TUCK[1]) ** 2
        if weight > 0.0:
            co[index, 2] = min(co[index, 2], co[index, 2] + (TUCK[2] - co[index, 2]) * weight)
            moved += 1
    me.vertices.foreach_set('co', co.ravel())
    me.update()
    print('tucked', int(hem.sum()), 'hem verts,', moved, 'moved')


class Frame:
    def __init__(self, points):
        band = (points[:, 2] > 0.05) & (points[:, 2] < 0.14)
        self.center = points[band, :2].mean(axis=0) if band.any() else np.zeros(2)
        self.scale = HEIGHT / (0.935 - CUT)

    def apply(self, points):
        points = np.asarray(points, dtype=np.float64)
        return np.stack(((points[..., 0] - self.center[0]) * self.scale, (points[..., 1] - self.center[1]) * self.scale, (points[..., 2] - CUT) * self.scale), axis=-1)

    def mesh(self, obj):
        co = np.empty(len(obj.data.vertices) * 3, np.float64)
        obj.data.vertices.foreach_get('co', co)
        obj.data.vertices.foreach_set('co', self.apply(co.reshape(-1, 3)).ravel())
        obj.data.update()


def high_material(source):
    image = next(n.image for n in source.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image and 'normal' not in n.image.name and 'metal' not in n.image.name)
    material = bpy.data.materials.new('BarkHigh')
    material.use_nodes = True
    tree = material.node_tree
    tree.nodes.clear()
    output = tree.nodes.new('ShaderNodeOutputMaterial')
    bsdf = tree.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.name = 'BSDF'
    emission = tree.nodes.new('ShaderNodeEmission')
    emission.name = 'Emit'
    tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    texture = tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    texture.name = 'OUT_src'
    coords = tree.nodes.new('ShaderNodeTexCoord')

    def combine(name, *values):
        node = tree.nodes.new('ShaderNodeCombineColor')
        node.name = name
        for socket, value in zip(node.inputs, values):
            if isinstance(value, float):
                socket.default_value = value
            else:
                tree.links.new(value, socket)
        return node

    def noise(scale, detail=3.0, distortion=0.0):
        node = tree.nodes.new('ShaderNodeTexNoise')
        node.inputs['Scale'].default_value = scale
        node.inputs['Detail'].default_value = detail
        node.inputs['Roughness'].default_value = 0.55
        node.inputs['Distortion'].default_value = distortion
        tree.links.new(coords.outputs['Object'], node.inputs['Vector'])
        return node.outputs['Fac']

    geometry = tree.nodes.new('ShaderNodeNewGeometry')
    split = tree.nodes.new('ShaderNodeSeparateXYZ')
    tree.links.new(geometry.outputs['Normal'], split.inputs['Vector'])
    up = tree.nodes.new('ShaderNodeMapRange')
    up.inputs['From Min'].default_value = -1.0
    tree.links.new(split.outputs['Z'], up.inputs['Value'])
    combine('OUT_shape', geometry.outputs['Pointiness'], up.outputs['Result'], noise(6.0, 4.0))
    combine('OUT_noise', noise(0.35), noise(1.6), noise(5.0, 5.0, 0.6))
    stretched = tree.nodes.new('ShaderNodeMapping')
    stretched.inputs['Scale'].default_value = (5.0, 5.0, 0.7)
    tree.links.new(coords.outputs['Object'], stretched.inputs['Vector'])
    fibre = tree.nodes.new('ShaderNodeTexNoise')
    fibre.inputs['Scale'].default_value = 2.2
    fibre.inputs['Detail'].default_value = 6.0
    fibre.inputs['Distortion'].default_value = 0.8
    tree.links.new(stretched.outputs['Vector'], fibre.inputs['Vector'])
    plates_map = tree.nodes.new('ShaderNodeMapping')
    plates_map.inputs['Scale'].default_value = PLATES[0]
    warp = tree.nodes.new('ShaderNodeVectorMath')
    warp.operation = 'MULTIPLY_ADD'
    warp_noise = tree.nodes.new('ShaderNodeTexNoise')
    warp_noise.inputs['Scale'].default_value = 1.4
    warp_noise.inputs['Detail'].default_value = 3.0
    tree.links.new(coords.outputs['Object'], warp_noise.inputs['Vector'])
    tree.links.new(warp_noise.outputs['Color'], warp.inputs[0])
    warp.inputs[1].default_value = (0.08, 0.08, 0.08)
    tree.links.new(coords.outputs['Object'], warp.inputs[2])
    tree.links.new(warp.outputs['Vector'], plates_map.inputs['Vector'])
    plates = tree.nodes.new('ShaderNodeTexVoronoi')
    plates.feature = 'DISTANCE_TO_EDGE'
    plates.inputs['Scale'].default_value = PLATES[1]
    plates.inputs['Randomness'].default_value = 0.9
    tree.links.new(plates_map.outputs['Vector'], plates.inputs['Vector'])
    cells = tree.nodes.new('ShaderNodeTexVoronoi')
    cells.inputs['Scale'].default_value = PLATES[1]
    cells.inputs['Randomness'].default_value = 0.9
    tree.links.new(plates_map.outputs['Vector'], cells.inputs['Vector'])
    pick = tree.nodes.new('ShaderNodeSeparateColor')
    tree.links.new(cells.outputs['Color'], pick.inputs['Color'])
    combine('OUT_fibre', fibre.outputs['Fac'], noise(2.2, 4.0), noise(9.0, 3.0))
    combine('OUT_plates', plates.outputs['Distance'], pick.outputs['Red'], 0.0)
    groove = tree.nodes.new('ShaderNodeMapRange')
    groove.inputs['From Min'].default_value = 0.0
    groove.inputs['From Max'].default_value = PLATES[2]
    groove.interpolation_type = 'SMOOTHSTEP'
    tree.links.new(plates.outputs['Distance'], groove.inputs['Value'])
    relief = tree.nodes.new('ShaderNodeMath')
    relief.operation = 'MULTIPLY_ADD'
    tree.links.new(groove.outputs['Result'], relief.inputs[0])
    relief.inputs[1].default_value = 1.0
    tree.links.new(fibre.outputs['Fac'], relief.inputs[2])
    height = tree.nodes.new('ShaderNodeSeparateXYZ')
    tree.links.new(geometry.outputs['Position'], height.inputs['Vector'])
    combine('OUT_height', height.outputs['Z'], 0.0, 0.0)
    bump = tree.nodes.new('ShaderNodeBump')
    bump.inputs['Strength'].default_value = 0.6
    bump.inputs['Distance'].default_value = 0.02
    tree.links.new(relief.outputs['Value'], bump.inputs['Height'])
    tree.links.new(bump.outputs['Normal'], bsdf.inputs['Normal'])
    return material


def compose_bark(maps, ao):
    shape, noise, fibre = maps['shape'], maps['noise'], maps['fibre']
    src = maps['src'][..., :3]
    covered = src.sum(axis=-1) > 0.0
    point = mp.normalized(shape[..., 0], 2.0, 98.0, covered)
    convex = smoothstep(0.55, 0.85, point)
    concave = 1.0 - smoothstep(0.15, 0.45, point)
    up = shape[..., 1]
    height = maps['height'][..., 0]
    large, medium, small = noise[..., 0], noise[..., 1], noise[..., 2]
    grain, patches, flecks = fibre[..., 0], fibre[..., 1], fibre[..., 2]
    tone = src @ np.array((0.2126, 0.7152, 0.0722), dtype=np.float32)
    painted = mp.blur(tone, 1)
    median = np.median(painted[covered]) if covered.any() else 0.3
    ridge = np.clip(0.5 + 0.9 * (painted / max(median, 1e-4) - 1.0), 0.0, 1.0)
    edge, plate = maps['plates'][..., 0], maps['plates'][..., 1]
    groove = (1.0 - smoothstep(0.0, PLATES[2], edge)) * (0.55 + 0.45 * medium)
    lip = smoothstep(0.0, PLATES[2], edge) * (1.0 - smoothstep(PLATES[2], PLATES[2] * 2.5, edge))
    ridge = np.clip(0.35 * ridge + 0.4 * convex + 0.5 * (grain - 0.5) + 0.35 * lip + 0.25 * (plate - 0.5), 0.0, 1.0)
    crevice = np.clip(1.1 * (1.0 - ao) + 0.6 * concave + 0.35 * (0.5 - grain) + 0.6 * groove, 0.0, 1.0)
    base = srgb(BARK[0])[None, None, :] * ((0.85 + 0.3 * large) * (0.78 + 0.44 * grain))[..., None]
    base = base * (1.0 - ridge)[..., None] + srgb(BARK[1])[None, None, :] * ridge[..., None]
    base = base * (1.0 - 0.75 * crevice)[..., None] + srgb(BARK[2])[None, None, :] * (0.75 * crevice)[..., None]
    low = 1.0 - smoothstep(2.5, 6.0, height)
    moss = smoothstep(0.55, 0.85, up) * smoothstep(0.55, 0.72, patches + 0.2 * concave) * (0.25 + 0.75 * low) * (0.55 + 0.45 * flecks)
    moss_tone = srgb(MOSS[0])[None, None, :] * (1.0 - flecks)[..., None] + srgb(MOSS[1])[None, None, :] * flecks[..., None]
    base = base * (1.0 - moss)[..., None] + moss_tone * moss[..., None]
    lichen = smoothstep(0.7, 0.76, small) * (1.0 - moss) * (1.0 - crevice) * 0.6
    base = base * (1.0 - lichen)[..., None] + srgb(LICHEN)[None, None, :] * lichen[..., None]
    rough = np.clip(0.82 + 0.1 * crevice - 0.08 * ridge + 0.08 * moss, 0.5, 0.98)
    orm = np.ones(base.shape[:2] + (3,), np.float32)
    orm[..., 0] = np.clip(0.2 + 0.8 * ao, 0.0, 1.0)
    orm[..., 1] = rough
    orm[..., 2] = 0.0
    return np.clip(base, 0.0, 1.0), orm


def leaf_shape(x, y, size, angle, lobes):
    c, s = math.cos(angle), math.sin(angle)
    lx = (c * x + s * y) / size
    ly = (-s * x + c * y) / size
    cy = 0.45
    dx, dy = lx, ly - cy
    r = np.hypot(dx, dy)
    theta = np.arctan2(dx, dy)
    lobe = np.zeros_like(r)
    for direction, reach in lobes:
        offset = np.abs(np.angle(np.exp(1j * (theta - direction))))
        lobe = np.maximum(lobe, reach * np.clip(1.0 - offset / 0.78, 0.0, 1.0) ** 1.1)
    serration = 0.022 * np.abs(np.sin(theta * 22.0)) * (0.4 + lobe)
    edge = 0.2 + lobe * 0.33 - serration
    inside = edge - r
    veins = np.zeros_like(r)
    for direction, reach in lobes:
        along = np.cos(theta - direction) * r
        across = np.abs(np.sin(theta - direction) * r)
        veins = np.maximum(veins, (1.0 - np.clip(across / 0.012, 0.0, 1.0)) * (along > 0.0) * (along < edge))
    petiole = (np.abs(lx) < 0.007) & (ly < cy - 0.05) & (ly > cy - 0.16)
    return inside, r / np.maximum(edge, 1e-3), veins, petiole, lx, ly


def leaf_atlas(size, rng):
    texel = SLOTS / size
    u = (np.arange(size) + 0.5) / size * SLOTS
    U, V = np.meshgrid(u, u)
    base = np.zeros((size, size, 3), np.float32)
    alpha = np.zeros((size, size), np.float32)
    height = np.zeros((size, size), np.float32)
    thin = np.zeros((size, size), np.float32)
    rough = np.ones((size, size), np.float32) * 0.8
    occlusion = np.ones((size, size), np.float32)
    palette = [srgb(c) for c in LEAF_COLORS]
    lobes_main = [(0.0, 1.0), (1.2, 0.85), (-1.2, 0.85), (2.25, 0.45), (-2.25, 0.45)]
    span = size // SLOTS
    for slot in range(SLOTS * SLOTS):
        sx, sy = slot % SLOTS, slot // SLOTS
        x0, y0 = sx * span, sy * span
        px = U[y0:y0 + span, x0:x0 + span] - sx - 0.5
        py = V[y0:y0 + span, x0:x0 + span] - sy - 0.5
        if slot < SPRAYS:
            stem_bend = rng.uniform(-0.15, 0.15)
            twig = np.zeros(px.shape, bool)
            leaves = []
            count = rng.randint(14, 19)
            for k in range(count):
                t = 0.12 + 0.7 * (k + rng.uniform(-0.3, 0.3)) / count
                side = 1.0 if k % 2 else -1.0
                ox = stem_bend * t * t + side * rng.uniform(0.04, 0.16) * (0.5 + t)
                oy = -0.5 + t
                angle = side * rng.uniform(0.25, 0.75) + stem_bend * 0.5
                reach = rng.uniform(0.25, 0.34) * (0.85 + 0.3 * t)
                leaves.append((ox + math.sin(angle) * reach * 0.3, oy + math.cos(angle) * reach * 0.3, reach, angle, rng.random()))
            twig_mask = twig.astype(np.float32)
            slot_base = np.zeros(px.shape + (3,), np.float32)
            slot_base[twig] = srgb(TWIG)
            slot_alpha = twig_mask.copy()
            slot_height = twig_mask * 0.4
            slot_shadow = np.ones(px.shape, np.float32)
            for tip_x, tip_y, reach, angle, pick in leaves:
                lobes = [(d + rng.uniform(-0.08, 0.08), r * rng.uniform(0.85, 1.1)) for d, r in lobes_main]
                inside, radial, veins, petiole, lx, ly = leaf_shape(px - tip_x + math.sin(angle) * reach * 0.45, py - tip_y + math.cos(angle) * reach * 0.45, reach, -angle, lobes)
                mask = np.clip(inside / (texel * 1.5 / reach) + 0.5, 0.0, 1.0)
                stalk = petiole.astype(np.float32)
                index = min(int(pick * len(palette)), len(palette) - 1)
                inner = palette[min(index + 1, len(palette) - 1)]
                outer = palette[max(index - 1, 0)] * 0.85
                blend = smoothstep(0.35, 1.0, radial)
                color = inner[None, None, :] * (1.0 - blend)[..., None] + outer[None, None, :] * blend[..., None]
                color = color * (1.0 + 0.25 * veins)[..., None]
                mottle = smoothstep(0.3, 0.9, np.abs(np.sin(lx * 7.0 + pick * 9.0) * np.sin(ly * 6.0 + pick * 5.0)))
                color = color * (1.0 - 0.18 * mottle * blend)[..., None]
                dome = np.clip(1.0 - radial, 0.0, 1.0) ** 0.6 * 0.6 + 0.25 * (1.0 - np.clip(np.abs(lx) / 0.25, 0.0, 1.0))
                slot_shadow = np.where(mask > 0.5, 1.0, slot_shadow * (1.0 - 0.35 * mask))
                slot_base = slot_base * (1.0 - mask)[..., None] * 0.9 + color * mask[..., None]
                slot_base = np.where(stalk[..., None] > 0.5, outer[None, None, :] * 0.55, slot_base)
                slot_alpha = np.maximum(slot_alpha, np.maximum(mask, stalk))
                slot_height = slot_height * (1.0 - mask) + (0.5 + (dome - 0.15 * veins)) * mask
            twig_only = twig_mask * (slot_alpha - twig_mask < 0.5)
            base[y0:y0 + span, x0:x0 + span] = slot_base
            alpha[y0:y0 + span, x0:x0 + span] = slot_alpha
            height[y0:y0 + span, x0:x0 + span] = slot_height
            thin[y0:y0 + span, x0:x0 + span] = slot_alpha * (1.0 - twig_only)
            rough[y0:y0 + span, x0:x0 + span] = 0.6 + 0.25 * twig_only
            occlusion[y0:y0 + span, x0:x0 + span] = np.clip(0.75 + 0.25 * smoothstep(-0.5, 0.2, py), 0.0, 1.0) * slot_shadow
        else:
            angle = rng.uniform(-0.4, 0.4)
            lobes = [(d + rng.uniform(-0.06, 0.06), r * rng.uniform(0.9, 1.05)) for d, r in lobes_main]
            inside, radial, veins, petiole, lx, ly = leaf_shape(px, py + 0.42, 0.95, angle, lobes)
            mask = np.clip(inside / (texel * 1.5 / 0.95) + 0.5, 0.0, 1.0)
            pick = slot - SPRAYS
            inner, outer = palette[(pick + 2) % len(palette)], palette[pick % len(palette)]
            blend = smoothstep(0.35, 1.0, radial)
            color = inner[None, None, :] * (1.0 - blend)[..., None] + outer[None, None, :] * blend[..., None]
            color = color * (1.0 + 0.25 * veins)[..., None] * 0.85
            dome = np.clip(1.0 - radial, 0.0, 1.0) ** 0.6 * 0.6
            base[y0:y0 + span, x0:x0 + span] = color
            alpha[y0:y0 + span, x0:x0 + span] = np.maximum(mask, petiole.astype(np.float32))
            height[y0:y0 + span, x0:x0 + span] = (0.5 + dome - 0.15 * veins) * mask
            thin[y0:y0 + span, x0:x0 + span] = mask
            rough[y0:y0 + span, x0:x0 + span] = 0.7
    gy, gx = np.gradient(height * 0.004, 1.0 / size * 1.2)
    gx, gy = mp.blur(gx, 1), mp.blur(gy, 1)
    normal = np.stack((-gx, -gy, np.ones_like(gx)), axis=-1)
    normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
    orm = np.ones((size, size, 3), np.float32)
    orm[..., 0] = occlusion
    orm[..., 1] = rough
    orm[..., 2] = 0.0
    return np.clip(base, 0.0, 1.0), alpha, normal * 0.5 + 0.5, orm, thin


def slot_uv(slot):
    sx, sy = slot % SLOTS, slot // SLOTS
    inset = 0.004
    return (sx / SLOTS + inset, sy / SLOTS + inset, (sx + 1) / SLOTS - inset, (sy + 1) / SLOTS - inset)


def sample_caps(centers, normals, areas, kind, count, rng):
    cap = np.nonzero(kind == 1)[0]
    weights = areas[cap] / areas[cap].sum()
    picks = np.asarray(rng.choice(cap, size=count, p=weights))
    return centers[picks], normals[picks]


def build_leaves(frame, points, normals, target, material, rng):
    world = frame.apply(points)
    crown = world.mean(axis=0)
    crown[2] = (world[:, 2].min() + world[:, 2].max()) * 0.5
    radii = np.maximum(world.max(axis=0) - world.min(axis=0), 1.0) * 0.5
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new('UVMap')
    card_centers = []
    split_normals = []
    for i in range(len(world)):
        outward = Vector(((world[i] - crown) / radii ** 2).tolist()).normalized()
        flat = Vector((outward.x, outward.y, 0.0))
        flat = flat.normalized() if flat.length > 1e-3 else Vector((1.0, 0.0, 0.0))
        n = (Vector((0.0, 0.0, 1.0)) * LEAF_FACING[0] + outward * LEAF_FACING[1] + Vector((rng.normal(), rng.normal(), rng.normal())) * LEAF_FACING[2]).normalized()
        inner = rng.random() < CARD_INNER[2]
        depth = rng.uniform(*CARD_INNER[:2]) if inner else rng.uniform(0.0, 0.25)
        center = Vector(world[i].tolist()) - outward * depth
        size = rng.uniform(*CARD_SIZE) * (0.85 if inner else 1.0)
        reach = (flat - n * flat.dot(n)).normalized()
        side = n.cross(reach).normalized()
        roll = rng.uniform(-LEAF_FACING[3], LEAF_FACING[3])
        up = (reach * math.cos(roll) + side * math.sin(roll)).normalized()
        right = up.cross(n).normalized()
        corners = [center - right * size * 0.5 - up * size * 0.5, center + right * size * 0.5 - up * size * 0.5, center + right * size * 0.5 + up * size * 0.5 - n * 0.12 * size, center - right * size * 0.5 + up * size * 0.5 - n * 0.12 * size]
        u0, v0, u1, v1 = slot_uv(int(rng.integers(0, SPRAYS)))
        uvs = [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]
        bent = Vector(((np.array(center[:]) - crown) / radii ** 2).tolist()).normalized()
        verts = [bm.verts.new(c) for c in corners]
        face = bm.faces.new(verts)
        for loop, k in zip(face.loops, range(4)):
            loop[uv_layer].uv = uvs[k]
        split_normals.extend([bent[:]] * 4)
        card_centers.append(np.array(center[:]))
    obj = kit.mesh_object('Leaves', bm, target, material)
    me = obj.data
    me.shade_smooth()
    loop_normals = [None] * len(me.loops)
    index = 0
    for poly in me.polygons:
        for loop in poly.loop_indices:
            loop_normals[loop] = split_normals[index]
            index += 1
    me.normals_split_custom_set(loop_normals)
    print('leaf cards', len(card_centers))
    return obj, np.array(card_centers), crown, radii


def build_litter(target, material, rng, avoid):
    count, inner, outer, (s0, s1) = LITTER
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new('UVMap')
    placed = 0
    while placed < count:
        r = inner + (outer - inner) * rng.random() ** 1.6
        a = rng.uniform(0.0, 2.0 * math.pi)
        center = Vector((math.cos(a) * r, math.sin(a) * r, 0.02 + 0.03 * rng.random()))
        if avoid(center):
            continue
        size = rng.uniform(s0, s1)
        yaw = rng.uniform(0.0, 2.0 * math.pi)
        tilt = Vector((rng.gauss(0.0, 0.15), rng.gauss(0.0, 0.15), 1.0)).normalized()
        right = Vector((math.cos(yaw), math.sin(yaw), 0.0))
        right = (right - tilt * right.dot(tilt)).normalized()
        up = tilt.cross(right)
        corners = [center + (-right - up) * size * 0.5, center + (right - up) * size * 0.5, center + (right + up) * size * 0.5, center + (-right + up) * size * 0.5]
        u0, v0, u1, v1 = slot_uv(SPRAYS + rng.randint(0, SLOTS * SLOTS - SPRAYS - 1))
        verts = [bm.verts.new(c) for c in corners]
        face = bm.faces.new(verts)
        for loop, uv in zip(face.loops, ((u0, v0), (u1, v0), (u1, v1), (u0, v1))):
            loop[uv_layer].uv = uv
        placed += 1
    obj = kit.mesh_object('Litter', bm, target, material)
    obj.data.shade_flat()
    return obj


def stone_pieces(frame, clusters, centers):
    pieces = []
    for members in clusters:
        world = frame.apply(centers[members])
        extent = np.ptp(world[:, :2], axis=0).max()
        parts = max(1, int(math.ceil(extent / STONE_MAX)))
        if parts == 1:
            pieces.append((world, members))
            continue
        _, labels = kmeans(world, parts, np.random.default_rng(len(pieces)))
        pieces.extend((world[labels == j], members[labels == j]) for j in range(parts) if (labels == j).sum() > 3)
    tips = np.array(ROOT_TIPS)
    return [(world, members) for world, members in pieces if np.hypot(*(tips - world.mean(axis=0)[:2]).T).min() > ROOT_TIP_RADIUS]


def rock_mask(obj, kind, graph, centers, areas, saturation, frame, pieces):
    seeds = np.zeros(len(centers), bool)
    for _, members in pieces:
        seeds[members] = True
    pale = ((kind == 0) | (kind == 2)) & (saturation < ROCK[0]) & (centers[:, 2] < ROCK[1])
    labels = graph.components(pale)
    rock = pale & np.isin(labels, np.unique(labels[seeds & pale]))
    world = frame.apply(centers)
    tips = np.array(ROOT_TIPS)
    near = np.min(np.hypot(world[:, None, 0] - tips[None, :, 0], world[:, None, 1] - tips[None, :, 1]), axis=1) < ROCK[2]
    rock &= ~near
    islands = np.zeros(len(centers), np.int32)
    obj.data.attributes['Island'].data.foreach_get('value', islands)
    share = np.bincount(islands, weights=areas * rock) / np.maximum(np.bincount(islands, weights=areas), 1e-12)
    rock = (share[islands] > ROCK[4]) & ~near & (centers[:, 2] < ROCK[1])
    rock = graph.smooth(rock.astype(np.float64), ROCK[3]) > 0.5
    rock = graph.despeckle(rock, areas, *ROCK[5])
    print('source rock faces', int(rock.sum()))
    return rock


def build_stones(pieces, target, rng):
    material = boulder_material('TreeStone', bake_size=STONE_TEXTURE)
    stones = []
    for k, (world, _) in enumerate(pieces):
        center = world.mean(axis=0)
        spread = world - center
        values, vectors = np.linalg.eigh(np.cov(spread[:, :2].T))
        axis = vectors[:, 1]
        yaw = math.atan2(axis[1], axis[0])
        local = spread[:, :2] @ vectors
        half = np.maximum(np.abs(local).max(axis=0) * 1.1 + 0.04, 0.22)
        top = world[:, 2].max()
        size = (half[1] * 2.0, half[0] * 2.0, float(np.clip(top * 1.08 + 0.05, 0.3, min(1.5, 1.3 * min(half) + 0.15))))
        bm = boulder(size, random.Random(k * 31 + 7))
        obj = kit.mesh_object(f'Stone{k}', bm, target, material)
        kit.bevel(obj, min(0.035, min(size) * 0.05), segments=1, angle=25.0)
        place = Matrix.Translation((center[0], center[1], 0.0)) @ Matrix.Rotation(yaw, 4, 'Z') @ Matrix.Rotation(rng.uniform(-0.18, 0.18), 4, 'X') @ Matrix.Rotation(rng.uniform(-0.18, 0.18), 4, 'Y')
        pk.tag(obj, rng=rng)
        obj.matrix_world = place
        print(f'stone collider ({center[0]:.2f}, {center[1]:.2f}) r {0.5 * math.sqrt(size[0] * size[1]):.2f} h {size[2]:.2f}')
        kit.select_only([obj])
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        stones.append(obj)
    stones_obj = kit.join(stones, 'Stones') if stones else None
    print('stones', len(stones))
    return stones_obj, material


def split_rock(source, target, material):
    rock = np.zeros(len(source.data.polygons), np.int32)
    source.data.attributes['Rock'].data.foreach_get('value', rock)
    part = source.copy()
    part.data = source.data.copy()
    part.name = 'SourceRock'
    target.objects.link(part)
    delete_faces(part, rock == 0)
    for obj in (source, part):
        for name in ('Rock', 'Island'):
            obj.data.attributes.remove(obj.data.attributes[name])
    while part.data.uv_layers:
        part.data.uv_layers.remove(part.data.uv_layers[0])
    part.data.materials.clear()
    part.data.materials.append(material)
    print('rock faces moved to stones', len(part.data.polygons))
    return part, rock.astype(bool)


def boulder_material(name, light=(0.3, 0.295, 0.285), dark=(0.1, 0.098, 0.095), moss=0.6, cover=0.55, bake_size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    normal_z = s.separate(s.geometry('Normal'))[2]
    large = s.noise(p, 1.3, detail=3.0)
    mid = s.noise(p, 5.0, detail=5.0, roughness=0.55)
    fine = s.noise(p, 40.0, detail=2.0)
    grain = s.noise(p, 110.0, detail=2.0)
    island = s.geometry('Random Per Island')
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.025, s.voronoi(s.add(p, s.mul(s.noise_color(p, 2.0, detail=2.0), 0.15)), 1.6, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.45, 0.6, s.noise(p, 1.1, detail=2.0)))
    hairline = s.mul(s.sub(1.0, s.smooth(0.0, 0.012, s.voronoi(s.add(p, s.mul(s.noise_color(p, 6.0, detail=2.0), 0.05)), 7.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.5, 0.65, s.noise(p, 1.8, detail=2.0)))
    pits = s.sub(1.0, s.smooth(0.0, 0.25, s.voronoi(p, 26.0, 'F1', 1.0, 'Distance')))
    tone = s.madd(s.smooth(0.3, 0.7, large), 0.45, s.add(s.mul(mid, 0.3), s.mul(island, 0.2)))
    base = s.mix(tone, dark, light)
    lichen = s.mul(s.smooth(0.64, 0.7, s.noise(p, 9.0, detail=3.0)), 0.45)
    base = s.mix(lichen, base, (0.36, 0.37, 0.3))
    dirt = pk.cavity(s, 0.12)
    base = s.mix(s.mul(dirt, 0.7), base, (0.025, 0.024, 0.02))
    base = s.mix(s.mul(cracks, 0.55), base, (0.04, 0.038, 0.035))
    base = s.mix(s.mul(hairline, 0.2), base, (0.07, 0.068, 0.065))
    edge = pk.wear(s, 0.03)
    base = s.mix(s.mul(edge, 0.4), base, (light[0] * 1.3, light[1] * 1.3, light[2] * 1.25))
    rough = s.remap(fine, 0.3, 0.7, 0.72, 0.92)
    height = s.add(s.mul(mid, 0.7), s.add(s.mul(fine, 0.14), s.add(s.mul(grain, 0.06), s.add(s.mul(pits, -0.08), s.add(s.mul(cracks, -0.5), s.mul(hairline, -0.12))))))
    up = s.smooth(1.0 - cover - 0.2, 1.0 - cover + 0.15, normal_z)
    patches = s.smooth(0.4, 0.56, s.add(s.noise(p, 2.4, detail=5.0, roughness=0.6), s.mul(s.sub(normal_z, 0.6), 0.25)))
    amount = s.mul(s.mul(up, patches), moss)
    tuft = s.noise(p, 30.0, detail=4.0)
    moss_tone = s.mix(s.add(s.mul(tuft, 0.6), s.mul(large, 0.4)), (0.03, 0.05, 0.012), (0.12, 0.17, 0.035))
    base = s.mix(amount, base, moss_tone)
    rough = s.lerp(amount, rough, 0.95)
    height = s.lerp(amount, height, s.add(0.45, s.mul(tuft, 0.4)))
    return s.finish(base, rough, height=height, bump=0.55, bump_distance=0.02, bake_size=bake_size)


def boulder(size, rng):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=3, radius=1.0)
    seed = nature.seed_of(rng)
    apex = Vector((rng.uniform(-0.35, 0.35), rng.uniform(-0.35, 0.35), 0.0))
    point = rng.uniform(*BOULDER[4])
    for vertex in bm.verts:
        direction = vertex.co.normalized()
        bump = 1.0 + BOULDER[2] * nature.fbm(direction * 1.7 + seed, 3)
        co = direction * bump
        rise = max(co.z, 0.0)
        pinch = 1.0 - point * rise * rise
        co = Vector((co.x * pinch + apex.x * point * rise * rise, co.y * pinch + apex.y * point * rise * rise, co.z * (1.0 + 0.35 * point)))
        vertex.co = co
    for _ in range(rng.randint(BOULDER[0], BOULDER[1])):
        elevation = rng.uniform(-0.05, 1.1)
        normal = (nature.around(rng.uniform(0.0, 2.0 * math.pi)) * math.cos(elevation) + nature.UP * math.sin(elevation)).normalized()
        kit.cut(bm, normal * nature.support(bm, normal) * rng.uniform(0.7, 0.9), normal)
    kit.cut(bm, Vector((0.0, 0.0, -0.55)), -nature.UP)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    low = Vector((min(v.co.x for v in bm.verts), min(v.co.y for v in bm.verts), min(v.co.z for v in bm.verts)))
    high = Vector((max(v.co.x for v in bm.verts), max(v.co.y for v in bm.verts), max(v.co.z for v in bm.verts)))
    middle = (low + high) * 0.5
    scale = Vector((size[0] * BOULDER[3] / (high.x - low.x), size[1] * BOULDER[3] / (high.y - low.y), size[2] / (high.z - low.z)))
    for vertex in bm.verts:
        vertex.co = Vector(((vertex.co.x - middle.x) * scale.x, (vertex.co.y - middle.y) * scale.y, (vertex.co.z - low.z) * scale.z - STONE_SINK))
    bm.normal_update()
    return bm


def lumps(obj, size, seed):
    remesh = obj.modifiers.new('Remesh', 'REMESH')
    remesh.mode = 'VOXEL'
    remesh.voxel_size = max(size * 0.035, 0.012)
    texture = bpy.data.textures.new(f'StoneLumps{seed}', 'CLOUDS')
    texture.noise_scale = size * 0.35
    texture.noise_depth = 3
    displace = obj.modifiers.new('Displace', 'DISPLACE')
    displace.texture = texture
    displace.texture_coords = 'OBJECT'
    displace.strength = size * 0.09
    displace.mid_level = 0.5
    crack = bpy.data.textures.new(f'StoneCracks{seed}', 'VORONOI')
    crack.noise_scale = size * 0.45
    crack.distance_metric = 'DISTANCE'
    crack.noise_intensity = 1.0
    cracks = obj.modifiers.new('Cracks', 'DISPLACE')
    cracks.texture = crack
    cracks.texture_coords = 'OBJECT'
    cracks.strength = -size * 0.03
    cracks.mid_level = 0.0
    smooth = obj.modifiers.new('Smooth', 'SMOOTH')
    smooth.iterations = 2
    smooth.factor = 0.5
    kit.apply_modifiers(obj)
    decimate = obj.modifiers.new('Decimate', 'DECIMATE')
    decimate.ratio = min(1.0, STONE_FACES / max(len(obj.data.polygons), 1))
    kit.apply_modifiers(obj)


def buried(points, stones):
    result = np.zeros(len(points), bool)
    if stones is None:
        return result
    corners = np.array([stones.matrix_world @ Vector(c) for c in stones.bound_box])
    low, high = corners.min(axis=0) - INSET, corners.max(axis=0) + INSET
    candidates = np.all((points > low) & (points < high), axis=1)
    tree = BVHTree.FromObject(stones, bpy.context.evaluated_depsgraph_get())
    for index in np.nonzero(candidates)[0]:
        point = Vector(points[index])
        hit = tree.find_nearest(point, 3.0)
        if hit[0] is not None and (point - hit[0]).dot(hit[1]) < -INSET:
            result[index] = True
    print('source faces buried in boulders', int(result.sum()))
    return result


def proxy_unwrap(obj, target):
    proxy = obj.copy()
    proxy.data = obj.data.copy()
    target.objects.link(proxy)
    modifier = proxy.modifiers.new('Smooth', 'SMOOTH')
    modifier.factor = 1.0
    modifier.iterations = UV_PROXY[0]
    kit.apply_modifiers(proxy)
    while proxy.data.uv_layers:
        proxy.data.uv_layers.remove(proxy.data.uv_layers[0])
    mp.UV_ANGLE = UV_PROXY[1]
    mp.unwrap(proxy)
    uv = np.empty(len(proxy.data.loops) * 2, np.float32)
    proxy.data.uv_layers[0].data.foreach_get('uv', uv)
    layer = obj.data.uv_layers.new(name='UVMap')
    layer.data.foreach_set('uv', uv)
    islands = len(set(map(tuple, np.round(uv.reshape(-1, 2), 2).tolist())))
    bpy.data.objects.remove(proxy, do_unlink=True)
    print('bark unwrapped on a smoothed proxy')


def kmeans(points, k, rng, iterations=25):
    centers = points[rng.choice(len(points), size=k, replace=False)].copy()
    labels = np.zeros(len(points), int)
    for _ in range(iterations):
        distance = ((points[:, None, :] - centers[None, :, :]) ** 2).sum(axis=-1)
        labels = distance.argmin(axis=1)
        for j in range(k):
            members = points[labels == j]
            if len(members):
                centers[j] = members.mean(axis=0)
    return centers, labels


def rig_tree(target, bark, leaves, card_centers, crown):
    rng = np.random.default_rng(5)
    centers, labels = kmeans(card_centers, CLUSTERS, rng)
    data = bpy.data.armatures.new(NAME)
    rig = bpy.data.objects.new(NAME, data)
    target.objects.link(rig)
    kit.select_only([rig])
    bpy.ops.object.mode_set(mode='EDIT')
    root = data.edit_bones.new('root')
    root.head, root.tail = (0.0, 0.0, 0.0), (0.0, 0.0, 0.5)
    trunk = data.edit_bones.new('trunk')
    trunk.head, trunk.tail = (0.0, 0.0, TRUNK_JOINT), (0.0, 0.0, TRUNK_JOINT + 0.5)
    trunk.parent = root
    crown_bone = data.edit_bones.new('crown')
    crown_bone.head, crown_bone.tail = Vector(crown.tolist()), Vector(crown.tolist()) + Vector((0.0, 0.0, 0.5))
    crown_bone.parent = trunk
    for j, c in enumerate(centers):
        bone = data.edit_bones.new(f'leaf{j:02d}')
        bone.head, bone.tail = Vector(c.tolist()), Vector(c.tolist()) + Vector((0.0, 0.0, 0.4))
        bone.parent = crown_bone
    bpy.ops.object.mode_set(mode='OBJECT')
    names = ['root', 'trunk', 'crown'] + [f'leaf{j:02d}' for j in range(CLUSTERS)]
    leaf_groups = {name: leaves.vertex_groups.new(name=name) for name in names}
    for card, label in enumerate(labels):
        leaf_groups[f'leaf{label:02d}'].add(list(range(card * 4, card * 4 + 4)), 1.0, 'REPLACE')
    bark_groups = {name: bark.vertex_groups.new(name=name) for name in names}
    tree = KDTree(len(centers))
    for j, c in enumerate(centers):
        tree.insert(Vector(c.tolist()), j)
    tree.balance()
    for vertex in bark.data.vertices:
        z = vertex.co.z
        bend = float(np.clip((z - 0.8) / (TRUNK_JOINT * 1.4), 0.0, 1.0))
        upper = float(smoothstep(*CROWN_SPAN, z))
        weights = {'root': 1.0 - bend, 'trunk': bend * (1.0 - upper), 'crown': bend * upper}
        if upper > 0.0:
            _, j, distance = tree.find(vertex.co)
            share = upper * 0.7 * max(0.0, 1.0 - distance / TWIG_REACH)
            if share > 0.0:
                for key in ('trunk', 'crown'):
                    weights[key] *= 1.0 - share
                weights[f'leaf{j:02d}'] = share
        for key, weight in weights.items():
            if weight > 1e-4:
                bark_groups[key].add([vertex.index], weight, 'REPLACE')
    for obj in (bark, leaves):
        obj.parent = rig
        obj.matrix_parent_inverse = Matrix.Identity(4)
        modifier = obj.modifiers.new('Armature', 'ARMATURE')
        modifier.object = rig
    return rig, centers


def write_maps(folder, slot, base, normal, orm, alpha=None, thin=None):
    files = {}
    files['albedo'] = os.path.join(folder, f'{NAME}_{slot}_albedo.png')
    kit.save_png(files['albedo'], base, srgb=True, alpha=alpha)
    files['normal'] = os.path.join(folder, f'{NAME}_{slot}_normal.png')
    kit.save_png(files['normal'], normal)
    files['orm'] = os.path.join(folder, f'{NAME}_{slot}_orm.png')
    kit.save_png(files['orm'], orm)
    if thin is not None:
        files['thin'] = os.path.join(folder, f'{NAME}_{slot}_thin.png')
        kit.save_png(files['thin'], np.repeat(thin[..., None], 3, axis=2), alpha=thin)
    return files


def assign(node, slot, files):
    placeholder = bpy.data.materials.new(slot)
    node.data.materials.clear()
    node.data.materials.append(kit.baked_material(placeholder, files))


def leaf_material(files):
    material = kit.baked_material(bpy.data.materials.new('Leaves'), files)
    tree = material.node_tree
    albedo = next(n for n in tree.nodes if n.type == 'TEX_IMAGE' and n.image and n.image.filepath.endswith('_albedo.png'))
    bsdf = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')
    tree.links.new(albedo.outputs['Alpha'], bsdf.inputs['Alpha'])
    material.use_backface_culling = False
    return material


def build(target, folder):
    started = time.time()
    rng = np.random.default_rng(11)
    prng = random.Random(11)
    source = load(target)
    kind, graph, centers, normals, areas, saturation = classify(source)
    clusters = stone_clusters(kind, graph, centers, areas, saturation)
    cap_points, cap_normals = sample_caps(centers, normals, areas, kind, CARDS, rng)
    frame = Frame(centers[kind == 0])
    pieces = stone_pieces(frame, clusters, centers)
    rock = rock_mask(source, kind, graph, centers, areas, saturation, frame, pieces)
    stones, stone_material = build_stones(pieces, target, prng)
    covered = buried(frame.apply(centers), stones) | (ground_faces(graph, centers, normals, areas) & ~rock)
    source.data.attributes.new('Rock', 'INT', 'FACE').data.foreach_set('value', rock.astype(np.int32))
    strip(source, kind, centers, normals, covered, frame.scale)
    frame.mesh(source)
    tuck(source, stones)
    print(f'cleaned in {time.time() - started:.0f} s, scale {frame.scale:.2f}')
    rock_part, rock = split_rock(source, target, stone_material)
    original = source.data.materials[0]
    source.data.materials.clear()
    source.data.materials.append(high_material(original))
    bark = source.copy()
    bark.data = source.data.copy()
    bark.name = 'Bark'
    target.objects.link(bark)
    delete_faces(bark, rock)
    ratio = BARK_FACES / len(bark.data.polygons)
    mp.protect_boundary(bark)
    mp.decimate(bark, BARK_FACES)
    bm = bmesh.new()
    bm.from_mesh(bark.data)
    cap_stubs(bm, STUB)
    bm.to_mesh(bark.data)
    bm.free()
    mp.protect_boundary(rock_part)
    mp.decimate(rock_part, max(ROCK[6], int(len(rock_part.data.polygons) * ratio * ROCK[7])))
    pk.tag(rock_part, rng=prng)
    stones = kit.join([stones, rock_part], 'Stones') if stones else rock_part
    kit.smooth_by_angle(stones, 35.0)
    kit.unwrap([stones], angle=60.0)
    kit.smooth_by_angle(bark, 50.0)
    while bark.data.uv_layers:
        bark.data.uv_layers.remove(bark.data.uv_layers[0])
    proxy_unwrap(bark, target)
    mp.CAGE = CAGE
    mp.AO_DISTANCE = AO_DISTANCE
    maps = mp.bake_from_high(source, bark, ('src', 'shape', 'noise', 'fibre', 'plates', 'height', 'normal'), BARK_TEXTURE)
    ao = mp.bake_ao(bark, [stones] if stones else [], BARK_TEXTURE)
    base, orm = compose_bark(maps, ao)
    bark_files = write_maps(folder, 'Bark', base, kit.sanitize_normals(maps['normal'], max_tilt=55.0), orm)
    bpy.data.objects.remove(source, do_unlink=True)
    assign(bark, 'Bark', bark_files)
    bark.data.materials[0].use_backface_culling = False
    if stones:
        hidden = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o not in (bark, stones)]
        for obj in hidden:
            obj.hide_render = True
        files = kit.bake_material(stone_material, [stones], folder, NAME, ao_distance=0.4)
        for obj in hidden:
            obj.hide_render = False
        stones.data.materials.clear()
        stones.data.materials.append(kit.baked_material(stone_material, files))
    leaf_base, leaf_alpha, leaf_normal, leaf_orm, leaf_thin = leaf_atlas(LEAF_TEXTURE, prng)
    leaf_files = write_maps(folder, 'Leaves', leaf_base, leaf_normal, leaf_orm, alpha=leaf_alpha, thin=leaf_thin)
    material = leaf_material(leaf_files)
    leaves, card_centers, crown, radii = build_leaves(frame, cap_points, cap_normals, target, material, rng)
    litter = build_litter(target, material, prng, lambda p: math.hypot(p.x, p.y) < 1.2)
    rig, cluster_centers = rig_tree(target, bark, leaves, card_centers, crown)
    report(bark, crown, radii, stones)
    print(f'built in {time.time() - started:.0f} s')
    objects = [bark, leaves, litter] + ([stones] if stones else [])
    return objects, rig, {'Leaves': LEAF_CUTOFF}, {'Leaves': (LEAF_TRANSLUCENCY, leaf_files['thin'])}


def report(bark, crown, radii, stones):
    co = np.empty(len(bark.data.vertices) * 3, np.float32)
    bark.data.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    band = (co[:, 2] > 0.6) & (co[:, 2] < 1.4)
    trunk = np.percentile(np.hypot(co[band, 0], co[band, 1]), 80) if band.any() else 1.0
    print('== engine constants (game model space: x, height, depth) ==')
    print('crown', (round(float(crown[0]), 2), round(float(crown[2]), 2), round(float(crown[1]), 2)), 'radii', np.round(radii, 2).tolist())
    print('trunk collider radius', round(float(trunk), 2), 'height', round(float(co[:, 2].max()), 2))
    if stones:
        corners = [stones.matrix_world @ Vector(c) for c in stones.bound_box]
        print('stones bounds', [round(min(c[i] for c in corners), 2) for i in range(3)], [round(max(c[i] for c in corners), 2) for i in range(3)])


def preview(objects, folder, views):
    kit.environment('forest.exr', 1.2)
    bpy.context.scene.cycles.transparent_max_bounces = 128
    statics = []
    for obj in objects:
        copy = obj.copy()
        copy.data = obj.data
        copy.modifiers.clear()
        copy.parent = None
        copy.matrix_world = Matrix.Identity(4)
        bpy.context.scene.collection.objects.link(copy)
        statics.append(copy)
        obj.hide_render = True
    shots = {'front': (0.0, 12.0), 'q': (35.0, 30.0), 'game': (0.0, 57.0), 'side': (90.0, 15.0), 'left': (270.0, 15.0), 'top': (0.0, 89.0), 'back': (180.0, 20.0)}
    closeups = {'bark': ((0.3, -0.8, 2.0), 0.0, 10.0, 3.5), 'leaves': ((0.0, -3.0, 8.5), 10.0, 15.0, 4.0), 'base': ((1.5, -2.0, 0.5), 20.0, 35.0, 4.5)}
    for view in views:
        if view in closeups:
            mp.closeup(os.path.join(folder, f'tree_{view}.png'), *closeups[view])
            continue
        yaw, pitch = shots[view]
        kit.render(os.path.join(folder, f'tree_{view}.png'), statics, yaw=yaw, pitch=pitch, size=(1100, 1100), samples=48)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    out = os.path.abspath(argv[argv.index('--out') + 1]) if '--out' in argv else None
    preview_dir = os.path.abspath(argv[argv.index('--preview') + 1]) if '--preview' in argv else None
    views = argv[argv.index('--views') + 1].split(',') if '--views' in argv else ['front', 'q', 'game']
    folder = out or preview_dir
    os.makedirs(folder, exist_ok=True)
    kit.reset()
    kit.enable_gpu()
    bpy.context.preferences.filepaths.save_version = 0
    target = kit.collection(NAME)
    objects, rig, masked, translucent = build(target, folder)
    if out:
        kit.export_gltf(objects, os.path.join(out, NAME + '.gltf'), {}, masked, rig, False, translucent, None)
        bpy.ops.outliner.orphans_purge(do_recursive=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, NAME + '.blend'))
    if preview_dir:
        preview(objects, preview_dir, views)


if __name__ == '__main__':
    main()
