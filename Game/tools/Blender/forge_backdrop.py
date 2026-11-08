import bpy
import bmesh
import math
import os
import random
import sys
import time
from mathutils import Matrix, Vector, kdtree
from mathutils.bvhtree import BVHTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import forge as fg

NAME = 'ForgeBackdrop'
F = fg.F
PIVOT = (0.0, 14.0, 3.6)
CRUCIBLE_SCALE = 1.4
BEARING = 2.0
TITAN_SHIFT = 3.0
FOOT_AT = (6.9, 15.6)
FOOT_SPLAY = 0.2
PLINTH_TOP = -1.95
PLINTH_TIERS = (4.1, 3.6, 3.15)
SOLE_DEPTH = 0.75
FOOT_FLOOR = PLINTH_TOP + SOLE_DEPTH
KNEE = (7.2, 20.8, 8.2)
ELBOW = (8.9, 25.8, 12.8)
FOOT_STATIONS = [(-2.4, 0.86, 0.95, 0.0, 0.0), (-2.15, 0.98, 1.2, 0.0, 0.0), (-1.6, 1.0, 1.5, 0.02, 0.0), (-0.9, 1.04, 1.6, 0.04, 0.0), (-0.2, 1.1, 1.32, 0.06, 0.0), (0.5, 1.2, 1.02, 0.08, 0.0), (1.1, 1.26, 0.8, 0.08, 0.0), (1.55, 1.27, 0.66, 0.08, 0.0), (1.85, 1.22, 0.56, 0.08, 0.0)]
TOE_ROOT = 1.55
TOES = [(0.9, 0.8, 0.66, 1.1), (0.25, 0.52, 0.5, 0.98), (-0.27, 0.48, 0.46, 0.86), (-0.71, 0.42, 0.42, 0.74), (-1.09, 0.34, 0.37, 0.6)]
SOLE_TABLE = [(-2.62, 0.0), (-2.55, 0.78), (-2.35, 1.0), (-1.5, 1.04), (-0.5, 1.12), (0.5, 1.24), (1.4, 1.32), (2.1, 1.32), (2.6, 1.12), (2.85, 0.72), (2.95, 0.0)]


def W(x, y, z=0.0):
    return Vector((-x, -y, z))


def T(x, y, z=0.0):
    return W(x, y - TITAN_SHIFT, z)


def placed(x, y, z=0.0, yaw=0.0):
    return Matrix.Translation(W(x, y, z)) @ Matrix.Rotation(yaw, 4, 'Z')


def facing_center(x, y):
    return math.atan2(-x, y)


def oriented(bm, start, end):
    a, b = Vector(start), Vector(end)
    rotation = (b - a).normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation(a) @ rotation, verts=bm.verts)
    return bm


def sleeve(start, end, radius, steps=20, lip=0.12):
    length = (Vector(end) - Vector(start)).length
    bm = pk.lathe([(radius, 0.0), (radius + lip, 0.06), (radius + lip, length - 0.06), (radius, length)], steps)
    return oriented(bm, start, end)


def ring(center, axis, major, minor, steps=28, sides=8):
    bm = pk.torus(major, minor, steps, sides)
    c = Vector(center)
    return oriented(bm, c, c + Vector(axis).normalized())


def prism_bm(points, z0, z1):
    bm = bmesh.new()
    bottom = [bm.verts.new((x, y, z0)) for x, y in points]
    top = [bm.verts.new((x, y, z1)) for x, y in points]
    bm.faces.new(bottom)
    bm.faces.new(list(reversed(top)))
    for i in range(len(points)):
        j = (i + 1) % len(points)
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def molten_material(name, bake_size=512, strength=5.0):
    s = kit.Shader(name)
    p = pk.local(s)
    warped = s.offset(p, s.mul(s.noise_color(p, 1.5, detail=2.0), 0.3))
    cells = s.voronoi(warped, 2.2, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    crust = s.smooth(0.05, 0.16, cells)
    heat = s.smooth(0.3, 0.75, s.noise(p, 1.2, detail=3.0))
    glow = s.clamp01(s.sub(1.0, s.mul(crust, s.sub(0.85, s.mul(heat, 0.6)))))
    base = s.mix(glow, (0.03, 0.025, 0.022), (0.9, 0.45, 0.12))
    return s.finish(base, s.lerp(glow, 0.85, 0.3), height=crust, bump=0.2, bump_distance=0.01, emission=glow, emission_color=(1.0, 0.45, 0.1), emission_strength=strength, bake_size=bake_size)


def furnace_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(p)
    flame = s.noise(s.stretch(p, (2.0, 2.0, 0.6)), 1.6, detail=4.0, distortion=1.5)
    low = s.sub(1.0, s.smooth(0.0, 3.5, z))
    glow = s.clamp01(s.add(s.mul(low, 0.7), s.mul(s.smooth(0.45, 0.75, flame), 0.6)))
    base = s.mix(glow, (0.04, 0.03, 0.025), (1.0, 0.55, 0.2))
    return s.finish(base, 0.8, height=flame, bump=0.1, bump_distance=0.01, emission=glow, emission_color=(1.0, 0.5, 0.14), emission_strength=4.0, bake_size=bake_size)


def materials():
    return {
        'stone': pk.stone_material('TitanStone', light=(0.25, 0.205, 0.17), dark=(0.085, 0.07, 0.06), bake_size=2048),
        'bronze': aged_bronze_material('TitanBronze'),
        'masonry': pk.masonry_material('CavernMasonry', light=(0.2, 0.17, 0.15), dark=(0.08, 0.068, 0.06), brick=(1.6, 0.8), mortar=(0.04, 0.034, 0.03), soot=0.7, bake_size=2048),
        'basalt': fg.block_material('CavernBasalt', 1024),
        'iron': forged_iron_material('BackIron'),
        'steel': pk.iron_material('BladeSteel', tone=(0.2, 0.2, 0.21), rust=0.2, bake_size=1024),
        'wood': pk.wood_material('HaftWood', light=(0.2, 0.12, 0.07), dark=(0.08, 0.05, 0.03), bake_size=512),
        'molten': molten_material('MoltenTop'),
        'furnace': furnace_material('FurnaceGlow'),
        "leather": pk.leather_material("BellowsLeather", tone=(0.16, 0.07, 0.04), bake_size=1024),
        "chain": fg.chain_material("BackChain", 1024),
        "obsidian": obsidian_material("ScatterObsidian"),
        "gold": aged_bronze_material("GoldIngot", tone=(0.85, 0.6, 0.2), bake_size=512),
        'eyes': pk.glow_material('EmberEyes', color=(1.0, 0.5, 0.12), strength=8.0, base=(1.0, 0.55, 0.2)),
        'hide': hide_material('TitanHide'),
        'titan': hide_material('TitanBasalt', light=(0.13, 0.117, 0.108), dark=(0.035, 0.032, 0.03), cracks=0.64, strength=1.5, seams=0.06),
        'mold': hide_material('MoldStone', light=(0.2, 0.178, 0.16), dark=(0.065, 0.058, 0.054)),
        'slag': hide_material('SlagCrust', light=(0.06, 0.052, 0.05), dark=(0.016, 0.014, 0.013), bake_size=1024),
        'castbronze': aged_bronze_material('CastBronze', tone=(0.3, 0.19, 0.09), bake_size=1024),
        'kiln': pk.masonry_material('KilnStone', light=(0.085, 0.075, 0.07), dark=(0.03, 0.026, 0.025), brick=(1.4, 0.7), mortar=(0.012, 0.01, 0.01), soot=0.5, glow=0.45, bake_size=2048, relief=0.8),
        'vatstone': pk.masonry_material('VatStone', light=(0.09, 0.08, 0.075), dark=(0.032, 0.028, 0.026), brick=(1.1, 0.65), mortar=(0.012, 0.01, 0.01), soot=0.4, glow=0.35, bake_size=1024, radius=3.2, relief=0.8),
        'vatcrust': crust_top_material('VatCrust'),
    }


def aged_bronze_material(name, tone=(0.34, 0.21, 0.1), bake_size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    noise = s.noise(p, 9.0, detail=4.0)
    blotch = s.smooth(0.45, 0.75, s.noise(p, 1.6, detail=3.0))
    hammer = s.voronoi(p, 14.0, 'F1', 1.0, 'Distance')
    pits = pk.cavity(s, 0.05)
    edge = pk.wear(s, 0.018)
    base = s.mix(noise, (tone[0] * 0.7, tone[1] * 0.7, tone[2] * 0.7), tone)
    tarnish = s.clamp01(s.add(s.mul(pits, 0.9), s.mul(blotch, 0.45)))
    base = s.mix(tarnish, base, (0.055, 0.04, 0.03))
    patina = s.mul(s.mul(pits, s.smooth(0.55, 0.75, s.noise(p, 3.5, detail=3.0))), 0.6)
    base = s.mix(patina, base, (0.06, 0.1, 0.085))
    base = s.mix(s.mul(edge, 0.85), base, (min(1.0, tone[0] * 1.9), min(1.0, tone[1] * 1.85), min(1.0, tone[2] * 1.7)))
    rough = s.remap(noise, 0.3, 0.7, 0.3, 0.46)
    rough = s.lerp(tarnish, rough, 0.72)
    rough = s.lerp(s.mul(edge, 0.8), rough, 0.18)
    metal = s.lerp(s.clamp01(s.add(tarnish, patina)), 1.0, 0.35)
    metal = s.lerp(edge, metal, 1.0)
    height = s.sub(s.add(s.mul(hammer, 0.55), s.mul(noise, 0.25)), s.mul(pits, 0.3))
    return s.finish(base, rough, height=height, bump=0.45, bump_distance=0.006, metallic=metal, bake_size=bake_size)


def forged_iron_material(name, tone=(0.05, 0.047, 0.045), bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    noise = s.noise(p, 7.0, detail=4.0)
    hammer = s.voronoi(p, 11.0, 'F1', 1.0, 'Distance')
    scale = s.smooth(0.5, 0.72, s.noise(p, 2.2, detail=4.0))
    pits = pk.cavity(s, 0.05)
    edge = pk.wear(s, 0.02)
    base = s.mix(noise, tone, (tone[0] * 1.8, tone[1] * 1.8, tone[2] * 1.8))
    rust = s.clamp01(s.add(s.mul(pits, 0.7), s.mul(scale, 0.35)))
    base = s.mix(rust, base, (0.09, 0.045, 0.025))
    base = s.mix(s.mul(edge, 0.9), base, (0.36, 0.35, 0.34))
    rough = s.remap(noise, 0.3, 0.7, 0.5, 0.7)
    rough = s.lerp(rust, rough, 0.88)
    rough = s.lerp(s.mul(edge, 0.85), rough, 0.25)
    metal = s.lerp(rust, 0.95, 0.2)
    metal = s.lerp(edge, metal, 1.0)
    height = s.sub(s.add(s.mul(hammer, 0.6), s.mul(noise, 0.3)), s.add(s.mul(pits, 0.25), s.mul(scale, 0.15)))
    return s.finish(base, rough, height=height, bump=0.55, bump_distance=0.006, metallic=metal, bake_size=bake_size)


def crust_top_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    warped = s.offset(p, s.mul(s.noise_color(p, 1.2, detail=2.0), 0.35))
    edge = s.voronoi(warped, 1.5, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    gap = s.sub(1.0, s.smooth(0.0, 0.06, edge))
    core = s.sub(1.0, s.smooth(0.0, 0.022, edge))
    noise = s.noise(p, 5.0, detail=4.0)
    glow = s.clamp01(s.add(s.mul(gap, 0.25), s.mul(core, 0.55)))
    base = s.mix(noise, (0.02, 0.018, 0.017), (0.065, 0.054, 0.05))
    base = s.mix(glow, base, (0.95, 0.42, 0.1))
    height = s.add(s.smooth(0.0, 0.09, edge), s.mul(noise, 0.2))
    return s.finish(base, s.lerp(gap, 0.86, 0.35), height=height, bump=0.6, bump_distance=0.03, emission=glow, emission_color=(1.0, 0.42, 0.12), emission_strength=2.8, bake_size=bake_size)


def hide_material(name, light=(0.21, 0.192, 0.176), dark=(0.07, 0.064, 0.06), bake_size=2048, cracks=0.5, strength=3.0, seams=0.7):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(s.coords('Object'))
    large = s.noise(p, 0.9, detail=3.0)
    mid = s.noise(p, 4.0, detail=5.0, roughness=0.55)
    fine = s.noise(p, 30.0, detail=2.0)
    tone = s.clamp01(s.add(s.mul(s.smooth(0.3, 0.7, large), 0.55), s.mul(mid, 0.45)))
    base = s.mix(tone, dark, light)
    base = s.mix(s.mul(s.smooth(0.55, 0.78, s.noise(p, 2.2, detail=3.0)), 0.35), base, (0.13, 0.1, 0.085))
    specks = s.sub(1.0, s.smooth(0.0, 0.12, s.voronoi(p, 70.0, 'F1', 1.0, 'Distance')))
    base = s.mix(s.mul(specks, 0.3), base, (0.05, 0.046, 0.044))
    seam = pk.cavity(s, 0.07)
    base = s.mix(s.mul(seam, 0.8), base, (0.022, 0.018, 0.016))
    edge = pk.wear(s, 0.03)
    base = s.mix(s.mul(edge, 0.4), base, (0.27, 0.25, 0.23))
    warped = s.offset(p, s.mul(s.noise_color(p, 0.8, detail=2.0), 0.4))
    crack = s.sub(1.0, s.smooth(0.0, 0.03, s.voronoi(warped, 0.85, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
    crack = s.mul(crack, s.smooth(cracks, cracks + 0.16, s.noise(p, 0.5, detail=2.0)))
    core = s.sub(1.0, s.smooth(0.0, 0.012, s.voronoi(warped, 0.85, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
    heat = s.sub(1.0, s.smooth(-1.2, 2.2, z))
    glow = s.clamp01(s.add(s.mul(s.mul(core, s.smooth(cracks, cracks + 0.16, s.noise(p, 0.5, detail=2.0))), s.add(0.45, s.mul(heat, 0.55))), s.mul(s.smooth(0.55, 0.9, seam), s.mul(heat, seams))))
    base = s.mix(s.mul(crack, 0.85), base, (0.02, 0.012, 0.01))
    base = s.mix(glow, base, (0.9, 0.38, 0.1))
    rough = s.remap(fine, 0.3, 0.7, 0.72, 0.9)
    rough = s.lerp(s.mul(edge, 0.4), rough, 0.6)
    height = s.sub(s.add(s.mul(mid, 0.7), s.mul(fine, 0.08)), s.mul(crack, 0.6))
    return s.finish(base, rough, height=height, bump=0.3, bump_distance=0.012, emission=glow, emission_color=(1.0, 0.42, 0.12), emission_strength=strength, bake_size=bake_size)


def to_world(bm, frame):
    bmesh.ops.transform(bm, matrix=frame, verts=bm.verts)
    if frame.to_3x3().determinant() < 0.0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    return bm


def surface_tree(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.transform(obj.matrix_world)
    tree = BVHTree.FromBMesh(bm)
    bm.free()
    return tree


def chisel(obj, rng, cell, depth, tilt, groove, groove_depth):
    mesh = obj.data
    spacing = sum((mesh.vertices[e.vertices[0]].co - mesh.vertices[e.vertices[1]].co).length for e in mesh.edges[:2000]) / min(len(mesh.edges), 2000)
    share = min(1.0, (spacing / cell) ** 2)
    seeds = [v.co.copy() for v in mesh.vertices if rng.random() < share]
    tree = kdtree.KDTree(len(seeds))
    for index, co in enumerate(seeds):
        tree.insert(co, index)
    tree.balance()
    lifts = [rng.uniform(-depth, depth) for _ in seeds]
    slants = [Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0))) * tilt for _ in seeds]
    moved = []
    for v in mesh.vertices:
        near = tree.find_n(v.co, 2)
        (c1, i1, d1), (_, _, d2) = near[0], near[1]
        push = lifts[i1] + slants[i1].dot(v.co - c1)
        gap = d2 - d1
        if gap < groove:
            push -= groove_depth * (1.0 - gap / groove)
        moved.append(v.co + v.normal * push)
    for v, co in zip(mesh.vertices, moved):
        v.co = co
    mesh.update()


def carved(shapes, target, material, name, voxel, faces, rng, cell, depth, tilt, groove, groove_depth, smooth=3):
    obj = kit.mesh_object(name, pk.merge_bm(shapes), target, material)
    remesh = obj.modifiers.new('Remesh', 'REMESH')
    remesh.mode = 'VOXEL'
    remesh.voxel_size = voxel
    smoother = obj.modifiers.new('Smooth', 'SMOOTH')
    smoother.factor = 0.6
    smoother.iterations = smooth
    kit.apply_modifiers(obj)
    chisel(obj, rng, cell, depth, tilt, groove, groove_depth)
    decimate = obj.modifiers.new('Decimate', 'DECIMATE')
    decimate.ratio = min(1.0, faces / max(len(obj.data.polygons), 1))
    kit.apply_modifiers(obj)
    pk.tag(obj, rng=rng)
    kit.smooth_by_angle(obj, 32.0)
    return obj


def ribbon(samples, across, width, thick, closed=False, embed=0.03):
    bm = bmesh.new()
    count = len(samples)
    normals = []
    for i in range(count):
        total = Vector()
        for j in range(i - 2, i + 3):
            if closed or 0 <= j < count:
                total += samples[j % count][1]
        normals.append(total.normalized())
    rings = []
    for (point, _), n in zip(samples, normals):
        w = (across - n * n.dot(across)).normalized() * (0.5 * width)
        low, high = point - n * embed, point + n * thick
        rings.append([bm.verts.new(low - w), bm.verts.new(low + w), bm.verts.new(high + w), bm.verts.new(high - w)])
    spans = count if closed else count - 1
    for i in range(spans):
        a, b = rings[i], rings[(i + 1) % count]
        for k in range(4):
            bm.faces.new((a[k], a[(k + 1) % 4], b[(k + 1) % 4], b[k]))
    if not closed:
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm, [(point, n) for (point, _), n in zip(samples, normals)]


def wrap(tree, center, u, v, start, stop, count, reach=8.0):
    samples = []
    for k in range(count + 1):
        a = start + (stop - start) * k / count
        d = u * math.cos(a) + v * math.sin(a)
        hit, normal, _, _ = tree.ray_cast(center + d * reach, -d, 2.0 * reach)
        if hit is not None:
            samples.append((hit, d if normal.dot(d) < 0.2 else normal))
    return samples


def strap(tree, center, u, v, start, stop, width, thick, count=36, rivets=0, rivet_size=0.06):
    closed = abs(stop - start) >= 2.0 * math.pi - 1e-4
    samples = wrap(tree, center, u, v, start, stop - (2.0 * math.pi / count if closed else 0.0), count - (1 if closed else 0))
    bm, placed = ribbon(samples, u.cross(v).normalized(), width, thick, closed)
    studs = []
    if rivets:
        for k in range(rivets):
            point, n = placed[int((k + 0.5) * len(placed) / rivets)]
            studs.append(pk.ellipsoid_bm(point + n * thick, (rivet_size, rivet_size, rivet_size), segments=6, rings=4))
    return bm, placed, studs


def sole_outline(grow, steps=44):
    ys = [SOLE_TABLE[0][0] + (SOLE_TABLE[-1][0] - SOLE_TABLE[0][0]) * 0.5 * (1.0 - math.cos(math.pi * k / steps)) for k in range(steps + 1)]

    def half(y):
        for (y0, w0), (y1, w1) in zip(SOLE_TABLE, SOLE_TABLE[1:]):
            if y <= y1:
                t = (y - y0) / (y1 - y0)
                return w0 + (w1 - w0) * t
        return SOLE_TABLE[-1][1]

    def bias(y):
        return 0.12 * min(1.0, max(0.0, (y - 0.5) / 2.0))

    stretch = (SOLE_TABLE[-1][0] - SOLE_TABLE[0][0] + 2.0 * grow) / (SOLE_TABLE[-1][0] - SOLE_TABLE[0][0])
    middle = 0.5 * (SOLE_TABLE[-1][0] + SOLE_TABLE[0][0])
    right = [(half(y) + grow * min(1.0, 4.0 * half(y)) + bias(y), middle + (y - middle) * stretch) for y in ys]
    left = [(-half(y) - grow * min(1.0, 4.0 * half(y)) + bias(y), middle + (y - middle) * stretch) for y in reversed(ys)]
    points = right + left[1:-1]
    return points


def outline_blocks(points, cuts, gap, z0, z1):
    blocks = []
    edges = [-1e3] + list(cuts) + [1e3]
    for a, b in zip(edges, edges[1:]):
        piece = fg.clip_half(points, (0.0, 1.0), a + 0.5 * gap)
        piece = fg.clip_half(piece, (0.0, -1.0), -(b - 0.5 * gap))
        if len(piece) >= 3:
            blocks.append(prism_bm(piece, z0, z1))
    return blocks


def outline_rivets(points, z, every, size):
    studs = []
    total = 0.0
    next_at = 0.5 * every
    count = len(points)
    for i in range(count):
        a, b = Vector(points[i]), Vector(points[(i + 1) % count])
        length = (b - a).length
        while next_at <= total + length and length > 1e-6:
            t = (next_at - total) / length
            p = a.lerp(b, t)
            out = Vector((b.y - a.y, a.x - b.x)).normalized()
            studs.append(pk.ellipsoid_bm((p.x + out.x * 0.01, p.y + out.y * 0.01, z), (size, size, size), segments=6, rings=4))
            next_at += every
        total += length
    return studs


def foot_frame(side):
    mirror = Matrix.Identity(4) if side > 0.0 else Matrix.Diagonal((-1.0, 1.0, 1.0, 1.0))
    return Matrix.Translation(W(side * FOOT_AT[0], FOOT_AT[1], FOOT_FLOOR)) @ mirror @ Matrix.Rotation(FOOT_SPLAY, 4, 'Z')


def carved_ring(w, h):
    pts = [(-0.9 * w, 0.0), (0.9 * w, 0.0)]
    for k in range(1, 10):
        t = math.pi * k / 10.0
        c, s = math.cos(t), math.sin(t)
        pts.append((w * math.copysign(abs(c) ** 0.62, c), h * s ** 0.55))
    return pts[:2] + pts[2:]


def block_loft(stations, rng, wobble=0.025):
    bm = bmesh.new()
    rings = []
    for y, w, h, c, b in stations:
        rings.append([bm.verts.new((c + x + rng.uniform(-wobble, wobble), y + rng.uniform(-wobble, wobble), b + z + (rng.uniform(-wobble, wobble) if z > 0.0 else 0.0))) for x, z in carved_ring(w, h)])
    count = len(rings[0])
    for a, b in zip(rings, rings[1:]):
        for k in range(count):
            j = (k + 1) % count
            bm.faces.new((a[k], a[j], b[j], b[k]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def toe_frame(x, root_y, spread):
    return Matrix.Translation((x, root_y, 0.0)) @ Matrix.Rotation(-spread, 4, 'Z')


def toe_mesh(width, height, length, rng):
    return block_loft(toe_stations(width, height, length), rng, wobble=0.015)


def foot_parts(rng):
    body = [block_loft(FOOT_STATIONS, rng)]
    nails = []
    for x, width, height, length in TOES:
        frame = toe_frame(x, TOE_ROOT, 0.06 * x)
        toe = toe_mesh(width - 0.07, height, length, rng)
        bmesh.ops.transform(toe, matrix=frame, verts=toe.verts)
        body.append(toe)
        nail = toenail_bm(width - 0.07, height, length)
        bmesh.ops.transform(nail, matrix=frame, verts=nail.verts)
        nails.append(nail)
    return body, nails


def shin_mesh(ankle, knee, fwd, lat, rng, sides=12, rows=10):
    axis = knee - ankle
    bm = bmesh.new()
    rings = []
    for i in range(rows + 1):
        s = i / rows
        center = ankle + axis * s
        radius = 1.02 + (1.32 - 1.02) * s
        calf = math.exp(-((s - 0.42) / 0.22) ** 2)
        ring_verts = []
        for k in range(sides):
            a = 2.0 * math.pi * (k + 0.5 * (i % 2)) / sides
            d = fwd * math.cos(a) + lat * math.sin(a)
            r = radius * (1.0 + 0.24 * calf * max(0.0, -math.cos(a))) * rng.uniform(0.985, 1.015)
            ring_verts.append(bm.verts.new(center + d * r))
        rings.append(ring_verts)
    for a, b in zip(rings, rings[1:]):
        for k in range(sides):
            j = (k + 1) % sides
            bm.faces.new((a[k], a[j], b[j], b[k]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def octagon_ring(center, axis, fwd, radius, z0, z1, sides=10):
    lat = axis.cross(fwd).normalized()
    pts = [(radius * math.cos(2.0 * math.pi * (k + 0.5) / sides), radius * math.sin(2.0 * math.pi * (k + 0.5) / sides)) for k in range(sides)]
    bm = prism_bm(pts, z0, z1)
    basis = Matrix((fwd, lat, axis)).transposed().to_4x4()
    if basis.determinant() < 0.0:
        basis = Matrix((lat, fwd, axis)).transposed().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation(center) @ basis, verts=bm.verts)
    return bm


def medallion(point, normal, up, size):
    n = normal.normalized()
    across = up.cross(n).normalized()
    up = n.cross(across).normalized()
    basis = Matrix((across, up, n)).transposed().to_4x4()
    frame = Matrix.Translation(point) @ basis
    parts = [pk.cylinder(size, 0.1, 16, z=0.0)]
    for r, minor in ((size * 0.95, 0.05), (size * 0.62, 0.045), (size * 0.32, 0.04)):
        parts.append(pk.torus(r, minor, 24, 6))
        bmesh.ops.translate(parts[-1], verts=parts[-1].verts, vec=(0.0, 0.0, 0.1))
    parts.append(pk.ellipsoid_bm((0.0, 0.0, 0.12), (size * 0.18, size * 0.18, size * 0.14), segments=10, rings=6))
    for k in range(8):
        a = 2.0 * math.pi * k / 8.0
        parts.append(pk.ellipsoid_bm((math.cos(a) * size * 0.8, math.sin(a) * size * 0.8, 0.12), (0.04, 0.04, 0.04), segments=6, rings=4))
    for bm in parts:
        bmesh.ops.transform(bm, matrix=frame, verts=bm.verts)
    return parts


def titan_leg(side, m, target, rng):
    frame = foot_frame(side)
    local = frame.inverted()
    knee = local @ Vector(T(side * KNEE[0], KNEE[1], KNEE[2]))
    ankle = Vector((0.0, -1.05, 1.05))
    damaged = side < 0.0
    shin_axis = (knee - ankle).normalized()
    fwd = (Vector((0.0, 1.0, 0.0)) - shin_axis * shin_axis.y).normalized()
    lat = shin_axis.cross(fwd).normalized()
    body, nail_bms = foot_parts(rng)
    body.append(shin_mesh(ankle - shin_axis * 0.4, knee, fwd, lat, rng))
    body.append(pk.ellipsoid_bm((0.0, -2.05, 1.0), (0.62, 0.42, 0.85), segments=10, rings=6))
    for b in body:
        to_world(b, frame)
    leg = carved(body, target, m['titan'], f'TitanLeg{int(side)}', 0.04, 14000, rng, 0.9, 0.012, 0.015, 0.05, 0.055, smooth=2)
    tree = surface_tree(leg)
    world = lambda p: frame @ Vector(p)
    axis_w = lambda p: (frame.to_3x3() @ Vector(p)).normalized()
    nail_obj = kit.mesh_object(f'TitanNails{int(side)}', pk.merge_bm([to_world(b, frame) for b in nail_bms]), target, m['titan'])
    pk.tag(nail_obj, rng=rng)
    kit.smooth_by_angle(nail_obj, 60.0)

    bronze, studs, irons, stone = [], [], [], []

    def heavy_strap(center, u, v, start, stop, width, thick, buckle):
        samples = wrap(tree, world(center), axis_w(u), axis_w(v), start, stop, 30)
        if len(samples) < 3:
            return
        across = axis_w(u).cross(axis_w(v)).normalized()
        bm, placed = ribbon(samples, across, width, thick)
        bronze.append(bm)
        for edge in (-1.0, 1.0):
            rim = [(p + across * edge * (0.5 * width - 0.04), n) for p, n in samples]
            bm, _ = ribbon(rim, across, 0.06, thick + 0.035)
            bronze.append(bm)
        for k in range(1, len(placed) - 1, 3):
            p, n = placed[k]
            studs.append(pk.ellipsoid_bm(p + n * (thick + 0.01), (0.075, 0.075, 0.06), segments=8, rings=4))
        if buckle:
            p, n = placed[min(len(placed) - 2, int(len(placed) * 0.85))]
            tangent = n.cross(across).normalized()
            basis = Matrix((across, tangent, n)).transposed().to_4x4()
            for size, c in (((0.5, 0.09, 0.12), (0.0, 0.33, 0.06)), ((0.5, 0.09, 0.12), (0.0, -0.33, 0.06)), ((0.09, 0.75, 0.12), (0.25, 0.0, 0.06)), ((0.09, 0.75, 0.12), (-0.25, 0.0, 0.06)), ((0.42, 0.07, 0.07), (0.0, 0.0, 0.12))):
                piece = pk.box(size, c)
                bmesh.ops.transform(piece, matrix=Matrix.Translation(p + n * thick) @ basis, verts=piece.verts)
                irons.append(piece)

    straps = [(1.35, 0.12, 0.0, math.pi), (0.35, -0.3, 0.0, math.pi), (-0.55, 0.32, 0.0, math.pi)]
    for k, (y, yaw, start, stop) in enumerate(straps):
        u = Vector((math.cos(yaw), math.sin(yaw), 0.0))
        if damaged and k == 1:
            heavy_strap((0.0, y, -0.02), u, Vector((0.0, 0.0, 1.0)), 0.0, 1.3, 0.46, 0.11, False)
            heavy_strap((0.0, y, -0.02), u, Vector((0.0, 0.0, 1.0)), 2.2, math.pi, 0.46, 0.11, False)
        else:
            heavy_strap((0.0, y, -0.02), u, Vector((0.0, 0.0, 1.0)), start, stop, 0.46, 0.11, True)
    heel = wrap(tree, world((0.0, -1.6, 0.55)), axis_w((1.0, 0.0, 0.0)), axis_w((0.0, -1.0, 0.0)), 0.0, math.pi, 24)
    if len(heel) > 2:
        bm, _ = ribbon(heel, axis_w((0.0, 0.0, 1.0)), 0.42, 0.1)
        bronze.append(bm)

    guard_low = ankle - shin_axis * 0.05
    stone.append(to_world(octagon_ring(guard_low, shin_axis, fwd, 1.32, 0.0, 0.55), frame))
    stone.append(to_world(octagon_ring(guard_low, shin_axis, fwd, 1.22, 0.55, 0.85), frame))
    for k, (s0, s1, r) in enumerate(((0.26, 0.31, 1.3), (0.33, 0.38, 1.33), (0.4, 0.45, 1.37), (0.82, 0.87, 1.58))):
        if damaged and k == 2:
            continue
        a, b = ankle.lerp(knee, s0), ankle.lerp(knee, s1)
        bronze.append(to_world(octagon_ring(a, shin_axis, fwd, r + 0.08, 0.0, (b - a).length, sides=12), frame))
    for s0, s1 in ((0.5, 0.56), (0.66, 0.72)):
        a, b = ankle.lerp(knee, s0), ankle.lerp(knee, s1)
        stone.append(to_world(octagon_ring(a, shin_axis, fwd, 1.5, 0.0, (b - a).length, sides=12), frame))

    span = 0.95

    def shell(s0, s1, a0, a1, lift, thick, steps_s, steps_a, skip=None):
        bm = bmesh.new()
        grid = []
        for i in range(steps_s + 1):
            s = s0 + (s1 - s0) * i / steps_s
            center = ankle.lerp(knee, s)
            radius = 0.0
            for j in range(15):
                a = -span - 0.2 + (2.0 * span + 0.4) * j / 14
                d = fwd * math.cos(a) + lat * math.sin(a)
                hit, _, _, _ = tree.ray_cast(world(center + d * 6.0), axis_w(-d), 12.0)
                if hit is not None:
                    radius = max(radius, (local @ hit - center).length)
            dirs = [fwd * math.cos(a0 + (a1 - a0) * j / steps_a) + lat * math.sin(a0 + (a1 - a0) * j / steps_a) for j in range(steps_a + 1)]
            grid.append([(bm.verts.new(world(center + d * (radius + lift))), bm.verts.new(world(center + d * (radius + lift + thick)))) for d in dirs])
        cells = {(i, j) for i in range(steps_s) for j in range(steps_a) if not (skip and skip(i, j))}
        for i, j in cells:
            a, b, c, e = grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]
            bm.faces.new((a[1], b[1], c[1], e[1]))
            bm.faces.new((e[0], c[0], b[0], a[0]))
            for (di, dj), (p, q) in (((-1, 0), (a, b)), ((0, 1), (b, c)), ((1, 0), (c, e)), ((0, -1), (e, a))):
                if (i + di, j + dj) not in cells:
                    bm.faces.new((p[0], q[0], q[1], p[1]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        return bm, grid

    plates = ((0.12, 0.25, 0.04), (0.23, 0.5, 0.1), (0.48, 0.8, 0.16))
    grids = []
    for k, (s0, s1, lift) in enumerate(plates):
        hole = (lambda i, j: 3 <= i <= 6 and 7 <= j <= 10) if damaged and k == 2 else None
        bm, grid = shell(s0, s1, -span, span, lift, 0.09, 8, 12, hole)
        bronze.append(bm)
        grids.append(grid)
        length = (knee - ankle).length
        ds = 0.09 / length
        for r0, r1, a0, a1 in ((s1 - ds, s1, -span, span), (s0, s0 + ds, -span, span)):
            rim, _ = shell(r0, r1, a0, a1, lift + 0.07, 0.05, 1, 12)
            bronze.append(rim)
    grid = grids[2]
    middle = grid[4][6]
    face_point = middle[1].co.copy()
    face_n = (face_point - world(ankle.lerp(knee, 0.64))).normalized()
    face_n = (face_n - axis_w(shin_axis) * face_n.dot(axis_w(shin_axis))).normalized()
    h_axis = axis_w(shin_axis).cross(face_n).normalized()
    emblem = Matrix.Translation(face_point - face_n * 0.03) @ Matrix((h_axis, axis_w(shin_axis), face_n)).transposed().to_4x4() @ Matrix.Scale(0.75, 4)
    flame = [(0.0, 1.05), (0.16, 0.7), (0.34, 0.86), (0.3, 0.5), (0.52, 0.6), (0.44, 0.22), (0.26, 0.3), (0.0, 0.16), (-0.26, 0.3), (-0.44, 0.22), (-0.52, 0.6), (-0.3, 0.5), (-0.34, 0.86), (-0.16, 0.7)]
    hammer = [[(-0.08, -0.85), (0.08, -0.85), (0.08, 0.2), (-0.08, 0.2)], [(-0.46, 0.08), (0.46, 0.08), (0.46, 0.42), (-0.46, 0.42)], [(-0.14, -0.95), (0.14, -0.95), (0.14, -0.82), (-0.14, -0.82)]]
    if not damaged:
        for points, z1 in [(flame, 0.16)] + [(h, 0.22) for h in hammer]:
            bm = prism_bm(points, 0.0, z1)
            bmesh.ops.transform(bm, matrix=emblem, verts=bm.verts)
            bronze.append(bm)
    for gi, rows in ((0, (1, 7)), (1, (1, 7)), (2, (1, 7))):
        for i in rows:
            for j in (1, 6, 11):
                p = grids[gi][i][j][1].co
                n = (p - world(ankle.lerp(knee, plates[gi][0] + (plates[gi][1] - plates[gi][0]) * i / 8))).normalized()
                studs.append(pk.ellipsoid_bm(p + n * 0.05, (0.07, 0.07, 0.06), segments=8, rings=4))
    for sign in (-1.0, 1.0):
        center = ankle + shin_axis * 0.55
        d = lat * sign
        hit, normal, _, _ = tree.ray_cast(world(center + d * 6.0), axis_w(-d), 12.0)
        if hit is not None:
            bronze += medallion(hit + normal * 0.3, normal, axis_w(shin_axis), 0.5)

    sole_parts = []
    layers = ((0.34, -SOLE_DEPTH, -0.5, (-1.4, 0.15, 1.7), Vector((0.03, -0.04, 0.0))), (0.26, -0.5, -0.26, (-0.9, 0.7, 2.1), Vector((-0.02, 0.03, 0.0))), (0.18, -0.26, 0.02, (-1.8, -0.3, 1.2), Vector((0.0, 0.0, 0.0))))
    for grow, z0, z1, cuts, shift in layers:
        outline = [(x + shift.x, y + shift.y) for x, y in sole_outline(grow)]
        for bm in outline_blocks(outline, cuts, 0.06, z0, z1):
            for corner in range(2):
                v = rng.choice(list(bm.verts))
                centroid = sum((u.co for u in bm.verts), Vector()) / len(bm.verts)
                normal = (v.co - centroid).normalized()
                kit.cut(bm, v.co - normal * rng.uniform(0.06, 0.14), normal)
            sole_parts.append(bm)
    trim = []
    for grow, z0, z1 in ((0.3, -0.53, -0.46), (0.22, -0.29, -0.23)):
        band = sole_outline(grow)
        trim.append(prism_bm(band, z0, z1))
        trim += outline_rivets(band, 0.5 * (z0 + z1), 0.6, 0.075)
    sole_obj = kit.mesh_object(f'TitanSole{int(side)}', pk.merge_bm([to_world(bm, frame) for bm in sole_parts] + stone), target, m['basalt'])
    pk.tag(sole_obj, rng=rng)
    kit.bevel(sole_obj, 0.05, segments=1, angle=40.0)

    bronze_obj = kit.mesh_object(f'TitanStraps{int(side)}', pk.merge_bm(bronze + [to_world(bm, frame) for bm in trim] + studs), target, m['bronze'])
    pk.tag(bronze_obj, rng=rng)
    kit.smooth_by_angle(bronze_obj, 40.0)

    if not damaged:
        shackle_bm, placed, _ = strap(tree, world(ankle + shin_axis * 1.2), axis_w(lat), axis_w(fwd), 0.0, 2.0 * math.pi, 0.4, 0.15)
        irons.append(shackle_bm)
        point, n = placed[int(len(placed) * 0.02)]
        lug = point + n * 0.2
        irons += chain_links([lug, world((-1.9, -0.6, -0.3)), world((-3.0, 0.3, -SOLE_DEPTH - 0.2)), world((-4.0, 1.4, -SOLE_DEPTH - 1.6))], 1.5, rng)
    iron_obj = kit.mesh_object(f'TitanShackle{int(side)}', pk.merge_bm(irons), target, m['iron'])
    pk.tag(iron_obj, rng=rng)
    kit.smooth_by_angle(iron_obj, 40.0)
    return [leg, nail_obj, sole_obj, bronze_obj, iron_obj]


def chain_links(points, scale, rng):
    links = []
    pitch = 0.62 * scale
    carry = 0.0
    k = 0
    for a, b in zip(points, points[1:]):
        a, b = Vector(a), Vector(b)
        axis = b - a
        length = axis.length
        direction = axis.normalized()
        t = carry
        while t < length:
            bm = link_bm(scale)
            rotation = direction.to_track_quat('X', 'Z').to_matrix().to_4x4() @ Matrix.Rotation(0.5 * math.pi * (k % 2) + rng.uniform(-0.15, 0.15), 4, 'X')
            bmesh.ops.transform(bm, matrix=Matrix.Translation(a + direction * t) @ rotation, verts=bm.verts)
            links.append(bm)
            t += pitch
            k += 1
        carry = t - length
    return links


def plinth(side, m, target, rng):
    frame = foot_frame(side) @ Matrix.Translation((0.0, 0.25, 0.0))
    top = PLINTH_TOP - FOOT_FLOOR
    stone, metal, irons = [], [], []
    corners = [(2.0 * math.pi * (k + 0.5) / 8.0 + rng.uniform(-0.09, 0.09), rng.uniform(0.94, 1.06)) for k in range(8)]
    tiers = ((PLINTH_TIERS[0], SUNK - FOOT_FLOOR, top - 0.95), (PLINTH_TIERS[1], top - 1.0, top - 0.45), (PLINTH_TIERS[2], top - 0.5, top))
    outlines = []
    for radius, z0, z1 in tiers:
        outline = [(math.cos(a) * radius * f, math.sin(a) * radius * f) for a, f in corners]
        outlines.append((outline, z1))
        for k in range(8):
            a, b = Vector(outline[k]), Vector(outline[(k + 1) % 8])
            for half in range(2):
                p0, p1 = a.lerp(b, half * 0.5 + 0.015), a.lerp(b, half * 0.5 + 0.485)
                inner0, inner1 = p0 * 0.55, p1 * 0.55
                block = prism_bm([tuple(p0), tuple(p1), tuple(inner1), tuple(inner0)], z0, z1 + rng.uniform(-0.06, 0.02))
                if rng.random() < 0.35:
                    corner = rng.choice([p0, p1])
                    normal = Vector((corner.x, corner.y, 0.0)).normalized() + Vector((0.0, 0.0, 0.7))
                    kit.cut(block, Vector((corner.x, corner.y, z1)) - normal.normalized() * rng.uniform(0.12, 0.3), normal.normalized())
                stone.append(to_world(block, frame))
        stone.append(to_world(prism_bm([(x * 0.57, y * 0.57) for x, y in outline], z0, z1 - 0.02), frame))
    outline, z1 = outlines[1]
    metal.append(to_world(ring_prism(outline, -0.02, 0.07, z1 - 0.32, z1 - 0.12), frame))
    for k in range(8):
        a, b = Vector(outline[k]), Vector(outline[(k + 1) % 8])
        mid = a.lerp(b, 0.5)
        out = Vector((b.y - a.y, a.x - b.x)).normalized()
        if out.dot(mid) < 0.0:
            out = -out
        along = (b - a).normalized()
        if k % 2 == 0:
            plate_frame = Matrix.Translation((mid.x + out.x * 0.08, mid.y + out.y * 0.08, z1 - 0.62)) @ Matrix(((along.x, 0.0, out.x, 0.0), (along.y, 0.0, out.y, 0.0), (0.0, 1.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))
            plate = pk.box((1.1, 0.75, 0.08))
            bmesh.ops.transform(plate, matrix=plate_frame, verts=plate.verts)
            metal.append(to_world(plate, frame))
            for dx in (-0.4, 0.4):
                for dz in (-0.25, 0.25):
                    stud = pk.ellipsoid_bm((dx, dz, 0.06), (0.09, 0.09, 0.07), segments=8, rings=4)
                    bmesh.ops.transform(stud, matrix=plate_frame, verts=stud.verts)
                    metal.append(to_world(stud, frame))
            eye = pk.torus(0.2, 0.06, 16, 6)
            bmesh.ops.transform(eye, matrix=plate_frame @ Matrix.Translation((0.0, -0.1, 0.2)) @ Matrix.Rotation(0.5 * math.pi, 4, 'X'), verts=eye.verts)
            irons.append(to_world(eye, frame))
            hang = frame @ Vector((mid.x + out.x * 0.35, mid.y + out.y * 0.35, z1 - 0.95))
            low = frame @ Vector((mid.x + out.x * 1.0, mid.y + out.y * 1.0, SUNK - FOOT_FLOOR + 1.0))
            irons += chain_links([hang, low], 0.9, rng)
        else:
            bracket = pk.box((0.25, 0.5, 1.0))
            corner = Vector(outline[(k + 1) % 8])
            cdir = corner.normalized()
            bmesh.ops.transform(bracket, matrix=Matrix.Translation((corner.x + cdir.x * 0.05, corner.y + cdir.y * 0.05, z1 - 0.55)) @ Matrix.Rotation(math.atan2(cdir.y, cdir.x), 4, 'Z'), verts=bracket.verts)
            irons.append(to_world(bracket, frame))
    stone_obj = kit.mesh_object(f'Plinth{int(side)}', pk.merge_bm(stone), target, m['basalt'])
    pk.tag(stone_obj, rng=rng)
    kit.bevel(stone_obj, 0.05, segments=1, angle=40.0)
    metal_obj = kit.mesh_object(f'PlinthBand{int(side)}', closed(pk.merge_bm(metal)), target, m['bronze'])
    pk.tag(metal_obj, rng=rng)
    kit.smooth_by_angle(metal_obj, 40.0)
    iron_obj = kit.mesh_object(f'PlinthIron{int(side)}', closed(pk.merge_bm(irons)), target, m['iron'])
    pk.tag(iron_obj, rng=rng)
    kit.smooth_by_angle(iron_obj, 40.0)
    x, y = side * FOOT_AT[0], FOOT_AT[1] - 0.25
    slag = slag_patch(f'PlinthSlag{int(side)}', x, y, 4.6, m, target, rng, rise=0.3)
    return [stone_obj, metal_obj, iron_obj, slag]


def closed(bm):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def resting_hand(side, m, target, rng):
    s = side
    shapes = []
    palm = Vector(T(s * 7.4, 20.9, 10.35))
    shapes.append(pk.ellipsoid_bm(tuple(palm), (0.95, 1.1, 0.42), segments=20, rings=10))
    for k in range(4):
        x = s * (6.85 + 0.37 * k)
        root = Vector(T(x, 20.15, 10.3))
        mid = Vector(T(x, 19.75, 9.8))
        tip = Vector(T(x, 19.45, 8.95 - 0.08 * abs(k - 1.5)))
        r = 0.27 - 0.02 * abs(k - 1.2)
        shapes.append(pk.capsule_bm(tuple(root), tuple(mid), r, r * 0.95, segments=12, rings=4))
        shapes.append(pk.capsule_bm(tuple(mid), tuple(tip), r * 0.95, r * 0.85, segments=12, rings=4))
    thumb_root = Vector(T(s * 6.6, 21.3, 10.35))
    shapes.append(pk.capsule_bm(tuple(thumb_root), tuple(Vector(T(s * 6.25, 20.3, 10.1))), 0.33, 0.27, segments=12, rings=4))
    shapes.append(pk.capsule_bm(tuple(palm + Vector((0.0, 0.0, 0.05))), T(s * ELBOW[0], ELBOW[1], ELBOW[2]), 0.85, 1.25, segments=18, rings=5))
    return carved(shapes, target, m['stone'], f'TitanHand{int(side)}', 0.06, 2500, rng, 0.6, 0.015, 0.02, 0.03, 0.03, smooth=3)


def titan(m, target, rng):
    objects = []
    body = []
    for side in (-1.0, 1.0):
        body.append(pk.capsule_bm(T(side * 3.3, 28.0, 9.8), T(side * KNEE[0], KNEE[1] + 0.3, KNEE[2] + 0.3), 2.3, 1.55, segments=20, rings=6))
        body.append(pk.ellipsoid_bm(T(side * KNEE[0], KNEE[1] + 0.2, KNEE[2] + 0.35), (1.5, 1.5, 1.45), segments=20, rings=12))
        body.append(pk.ellipsoid_bm(T(side * 6.7, 29.0, 19.0), (2.6, 2.7, 2.5), segments=20, rings=12))
        body.append(pk.capsule_bm(T(side * 7.0, 28.6, 18.4), T(side * ELBOW[0], ELBOW[1], ELBOW[2]), 1.9, 1.4, segments=18, rings=5))
    body.append(pk.ellipsoid_bm(T(0.0, 28.6, 10.0), (5.6, 3.7, 2.7), segments=24, rings=12))
    body.append(pk.ellipsoid_bm(T(0.0, 29.2, 15.6), (6.3, 4.0, 5.6), segments=24, rings=14))
    body.append(pk.ellipsoid_bm(T(0.0, 28.2, 11.8), (5.0, 3.4, 2.6), segments=24, rings=12))
    body.append(pk.capsule_bm(T(0.0, 29.3, 19.5), T(0.0, 28.4, 22.2), 2.0, 1.8, segments=16, rings=4))
    body.append(pk.ellipsoid_bm(T(0.0, 22.6, 8.4), (3.0, 2.2, 0.9), segments=16, rings=8))
    objects.append(pk.sculpt(body, target, m['stone'], 'TitanBody', 0.13, smooth=6, faces=8000, rng=rng))
    for side in (-1.0, 1.0):
        objects.append(resting_hand(side, m, target, random.Random(311 + int(side))))
        objects += titan_leg(side, m, target, random.Random(523 + int(side)))
        objects += plinth(side, m, target, random.Random(727 + int(side)))

    head = [
        pk.ellipsoid_bm(T(0.0, 27.6, 23.6), (2.2, 2.5, 2.9), segments=24, rings=14),
        pk.ellipsoid_bm(T(0.0, 25.4, 24.4), (1.7, 0.55, 0.45), segments=16, rings=8),
        pk.cone_bm(T(0.0, 25.3, 24.2), T(0.0, 24.6, 23.0), 0.42, sides=8),
        pk.ellipsoid_bm(T(0.0, 25.9, 21.6), (1.9, 1.3, 2.0), segments=18, rings=10),
        pk.cone_bm(T(0.0, 25.8, 21.0), T(0.0, 25.4, 18.4), 1.4, sides=10, flatten=0.7),
        pk.ellipsoid_bm(T(-0.75, 25.0, 22.7), (0.8, 0.35, 0.3), segments=12, rings=6),
        pk.ellipsoid_bm(T(0.75, 25.0, 22.7), (0.8, 0.35, 0.3), segments=12, rings=6),
    ]
    objects.append(pk.sculpt(head, target, m['stone'], 'TitanHead', 0.07, smooth=4, faces=4000, rng=rng))

    hood = [pk.ellipsoid_bm(T(0.0, 28.1, 24.4), (3.3, 3.6, 3.9), segments=28, rings=16)]
    for side in (-1.0, 1.0):
        hood.append(pk.capsule_bm(T(side * 2.6, 28.4, 23.0), T(side * 5.2, 29.0, 19.6), 1.3, 1.6, segments=14, rings=4))
    carve = [pk.ellipsoid_bm(T(0.0, 26.4, 23.9), (2.7, 3.6, 3.35), segments=24, rings=14)]
    objects.append(pk.sculpt(hood, target, m['stone'], 'TitanHood', 0.09, smooth=4, faces=3000, rng=rng, carve=carve))

    eyes = [pk.part(f'Eye{int(side)}', pk.ellipsoid_bm(T(side * 0.78, 25.15, 23.75), (0.36, 0.14, 0.17), segments=10, rings=6), target, m['eyes'], smooth=60.0) for side in (-1.0, 1.0)]
    objects.append(kit.join(eyes, 'TitanEyes'))

    bronze = []
    for side in (-1.0, 1.0):
        elbow, wrist = Vector(T(side * ELBOW[0], ELBOW[1], ELBOW[2])), Vector(T(side * 7.4, 20.9, 10.35))
        bronze.append(sleeve(elbow.lerp(wrist, 0.55), elbow.lerp(wrist, 0.78), 1.05, lip=0.1))
        shoulder = Vector(T(side * 7.0, 28.6, 18.4))
        bronze.append(sleeve(shoulder.lerp(elbow, 0.45), shoulder.lerp(elbow, 0.7), 1.82))
        for k in range(3):
            tilt = Vector((0.0, 0.0, 1.0)).rotation_difference(Vector((side * (0.4 + 0.12 * k), 0.0, 1.0)).normalized())
            bronze.append(pk.ellipsoid_bm(T(side * (7.1 + 0.35 * k), 28.8 - 0.15 * k, 20.3 - 0.85 * k), (2.9 - 0.15 * k, 3.0 - 0.15 * k, 1.05), rotation=tilt, segments=22, rings=8))
        bronze.append(pk.ellipsoid_bm(T(side * KNEE[0], KNEE[1] - 1.25, KNEE[2] - 0.45), (1.1, 0.45, 0.95), segments=16, rings=8))
    bronze.append(ring(T(0.0, 28.4, 12.0), (0.0, 0.0, 1.0), 5.2, 0.42, steps=40, sides=8))
    bronze.append(pk.ellipsoid_bm(T(0.0, 26.0, 15.8), (4.3, 1.0, 3.6), segments=22, rings=10))
    bronze.append(pk.box((1.6, 0.6, 1.3), T(0.0, 24.8, 12.0)))
    obj = kit.mesh_object('TitanArmour', pk.merge_bm(bronze), target, m['bronze'])
    pk.tag(obj, rng=rng)
    kit.smooth_by_angle(obj, 40.0)
    objects.append(obj)

    throne = [pk.part('ThroneBlock', pk.chipped((15.5, 7.5, 13.0), rng, chips=4, depth=0.4, center=tuple(T(0.0, 31.6, 2.0))), target, m['masonry'], bevel=(0.08, 2, 40.0))]
    for side in (-1.0, 1.0):
        throne.append(pk.part(f'ThroneArm{int(side)}', pk.chipped((2.4, 8.5, 4.0), rng, chips=3, depth=0.3, center=tuple(T(side * 9.4, 29.0, 9.5))), target, m['masonry'], bevel=(0.08, 2, 40.0)))
    objects.append(kit.join(throne, 'Throne'))
    return objects


def cradle(m, target, rng):
    px, py, pz = PIVOT
    parts = []
    for side in (-1.0, 1.0):
        bearing = ring(W(side * BEARING, py, pz), (1.0, 0.0, 0.0), 0.5, 0.2, steps=20, sides=8)
        parts.append(pk.part(f'CradleBearing{int(side)}', bearing, target, m['bronze'], smooth=40.0))
        parts.append(pk.part(f'CradleCheek{int(side)}', pk.box((0.34, 1.3, 1.3), W(side * BEARING, py, pz)), target, m['bronze'], bevel=(0.06, 2, 40.0)))
        parts.append(pk.part(f'CradleEye{int(side)}', ring(W(side * BEARING, py, pz + 0.85), (0.0, 1.0, 0.0), 0.3, 0.1, steps=16, sides=6), target, m['iron'], smooth=40.0))
        for k in range(3):
            parts.append(pk.part(f'CradleRivet{int(side)}{k}', pk.ellipsoid_bm(W(side * (BEARING + 0.19), py - 0.4 + 0.4 * k, pz + 0.45), (0.07, 0.07, 0.07), segments=6, rings=4), target, m['iron'], smooth=60.0))
    return kit.join(parts, 'Cradle')


def crucible(m, target, rng):
    parts = []
    profile = [(0.0, -0.86), (0.78, -0.86), (0.92, -0.74), (1.02, -0.2), (1.07, 0.5), (1.15, 0.7), (1.17, 0.8), (1.08, 0.84), (0.98, 0.76), (0.93, 0.1), (0.86, -0.45), (0.74, -0.64), (0.0, -0.66)]
    parts.append(pk.part('CrucibleBody', pk.lathe(profile, 32), target, m['iron'], smooth=35.0))
    for z, radius in ((-0.58, 0.985), (0.0, 1.04), (0.52, 1.085)):
        parts.append(pk.part(f'CrucibleHoop{int(z * 10)}', ring((0.0, 0.0, z), (0.0, 0.0, 1.0), radius + 0.02, 0.06, steps=40, sides=6), target, m['bronze'], smooth=40.0))
        for k in range(14):
            a = 2.0 * math.pi * (k + 0.5) / 14.0
            parts.append(pk.part(f'CrucibleRivet{int(z * 10)}_{k}', pk.ellipsoid_bm((math.cos(a) * (radius + 0.08), math.sin(a) * (radius + 0.08), z), (0.04, 0.04, 0.04), segments=6, rings=4), target, m['iron'], smooth=60.0))
    spout = [(-0.34, 0.85), (0.34, 0.85), (0.16, 1.42), (-0.16, 1.42)]
    lip = prism_bm(spout, 0.62, 0.8)
    parts.append(pk.part('CrucibleSpout', lip, target, m['iron'], bevel=(0.025, 2, 40.0)))
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'Trunnion{int(side)}', oriented(pk.lathe([(0.2, 0.0), (0.2, 0.4), (0.26, 0.4), (0.26, 0.52), (0.0, 0.52)], 16), (side * 0.98, 0.0, 0.0), (side * 2.0, 0.0, 0.0)), target, m['bronze'], smooth=40.0))
        parts.append(pk.part(f'TrunnionBoss{int(side)}', pk.box((0.14, 0.62, 0.62), (side * 1.05, 0.0, 0.0)), target, m['bronze'], bevel=(0.04, 2, 40.0)))
    body = kit.join(parts, 'Crucible')
    body.matrix_world = Matrix.Scale(CRUCIBLE_SCALE, 4) @ body.matrix_world
    return [body]


def tapered(bottom, top, z0, z1, x=0.0):
    bm = pk.box((bottom[0], bottom[1], z1 - z0), (x, 0.0, 0.5 * (z0 + z1)))
    for v in bm.verts:
        if v.co.z > 0.5 * (z0 + z1):
            v.co.x = x + (v.co.x - x) * top[0] / bottom[0]
            v.co.y *= top[1] / bottom[1]
    return bm


ANVIL_PROFILE = [(0.0, 1.45, 0.95, 0.2), (0.1, 1.43, 0.93, 0.2), (0.3, 1.12, 0.72, 0.2), (0.55, 0.8, 0.52, 0.2), (0.85, 0.72, 0.47, 0.2), (1.1, 0.95, 0.52, 0.22), (1.3, 1.48, 0.6, 0.25), (1.4, 1.56, 0.61, 0.25), (1.94, 1.56, 0.61, 0.25), (2.0, 1.52, 0.58, 0.25)]


def rounded_ring(cx, hx, hy, z, steps=24, power=4.0):
    pts = []
    for k in range(steps):
        t = 2.0 * math.pi * k / steps
        c, s = math.cos(t), math.sin(t)
        pts.append((cx + hx * math.copysign(abs(c) ** (2.0 / power), c), hy * math.copysign(abs(s) ** (2.0 / power), s), z))
    return pts


def profile_at(z):
    for (z0, x0, y0, _), (z1, x1, y1, _) in zip(ANVIL_PROFILE, ANVIL_PROFILE[1:]):
        if z0 <= z <= z1:
            t = (z - z0) / (z1 - z0)
            return x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
    return ANVIL_PROFILE[-1][1], ANVIL_PROFILE[-1][2]


def loft_rings(rings):
    bm = bmesh.new()
    verts = [[bm.verts.new(p) for p in ring_points] for ring_points in rings]
    count = len(verts[0])
    for a, b in zip(verts, verts[1:]):
        for k in range(count):
            j = (k + 1) % count
            bm.faces.new((a[k], a[j], b[j], b[k]))
    bm.faces.new(list(reversed(verts[0])))
    bm.faces.new(verts[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def great_anvil(name, x, y, top, dx, dy, m, target, rng):
    frame = frame_at(x, y, top, dx, dy)
    stone, iron, bronze, steel, wood, hot = [], [], [], [], [], []
    stone.append(pk.lathe([(2.3, SUNK - top), (2.1, -0.3), (2.0, 0.0), (0.0, 0.0)], 8, phase=math.pi / 8.0))
    bronze.append(pk.lathe([(2.14, -0.62), (2.2, -0.58), (2.2, -0.32), (2.14, -0.28)], 8, phase=math.pi / 8.0))
    for k in range(16):
        a = 2.0 * math.pi * (k + 0.5) / 16.0
        bronze.append(pk.ellipsoid_bm((math.cos(a) * 2.2, math.sin(a) * 2.2, -0.45), (0.06, 0.06, 0.06), segments=6, rings=4))
    body_rings = [rounded_ring(cx, hx, hy, z) for z, hx, hy, cx in ANVIL_PROFILE]
    iron.append(loft_rings(body_rings))
    for fx in (-0.95, 1.35):
        for fy in (-0.62, 0.62):
            iron.append(tapered((0.7, 0.55), (0.55, 0.45), 0.0, 0.24, fx))
            bmesh.ops.translate(iron[-1], verts=iron[-1].verts, vec=(0.0, fy, 0.0))
    horn_rings = []
    for i in range(13):
        t = i / 12.0
        r = 0.55 * (1.0 - t) ** 0.85 + 0.035
        zc = 2.0 - 0.9 * r - 0.14 * t * t
        horn_rings.append([(-1.2 - 2.5 * t, r * math.cos(2.0 * math.pi * k / 16), zc + 0.9 * r * math.sin(2.0 * math.pi * k / 16)) for k in range(16)])
    iron.append(loft_rings(horn_rings))
    heel_rings = []
    for i in range(5):
        t = i / 4.0
        z0 = 1.42 + 0.2 * t
        heel_rings.append([(1.7 + 0.9 * t, (0.5 - 0.08 * t) * math.copysign(abs(math.cos(a)) ** 0.5, math.cos(a)), 0.5 * (z0 + 2.0) + 0.5 * (2.0 - z0) * math.copysign(abs(math.sin(a)) ** 0.5, math.sin(a))) for a in (2.0 * math.pi * k / 16 for k in range(16))])
    iron.append(loft_rings(heel_rings))
    steel.append(pk.box((3.0, 1.12, 0.07), (0.25, 0.0, 2.02)))
    steel.append(pk.box((0.82, 0.86, 0.07), (2.15, 0.0, 2.02)))
    iron.append(pk.box((0.2, 0.2, 0.02), (2.2, 0.0, 2.06)))
    iron.append(pk.cylinder(0.08, 0.02, 12, z=2.05))
    bmesh.ops.translate(iron[-1], verts=iron[-1].verts, vec=(1.75, 0.0, 0.0))
    for z in (0.5, 1.05):
        hx, hy = profile_at(z)
        bronze.append(loft_rings([rounded_ring(0.21, hx + 0.05, hy + 0.05, z - 0.07), rounded_ring(0.21, hx + 0.05, hy + 0.05, z + 0.07)]))
        for px in (-0.45, 0.2, 0.85):
            for py in (-1.0, 1.0):
                bronze.append(pk.ellipsoid_bm((px, py * (hy + 0.07), z), (0.055, 0.055, 0.055), segments=8, rings=4))
    blank = blade_bm(1.9, 0.19, 0.035, tip=0.22, taper=0.3)
    bmesh.ops.transform(blank, matrix=Matrix.Translation((-0.75, 0.05, 2.09)) @ Matrix(((0.0, 0.0, 1.0, 0.0), (1.0, 0.0, 0.0, 0.0), (0.0, 1.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0))), verts=blank.verts)
    hot.append(blank)
    hot.append(pk.box((0.7, 0.07, 0.05), (-1.1, 0.05, 2.09)))
    bronze.append(ring((-1.25, 0.0, 1.47), (1.0, 0.0, 0.0), 0.6, 0.06, steps=28, sides=6))
    shield = [(-0.5, 0.22), (0.5, 0.22), (0.5, -0.08), (0.0, -0.4), (-0.5, -0.08)]
    for side in (-1.0, 1.0):
        face = Matrix.Translation((0.35, side * 0.615, 1.7)) @ Matrix.Diagonal((1.0, side, 1.0, 1.0)) @ Matrix.Rotation(-0.5 * math.pi, 4, "X")
        for piece in (prism_bm(shield, 0.0, 0.04), ring_prism(shield, -0.02, 0.06, 0.0, 0.08), prism_bm([(-0.04, -0.25), (0.04, -0.25), (0.04, 0.06), (-0.04, 0.06)], 0.04, 0.11), prism_bm([(-0.2, 0.04), (0.2, 0.04), (0.2, 0.16), (-0.2, 0.16)], 0.04, 0.12)):
            if side < 0.0:
                bmesh.ops.reverse_faces(piece, faces=piece.faces[:])
            bmesh.ops.transform(piece, matrix=face, verts=piece.verts)
            bmesh.ops.recalc_face_normals(piece, faces=piece.faces[:])
            bronze.append(piece)
    for k in range(8):
        a = math.pi / 8.0 + 2.0 * math.pi * k / 8.0
        strap_frame = Matrix.Translation((math.cos(a) * 2.08, math.sin(a) * 2.08, 0.0)) @ Matrix.Rotation(a, 4, "Z")
        piece = pk.box((0.1, 0.32, 2.1), (0.0, 0.0, -1.35))
        bmesh.ops.transform(piece, matrix=strap_frame, verts=piece.verts)
        bronze.append(piece)
        for z in (-0.6, -1.3, -2.0):
            stud = pk.ellipsoid_bm((0.06, 0.0, z), (0.06, 0.06, 0.06), segments=6, rings=4)
            bmesh.ops.transform(stud, matrix=strap_frame, verts=stud.verts)
            bronze.append(stud)
    lean = Matrix.Translation((0.6, 1.55, 0.32)) @ Matrix.Rotation(0.15, 4, 'Z')
    head = loft_rings([rounded_ring(0.0, r, r, z, steps=8, power=2.4) for z, r in ((-0.58, 0.22), (-0.5, 0.3), (0.5, 0.3), (0.58, 0.22))])
    bmesh.ops.transform(head, matrix=lean @ Matrix.Rotation(0.5 * math.pi, 4, "Y"), verts=head.verts)
    iron.append(head)
    for bx in (-0.36, 0.36):
        bronze.append(placed_bm(pk.box((0.12, 0.66, 0.66), (bx, 0.0, 0.0)), lean))
    wood.append(placed_bm(pk.rod((0.0, 0.25, 0.0), (0.3, 2.9, -1.6), 0.13, 10), lean))
    for side in (-1.0, 1.0):
        iron.append(path_loft([(-1.7, -1.55 - side * 0.16, 0.07), (0.2, -1.55, 0.07 + 0.05 * side), (0.62, -1.55 + side * 0.12, 0.08 + 0.05 * side), (0.95, -1.55 + side * 0.04, 0.1 + 0.05 * side)], [0.05, 0.06, 0.05, 0.045], sides=6))
    iron.append(pk.cylinder(0.09, 0.22, 10, z=0.0))
    bmesh.ops.translate(iron[-1], verts=iron[-1].verts, vec=(0.2, -1.55, 0.0))
    iron.append(pk.cylinder(0.12, 0.3, 10, z=-0.02))
    objects = []
    for suffix, shapes, material in (('Stump', stone, 'kiln'), ('', iron, 'iron'), ('Bronze', bronze, 'castbronze'), ('Face', steel, 'steel'), ('Haft', wood, 'wood'), ('Hot', hot, 'molten')):
        if not shapes:
            continue
        obj = pk.part(f'{name}{suffix}', pk.merge_bm(shapes), target, m[material], matrix=frame)
        if material in ('iron', 'kiln', 'wood'):
            kit.bevel(obj, 0.03, segments=2, angle=40.0)
        else:
            kit.smooth_by_angle(obj, 40.0)
        objects.append(obj)
    return objects


def catwalk(name, start, end, width, m, target, rng):
    a, b = Vector(W(*start)), Vector(W(*end))
    axis = b - a
    length = axis.length
    flat = Vector((axis.x, axis.y, 0.0))
    frame = Matrix.Translation(a) @ Matrix.Rotation(math.atan2(flat.y, flat.x), 4, 'Z') @ Matrix.Rotation(-math.atan2(axis.z, flat.length), 4, 'Y')
    iron, bronze = [], []
    planks = max(2, int(length / 0.95))
    for k in range(planks):
        x = (k + 0.5) * length / planks
        iron.append(pk.box((length / planks - 0.06, width, 0.1), (x, 0.0, 0.0)))
    for side in (-1.0, 1.0):
        iron.append(pk.box((length, 0.22, 0.32), (0.5 * length, side * (0.5 * width - 0.11), -0.2)))
        posts = max(2, int(length / 1.3) + 1)
        for k in range(posts):
            x = k * length / (posts - 1)
            iron.append(pk.box((0.1, 0.1, 1.05), (x, side * (0.5 * width - 0.05), 0.55)))
        bronze.append(pk.box((length + 0.1, 0.12, 0.1), (0.5 * length, side * (0.5 * width - 0.05), 1.08)))
    objects = []
    for suffix, shapes, material in (('', iron, 'iron'), ('Rail', bronze, 'castbronze')):
        obj = pk.part(f'{name}{suffix}', pk.merge_bm(shapes), target, m[material], matrix=frame)
        kit.bevel(obj, 0.015, segments=1, angle=40.0)
        objects.append(obj)
    legs = []
    for k in range(max(1, int(length / 3.0))):
        t = (k + 0.5) / max(1, int(length / 3.0))
        p = a.lerp(b, t)
        legs.append(pk.box((0.35, 0.35, p.z - SUNK), (p.x, p.y, 0.5 * (p.z + SUNK) - 0.2)))
    if legs:
        leg = kit.mesh_object(f'{name}Legs', pk.merge_bm(legs), target, m['iron'])
        pk.tag(leg, rng=rng)
        kit.bevel(leg, 0.02, segments=1, angle=40.0)
        objects.append(leg)
    return objects


def flanged_pipe(name, points, radius, m, target, rng):
    pipe = pk.tube([W(*p) for p in points], radius, target, m['castbronze'], name, resolution=6, sides=12, rng=rng)
    rings = []
    for (ax, ay, az), (bx, by, bz) in zip(points, points[1:]):
        a, b = Vector(W(ax, ay, az)), Vector(W(bx, by, bz))
        for t in (0.2, 0.8):
            c = a.lerp(b, t)
            rings.append(ring(c, b - a, radius + 0.05, 0.07, steps=20, sides=6))
    flange = kit.mesh_object(f'{name}Flanges', pk.merge_bm(rings), target, m['iron'])
    pk.tag(flange, rng=rng)
    kit.smooth_by_angle(flange, 40.0)
    return [pipe, flange]


def vat(index, x, y, scale, m, target, rng):
    ground = placed(x, y, 0.0, facing_center(x, y))
    frame = ground @ Matrix.Scale(scale, 4)
    stone = []
    for k, (size, z0, z1) in enumerate((((8.4, 8.4), SUNK / scale, 0.35), ((7.0, 7.0), 0.3, 1.05))):
        stone.append(pk.part(f'VatPlinth{index}_{k}', pk.chipped((size[0], size[1], z1 - z0), rng, chips=4, depth=0.25, center=(0.0, 0.0, 0.5 * (z0 + z1))), target, m['kiln'], matrix=frame, bevel=(0.06, 2, 40.0)))
    for k in range(14):
        a = rng.uniform(0.0, 2.0 * math.pi)
        r = rng.uniform(4.3, 5.2) * scale
        s = rng.uniform(0.5, 1.1)
        stone.append(pk.part(f'VatRubble{index}_{k}', pk.chipped((s, s * 0.9, s * 0.7), rng, chips=2, depth=0.08, center=(math.cos(a) * r, math.sin(a) * r, LAVA + 0.2 + s * 0.2)), target, m['basalt'], matrix=ground @ Matrix.Rotation(rng.uniform(-0.2, 0.2), 4, 'Z'), bevel=(0.04, 1, 40.0)))
    profile = [(0.0, 1.0), (2.55, 1.0), (3.05, 1.5), (3.4, 2.8), (3.48, 4.1), (3.35, 5.6), (3.08, 6.8), (3.15, 6.95), (3.15, 7.25), (2.8, 7.25), (2.7, 6.9), (0.0, 6.9)]

    def radius_at(z):
        for (r0, z0), (r1, z1) in zip(profile[1:7], profile[2:8]):
            if z0 <= z <= z1:
                return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
        return profile[1][0]

    body = [pk.part(f'VatBody{index}', pk.lathe(profile, 40), target, m['vatstone'], matrix=frame, smooth=30.0)]
    metal = []
    for z0, z1 in ((1.45, 1.8), (3.9, 4.3), (6.45, 6.8)):
        r = radius_at(0.5 * (z0 + z1))
        metal.append(pk.part(f'VatHoop{index}{int(z0 * 10)}', sleeve((0.0, 0.0, z0), (0.0, 0.0, z1), r - 0.02, steps=48, lip=0.1), target, m['bronze'], matrix=frame, smooth=40.0))
        for k in range(20):
            a = 2.0 * math.pi * (k + 0.5) / 20.0
            metal.append(pk.part(f'VatRivet{index}{int(z0 * 10)}_{k}', pk.ellipsoid_bm((math.cos(a) * (r + 0.09), math.sin(a) * (r + 0.09), 0.5 * (z0 + z1)), (0.07, 0.07, 0.07), segments=6, rings=4), target, m['iron'], matrix=frame, smooth=60.0))
    straps = []
    for k in range(10):
        a = 2.0 * math.pi * (k + 0.25) / 10.0
        d = Vector((math.cos(a), math.sin(a), 0.0))
        samples = [(d * radius_at(z) + Vector((0.0, 0.0, z)), d) for z in [1.1 + 0.29 * i for i in range(21)]]
        bm, _ = ribbon(samples, Vector((-d.y, d.x, 0.0)), 0.3, 0.06)
        straps.append(bm)
    metal.append(pk.part(f'VatStraps{index}', pk.merge_bm(straps), target, m['bronze'], matrix=frame, smooth=40.0))
    spout = [prism_bm([(-0.75, 2.9), (0.75, 2.9), (0.34, 4.5), (-0.34, 4.5)], 6.85, 7.05)]
    for s in (-1.0, 1.0):
        spout.append(prism_bm([(s * 0.62, 2.9), (s * 0.8, 2.9), (s * 0.42, 4.5), (s * 0.26, 4.5)], 6.85, 7.45))
    tilt = Matrix.Translation((0.0, 2.9, 7.0)) @ Matrix.Rotation(-0.14, 4, 'X') @ Matrix.Translation((0.0, -2.9, -7.0))
    metal.append(pk.part(f'VatSpout{index}', pk.merge_bm(spout), target, m['bronze'], matrix=frame @ tilt, bevel=(0.03, 2, 40.0)))
    metal.append(pk.part(f'VatSpoutMelt{index}', prism_bm([(-0.55, 2.7), (0.55, 2.7), (0.24, 4.4), (-0.24, 4.4)], 7.05, 7.09), target, m['molten'], matrix=frame @ tilt))
    for s in (-1.0, 1.0):
        hub = Vector((s * 3.4, 0.0, 4.1))
        metal.append(pk.part(f'VatAxle{index}{int(s)}', pk.rod(hub, hub + Vector((s * 0.9, 0.0, 0.0)), 0.5, 16), target, m['bronze'], matrix=frame, smooth=40.0))
        metal.append(pk.part(f'VatBoss{index}{int(s)}', pk.rod(hub + Vector((s * 0.9, 0.0, 0.0)), hub + Vector((s * 1.25, 0.0, 0.0)), 0.75, 16), target, m['bronze'], matrix=frame, bevel=(0.03, 2, 40.0)))
        metal.append(pk.part(f'VatCap{index}{int(s)}', pk.rod(hub + Vector((s * 1.25, 0.0, 0.0)), hub + Vector((s * 1.45, 0.0, 0.0)), 0.32, 12), target, m['iron'], matrix=frame, smooth=40.0))
        x = s * 4.35
        for leg in (-1.0, 1.0):
            metal.append(pk.part(f'VatLeg{index}{int(s)}{int(leg)}', pk.rod((x, leg * 2.1, 1.05), (x, leg * 0.35, 3.75), 0.24, 4), target, m['bronze'], matrix=frame, bevel=(0.03, 1, 40.0)))
        metal.append(pk.part(f'VatFoot{index}{int(s)}', pk.box((0.6, 4.8, 0.45), (x, 0.0, 1.28)), target, m['bronze'], matrix=frame, bevel=(0.03, 2, 40.0)))
        metal.append(pk.part(f'VatBrace{index}{int(s)}', pk.box((0.4, 2.6, 0.32), (x, 0.0, 2.35)), target, m['bronze'], matrix=frame, bevel=(0.03, 2, 40.0)))
    links = []
    for s in (-1.0, 1.0):
        for leg in (-1.0, 1.0):
            links += chain_links([frame @ Vector((s * 4.9, leg * 0.45, 3.55)), frame @ Vector((s * 5.0, leg * 1.1, 2.2)), frame @ Vector((s * 4.7, leg * 2.6, 1.15))], 0.85 * scale, rng)
    chain = kit.mesh_object(f'VatChains{index}', pk.merge_bm(links), target, m['chain'])
    pk.tag(chain)
    kit.smooth_by_angle(chain, 50.0)
    melt = pk.part(f'VatMelt{index}', pk.cylinder(2.76, 0.12, 40, z=6.84), target, m['vatcrust'], matrix=frame, smooth=30.0)
    return [kit.join(stone, f'VatPlinth{index}'), kit.join(body, f'VatBody{index}'), kit.join(metal, f'Vat{index}'), chain, kit.join([melt], f'VatMelt{index}')]


def pillars(m, target, rng):
    objects = []
    spots = [(-7.0, -18.0), (7.0, -18.0), (0.0, -19.0)]
    for index, (x, y) in enumerate(spots):
        parts = []
        for k in range(rng.randint(3, 5)):
            radius = rng.uniform(0.7, 1.3)
            top = rng.uniform(-1.6, 0.6) if y < 0.0 else rng.uniform(-0.5, 2.5)
            column = pk.cylinder(radius, top + 5.0, 6, z=-5.0)
            angle = rng.uniform(0.0, 2.0 * math.pi)
            frame = placed(x + math.cos(angle) * rng.uniform(0.0, 1.8), y + math.sin(angle) * rng.uniform(0.0, 1.8), 0.0, rng.uniform(0.0, math.pi))
            parts.append(pk.part(f'Pillar{index}_{k}', column, target, m['basalt'], matrix=frame, bevel=(0.05, 2, 40.0)))
        objects.append(kit.join(parts, f'Pillars{index}'))
    return objects


def channel(index, angle_degrees, m, target, rng, inner=18.6, outer=26.0):
    angle = math.radians(angle_degrees)
    length = outer - inner
    middle = 0.5 * (inner + outer)
    x, y = math.cos(angle) * middle, math.sin(angle) * middle
    frame = placed(x, y, 0.0, facing_center(x, y))
    stone = []
    stone.append(pk.part(f'ChannelBed{index}', pk.chipped((1.9, length, 0.5), rng, chips=2, depth=0.08, center=(0.0, 0.0, 0.15)), target, m['masonry'], matrix=frame, bevel=(0.04, 2, 40.0)))
    for side in (-1.0, 1.0):
        stone.append(pk.part(f'ChannelWall{index}{int(side)}', pk.box((0.35, length, 0.75), (side * 0.78, 0.0, 0.55)), target, m['masonry'], matrix=frame, bevel=(0.04, 2, 40.0)))
    for k in range(3):
        v = -0.5 * length + (k + 0.5) * length / 3.0
        stone.append(pk.part(f'ChannelPier{index}{k}', pk.chipped((1.6, 1.4, 4.2), rng, chips=3, depth=0.15, center=(0.0, v, -2.0)), target, m['basalt'], matrix=frame, bevel=(0.05, 2, 40.0)))
    metal = []
    for k in range(int(length / 1.6)):
        v = -0.5 * length + (k + 0.5) * length / int(length / 1.6)
        metal.append(pk.part(f'ChannelStrap{index}{k}', pk.box((2.0, 0.16, 0.1), (0.0, v, 0.95)), target, m['iron'], matrix=frame, bevel=(0.02, 1, 30.0)))
    melt = [pk.part(f'ChannelMeltTop{index}', pk.box((1.2, length + 0.1, 0.06), (0.0, 0.0, 0.48)), target, m['molten'], matrix=frame)]
    return [kit.join(stone, f'Channel{index}'), kit.join(metal, f'ChannelStraps{index}'), kit.join(melt, f'ChannelMelt{index}')]


def bellows(index, frame, m, target, rng):
    parts = []
    leather = []
    for k, (z, width) in enumerate(((1.0, 2.6), (2.0, 2.9), (3.0, 2.6))):
        leather.append(pk.part(f'BellowsFold{index}{k}', pk.chipped((width, 3.6 - 0.4 * k, 0.9), rng, chips=2, depth=0.06, center=(0.0, -0.3 * k, z)), target, m['leather'], matrix=frame, bevel=(0.12, 3, 40.0)))
    parts.append(pk.part(f"BellowsBoard{index}", pk.box((3.0, 4.2, 0.22), (0.0, 0.0, 0.45)), target, m["wood"], matrix=frame, bevel=(0.04, 2, 40.0)))
    leather.append(pk.part(f"BellowsLid{index}", pk.box((3.0, 4.2, 0.22), (0.0, 0.0, 3.6)), target, m["wood"], matrix=frame, bevel=(0.04, 2, 40.0)))
    parts.append(pk.part(f'BellowsNozzle{index}', pk.rod((0.0, 2.1, 2.0), (0.0, 3.4, 1.6), 0.32, 10, end_radius=0.18), target, m['bronze'], matrix=frame, smooth=40.0))
    parts.append(pk.part(f'BellowsLever{index}', pk.rod((0.0, -2.0, 3.7), (0.0, -3.6, 6.4), 0.18, 8), target, m['iron'], matrix=frame, smooth=40.0))
    parts.append(pk.part(f'BellowsPlinth{index}', pk.chipped((3.6, 5.0, 3.6), rng, chips=3, depth=0.2, center=(0.0, 0.0, -1.5)), target, m['masonry'], matrix=frame, bevel=(0.05, 2, 40.0)))
    return [kit.join(parts, f'Bellows{index}'), kit.join(leather, f"BellowsPump{index}")]


LAVA = -3.0
SUNK = -4.6


def xz_prism(points, y0, y1):
    bm = bmesh.new()
    front = [bm.verts.new((x, y1, z)) for x, z in points]
    back = [bm.verts.new((x, y0, z)) for x, z in points]
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    for i in range(len(points)):
        j = (i + 1) % len(points)
        bm.faces.new((back[i], back[j], front[j], front[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def arch_points(half, straight, steps=10, floor=0.0):
    points = [(-half, floor), (-half, straight)]
    points += [(-math.cos(math.pi * k / steps) * half, straight + math.sin(math.pi * k / steps) * half) for k in range(1, steps)]
    points += [(half, straight), (half, floor)]
    return points


def tunnel_bm(points, depth):
    bm = bmesh.new()
    front = [bm.verts.new((x, 0.0, z)) for x, z in points]
    back = [bm.verts.new((x, -depth, z)) for x, z in points]
    bm.faces.new(list(reversed(back)))
    for i in range(len(points)):
        j = (i + 1) % len(points)
        bm.faces.new((back[i], back[j], front[j], front[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    return bm


def column_bm(radius, bottom, top, rng, sides=6):
    pieces = []
    z = bottom
    drums = max(1, int((top - bottom) / rng.uniform(1.6, 2.6)))
    for k in range(drums):
        z1 = top if k == drums - 1 else z + (top - bottom) / drums * rng.uniform(0.8, 1.2)
        r = radius * rng.uniform(0.94, 1.04)
        sides_k = sides if rng.random() < 0.7 else sides - 1
        drum = pk.cylinder(r, max(z1 - z - 0.04, 0.1), sides_k, z=z, top=r * rng.uniform(0.95, 1.0))
        bmesh.ops.rotate(drum, verts=drum.verts, cent=(0.0, 0.0, 0.0), matrix=Matrix.Rotation(rng.uniform(-0.12, 0.12), 3, 'Z'))
        bmesh.ops.translate(drum, verts=drum.verts, vec=(rng.uniform(-0.04, 0.04) * radius, rng.uniform(-0.04, 0.04) * radius, 0.0))
        if rng.random() < 0.35:
            corner = rng.uniform(0.0, 2.0 * math.pi)
            normal = Vector((math.cos(corner), math.sin(corner), rng.uniform(0.3, 1.2))).normalized()
            kit.cut(drum, Vector((math.cos(corner) * r * 0.8, math.sin(corner) * r * 0.8, z1 - 0.05)), normal)
        pieces.append(drum)
        z = z1
    bm = pk.merge_bm(pieces)
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0.0, 0.0, 0.0), matrix=Matrix.Rotation(rng.uniform(0.0, math.pi), 3, 'Z'))
    if rng.random() < 0.5:
        normal = Vector((rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), 1.0)).normalized()
        kit.cut(bm, Vector((0.0, 0.0, top - rng.uniform(0.1, 0.4))), normal)
    return bm


def obsidian_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(p)
    warped = s.offset(p, s.mul(s.noise_color(p, 1.8, detail=2.0), 0.25))
    vein = s.sub(1.0, s.smooth(0.0, 0.025, s.voronoi(warped, 1.6, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
    vein = s.mul(vein, s.smooth(0.46, 0.62, s.noise(p, 1.3, detail=2.0)))
    low = s.sub(1.0, s.smooth(-0.8, 0.9, z))
    glow = s.mul(vein, low)
    sheen = s.noise(p, 5.0, detail=3.0)
    base = s.mix(sheen, (0.01, 0.009, 0.012), (0.045, 0.04, 0.05))
    base = s.mix(s.mul(vein, 0.6), base, (0.14, 0.05, 0.03))
    rough = s.remap(sheen, 0.3, 0.7, 0.04, 0.2)
    rough = s.lerp(vein, rough, 0.7)
    return s.finish(base, rough, height=s.mul(vein, -1.0), bump=0.2, bump_distance=0.008, emission=glow, emission_color=(1.0, 0.33, 0.07), emission_strength=3.0, bake_size=bake_size)


def jitter(bm, rng, amount, keep_below=None):
    for v in bm.verts:
        if keep_below is not None and v.co.z < keep_below:
            continue
        v.co += Vector((rng.uniform(-amount, amount), rng.uniform(-amount, amount), rng.uniform(-amount, amount) * 0.5))


def scatter_rock(index, m, target, rng):
    shapes = []
    for k in range(rng.randint(3, 6)):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        dist = rng.uniform(0.0, 1.4)
        size = (rng.uniform(0.9, 1.8), rng.uniform(0.8, 1.6), rng.uniform(0.6, 1.5))
        shapes.append(pk.ellipsoid_bm((math.cos(angle) * dist, math.sin(angle) * dist, rng.uniform(-0.6, 0.3)), size, rotation=Vector((0.0, 0.0, 1.0)).rotation_difference(Vector((rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), 1.0)).normalized()), segments=14, rings=8))
    obj = pk.sculpt(shapes, target, m['basalt'], f'ScatterRock{index}', 0.12, smooth=2, faces=700, rng=rng)
    from mathutils import noise
    seed = Vector((rng.uniform(0, 50), rng.uniform(0, 50), rng.uniform(0, 50)))
    for v in obj.data.vertices:
        n = noise.noise(v.co * 1.4 + seed) * 0.28 + noise.noise(v.co * 3.7 + seed) * 0.08
        v.co += v.normal * n
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for k in range(3):
        normal = Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(0.2, 1.0))).normalized()
        kit.cut(bm, normal * rng.uniform(0.7, 1.2), normal)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.shade_flat()
    return obj


def basalt_island(index, m, target, rng):
    cx, cy = F['ForgeIslandCenters'][index]
    radius = F['ForgeIslandRadii'][index]
    top = F['ForgeIslandTops'][index]
    spacing = 1.08
    columns = []
    rows = int(radius / (spacing * 0.866)) + 1
    for j in range(-rows, rows + 1):
        for i in range(-rows - 1, rows + 2):
            x = (i + 0.5 * (j % 2)) * spacing + rng.uniform(-0.12, 0.12)
            y = j * spacing * 0.866 + rng.uniform(-0.12, 0.12)
            edge = math.hypot(x, y) / radius
            if edge > 1.0 + rng.uniform(-0.15, 0.05):
                continue
            height = top - edge * edge * rng.uniform(0.8, 2.0) + rng.uniform(-0.18, 0.12)
            if edge > 0.8 and rng.random() < 0.3:
                height = LAVA + rng.uniform(0.15, 0.7)
            bm = column_bm(rng.uniform(0.55, 0.64), SUNK, height, rng)
            bmesh.ops.translate(bm, verts=bm.verts, vec=W(cx + x, cy + y))
            columns.append(bm)
    obj = kit.mesh_object(f'Island{index}', pk.merge_bm(columns), target, m['basalt'])
    pk.tag(obj, rng=rng)
    kit.bevel(obj, 0.035, segments=1, angle=40.0)
    return obj


def tapered_box(w, d, h, taper, center_y):
    bm = pk.box((w, d, h), (0.0, center_y, 0.5 * h))
    for v in bm.verts:
        if v.co.z > 0.5 * h:
            v.co.x *= 1.0 - taper
            v.co.y = center_y + (v.co.y - center_y) * (1.0 - 0.6 * taper)
    return bm


def plaque(name, m, target, frame):
    points = [(-0.75, 0.0), (0.75, 0.0), (0.75, -2.6), (0.0, -3.4), (-0.75, -2.6)]
    plate = upright(prism_bm(points, -0.06, 0.06))
    rim = upright(ring_prism(points, -0.02, 0.12, -0.1, 0.1))
    gem = upright(prism_bm([(0.0, -0.9), (0.32, -1.5), (0.0, -2.1), (-0.32, -1.5)], 0.06, 0.16))
    parts = [pk.part(f'{name}Plate', placed_bm(plate, frame), target, m['iron'], smooth=40.0)]
    parts.append(pk.part(f'{name}Rim', pk.merge_bm([placed_bm(rim, frame), placed_bm(gem, frame)]), target, m['bronze'], smooth=40.0))
    return parts


def rivet_row(start, end, count, size=0.07):
    a, b = Vector(start), Vector(end)
    return [pk.ellipsoid_bm(tuple(a.lerp(b, (k + 0.5) / count)), (size, size, size), segments=6, rings=4) for k in range(count)]


def furnace(name, x, y, base, size, opening, chimney, m, target, rng, banners=True, stairs=True, ruined=False, stacks=0):
    w, d, h = size
    half, straight = opening
    frame = placed(x, y, base, facing_center(x, y))
    taper = 0.14
    stone, metal, glow, studs = [], [], [], []
    body = pk.part(f'KilnBody{name}', tapered_box(w, d, h, taper, -0.5 * d), target, m['kiln'], bevel=(0.06, 2, 40.0))
    cutter = kit.mesh_object(f'KilnCut{name}', xz_prism(arch_points(half, straight, floor=-0.5), -d * 0.6, 1.0), target)
    fg.boolean(body, cutter, 'DIFFERENCE')
    fg.remove(cutter)
    if ruined:
        bm = bmesh.new()
        bm.from_mesh(body.data)
        kit.cut(bm, Vector((0.25 * w, -0.3 * d, 0.55 * h)), Vector((0.55, 0.25, 0.8)).normalized())
        bm.to_mesh(body.data)
        bm.free()
        for k in range(9):
            s = rng.uniform(0.6, 1.3)
            stone.append(pk.part(f"KilnFallen{name}{k}", pk.chipped((s * 1.4, s, s * 0.8), rng, chips=2, depth=0.1, center=(rng.uniform(0.0, 0.6 * w), rng.uniform(-0.2 * d, 2.5), rng.uniform(-0.3, 0.6 * s))), target, m["kiln"], matrix=frame @ Matrix.Rotation(rng.uniform(-0.6, 0.6), 4, "Z") @ Matrix.Rotation(rng.uniform(-0.3, 0.3), 4, "X"), bevel=(0.04, 1, 40.0)))
    pk.tag(body, rng=rng)
    body.matrix_world = frame
    stone.append(body)
    stone.append(pk.part(f'KilnFoot{name}', pk.chipped((w + 2.4, d + 2.4, base - SUNK), rng, chips=4, depth=0.4, center=(0.0, -0.5 * d, 0.5 * (SUNK - base) - 0.02)), target, m['basalt'], matrix=frame, bevel=(0.06, 2, 40.0)))
    stone.append(pk.part(f'KilnPlinth{name}', pk.chipped((w + 1.0, d + 1.0, 0.7), rng, chips=3, depth=0.1, center=(0.0, -0.5 * d, -0.35)), target, m['kiln'], matrix=frame, bevel=(0.05, 2, 40.0)))
    stone.append(pk.part(f'KilnLedge{name}', pk.chipped((2.0 * half + 4.0, 3.2, 0.7), rng, chips=2, depth=0.08, center=(0.0, 1.5, -0.35)), target, m['kiln'], matrix=frame, bevel=(0.05, 2, 40.0)))
    top_w, top_d = w * (1.0 - taper), d * (1.0 - 0.6 * taper)
    if not ruined:
        stone.append(pk.part(f'KilnCrown{name}', pk.chipped((top_w + 0.8, top_d + 0.8, 0.9), rng, chips=3, depth=0.1, center=(0.0, -0.5 * d, h + 0.2)), target, m['kiln'], matrix=frame, bevel=(0.06, 2, 40.0)))
    steps = 4 if stairs else 0
    for k in range(steps):
        top = -0.45 * (k + 1)
        stone.append(pk.part(f'KilnStep{name}{k}', pk.chipped((2.0 * half + 2.4 - 0.2 * k, 0.6, top - SUNK), rng, chips=2, depth=0.05, center=(0.0, 3.4 + 0.6 * k, 0.5 * (top + SUNK))), target, m['kiln'], matrix=frame, bevel=(0.04, 2, 40.0)))
    for k in range(10):
        a = -0.5 * math.pi + math.pi * (k + 0.5) / 10.0
        r0, r1 = half + 0.68, half + 1.3
        cx, cz = 0.0, straight
        quad = [(cx + math.sin(a - 0.13) * r0, cz + math.cos(a - 0.13) * r0), (cx + math.sin(a - 0.13) * r1, cz + math.cos(a - 0.13) * r1), (cx + math.sin(a + 0.13) * r1, cz + math.cos(a + 0.13) * r1), (cx + math.sin(a + 0.13) * r0, cz + math.cos(a + 0.13) * r0)]
        stone.append(pk.part(f'KilnVoussoir{name}{k}', xz_prism(quad, -0.15, 0.32 + rng.uniform(0.0, 0.08)), target, m['kiln'], matrix=frame, bevel=(0.03, 1, 40.0)))
    for side in (-1.0, 1.0):
        for k in range(2):
            z0, z1 = k * straight * 0.5, (k + 1) * straight * 0.5 - 0.04
            stone.append(pk.part(f'KilnJamb{name}{int(side)}{k}', pk.chipped((0.62, 0.5, z1 - z0), rng, chips=1, depth=0.04, center=(side * (half + 1.0), 0.1, 0.5 * (z0 + z1))), target, m['kiln'], matrix=frame, bevel=(0.03, 1, 40.0)))
    inner = arch_points(half - 0.06, straight)
    liner = pk.part(f'Furnace{name}', tunnel_bm(inner, d * 0.55), target, m['furnace'])
    liner.matrix_world = frame @ Matrix.Translation((0.0, -0.02, 0.0))
    glow.append(liner)
    outer = arch_points(half + 0.65, straight, floor=-0.05)
    ring_points = list(reversed(arch_points(half - 0.02, straight, floor=-0.05)))
    metal.append(pk.part(f'KilnArch{name}', xz_prism(outer + ring_points, -0.1, 0.42), target, m['bronze'], matrix=frame, bevel=(0.03, 2, 40.0)))
    front = lambda z: 0.6 * taper * d * 0.5 * z / h
    columns = [side * (0.5 * w - 0.25) for side in (-1.0, 1.0)] + [side * (half + 1.75) for side in (-1.0, 1.0)]
    for k, px in enumerate(columns):
        top = h - 0.4 if abs(px) > half + 2.0 else straight + half + 1.9
        shrink = 1.0 - taper * top / h if abs(px) > half + 2.0 else 1.0
        post = pk.box((0.55, 0.22, top + 0.4), (px, 0.16, 0.5 * (top + 0.4) - 0.3))
        for v in post.verts:
            if abs(px) > half + 2.0:
                v.co.x *= 1.0 - taper * max(v.co.z, 0.0) / h
            v.co.y -= front(max(v.co.z, 0.0))
        metal.append(pk.part(f'KilnPost{name}{k}', post, target, m['bronze'], matrix=frame, bevel=(0.03, 2, 40.0)))
        start = 1.0 - taper * 0.6 / h if abs(px) > half + 2.0 else 1.0
        studs += [placed_bm(s, frame) for s in rivet_row((px * start, 0.3, 0.6), (px * shrink, 0.3 - front(top), top - 0.3), max(3, int(top / 1.1)))]
    for z in (0.32 * h, 0.7 * h):
        if z < straight + half + 1.4:
            continue
        width = w * (1.0 - taper * z / h)
        metal.append(pk.part(f'KilnBand{name}{int(z * 10)}', pk.box((width + 0.1, 0.24, 0.42), (0.0, 0.14 - front(z), z)), target, m['bronze'], matrix=frame, bevel=(0.03, 2, 40.0)))
        studs += [placed_bm(s, frame) for s in rivet_row((-0.5 * width, 0.3 - front(z), z), (0.5 * width, 0.3 - front(z), z), int(width / 0.9))]
    for side in (-1.0, 1.0):
        span = 0.5 * w - half - 2.05
        if span > 0.4:
            for z in (0.32 * h,):
                if z < straight + half + 1.4:
                    metal.append(pk.part(f'KilnBandSide{name}{int(side)}', pk.box((span, 0.24, 0.42), (side * (half + 2.05 + 0.5 * span), 0.14 - front(z), z)), target, m['bronze'], matrix=frame, bevel=(0.03, 2, 40.0)))
    bars = 5
    for k in range(bars):
        bx = -half * 0.75 + k * half * 1.5 / (bars - 1)
        metal.append(pk.part(f'KilnGrate{name}{k}', pk.box((0.16, 0.16, straight + 0.4), (bx, 0.45, 0.5 * (straight + 0.4))), target, m['iron'], matrix=frame, bevel=(0.02, 1, 30.0)))
        metal.append(pk.part(f'KilnSpike{name}{k}', pk.cone_bm((bx, 0.45, straight + 0.38), (bx, 0.45, straight + 0.8), 0.14, sides=4), target, m['iron'], matrix=frame))
    metal.append(pk.part(f'KilnGrateBar{name}', pk.box((half * 1.7, 0.14, 0.18), (0.0, 0.45, straight * 0.45)), target, m['iron'], matrix=frame, bevel=(0.02, 1, 30.0)))
    for k in range(stacks):
        sx, sy = (k - 0.5 * (stacks - 1)) * 0.42 * w + rng.uniform(-0.4, 0.4), -d - 1.6 - 0.8 * (k % 2)
        tall = h + 8.0 + 4.0 * k + rng.uniform(0.0, 2.0)
        stack = frame @ Matrix.Translation((sx, sy, 0.0))
        stone.append(pk.part(f"KilnTower{name}{k}", pk.lathe([(2.0, SUNK - base), (1.9, 0.0), (1.35, tall)], 4, phase=0.25 * math.pi), target, m["kiln"], matrix=stack, bevel=(0.04, 1, 40.0)))
        stone.append(pk.part(f"KilnTowerCap{name}{k}", pk.lathe([(1.6, tall - 0.1), (1.7, tall + 0.7), (1.15, tall + 0.7), (1.05, tall - 0.1)], 4, phase=0.25 * math.pi), target, m["kiln"], matrix=stack, bevel=(0.03, 1, 40.0)))
        for b in range(4):
            z = tall * (0.2 + 0.2 * b)
            r = 1.9 - 0.55 * z / tall + 0.1
            metal.append(pk.part(f"KilnTowerBand{name}{k}{b}", pk.lathe([(r, z), (r, z + 0.45)], 4, phase=0.25 * math.pi), target, m["bronze"], matrix=stack, bevel=(0.02, 1, 40.0)))
    if chimney > 0.0 and not ruined:
        cx, cy = 0.25 * w, -0.65 * d
        stack = frame @ Matrix.Translation((cx, cy, 0.0))
        z0 = h + 0.6
        stone.append(pk.part(f'KilnStack{name}', pk.lathe([(2.3, z0), (1.6, z0 + chimney)], 4, phase=0.25 * math.pi), target, m['kiln'], matrix=stack, bevel=(0.04, 1, 40.0)))
        stone.append(pk.part(f'KilnStackCap{name}', pk.lathe([(1.95, z0 + chimney - 0.1), (2.05, z0 + chimney + 0.6), (1.5, z0 + chimney + 0.6), (1.4, z0 + chimney - 0.1)], 4, phase=0.25 * math.pi), target, m['kiln'], matrix=stack, bevel=(0.03, 1, 40.0)))
        for k in range(3):
            z = z0 + chimney * (0.2 + 0.28 * k)
            r = 2.3 - 0.7 * (z - z0) / chimney + 0.12
            metal.append(pk.part(f'KilnStackBand{name}{k}', pk.lathe([(r, z), (r, z + 0.45)], 4, phase=0.25 * math.pi), target, m['bronze'], matrix=stack, bevel=(0.02, 1, 40.0)))
    if banners and not ruined:
        z = min(h - 1.0, straight + half + 5.2)
        inner = half + 1.75 + 0.3
        outer = (0.5 * w - 0.25) * (1.0 - taper * z / h) - 0.3
        if outer - inner > 0.7:
            for k, offset in enumerate((-1.0, 1.0)):
                fit = Matrix.Scale(min(1.0, (outer - inner) / 1.6), 4)
                metal += plaque(f'KilnPlaque{name}{k}', m, target, frame @ Matrix.Translation((offset * 0.5 * (inner + outer), 0.32 - front(z), z)) @ fit)
    for k in range(int(w * 1.6)):
        a = rng.uniform(0.0, 2.0 * math.pi)
        r = rng.uniform(0.55, 0.75) * (w + d) * 0.5
        s = rng.uniform(0.5, 1.3)
        px, py = math.cos(a) * r, -0.5 * d + math.sin(a) * r
        if py > 1.0 and abs(px) < half + 2.0:
            continue
        on_foot = abs(px) < 0.5 * (w + 2.4) - 0.3 and abs(py + 0.5 * d) < 0.5 * (d + 2.4) - 0.3
        pz = rng.uniform(-0.6, 0.2) * s if on_foot else LAVA - base + 0.15 * s
        stone.append(pk.part(f'KilnRubble{name}{k}', pk.chipped((s, s * 0.85, s * 0.7), rng, chips=2, depth=0.08, center=(px, py, pz)), target, m['basalt'], matrix=frame @ Matrix.Rotation(rng.uniform(-0.15, 0.15), 4, 'Z'), bevel=(0.04, 1, 40.0)))
    rivets = pk.part(f'KilnRivets{name}', pk.merge_bm(studs), target, m['iron'], smooth=60.0)
    metal.append(rivets)
    return [kit.join(stone, f'Kiln{name}'), kit.join(metal, f'KilnIron{name}'), kit.join(glow, f'Furnace{name}')], frame


def trough(name, start, end, height, m, target, rng):
    sx, sy = start
    ex, ey = end
    length = math.hypot(ex - sx, ey - sy)
    delta = W(ex, ey) - W(sx, sy)
    yaw = math.atan2(delta.y, delta.x)
    frame = Matrix.Translation(W(0.5 * (sx + ex), 0.5 * (sy + ey), height)) @ Matrix.Rotation(yaw, 4, 'Z')
    stone = [pk.part(f'TroughBed{name}', pk.chipped((length, 2.2, 0.6), rng, chips=2, depth=0.08, center=(0.0, 0.0, -0.3)), target, m['masonry'], matrix=frame, bevel=(0.04, 2, 40.0))]
    metal = []
    for side in (-1.0, 1.0):
        stone.append(pk.part(f'TroughWall{name}{int(side)}', pk.box((length, 0.4, 0.8), (0.0, side * 0.9, 0.4)), target, m['masonry'], matrix=frame, bevel=(0.04, 2, 40.0)))
        metal.append(pk.part(f'TroughLip{name}{int(side)}', pk.box((length, 0.5, 0.1), (0.0, side * 0.9, 0.84)), target, m['bronze'], matrix=frame, bevel=(0.02, 1, 30.0)))
    for k in range(max(2, int(length / 4.5))):
        px = -0.5 * length + 1.2 + k * (length - 2.4) / max(1, int(length / 4.5) - 1)
        stone.append(pk.part(f'TroughPier{name}{k}', pk.chipped((1.6, 1.8, height - SUNK), rng, chips=3, depth=0.15, center=(px, 0.0, 0.5 * (SUNK - height) - 0.3)), target, m['basalt'], matrix=frame, bevel=(0.05, 2, 40.0)))
    melt = [pk.part(f'ChannelMeltTop{name}', pk.box((length + 0.05, 1.4, 0.06), (0.0, 0.0, 0.42)), target, m['molten'], matrix=frame)]
    return [kit.join(stone, f'Trough{name}'), kit.join(metal, f'TroughIron{name}'), kit.join(melt, f'ChannelMelt{name}')]


def link_bm(scale=1.0):
    bm = pk.torus(0.3 * scale, 0.09 * scale, 8, 4)
    bmesh.ops.scale(bm, vec=(1.55, 1.0, 1.0), verts=bm.verts)
    return bm


def hanging_chain(name, top, bottom, m, target, scale=1.0):
    a, b = Vector(W(*top)), Vector(W(*bottom))
    axis = b - a
    length = axis.length
    pitch = 0.62 * scale
    count = max(1, int(length / pitch))
    direction = axis.normalized()
    links = []
    for k in range(count):
        bm = link_bm(scale)
        rotation = direction.to_track_quat('X', 'Z').to_matrix().to_4x4() @ Matrix.Rotation(0.5 * math.pi * (k % 2), 4, 'X')
        bmesh.ops.transform(bm, matrix=Matrix.Translation(a + direction * (pitch * (k + 0.5))) @ rotation, verts=bm.verts)
        links.append(bm)
    obj = kit.mesh_object(name, pk.merge_bm(links), target, m['chain'])
    pk.tag(obj)
    kit.smooth_by_angle(obj, 50.0)
    return obj


def gantry(m, target, rng):
    parts = []
    for side in (-1.0, 1.0):
        x = side * 11.5
        parts.append(pk.part(f'GantryLeg{int(side)}', pk.box((1.3, 1.3, 36.0), W(x, 21.0, 13.0)), target, m['iron'], bevel=(0.05, 2, 40.0)))
        parts.append(pk.part(f'GantryFoot{int(side)}', pk.chipped((3.2, 3.2, 3.0), rng, chips=3, depth=0.2, center=tuple(W(x, 21.0, -3.1))), target, m['basalt'], bevel=(0.05, 2, 40.0)))
        for z in (4.0, 12.0, 20.0):
            parts.append(pk.part(f'GantryCollar{int(side)}{int(z)}', pk.box((1.6, 1.6, 0.5), W(x, 21.0, z)), target, m['bronze'], bevel=(0.03, 2, 40.0)))
        parts.append(pk.part(f'GantryBrace{int(side)}', pk.rod(W(x, 21.0, 22.0), W(side * 4.0, 21.0, 30.5), 0.35, 8), target, m['iron'], smooth=40.0))
    parts.append(pk.part('GantryBeam', pk.box((25.0, 1.6, 1.6), W(0.0, 21.0, 31.0)), target, m['iron'], bevel=(0.05, 2, 40.0)))
    objects = [kit.join(parts, 'Gantry')]
    for side in (-1.0, 1.0):
        objects.append(hanging_chain(f'GantryChain{int(side)}', (side * 3.0, 21.0, 30.2), (side * BEARING, PIVOT[1], PIVOT[2] + 0.55), m, target, 1.25))
    return objects


def hanging_crucible(name, x, y, z, scale, m, target, rng):
    frame = Matrix.Translation(W(x, y, z)) @ Matrix.Scale(scale, 4)
    profile = [(0.0, -0.9), (0.7, -0.9), (0.95, -0.6), (1.1, 0.2), (1.2, 0.85), (1.05, 0.95), (0.98, 0.5), (0.0, 0.5)]
    parts = [pk.part(f'SmallCrucibleBody{name}', pk.lathe(profile, 20), target, m['iron'], matrix=frame, smooth=35.0)]
    parts.append(pk.part(f'SmallCrucibleHoop{name}', ring((0.0, 0.0, 0.3), (0.0, 0.0, 1.0), 1.15, 0.08, steps=24, sides=6), target, m['bronze'], matrix=frame, smooth=40.0))
    melt = pk.part(f'SmallCrucibleMelt{name}', pk.cylinder(0.97, 0.05, 20, z=0.62), target, m['molten'], matrix=frame, smooth=30.0)
    objects = [kit.join(parts, f'SmallCrucible{name}'), kit.join([melt], f'VatMeltSmall{name}')]
    top = (x, y, 17.0)
    for k in range(3):
        a = 2.0 * math.pi * k / 3.0
        objects.append(hanging_chain(f'CrucibleChain{name}{k}', top, (x + math.cos(a) * 1.1 * scale, y + math.sin(a) * 1.1 * scale, z + 0.9 * scale), m, target, 0.8))
    return objects


def ingots(m, target, rng, x, y, z):
    frame = placed(x, y, z, rng.uniform(0.0, math.pi))
    bars = []
    for layer in range(3):
        for k in range(4 - layer):
            px = (k - 0.5 * (3 - layer)) * 0.85
            bm = tapered((0.72, 1.95), (0.56, 1.75), 0.19 + layer * 0.38 - 0.19, 0.19 + layer * 0.38 + 0.17, px)
            bars.append(pk.part(f'Ingot{x:.0f}{layer}{k}', bm, target, m['gold'], matrix=frame @ Matrix.Rotation(0.5 * math.pi * layer, 4, 'Z'), bevel=(0.035, 2, 40.0)))
    return kit.join(bars, f'Ingots{abs(int(x))}')


def cliffs(m, target, rng):
    sectors = {}
    angle = 0.0
    while angle < 360.0:
        a = angle % 360.0
        skip = abs(a - 32.0) < 13.0 or abs(a - 148.0) < 13.0 or 172.0 < a < 210.0 or 72.0 < a < 108.0 or a < 22.0 or a > 345.0
        if not skip:
            south = 200.0 < a < 340.0
            r = rng.uniform(31.0, 35.0)
            cx, cy = math.cos(math.radians(a)) * r, math.sin(math.radians(a)) * r
            for k in range(rng.randint(3, 5)):
                ox, oy = rng.uniform(-2.0, 2.0), rng.uniform(-2.0, 2.0)
                height = rng.uniform(5.0, 12.0) if south else rng.uniform(12.0, 30.0)
                bm = column_bm(rng.uniform(1.4, 2.4), SUNK, height, rng)
                bmesh.ops.translate(bm, verts=bm.verts, vec=W(cx + ox, cy + oy))
                sectors.setdefault(int(a // 45.0), []).append(bm)
            if not south and rng.random() < 0.5:
                bm = pk.chipped((rng.uniform(5.0, 8.0), rng.uniform(4.0, 6.0), rng.uniform(16.0, 26.0)), rng, chips=4, depth=0.6, center=(0.0, 0.0, 6.0))
                bmesh.ops.transform(bm, matrix=Matrix.Translation(W(cx * 1.12, cy * 1.12)) @ Matrix.Rotation(rng.uniform(0.0, math.pi), 4, 'Z'), verts=bm.verts)
                sectors.setdefault(int(a // 45.0), []).append(bm)
        angle += rng.uniform(6.0, 9.0)
    objects = []
    for sector, shapes in sectors.items():
        obj = kit.mesh_object(f'Cliff{sector}', pk.merge_bm(shapes), target, m['basalt'])
        pk.tag(obj, rng=rng)
        kit.bevel(obj, 0.05, segments=1, angle=40.0)
        objects.append(obj)
    return objects


def pipes(m, target, rng):
    objects = []
    runs = [
        [W(-27.5, 4.0, 9.0), W(-24.5, 6.0, 9.5), W(-21.5, 10.5, 7.0), W(-19.5, 12.0, 4.5)],
        [W(-30.0, -12.0, 7.0), W(-25.5, -15.5, 5.5), W(-22.5, -16.0, 3.0)],
    ]
    for k, points in enumerate(runs):
        objects.append(pk.tube(points, 0.42, target, m['bronze'], f'Pipe{k}', resolution=6, sides=10, rng=rng))
    return objects


def frame_at(x, y, z, dx, dy, tilt=0.0, roll=0.0):
    d = Vector((-dx, -dy, 0.0)).normalized()
    return Matrix.Translation(W(x, y, z)) @ Matrix.Rotation(math.atan2(d.y, d.x), 4, 'Z') @ Matrix.Rotation(roll, 4, 'X') @ Matrix.Rotation(tilt, 4, 'Y')


def offset_points(points, distance, cap=1.6):
    area = sum(points[i][0] * points[(i + 1) % len(points)][1] - points[(i + 1) % len(points)][0] * points[i][1] for i in range(len(points)))
    orient = 1.0 if area > 0.0 else -1.0
    out = []
    for i in range(len(points)):
        a, b, c = Vector(points[i - 1]), Vector(points[i]), Vector(points[(i + 1) % len(points)])
        n1 = Vector((orient * (b - a).y, -orient * (b - a).x)).normalized()
        n2 = Vector((orient * (c - b).y, -orient * (c - b).x)).normalized()
        bis = n1 + n2
        if bis.length < 1e-6:
            out.append(tuple(b + n1 * distance))
            continue
        bis.normalize()
        out.append(tuple(b + bis * distance * min(1.0 / max(n1.dot(bis), 1e-3), cap)))
    return out


def ring_prism(points, inner, outer, z0, z1):
    a = offset_points(points, inner)
    b = offset_points(points, outer)
    bm = bmesh.new()
    count = len(points)
    rings = [[bm.verts.new((x, y, z)) for x, y in ring] for ring, z in ((a, z0), (b, z0), (b, z1), (a, z1))]
    for i in range(count):
        j = (i + 1) % count
        for r in range(4):
            s = (r + 1) % 4
            bm.faces.new((rings[r][i], rings[r][j], rings[s][j], rings[s][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def mirrored(right, top, bottom):
    return [top] + right + [bottom] + [(-x, y) for x, y in reversed(right)]


def axe_outline(handle=0.0, spike=0.0):
    arc = [(0.6 + 2.5 * math.cos(math.radians(a)), 2.5 * math.sin(math.radians(a))) for a in range(44, -45, -8)]
    right = [(0.22, 0.75 + spike), (0.22, 0.75)] if spike > 0.0 else []
    right += [(0.45, 0.75), (0.5, 0.55), (1.1, 0.75), (1.8, 1.25)] + arc + [(1.8, -1.25), (1.1, -0.75), (0.5, -0.55), (0.45, -0.75)]
    if handle > 0.0:
        right += [(0.22, -0.75), (0.22, -0.75 - handle)]
    return mirrored(right, (0.0, 0.75 + spike), (0.0, -0.75 - handle))


def spear_outline():
    return mirrored([(0.55, 1.0), (0.28, 0.0), (0.28, -0.7)], (0.0, 3.6), (0.0, -0.7))


def upright(bm):
    bmesh.ops.transform(bm, matrix=Matrix.Rotation(0.5 * math.pi, 4, 'X'), verts=bm.verts)
    return bm


def mold_slab(points, thick, rim=0.16, band=0.06):
    stone = prism_bm(points, -0.5 * thick, 0.5 * thick)
    trim = ring_prism(points, -0.03, rim, -0.5 * thick - band, 0.5 * thick + band)
    return stone, trim


def deduped(points, eps=1e-4):
    out = []
    for p in points:
        if not out or abs(p[0] - out[-1][0]) + abs(p[1] - out[-1][1]) > eps:
            out.append(p)
    while len(out) > 1 and abs(out[0][0] - out[-1][0]) + abs(out[0][1] - out[-1][1]) <= eps:
        out.pop()
    return out


def split(points, gap):
    return deduped(fg.clip_half(points, (1.0, 0.0), 0.5 * gap)), deduped(fg.clip_half(points, (-1.0, 0.0), 0.5 * gap))


def slag_patch(name, x, y, radius, m, target, rng, rise=0.35):
    shapes = []
    pad = [(math.cos(2.0 * math.pi * k / 14) * radius * rng.uniform(0.82, 1.08), math.sin(2.0 * math.pi * k / 14) * radius * rng.uniform(0.82, 1.08)) for k in range(14)]
    base = prism_bm(pad, LAVA - 0.5, LAVA + 0.1)
    jitter(base, rng, 0.06, keep_below=LAVA - 0.2)
    shapes.append(base)
    for _ in range(int(radius * radius * 5.0) + 4):
        a = rng.uniform(0.0, 2.0 * math.pi)
        d = radius * math.sqrt(rng.random()) * 0.95
        s = rng.uniform(0.22, 0.6) * (1.15 - 0.5 * d / radius)
        chunk = pk.chipped((s, s * rng.uniform(0.7, 1.1), s * rng.uniform(0.5, 0.9) * rise / 0.35), rng, chips=2, depth=0.07 * s)
        tilt = Vector((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 1.0)).normalized()
        bmesh.ops.transform(chunk, matrix=Matrix.Translation((math.cos(a) * d, math.sin(a) * d, LAVA + 0.05 + s * 0.15)) @ Vector((0.0, 0.0, 1.0)).rotation_difference(tilt).to_matrix().to_4x4() @ Matrix.Rotation(rng.uniform(0.0, math.pi), 4, 'Z'), verts=chunk.verts)
        shapes.append(chunk)
    bm = pk.merge_bm(shapes)
    bmesh.ops.translate(bm, verts=bm.verts, vec=W(x, y, 0.0))
    obj = kit.mesh_object(name, bm, target, m['slag'])
    pk.tag(obj, rng=rng)
    kit.bevel(obj, 0.025, segments=1, angle=40.0)
    return obj


def cast_parts(name, stone, bronze, m, target, rng, iron=None, wood=None):
    objects = []
    for suffix, shapes, material in (('', stone, 'mold'), ('Trim', bronze, 'castbronze'), ('Iron', iron, 'iron'), ('Haft', wood, 'wood')):
        if not shapes:
            continue
        obj = kit.mesh_object(f'{name}{suffix}', pk.merge_bm(shapes), target, m[material])
        pk.tag(obj, rng=rng)
        if material in ('mold', 'wood'):
            kit.bevel(obj, 0.035, segments=2, angle=40.0)
        else:
            kit.smooth_by_angle(obj, 40.0)
        objects.append(obj)
    return objects


def placed_bm(bm, frame):
    bmesh.ops.transform(bm, matrix=frame, verts=bm.verts)
    return bm


def castings(m, target, rng):
    objects = []
    frame = frame_at(-12.4, 8.2, LAVA + 0.36, 1.0, 1.0, roll=0.04, tilt=-0.03) @ Matrix.Rotation(-0.5 * math.pi, 4, "Z")
    objects += weapon_parts("CastSword", m, target, frame @ Matrix.Translation((0.0, -4.2, 0.0)) @ Matrix.Rotation(-0.5 * math.pi, 4, "X"), 1.7, 6.2)
    objects.append(slag_patch('CastSwordSlag', -12.4, 8.2, 2.4, m, target, rng, rise=0.25))

    frame = frame_at(12.2, 7.2, LAVA + 0.36, -1.0, -1.0, roll=-0.05, tilt=0.03) @ Matrix.Rotation(-0.5 * math.pi, 4, "Z")
    objects += axe_parts("CastAxe", m, target, frame @ Matrix.Translation((0.0, -3.2, 0.0)) @ Matrix.Rotation(-0.5 * math.pi, 4, "X"), 1.8, 6.4)
    objects.append(slag_patch('CastAxeSlag', 12.2, 7.2, 2.2, m, target, rng, rise=0.25))

    head = axe_outline()
    stone, bronze = [], []
    for k, half in enumerate(split([(x * 0.85, y * 0.85) for x, y in head], 0.0)):
        side = 1.0 if k == 0 else -1.0
        slab, trim = mold_slab(half, 0.8)
        frame = frame_at(-18.6, 1.0, LAVA + 1.2, -0.87, -0.5) @ Matrix.Translation((side * 0.25, 0.0, 0.0)) @ Matrix.Rotation(side * 0.12, 4, 'Y')
        stone.append(placed_bm(upright(slab), frame))
        bronze.append(placed_bm(upright(trim), frame))
    objects += cast_parts('MoldAxe', stone, bronze, m, target, rng)
    objects.append(slag_patch('MoldAxeSlag', -18.6, 1.0, 2.4, m, target, rng))

    stone, bronze = [], []
    frame = frame_at(-11.5, -10.5, LAVA + 0.75, 1.0, 0.35, roll=0.06)
    for side in (-1.0, 1.0):
        block = Matrix.Translation((side * 0.68, 0.0, 0.0)) @ Matrix.Rotation(side * 0.06, 4, 'Y')
        stone.append(placed_bm(pk.chipped((1.1, 1.5, 1.5), rng, chips=3, depth=0.08), frame @ block))
        for x in (-0.3, 0.3):
            bronze.append(placed_bm(pk.box((0.18, 1.6, 1.6), (x, 0.0, 0.0)), frame @ block))
    bronze.append(placed_bm(pk.cylinder(0.32, 0.5, 12, z=-0.25), frame @ Matrix.Rotation(0.5 * math.pi, 4, 'Y')))
    objects += cast_parts('MoldHammer', stone, bronze, m, target, rng)
    objects.append(slag_patch('MoldHammerSlag', -11.5, -10.5, 2.0, m, target, rng))

    stone, bronze = [], []
    for k, half in enumerate(split(spear_outline(), 0.0)):
        side = 1.0 if k == 0 else -1.0
        slab, trim = mold_slab(half, 0.6, rim=0.12)
        frame = frame_at(-15.8, -5.2, LAVA + 0.3, -0.9, -0.4) @ Matrix.Rotation(0.45, 4, 'Y') @ Matrix.Translation((side * 0.16, 0.0, 0.0))
        stone.append(placed_bm(upright(slab), frame))
        bronze.append(placed_bm(upright(trim), frame))
        bronze.append(placed_bm(upright(pk.box((0.7, 0.4, 0.7), (0.0, -0.35, 0.0))), frame))
    objects += cast_parts('MoldSpear', stone, bronze, m, target, rng)
    objects.append(slag_patch('MoldSpearSlag', -15.8, -5.2, 1.6, m, target, rng))

    stone, bronze = [], []
    disc = circle_points(1.5, 28)
    for k, half in enumerate(split(disc, 0.0)):
        side = 1.0 if k == 0 else -1.0
        slab, trim = mold_slab(half, 0.5)
        boss = prism_bm(fg.clip_half(circle_points(0.45, 16), (side, 0.0), 0.0), 0.25, 0.48)
        frame = frame_at(12.2, -10.0, LAVA + 1.05, -0.87, 0.5) @ Matrix.Translation((side * 0.22, 0.0, 0.0)) @ Matrix.Rotation(side * 0.1, 4, 'Y')
        stone.append(placed_bm(upright(slab), frame))
        bronze.append(placed_bm(upright(trim), frame))
        bronze.append(placed_bm(upright(boss), frame))
    objects += cast_parts('MoldShield', stone, bronze, m, target, rng)
    objects.append(slag_patch('MoldShieldSlag', 12.2, -10.0, 2.0, m, target, rng))

    iron = []
    frame = frame_at(17.6, -5.4, LAVA + 0.45, 1.0, -1.05, roll=0.05)
    for side in (-1.0, 1.0):
        path = [(-2.6, side * 0.32), (0.4, 0.0), (1.15, -side * 0.38), (1.8, -side * 0.48), (2.25, -side * 0.15)]
        for (ax, ay), (bx, by) in zip(path, path[1:]):
            a, b = Vector((ax, ay, side * 0.06)), Vector((bx, by, side * 0.06))
            seg = pk.box(((b - a).length + 0.2, 0.22, 0.2))
            angle = math.atan2(b.y - a.y, b.x - a.x)
            iron.append(placed_bm(seg, frame @ Matrix.Translation((a + b) * 0.5) @ Matrix.Rotation(angle, 4, 'Z')))
    pivot = [placed_bm(pk.cylinder(0.24, 0.42, 14, z=-0.21), frame @ Matrix.Translation((0.4, 0.0, 0.0)))]
    pivot.append(placed_bm(pk.cylinder(0.12, 0.5, 10, z=-0.25), frame @ Matrix.Translation((0.4, 0.0, 0.0))))
    objects += cast_parts('CastTongs', None, pivot, m, target, rng, iron=iron)
    objects.append(slag_patch('CastTongsSlag', 17.6, -5.4, 2.2, m, target, rng, rise=0.25))

    stone, bronze, wood = [], [], []
    frame = frame_at(-3.4, -20.6, LAVA + 0.62, 1.0, 1.0, roll=0.05)
    stone.append(placed_bm(pk.chipped((1.3, 1.25, 1.25), rng, chips=3, depth=0.06), frame))
    for x in (-0.48, 0.48):
        bronze.append(placed_bm(pk.box((0.18, 1.36, 1.36), (x, 0.0, 0.0)), frame))
    handle = frame @ Matrix.Rotation(0.5 * math.pi, 4, 'Z')
    wood.append(placed_bm(pk.rod((0.0, 0.6, -0.1), (0.0, 2.3, -0.25), 0.24, 10), handle))
    wood.append(placed_bm(pk.cone_bm((0.0, 2.25, -0.25), (0.05, 2.75, -0.2), 0.24, sides=7), handle))
    wood.append(placed_bm(pk.rod((0.35, 3.2, -0.42), (0.75, 5.1, -0.45), 0.24, 10), handle))
    wood.append(placed_bm(pk.cone_bm((0.35, 3.25, -0.42), (0.28, 2.85, -0.4), 0.24, sides=7), handle))
    bronze.append(placed_bm(pk.rod((0.66, 4.6, -0.45), (0.73, 5.0, -0.45), 0.29, 10), handle))
    objects += cast_parts('CastHammer', stone, bronze, m, target, rng, wood=wood)
    objects.append(slag_patch('CastHammerSlag', -5.0, -22.2, 2.6, m, target, rng, rise=0.25))
    return objects


def circle_points(radius, steps):
    return [(radius * math.cos(2.0 * math.pi * k / steps), radius * math.sin(2.0 * math.pi * k / steps)) for k in range(steps)]


def toe_stations(width, height, length):
    half = 0.5 * width
    return [(-0.3, half, height * 0.96, 0.0, 0.0), (0.5 * length, half * 1.02, height * 1.04, 0.0, 0.0), (0.58 * length, half * 0.95, height * 0.9, 0.0, 0.01), (0.82 * length, half * 0.94, height * 0.84, 0.0, 0.02), (0.94 * length, half * 0.84, height * 0.72, 0.0, 0.04), (length, half * 0.55, height * 0.45, 0.0, 0.09)]


def station_at(stations, y):
    for (y0, w0, h0, _, b0), (y1, w1, h1, _, b1) in zip(stations, stations[1:]):
        if y0 <= y <= y1:
            t = (y - y0) / (y1 - y0)
            return w0 + (w1 - w0) * t, h0 + (h1 - h0) * t, b0 + (b1 - b0) * t
    _, w, h, _, b = stations[-1]
    return w, h, b


def toenail_bm(width, height, length):
    stations = toe_stations(width, height, length)
    rows, cols = 7, 9
    y0, y1 = 0.6 * length, 0.985 * length
    bm = bmesh.new()
    grid = []
    for i in range(rows + 1):
        t = i / rows
        y = y0 + (y1 - y0) * t
        w, h, b = station_at(stations, y)
        span = 0.62 * (1.0 - 0.35 * t * t)
        rise = 0.035 * min(1.0, t * 3.0)
        row = []
        for j in range(cols + 1):
            a = 0.5 * math.pi + span * (2.0 * j / cols - 1.0)
            c, s = math.cos(a), math.sin(a)
            p = Vector((w * math.copysign(abs(c) ** 0.62, c), y, b + h * s ** 0.55))
            out = Vector((p.x, 0.0, p.z - b - 0.35 * h)).normalized()
            edge = 1.0 - 0.6 * abs(2.0 * j / cols - 1.0) ** 3
            row.append((bm.verts.new(p - out * 0.04), bm.verts.new(p + out * rise * edge)))
        grid.append(row)
    for i in range(rows):
        for j in range(cols):
            a, b2, c2, d = grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]
            bm.faces.new((a[1], b2[1], c2[1], d[1]))
            bm.faces.new((d[0], c2[0], b2[0], a[0]))
    for i in range(rows):
        for j in (0, cols):
            bm.faces.new((grid[i][j][0], grid[i][j][1], grid[i + 1][j][1], grid[i + 1][j][0]))
    for j in range(cols):
        for i in (0, rows):
            bm.faces.new((grid[i][j][0], grid[i][j + 1][0], grid[i][j + 1][1], grid[i][j][1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def path_loft(points, radii, sides=10, squash=1.0, caps=True, up=Vector((0.0, 0.0, 1.0))):
    pts = [Vector(p) for p in points]
    bm = bmesh.new()
    rings = []
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        ref = up if abs(t.dot(up)) < 0.9 else Vector((1.0, 0.0, 0.0))
        n1 = t.cross(ref).normalized()
        n2 = t.cross(n1).normalized()
        r = radii[i]
        rings.append([bm.verts.new(p + n1 * (math.cos(2.0 * math.pi * k / sides) * r) + n2 * (math.sin(2.0 * math.pi * k / sides) * r * squash)) for k in range(sides)])
    for a, b in zip(rings, rings[1:]):
        for k in range(sides):
            j = (k + 1) % sides
            bm.faces.new((a[k], a[j], b[j], b[k]))
    if caps:
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def blade_bm(length, width, thick, tip=0.2, fuller=0.7, taper=0.35):
    bm = bmesh.new()
    rings = []
    steps = 14
    for i in range(steps + 1):
        u = i / steps
        z = length * u
        w = width * (1.0 - taper * u)
        t = thick * (1.0 - 0.45 * u)
        if u > 1.0 - tip:
            k = (u - (1.0 - tip)) / tip
            w *= max(0.02, 1.0 - k) ** 0.75
            t *= max(0.05, 1.0 - k) ** 0.6
        f = 0.55 if u < fuller else 0.55 + 0.45 * min(1.0, (u - fuller) / 0.12)
        shape = [(w, 0.0), (0.5 * w, t), (0.16 * w, t), (0.0, f * t), (-0.16 * w, t), (-0.5 * w, t), (-w, 0.0), (-0.5 * w, -t), (-0.16 * w, -t), (0.0, -f * t), (0.16 * w, -t), (0.5 * w, -t)]
        rings.append([bm.verts.new((x, y, z)) for x, y in shape])
    apex = bm.verts.new((0.0, 0.0, length * 1.0 + 0.02 * length))
    count = len(rings[0])
    for a, b in zip(rings, rings[1:]):
        for k in range(count):
            j = (k + 1) % count
            bm.faces.new((a[k], a[j], b[j], b[k]))
    bm.faces.new(list(reversed(rings[0])))
    for k in range(count):
        bm.faces.new((rings[-1][k], rings[-1][(k + 1) % count], apex))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def weapon_parts(name, m, target, frame, s, blade_len, kind='sword'):
    steel, iron, bronze, leather = [], [], [], []
    grip = 0.95 * s if kind == 'sword' else 0.55 * s
    pommel = pk.lathe([(0.0, -0.32 * s), (0.12 * s, -0.31 * s), (0.2 * s, -0.22 * s), (0.22 * s, -0.12 * s), (0.17 * s, -0.03 * s), (0.08 * s, 0.0), (0.0, 0.0)], 8)
    bronze.append(pommel)
    bronze.append(pk.cylinder(0.075 * s, 0.06 * s, 8, z=-0.37 * s))
    radii = [0.085 * s if k % 2 else 0.097 * s for k in range(11)]
    radii[0] = radii[-1] = 0.1 * s
    leather.append(path_loft([(0.0, 0.0, grip * k / 10.0) for k in range(11)], radii, sides=8, squash=0.8))
    bronze.append(pk.cylinder(0.11 * s, 0.07 * s, 8, z=grip - 0.05 * s))
    guard_z = grip + 0.04 * s
    span = (0.95 if kind == 'sword' else 0.55) * s
    curve = 0.12 * s if kind == 'sword' else -0.1 * s
    pts, rad = [], []
    for k in range(11):
        u = 2.0 * k / 10.0 - 1.0
        pts.append((u * span, 0.0, guard_z + curve * u * u))
        rad.append(s * (0.085 - 0.03 * abs(u)))
    iron.append(path_loft(pts, rad, sides=8, squash=0.75))
    for side in (-1.0, 1.0):
        iron.append(pk.ellipsoid_bm((side * span, 0.0, guard_z + curve), (0.075 * s, 0.075 * s, 0.075 * s), segments=8, rings=5))
    iron.append(tapered((0.34 * s, 0.17 * s), (0.26 * s, 0.13 * s), guard_z - 0.08 * s, guard_z + 0.14 * s))
    width = (0.2 if kind == 'sword' else 0.17) * s
    blade = blade_bm(blade_len, width, 0.035 * s, tip=0.18 if kind == 'sword' else 0.3, taper=0.35 if kind == 'sword' else 0.55)
    bmesh.ops.translate(blade, verts=blade.verts, vec=(0.0, 0.0, guard_z + 0.12 * s))
    steel.append(blade)
    out = []
    for suffix, shapes, material in (('Blade', steel, 'steel'), ('Guard', iron, 'iron'), ('Fittings', bronze, 'castbronze'), ('Grip', leather, 'leather')):
        obj = pk.part(f'{name}{suffix}', pk.merge_bm(shapes), target, m[material], matrix=frame)
        kit.smooth_by_angle(obj, 35.0 if material != 'steel' else 20.0)
        out.append(obj)
    return out


def axe_parts(name, m, target, frame, s, height):
    wood, steel, iron, leather = [], [], [], []
    haft = [(0.03 * s * math.sin(math.pi * k / 10.0), 0.0, height * k / 10.0) for k in range(11)]
    wood.append(path_loft(haft, [0.12 * s * (1.1 - 0.15 * k / 10.0) for k in range(11)], sides=10, squash=0.78))
    iron.append(pk.lathe([(0.0, -0.05 * s), (0.14 * s, -0.04 * s), (0.15 * s, 0.1 * s), (0.12 * s, 0.14 * s), (0.0, 0.14 * s)], 10))
    for k in range(6):
        z = 0.35 * s + k * 0.13 * s
        leather.append(path_loft([(0.0, 0.0, z), (0.0, 0.0, z + 0.1 * s)], [0.142 * s, 0.142 * s], sides=10, squash=0.8))
    head_z = height - 0.5 * s
    length = 0.95 * s
    rings = []
    for i in range(10):
        u = i / 9.0
        x = 0.12 * s + length * u
        half_h = s * (0.16 + 0.42 * u ** 1.7)
        center = head_z - s * 0.16 * u ** 1.4
        t = s * (0.11 * (1.0 - u) ** 1.3 + 0.008)
        ring_pts = []
        for k in range(12):
            a = 2.0 * math.pi * k / 12.0
            v = math.sin(a)
            bulge = 0.12 * s * (1.0 - v * v) * u ** 4
            ring_pts.append((x + bulge, t * math.cos(a) * (1.0 if abs(math.cos(a)) > 0.3 else 0.9), center + half_h * math.copysign(abs(v) ** 0.7, v)))
        rings.append(ring_pts)
    steel.append(loft_rings(rings))
    steel.append(pk.box((0.26 * s, 0.24 * s, 0.34 * s), (-0.12 * s, 0.0, head_z)))
    iron.append(loft_rings([rounded_ring(0.0, 0.17 * s, 0.15 * s, head_z - 0.24 * s), rounded_ring(0.0, 0.17 * s, 0.15 * s, head_z + 0.24 * s)]))
    out = []
    for suffix, shapes, material in (('Haft', wood, 'wood'), ('Head', steel, 'steel'), ('Iron', iron, 'iron'), ('Wrap', leather, 'leather')):
        obj = pk.part(f'{name}{suffix}', pk.merge_bm(shapes), target, m[material], matrix=frame)
        kit.smooth_by_angle(obj, 30.0)
        out.append(obj)
    return out


def sword(name, m, target, rng, frame, length):
    return weapon_parts(name, m, target, frame, 1.0, length)


def axe(name, m, target, rng, frame, height):
    return axe_parts(name, m, target, frame, 1.0, height)


def rack(m, target, rng, x, y, z, yaw):
    frame = placed(x, y, z, yaw)
    stone, iron, bronze = [], [], []
    stone.append(pk.chipped((6.6, 9.8, 0.7), rng, chips=3, depth=0.25, center=(0.0, 0.0, 0.35)))
    stone.append(pk.chipped((5.6, 8.9, 0.35), rng, chips=2, depth=0.1, center=(0.0, 0.0, 0.87)))
    bronze.append(ring_prism([(-2.8, -4.45), (2.8, -4.45), (2.8, 4.45), (-2.8, 4.45)], -0.02, 0.07, 0.86, 1.0))
    for py in (-4.6, 4.6):
        for px in (1.4, -1.4):
            base = Vector((px, py, 1.04))
            stone.append(pk.chipped((0.95, 0.95, 0.45), rng, chips=1, depth=0.04, center=(px, py, 1.27)))
            iron.append(pk.lathe([(0.32, 1.5), (0.28, 1.6), (0.25, 7.0), (0.3, 7.1)], 8, phase=math.pi / 8.0))
            bmesh.ops.translate(iron[-1], verts=iron[-1].verts, vec=(px, py, 0.0))
            bronze.append(pk.lathe([(0.32, 6.95), (0.42, 7.15), (0.42, 7.3), (0.34, 7.38)], 8, phase=math.pi / 8.0))
            bmesh.ops.translate(bronze[-1], verts=bronze[-1].verts, vec=(px, py, 0.0))
            bronze.append(pk.lathe([(0.33, 1.5), (0.4, 1.58), (0.36, 1.75), (0.29, 1.8)], 8, phase=math.pi / 8.0))
            bmesh.ops.translate(bronze[-1], verts=bronze[-1].verts, vec=(px, py, 0.0))
            bronze.append(pk.lathe([(0.0, 8.05), (0.14, 7.95), (0.18, 7.75), (0.1, 7.6), (0.0, 7.5)], 10))
            bmesh.ops.translate(bronze[-1], verts=bronze[-1].verts, vec=(px, py, 0.0))
    for px in (1.4, -1.4):
        iron.append(pk.box((0.38, 9.6, 0.42), (px, 0.0, 7.55)))
        for k in range(12):
            iron.append(pk.ellipsoid_bm((px + 0.2, -4.4 + 8.8 * (k + 0.5) / 12.0, 7.55), (0.045, 0.045, 0.045), segments=6, rings=4))
        iron.append(pk.box((0.32, 9.4, 0.3), (px, 0.0, 2.1)))
        for side in (-1.0, 1.0):
            iron.append(path_loft([(px, side * 4.55, 5.6), (px, side * 3.5, 7.35)], [0.09, 0.09], sides=6))
    for py in (-4.6, 4.6):
        iron.append(pk.box((3.2, 0.38, 0.36), (0.0, py, 7.75)))
        bronze.append(pk.box((0.9, 0.08, 0.6), (0.0, py + math.copysign(0.22, py), 7.55)))
    weapons = []
    for k, py in enumerate((-3.4, -1.4, 0.6, 2.6)):
        iron.append(path_loft([(1.4, py, 7.3), (1.4, py, 7.05), (1.75, py, 6.95)], [0.06, 0.06, 0.05], sides=6))
        for side in (-0.3, 0.3):
            iron.append(pk.box((0.14, 0.12, 0.35), (1.15, py + side, 2.4)))
        lean = Matrix.Translation((1.15, py, 1.6)) @ Matrix.Rotation(0.06, 4, 'Y')
        weapons += sword(f'Sword{k}', m, target, rng, frame @ lean, rng.uniform(3.6, 4.2))
    for k, py in enumerate((-3.0, 0.0, 3.0)):
        iron.append(path_loft([(-1.4, py - 0.35, 6.4), (-1.75, py - 0.35, 6.3)], [0.06, 0.05], sides=6))
        iron.append(path_loft([(-1.4, py + 0.35, 6.4), (-1.75, py + 0.35, 6.3)], [0.06, 0.05], sides=6))
        lean = Matrix.Translation((-1.75, py, 1.12)) @ Matrix.Rotation(0.05, 4, 'Y') @ Matrix.Rotation(0.5 * math.pi, 4, 'Z')
        weapons += axe(f'Axe{k}', m, target, rng, frame @ lean, rng.uniform(5.2, 5.6))
    out = []
    for suffix, shapes, material in (('RackPlinth', stone, 'kiln'), ('Rack', iron, 'iron'), ('RackBronze', bronze, 'castbronze')):
        obj = pk.part(suffix, pk.merge_bm(shapes), target, m[material], matrix=frame)
        if material == 'kiln':
            kit.bevel(obj, 0.05, segments=2, angle=40.0)
        else:
            kit.smooth_by_angle(obj, 35.0)
        out.append(obj)
    return out + [kit.join(weapons, 'RackWeapons')]


def sunken_blade(m, target, rng):
    frame = placed(10.5, -14.0, -3.15, 2.2) @ Matrix.Rotation(-0.42, 4, 'Y') @ Matrix.Rotation(0.5 * math.pi, 4, 'Z')
    return kit.join(weapon_parts('Sunken', m, target, frame, 1.25, 3.4, 'dagger'), 'SunkenBlade')


def hull_chunk(center, size, lean, rng, points=18, cuts=3):
    bm = bmesh.new()
    rot = Vector((0.0, 0.0, 1.0)).rotation_difference(Vector((lean[0], lean[1], 1.0)).normalized()).to_matrix()
    for _ in range(points):
        d = Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0)))
        if d.length > 1.0:
            d.normalize()
        p = Vector((d.x * size[0], d.y * size[1], (d.z * 0.5 + 0.5) * size[2]))
        bm.verts.new(Vector(center) + rot @ p)
    result = bmesh.ops.convex_hull(bm, input=bm.verts[:])
    bmesh.ops.delete(bm, geom=list({g for g in result['geom_interior'] + result['geom_unused'] if isinstance(g, bmesh.types.BMVert)}), context='VERTS')
    for _ in range(cuts):
        n = Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(0.1, 1.0))).normalized()
        c = Vector(center) + rot @ Vector((0.0, 0.0, size[2] * 0.5))
        reach = max(abs(n.x) * size[0], abs(n.y) * size[1], abs(n.z) * size[2] * 0.5)
        kit.cut(bm, c + n * reach * rng.uniform(0.55, 0.85), n)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def scatter_spike(index, m, target, rng):
    chunks = []
    main_h = rng.uniform(2.2, 3.4)
    chunks.append(hull_chunk((0.0, 0.0, -1.2), (rng.uniform(1.0, 1.4), rng.uniform(0.8, 1.2), main_h + 1.2), (rng.uniform(-0.25, 0.25), rng.uniform(-0.25, 0.25)), rng, cuts=4))
    for k in range(rng.randint(3, 5)):
        a = rng.uniform(0.0, 2.0 * math.pi)
        d = rng.uniform(0.9, 1.7)
        h = main_h * rng.uniform(0.35, 0.75)
        chunks.append(hull_chunk((math.cos(a) * d, math.sin(a) * d, -1.0), (rng.uniform(0.5, 0.9), rng.uniform(0.45, 0.8), h + 1.0), (math.cos(a) * rng.uniform(0.2, 0.55), math.sin(a) * rng.uniform(0.2, 0.55)), rng))
    for k in range(rng.randint(5, 8)):
        a = rng.uniform(0.0, 2.0 * math.pi)
        d = rng.uniform(1.4, 2.4)
        s = rng.uniform(0.2, 0.45)
        chunks.append(hull_chunk((math.cos(a) * d, math.sin(a) * d, -0.4), (s, s * 0.8, s * 1.4), (rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6)), rng, points=10, cuts=1))
    obj = kit.mesh_object(f'ScatterSpike{index}', pk.merge_bm(chunks), target, m['obsidian'])
    pk.tag(obj)
    obj.data.shade_flat()
    return obj


def polar(degrees, radius):
    return math.cos(math.radians(degrees)) * radius, math.sin(math.radians(degrees)) * radius


def build(target):
    rng = random.Random(97)
    m = materials()
    tops = F['ForgeIslandTops']
    objects = titan(m, target, rng)
    objects.append(cradle(m, target, rng))
    objects += crucible(m, target, rng)
    objects += vat(0, 24.5, -9.8, 0.8, m, target, rng) + vat(1, -24.5, 7.0, 0.8, m, target, rng)
    objects += gantry(m, target, rng)
    for index in range(len(F['ForgeIslandCenters'])):
        objects.append(basalt_island(index, m, target, rng))
    parts, frame = furnace('NE', *polar(32.0, 26.0), 0.5, (11.0, 9.0, 13.0), (2.2, 3.2), 16.0, m, target, rng, stairs=False)
    objects += parts + bellows(0, frame @ Matrix.Translation((8.4, -3.2, -0.2)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Z'), m, target, rng)
    parts, frame = furnace('NW', *polar(148.0, 26.0), 0.5, (10.0, 9.0, 11.5), (2.0, 3.0), 20.0, m, target, rng, stairs=False)
    objects += parts + bellows(1, frame @ Matrix.Translation((-8.0, -3.2, -0.2)) @ Matrix.Rotation(-0.5 * math.pi, 4, 'Z'), m, target, rng)
    objects += furnace('E', 27.0, 3.5, 0.0, (11.0, 7.5, 7.0), (2.4, 2.0), 7.0, m, target, rng)[0]
    objects += catwalk('WalkE', (21.3, 2.5, -1.75), (22.4, -0.8, 0.15), 1.6, m, target, rng)
    objects += catwalk('WalkW', (-23.0, 4.2, 0.8), (-24.2, 1.6, 0.05), 1.6, m, target, rng)
    objects += flanged_pipe('VatPipeE', [(25.8, -8.2, 5.0), (27.2, -6.0, 7.0), (28.6, -2.5, 7.0), (29.0, -0.4, 5.5)], 0.38, m, target, rng)
    objects += flanged_pipe('VatPipeW', [(-25.8, 5.2, 5.0), (-27.0, 3.6, 7.0), (-28.5, 2.0, 6.5)], 0.38, m, target, rng)
    objects += furnace('W', -27.0, -2.0, 0.0, (8.0, 7.0, 10.5), (1.6, 2.8), 0.0, m, target, rng, stacks=3)[0]
    objects += furnace('SE', 21.0, -19.0, tops[0], (5.5, 4.5, 5.0), (1.2, 1.6), 6.0, m, target, rng, banners=False, ruined=True)[0]
    objects += furnace('SW', -21.5, -12.0, tops[2], (5.0, 4.5, 4.8), (1.1, 1.5), 7.0, m, target, rng, banners=False)[0]
    objects += rack(m, target, rng, 15.5, -14.0, tops[0], 0.0)
    objects.append(ingots(m, target, rng, 19.5, -12.0, tops[0]))
    objects.append(ingots(m, target, rng, -16.0, -12.5, tops[2]))
    objects += great_anvil('AnvilE', 22.8, -3.0, 0.2, 1.0, 0.0, m, target, random.Random(41))
    objects += great_anvil('AnvilW', -20.6, -6.6, -0.1, -1.0, 0.0, m, target, random.Random(43))
    objects += channel(0, 32.0, m, target, rng, 18.6, 22.9) + channel(1, 148.0, m, target, rng, 18.6, 22.9)
    objects += trough('S', (4.0, -33.0), (4.0, -21.0), 0.6, m, target, rng)
    objects += hanging_crucible('A', -18.5, 10.0, 3.5, 1.5, m, target, rng)
    objects.append(sunken_blade(m, target, rng))
    objects += pillars(m, target, rng)
    objects += castings(m, target, random.Random(919))
    objects += cliffs(m, target, rng)
    objects += pipes(m, target, rng)
    for index in range(3):
        objects.append(scatter_spike(index, m, target, rng))
        objects.append(scatter_rock(index, m, target, rng))
    for obj in objects:
        kit.select_only([obj])
        with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
            bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return objects


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    started = time.perf_counter()
    objects = build(target)
    print(f'BUILD {time.perf_counter() - started:.1f} s')
    pk.unwrap_all(objects, {'TitanStone': 0.002, 'CavernMasonry': 0.002, 'TitanBronze': 0.003, 'TitanHide': 0.002, 'KilnStone': 0.002, 'MoldStone': 0.003})
    for obj in objects:
        print('TRIS', obj.name, sum(len(p.vertices) - 2 for p in obj.data.polygons))
    if args.get('--preview'):
        folder = args['--preview']
        kit.environment('forest.exr', strength=0.25)
        moving = [obj for obj in objects if obj.name.startswith('Crucible')]
        for obj in moving:
            obj.matrix_world = Matrix.Translation(W(*PIVOT))
        titan_parts = [obj for obj in objects if obj.name.startswith(('Titan', 'Crucible', 'Cradle'))]
        kit.render(os.path.join(folder, 'backdrop_titan.png'), titan_parts, yaw=200.0, pitch=35.0, fov=40.0, fill=0.8, size=(1280, 720), samples=32)
        kit.render(os.path.join(folder, 'backdrop_hands.png'), [obj for obj in objects if obj.name.startswith(('TitanArms', 'Crucible', 'Cradle'))], yaw=180.0, pitch=57.0, fov=40.0, fill=0.9, size=(1280, 720), samples=32)
        kit.render(os.path.join(folder, 'backdrop_top.png'), objects, yaw=180.0, pitch=70.0, fill=1.0, size=(1280, 720), samples=32)
        for obj in moving:
            obj.matrix_world = Matrix.Identity(4)
    if args.get('--out'):
        started = time.perf_counter()
        pk.export(objects, args['--out'], NAME, ao_distance=1.2)
        print(f'EXPORT {time.perf_counter() - started:.1f} s')


if __name__ == '__main__':
    main()
