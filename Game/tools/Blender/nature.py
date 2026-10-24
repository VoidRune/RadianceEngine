import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector, noise

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

NAME = 'Nature'
UP = Vector((0.0, 0.0, 1.0))


def fbm(p, octaves=3):
    total, amplitude, frequency = 0.0, 1.0, 1.0
    for _ in range(octaves):
        total += amplitude * noise.noise(p * frequency)
        amplitude *= 0.5
        frequency *= 2.0
    return total


def seed_of(rng):
    return Vector((rng.uniform(-60.0, 60.0), rng.uniform(-60.0, 60.0), rng.uniform(-60.0, 60.0)))


def around(angle):
    return Vector((math.cos(angle), math.sin(angle), 0.0))


def random_unit(rng):
    z = rng.uniform(-1.0, 1.0)
    return around(rng.uniform(0.0, 2.0 * math.pi)) * math.sqrt(1.0 - z * z) + UP * z


def support(bm, direction):
    return max(vertex.co.dot(direction) for vertex in bm.verts)


def frame(normal, roll):
    n = normal.normalized()
    helper = UP if abs(n.z) < 0.9 else Vector((1.0, 0.0, 0.0))
    t = helper.cross(n).normalized()
    b = n.cross(t)
    return t * math.cos(roll) + b * math.sin(roll), b * math.cos(roll) - t * math.sin(roll)


def rock_bm(radius, height, rng, cuts=9, lumps=0.12, lift=0.45):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=3, radius=1.0)
    seed = seed_of(rng)
    for vertex in bm.verts:
        direction = vertex.co.normalized()
        bump = 1.0 + lumps * fbm(direction * 1.7 + seed, 3)
        vertex.co = Vector((direction.x * radius * bump, direction.y * radius * bump, direction.z * height * bump))
    for _ in range(cuts):
        elevation = rng.uniform(-0.1, 1.3)
        normal = (around(rng.uniform(0.0, 2.0 * math.pi)) * math.cos(elevation) + UP * math.sin(elevation)).normalized()
        kit.cut(bm, normal * support(bm, normal) * rng.uniform(0.8, 0.92), normal)
    bmesh.ops.translate(bm, vec=(0.0, 0.0, lift * height), verts=bm.verts)
    kit.cut(bm, Vector((0.0, 0.0, -0.04)), -UP)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def blob(shapes, target, material, name, voxel, rng, lumps=0.05, scale=3.5, smooth=3, faces=2500, floor=None):
    obj = kit.mesh_object(name, pk.merge_bm(shapes), target, material)
    pk.remesh(obj, voxel)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.normal_update()
    seed = seed_of(rng)
    for vertex in bm.verts:
        vertex.co += vertex.normal * fbm(vertex.co * scale + seed, 3) * lumps
    if floor is not None:
        kit.cut(bm, Vector((0.0, 0.0, floor)), -UP)
    bm.to_mesh(obj.data)
    bm.free()
    if smooth > 0:
        smoother = obj.modifiers.new('Smooth', 'SMOOTH')
        smoother.factor = 0.5
        smoother.iterations = smooth
        kit.apply_modifiers(obj)
    decimate = obj.modifiers.new('Decimate', 'DECIMATE')
    decimate.ratio = min(1.0, faces / max(len(obj.data.polygons), 1))
    kit.apply_modifiers(obj)
    pk.tag(obj, rng=rng)
    obj.data.shade_smooth()
    return obj


def cards(name, placements, target, material, normal_of, rng):
    columns = int(material.get('slots', 8))
    cell = 1.0 / columns
    pad = cell * 0.04
    corners = ((-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5))
    verts, faces, uvs, local, normals = [], [], [], [], []
    for origin, across, along, width, length in placements:
        facing = across.cross(along).normalized()
        slot = rng.randrange(columns * columns)
        shade = (slot * 0.618034) % 1.0
        column, row = slot % columns, slot // columns
        for side in (1.0, -1.0):
            start = len(verts)
            for u, v in corners:
                point = origin + across * (u * width) + along * (v * length)
                verts.append(tuple(point))
                local.extend((u, v, shade))
                uvs.append((column * cell + pad + (u + 0.5) * (cell - 2.0 * pad), row * cell + pad + (v + 0.5) * (cell - 2.0 * pad)))
                normals.append(tuple(normal_of(point, facing * side)))
            loop = [start, start + 1, start + 2, start + 3]
            faces.append(loop if side > 0.0 else loop[::-1])
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    layer = mesh.uv_layers.new(name='UVMap')
    for loop in mesh.loops:
        layer.data[loop.index].uv = uvs[loop.vertex_index]
    mesh.attributes.new('Local', 'FLOAT_VECTOR', 'POINT').data.foreach_set('vector', local)
    mesh.materials.append(material)
    mesh.shade_smooth()
    mesh.normals_split_custom_set_from_vertices(normals)
    obj = bpy.data.objects.new(name, mesh)
    target.objects.link(obj)
    return obj


def shell(center, radii, count, size, rng, low=-0.45, push=(0.94, 1.12), wander=0.6):
    placements = []
    for _ in range(count):
        z = rng.uniform(low, 1.0)
        direction = around(rng.uniform(0.0, 2.0 * math.pi)) * math.sqrt(max(0.0, 1.0 - z * z)) + UP * z
        surface = center + Vector((direction.x * radii[0], direction.y * radii[1], direction.z * radii[2])) * rng.uniform(*push)
        outward = Vector((direction.x / radii[0], direction.y / radii[1], direction.z / radii[2])).normalized()
        across, along = frame(outward + random_unit(rng) * wander, rng.uniform(0.0, 2.0 * math.pi))
        scale = rng.uniform(size[0], size[1])
        placements.append((surface, across, along, scale, scale))
    return placements


def ellipsoid_normal(center, radii, bend=0.8):
    def normal_of(point, facing):
        d = point - center
        outward = Vector((d.x / (radii[0] * radii[0]), d.y / (radii[1] * radii[1]), d.z / (radii[2] * radii[2]))).normalized()
        return (outward * bend + facing * (1.0 - bend)).normalized()
    return normal_of


def scatter_clumps(center, radii, count, size, rng, low=-0.3):
    shapes = []
    for _ in range(count):
        z = rng.uniform(low, 1.0)
        direction = around(rng.uniform(0.0, 2.0 * math.pi)) * math.sqrt(max(0.0, 1.0 - z * z)) + UP * z
        point = center + Vector((direction.x * radii[0], direction.y * radii[1], direction.z * radii[2]))
        r = rng.uniform(size[0], size[1])
        shapes.append(pk.ellipsoid_bm(point, (r, r, r * 0.82), segments=10, rings=6))
    return shapes


def trim(obj, floor):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    kit.cut(bm, Vector((0.0, 0.0, floor)), -UP)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    return obj


def rock(name, spec, m, target, rng):
    radius, height, material, cuts = spec
    return pk.part(name, rock_bm(radius, height, rng, cuts=cuts), target, m[material], rng=rng, bevel=(0.035, 2, 32.0))


def rock_pair(name, spec, m, target, rng):
    main = rock(name, spec, m, target, rng)
    radius, height, material, cuts = spec
    side = pk.part(name + '_Side', rock_bm(radius * 0.5, height * 0.55, rng, cuts=6), target, m[material], rng=rng, bevel=(0.03, 2, 32.0))
    side.matrix_world = Matrix.Translation(around(rng.uniform(0.0, 6.28)) * radius * 1.05) @ Matrix.Rotation(rng.uniform(0.0, 6.28), 4, 'Z')
    return kit.join([main, side], name)


def oak(key, m, target, rng, height=1.45, crown=1.35, count=250):
    lean = Vector((rng.uniform(-0.1, 0.1), rng.uniform(-0.1, 0.1), 0.0))
    top = UP * height + lean
    wood = [pk.capsule_bm((0.0, 0.0, -0.15), top, 0.26, 0.16, segments=12, rings=4)]
    for k in range(5):
        out = around(k * 2.0 * math.pi / 5.0 + rng.uniform(-0.3, 0.3))
        wood.append(pk.capsule_bm(out * 0.06 + UP * 0.3, out * rng.uniform(0.42, 0.58) - UP * 0.06, 0.11, 0.035, segments=8, rings=3))
    center = UP * (height + 0.95) + lean
    radii = (crown * 0.8, crown * 0.76, crown * 0.56)
    for k in range(5):
        out = around(k * 2.0 * math.pi / 5.0 + rng.uniform(-0.4, 0.4))
        start = top - UP * rng.uniform(0.0, 0.3)
        end = center + Vector((out.x * radii[0], out.y * radii[1], rng.uniform(-0.2, 0.3) * radii[2])) * 0.78
        wood.append(pk.capsule_bm(start, end, 0.1, 0.035, segments=8, rings=3))
    wood.append(pk.capsule_bm(top, center + UP * 0.35, 0.12, 0.05, segments=8, rings=3))
    trunk = trim(pk.sculpt(wood, target, m['bark'], f'Oak{key}_Trunk', voxel=0.025, smooth=4, smooth_factor=0.5, faces=1800, rng=rng), -0.05)
    inner = (radii[0] * 0.78, radii[1] * 0.78, radii[2] * 0.78)
    core_shapes = [pk.ellipsoid_bm(center, inner, segments=16, rings=10)] + scatter_clumps(center, inner, 16, (0.25, 0.4), rng)
    core = blob(core_shapes, target, m['canopy'], f'Oak{key}_Core', voxel=0.06, rng=rng, faces=900)
    leaves = cards(f'Oak{key}_Leaves', shell(center, radii, count, (0.6, 0.8), rng), target, m['cards'], ellipsoid_normal(center, radii), rng)
    return kit.join([trunk, core, leaves], f'Oak_{key}')


def frond_layer(z, reach, count, droop, rng, phase, width=0.5, origin=0.05):
    placements = []
    for j in range(count):
        theta = phase + j * 2.0 * math.pi / count + rng.uniform(-0.2, 0.2)
        out = around(theta)
        tilt = droop + rng.uniform(-0.1, 0.12)
        along = (out * math.cos(tilt) - UP * math.sin(tilt)).normalized()
        across = around(theta + 0.5 * math.pi)
        roll = rng.uniform(-0.35, 0.35)
        across = (across * math.cos(roll) + along.cross(across) * math.sin(roll)).normalized()
        length = reach * rng.uniform(0.9, 1.08)
        placements.append((UP * z + out * origin + along * (length * 0.5), across, along, length * width, length))
    return placements


def cone_normal(apex, bend=0.75):
    def normal_of(point, facing):
        radial = Vector((point.x, point.y, 0.0))
        outward = (radial.normalized() * 0.75 + UP * 0.66).normalized() if radial.length > 1e-4 else UP
        if facing.z < 0.0:
            facing = -facing
        return (outward * bend + facing * (1.0 - bend)).normalized()
    return normal_of


def pine_core(name, bottom, radius, height, rng, material, target, teeth=7):
    profile = [(0.0, bottom + height), (radius * 0.35, bottom + height * 0.6), (radius, bottom), (radius * 0.4, bottom + height * 0.08), (0.0, bottom + height * 0.18)]
    bm = pk.lathe(profile, teeth * 2)
    for vertex in bm.verts:
        r = math.hypot(vertex.co.x, vertex.co.y)
        if r > radius * 0.6:
            angle = math.atan2(vertex.co.y, vertex.co.x)
            vertex.co.x *= 1.0 + 0.12 * math.sin(teeth * angle)
            vertex.co.y *= 1.0 + 0.12 * math.sin(teeth * angle)
            vertex.co.z -= 0.05 + rng.uniform(0.0, 0.04)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return pk.part(name, bm, target, material, rng=rng, smooth=50.0)


def pine(key, m, target, rng, height=3.9, base=1.2, levels=7):
    wood = [pk.capsule_bm((0.0, 0.0, -0.15), (0.0, 0.0, height * 0.92), 0.19, 0.03, segments=10, rings=3)]
    for k in range(4):
        out = around(k * 0.5 * math.pi + rng.uniform(-0.3, 0.3))
        wood.append(pk.capsule_bm(out * 0.05 + UP * 0.22, out * rng.uniform(0.32, 0.42) - UP * 0.05, 0.08, 0.03, segments=8, rings=3))
    parts = [trim(pk.sculpt(wood, target, m['bark'], f'Pine{key}_Trunk', voxel=0.022, smooth=3, smooth_factor=0.5, faces=900, rng=rng), -0.05)]
    placements = []
    for level in range(levels):
        f = level / (levels - 1)
        z = 0.75 + f * (height - 1.35)
        reach = base * (1.0 - 0.8 * f) + 0.22
        placements += frond_layer(z, reach, round(10 - 4.0 * f), 0.42 - 0.18 * f, rng, rng.uniform(0.0, 6.28))
        placements += frond_layer(z + 0.14, reach * 0.86, round(8 - 3.0 * f), 0.3, rng, rng.uniform(0.0, 6.28))
        placements += frond_layer(z + 0.28, reach * 0.66, round(6 - 2.0 * f), 0.12, rng, rng.uniform(0.0, 6.28))
        if level < levels - 1:
            parts.append(pine_core(f'Pine{key}_Core{level}', z - 0.05, reach * 0.4, 0.55, rng, m['needles'], target))
    placements += frond_layer(height - 0.55, 0.55, 4, -0.9, rng, rng.uniform(0.0, 6.28), width=0.55, origin=0.0)
    parts.append(cards(f'Pine{key}_Fronds', placements, target, m['fronds'], cone_normal(UP * height), rng))
    return kit.join(parts, f'Pine_{key}')


def bush(key, m, target, rng, radius=0.6, height=0.72, lobes=6, count=64):
    center = UP * height * 0.45
    radii = (radius * 0.85, radius * 0.8, height * 0.52)
    inner = (radii[0] * 0.82, radii[1] * 0.82, radii[2] * 0.82)
    shapes = [pk.ellipsoid_bm(center, inner, segments=14, rings=8)]
    for k in range(lobes):
        out = around(k * 2.0 * math.pi / lobes + rng.uniform(-0.3, 0.3))
        r = rng.uniform(0.2, 0.3)
        shapes.append(pk.ellipsoid_bm(center + Vector((out.x * inner[0], out.y * inner[1], 0.0)) * 0.55 - UP * 0.05, (r, r, r * 0.8), segments=12, rings=7))
    core = blob(shapes, target, m['canopy'], f'Bush{key}_Core', voxel=0.04, rng=rng, lumps=0.03, scale=5.0, faces=600, floor=-0.03)
    leaves = cards(f'Bush{key}_Leaves', shell(center, radii, count, (0.4, 0.52), rng, low=-0.15), target, m['cards'], ellipsoid_normal(center, radii), rng)
    return kit.join([core, leaves], f'Bush_{key}')


def roots(key, m, target, rng, strands=6, length=2.2, spread=0.28):
    parts = []
    for k in range(strands):
        start = Vector((rng.uniform(-spread, spread), rng.uniform(0.0, 0.14), rng.uniform(-0.06, 0.04)))
        reach = length * rng.uniform(0.45, 1.1)
        drift = rng.uniform(-0.18, 0.18)
        points = [start, start + Vector((drift * 0.3, -rng.uniform(0.16, 0.3), -0.12 * reach))]
        for i in range(2, 6):
            f = i / 5.0
            last = points[-1]
            points.append(Vector((last.x + drift * 0.25 + rng.uniform(-0.07, 0.07), last.y - rng.uniform(0.0, 0.06), -f * reach)))
        radius = rng.uniform(0.035, 0.06)
        parts.append(pk.tube(points, radius, target, m['bark'], f'Roots{key}_Strand{k}', resolution=5, sides=7, radii=[1.0, 0.8, 0.62, 0.45, 0.28, 0.1], rng=rng))
        for b in range(rng.randint(0, 2)):
            fork = points[rng.randint(2, 4)]
            twig = [fork, fork + Vector((rng.uniform(-0.25, 0.25), rng.uniform(-0.12, 0.02), -rng.uniform(0.15, 0.3))), fork + Vector((rng.uniform(-0.4, 0.4), rng.uniform(-0.15, 0.0), -rng.uniform(0.4, 0.8)))]
            parts.append(pk.tube(twig, radius * 0.45, target, m['bark'], f'Roots{key}_Twig{k}_{b}', resolution=4, sides=5, radii=[1.0, 0.55, 0.12], rng=rng))
    knot = pk.sculpt([pk.ellipsoid_bm((0.0, 0.06, -0.02), (spread + 0.12, 0.16, 0.12))] + [pk.ellipsoid_bm((rng.uniform(-spread, spread), rng.uniform(0.0, 0.1), rng.uniform(-0.08, 0.02)), (0.1, 0.09, 0.09)) for _ in range(4)], target, m['bark'], f'Roots{key}_Knot', voxel=0.02, smooth=3, faces=500, rng=rng)
    parts.append(knot)
    return kit.join(parts, f'Roots_{key}')


def blossom(name, head, facing, radius, petal, heart, target, rng, petals=5):
    parts = []
    turn = Matrix.Translation(head) @ facing.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    for k in range(petals):
        angle = k * 2.0 * math.pi / petals + rng.uniform(-0.12, 0.12)
        shape = pk.ellipsoid_bm((0.0, 0.0, 0.0), (radius * 0.56, radius * 0.34, radius * 0.1), segments=8, rings=4)
        matrix = turn @ Matrix.Rotation(angle, 4, 'Z') @ Matrix.Translation((radius * 0.5, 0.0, 0.0)) @ Matrix.Rotation(-0.35, 4, 'Y')
        parts.append(pk.part(f'{name}_Petal{k}', shape, target, petal, matrix=matrix, rng=rng, smooth=70.0))
    parts.append(pk.part(f'{name}_Heart', pk.ellipsoid_bm((0.0, 0.0, radius * 0.06), (radius * 0.3, radius * 0.3, radius * 0.2), segments=8, rings=5), target, heart, matrix=turn, rng=rng, smooth=70.0))
    return parts


def flowers(key, palette, m, target, rng, heads=10, spread=0.3):
    parts = []
    for k in range(8):
        angle = k * 2.0 * math.pi / 8.0 + rng.uniform(-0.2, 0.2)
        length = rng.uniform(0.13, 0.19)
        leaf = pk.ellipsoid_bm((0.0, 0.0, 0.0), (length, length * 0.34, 0.014), segments=10, rings=6)
        matrix = Matrix.Translation(around(angle) * length * 0.78 + UP * 0.03) @ Matrix.Rotation(angle, 4, 'Z') @ Matrix.Rotation(-0.3, 4, 'Y')
        parts.append(pk.part(f'Flowers{key}_Leaf{k}', leaf, target, m['stems'], matrix=matrix, rng=rng, smooth=60.0))
    for k in range(16):
        out = around(rng.uniform(0.0, 2.0 * math.pi))
        length = rng.uniform(0.16, 0.28)
        points = [out * 0.03, out * length * 0.3 + UP * length * 0.6, out * length * 0.72 + UP * length * 0.82]
        parts.append(pk.tube(points, 0.009, target, m['stems'], f'Flowers{key}_Blade{k}', resolution=3, sides=4, radii=[1.0, 0.75, 0.15], rng=rng))
    for k in range(heads):
        out = around(rng.uniform(0.0, 2.0 * math.pi))
        root = out * rng.uniform(0.0, spread * 0.45)
        head = root + out * rng.uniform(0.02, spread * 0.4) + UP * rng.uniform(0.16, 0.32)
        parts.append(pk.part(f'Flowers{key}_Stem{k}', pk.rod(root, head, 0.006, 5, end_radius=0.004), target, m['stems'], rng=rng, smooth=50.0))
        facing = (UP + out * rng.uniform(0.15, 0.45)).normalized()
        parts += blossom(f'Flowers{key}_Head{k}', head, facing, rng.uniform(0.05, 0.066), m[palette[k % len(palette)]], m['heart'], target, rng)
    return kit.join(parts, f'Flowers_{key}')


def mushroom(name, root, height, cap, lean, m, target, rng):
    stem = pk.lathe([(0.0, -0.01), (cap * 0.32, -0.01), (cap * 0.24, 0.03), (cap * 0.2, height * 0.6), (cap * 0.19, height), (0.0, height)], 10)
    for vertex in stem.verts:
        vertex.co += lean * (max(vertex.co.z, 0.0) / height) ** 2
    bmesh.ops.translate(stem, vec=root, verts=stem.verts)
    top = Vector(root) + lean + UP * height
    hat = pk.lathe([(0.0, cap * 0.55), (cap * 0.55, cap * 0.46), (cap * 0.9, cap * 0.2), (cap, -cap * 0.04), (cap * 0.92, -cap * 0.1), (cap * 0.5, -cap * 0.03), (0.0, 0.0)], 16)
    tilt = (UP + lean * 2.0).normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.transform(hat, matrix=Matrix.Translation(top) @ tilt, verts=hat.verts)
    return [pk.part(name + '_Stem', stem, target, m['stem'], rng=rng, smooth=50.0), pk.part(name + '_Cap', hat, target, m['cap'], rng=rng, smooth=50.0)]


def mushrooms(key, spec, m, target, rng):
    parts = []
    for index, (angle, distance, height, cap) in enumerate(spec):
        out = around(angle)
        lean = out * rng.uniform(0.01, 0.04) * (1.0 + distance * 8.0)
        parts += mushroom(f'Mushrooms{key}_{index}', out * distance, height, cap, lean, m, target, rng)
    return kit.join(parts, f'Mushrooms_{key}')


def leaf_layout():
    leaves = []
    for k in range(8):
        t = k / 7.0
        side = 1.0 if k % 2 == 0 else -1.0
        angle = side * (1.0 - 0.45 * t)
        length = 0.3 - 0.08 * t
        attach = Vector((0.0, -0.32 + 0.6 * t, 0.0))
        direction = Vector((math.sin(angle), math.cos(angle), 0.0))
        center = attach + direction * (length * 0.5)
        leaves.append((center.x, center.y, angle, length, length * 0.5))
    leaves.append((0.0, 0.38, 0.0, 0.2, 0.1))
    return leaves


def spray_material(name, light=(0.13, 0.22, 0.05), dark=(0.03, 0.07, 0.02), bake_size=2048):
    s = kit.Shader(name)
    u, v, w = s.separate(pk.local(s))
    cover = None
    dome = None
    vein = None
    for cx, cy, angle, length, width in leaf_layout():
        du, dv = s.sub(u, cx), s.sub(v, cy)
        along = s.mul(s.add(s.mul(du, math.sin(angle)), s.mul(dv, math.cos(angle))), 2.0 / length)
        across = s.sub(s.mul(du, math.cos(angle)), s.mul(dv, math.sin(angle)))
        spread = s.mul(s.sub(1.0, s.mul(along, 0.4)), width * 0.5)
        ratio = s.math('DIVIDE', across, spread)
        shape = s.add(s.mul(along, along), s.mul(ratio, ratio))
        mask = s.sub(1.0, s.smooth(0.82, 1.0, shape))
        rise = s.clamp01(s.sub(1.0, shape))
        line = s.mul(mask, s.sub(1.0, s.smooth(0.0, 0.1, s.math('ABSOLUTE', ratio))))
        cover = mask if cover is None else s.math('MAXIMUM', cover, mask)
        dome = rise if dome is None else s.math('MAXIMUM', dome, rise)
        vein = line if vein is None else s.math('MAXIMUM', vein, line)
    twig = s.mul(s.sub(1.0, s.smooth(0.008, 0.016, s.math('ABSOLUTE', s.sub(u, s.mul(s.math('SINE', s.mul(v, 4.0)), 0.025))))), s.sub(1.0, s.smooth(0.28, 0.32, v)))
    alpha = s.math('MAXIMUM', cover, twig)
    patch = s.noise(s.combine(s.mul(u, 2.0), s.mul(v, 2.0), s.mul(w, 9.0)), 1.6, detail=2.0)
    tone = s.clamp01(s.add(s.mul(patch, 0.55), s.mul(w, 0.45)))
    base = s.mix(tone, dark, light)
    base = s.mix(s.mul(s.smooth(0.72, 0.95, w), 0.55), base, (light[0] * 1.6, light[1] * 1.3, light[2] * 0.7))
    base = s.mix(s.mul(s.sub(1.0, s.smooth(0.0, 0.45, dome)), 0.5), base, (dark[0] * 0.7, dark[1] * 0.7, dark[2] * 0.7))
    base = s.mix(s.mul(vein, 0.35), base, (light[0] * 1.4, light[1] * 1.35, light[2] * 1.1))
    base = s.mix(s.mul(twig, s.sub(1.0, cover)), base, (0.09, 0.065, 0.04))
    base = s.mix(s.mul(s.smooth(0.15, -0.5, v), 0.5), base, (dark[0] * 0.45, dark[1] * 0.45, dark[2] * 0.45))
    height = s.sub(s.mul(dome, 0.8), s.mul(vein, 0.3))
    material = s.finish(base, s.remap(patch, 0.0, 1.0, 0.52, 0.7), height=height, bump=0.6, bump_distance=0.004, specular=0.45, bake_size=bake_size, alpha=alpha)
    material['keep_uv'] = True
    material['no_ao'] = True
    material['slots'] = 8
    return material


def frond_material(name, light=(0.05, 0.115, 0.07), dark=(0.007, 0.022, 0.016), bake_size=2048):
    s = kit.Shader(name)
    u, v, w = s.separate(pk.local(s))
    t = s.add(v, 0.5)
    envelope = s.math('POWER', s.clamp01(s.math('SINE', s.mul(t, math.pi))), 0.55)
    reach = s.add(s.mul(envelope, s.sub(0.48, s.mul(t, 0.17))), 0.001)
    side = s.math('ABSOLUTE', u)
    ratio = s.math('DIVIDE', side, reach)
    inner = s.sub(1.0, s.smooth(0.6, 0.72, ratio))
    strand = s.math('ABSOLUTE', s.sub(s.math('FRACT', s.mul(s.sub(v, s.mul(side, 0.9)), 28.0)), 0.5))
    taper = s.sub(1.0, s.mul(s.smooth(0.5, 1.0, ratio), 0.75))
    needles = s.mul(s.sub(1.0, s.smooth(s.mul(taper, 0.26), s.mul(taper, 0.36), strand)), s.sub(1.0, s.smooth(0.92, 1.0, ratio)))
    stem = s.mul(s.sub(1.0, s.smooth(0.009, 0.018, side)), s.sub(1.0, s.smooth(0.94, 0.99, t)))
    alpha = s.math('MAXIMUM', s.math('MAXIMUM', inner, needles), stem)
    ridge = s.sub(1.0, s.smooth(0.0, 0.5, strand))
    depth = s.sub(1.0, s.smooth(0.2, 0.7, ratio))
    tone = s.clamp01(s.add(s.mul(ridge, 0.35), s.add(s.mul(w, 0.3), s.mul(s.sub(1.0, depth), 0.35))))
    base = s.mix(tone, dark, light)
    base = s.mix(s.mul(s.smooth(0.72, 1.0, t), 0.5), base, (light[0] * 1.5, light[1] * 1.45, light[2] * 1.05))
    base = s.mix(s.mul(stem, 0.7), base, (0.08, 0.06, 0.04))
    base = s.mix(s.mul(s.sub(1.0, s.smooth(0.0, 0.45, t)), 0.5), base, (dark[0] * 0.45, dark[1] * 0.45, dark[2] * 0.45))
    material = s.finish(base, s.remap(ridge, 0.0, 1.0, 0.78, 0.6), height=ridge, bump=0.45, bump_distance=0.004, bake_size=bake_size, alpha=alpha)
    material['keep_uv'] = True
    material['no_ao'] = True
    material['slots'] = 8
    return material


def grass_material(name, light=(0.15, 0.23, 0.055), dark=(0.025, 0.06, 0.018), bake_size=1024):
    s = kit.Shader(name)
    u, v, w = s.separate(pk.local(s))
    t = s.add(v, 0.5)
    layout = random.Random(5)
    cover = None
    for _ in range(26):
        root = layout.uniform(-0.42, 0.42)
        lean = layout.uniform(-0.32, 0.32)
        top = layout.uniform(0.45, 0.97)
        width = layout.uniform(0.024, 0.042)
        center = s.add(s.mul(s.mul(t, t), lean), root)
        reach = s.math('MAXIMUM', s.mul(s.sub(1.0, s.math('DIVIDE', t, top)), width), 0.0001)
        mask = s.sub(1.0, s.smooth(s.mul(reach, 0.7), reach, s.math('ABSOLUTE', s.sub(u, center))))
        cover = mask if cover is None else s.math('MAXIMUM', cover, mask)
    patch = s.noise(s.combine(s.mul(u, 3.0), s.mul(w, 7.0), 0.0), 2.0, detail=2.0)
    tone = s.clamp01(s.add(s.mul(t, 0.55), s.add(s.mul(w, 0.25), s.mul(patch, 0.25))))
    base = s.mix(tone, dark, light)
    base = s.mix(s.mul(s.smooth(0.55, 1.0, t), 0.45), base, (light[0] * 1.7, light[1] * 1.35, light[2] * 0.7))
    material = s.finish(base, 0.62, height=t, bump=0.1, bump_distance=0.004, bake_size=bake_size, alpha=cover)
    material['keep_uv'] = True
    material['no_ao'] = True
    material['slots'] = 4
    return material


def grass(key, m, target, rng, count=3, width=0.42, height=0.3):
    placements = []
    phase = rng.uniform(0.0, math.pi)
    for k in range(count):
        theta = phase + k * math.pi / count + rng.uniform(-0.2, 0.2)
        tilt = rng.uniform(-0.18, 0.18)
        along = (UP * math.cos(tilt) + around(theta + 0.5 * math.pi) * math.sin(tilt)).normalized()
        w = width * rng.uniform(0.85, 1.15)
        h = height * rng.uniform(0.85, 1.15)
        placements.append((UP * (h * 0.5 - 0.02) + around(rng.uniform(0.0, 2.0 * math.pi)) * rng.uniform(0.0, 0.05), around(theta), along, w, h))
    return cards(f'Grass_{key}', placements, target, m['grass'], lambda point, facing: (UP * 0.8 + facing * 0.2).normalized(), rng)


def bark_material(name, tone=(0.14, 0.105, 0.08), bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(s.coords('Object'))
    ridges = s.noise(s.stretch(p, (13.0, 13.0, 1.4)), 1.0, detail=5.0, roughness=0.6, distortion=1.2)
    cracks = s.sub(1.0, s.smooth(0.0, 0.05, s.math('ABSOLUTE', s.sub(s.noise(s.stretch(p, (7.0, 7.0, 0.9)), 1.0, detail=3.0, distortion=0.6), 0.5))))
    base = s.mix(s.smooth(0.3, 0.72, ridges), (tone[0] * 0.5, tone[1] * 0.5, tone[2] * 0.5), tone)
    base = s.mix(s.mul(s.smooth(0.55, 0.8, s.noise(p, 3.0, detail=3.0)), 0.35), base, (0.19, 0.18, 0.16))
    base = s.mix(s.mul(cracks, 0.6), base, (0.03, 0.022, 0.015))
    foot = s.mul(s.sub(1.0, s.smooth(0.05, 0.45, z)), s.smooth(0.4, 0.6, s.noise(p, 4.0, detail=4.0)))
    base = s.mix(s.mul(foot, 0.75), base, (0.05, 0.08, 0.022))
    dirt = pk.cavity(s, 0.06)
    base = s.mix(s.mul(dirt, 0.6), base, (0.02, 0.015, 0.01))
    height = s.sub(s.mul(ridges, 0.8), s.mul(cracks, 0.5))
    return s.finish(base, s.remap(ridges, 0.3, 0.7, 0.8, 0.94), height=height, bump=0.45, bump_distance=0.012, bake_size=bake_size)


def rock_material(name, light=(0.2, 0.19, 0.175), dark=(0.065, 0.062, 0.058), moss=0.3, cover=0.5, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    normal_z = s.separate(s.geometry('Normal'))[2]
    large = s.noise(p, 1.3, detail=3.0)
    mid = s.noise(p, 5.0, detail=5.0, roughness=0.55)
    fine = s.noise(p, 40.0, detail=2.0)
    island = s.geometry('Random Per Island')
    tone = s.madd(s.smooth(0.3, 0.7, large), 0.45, s.add(s.mul(mid, 0.3), s.mul(island, 0.2)))
    base = s.mix(tone, dark, light)
    warm = s.smooth(0.55, 0.75, s.noise(p, 2.2, detail=3.0))
    base = s.mix(s.mul(warm, 0.35), base, (light[0] * 1.05, light[1] * 0.92, light[2] * 0.78))
    lichen = s.mul(s.smooth(0.64, 0.7, s.noise(p, 9.0, detail=3.0)), 0.45)
    base = s.mix(lichen, base, (0.36, 0.37, 0.3))
    streaks = s.smooth(0.5, 0.72, s.noise(s.stretch(p, (14.0, 14.0, 0.8)), 1.0, detail=3.0, roughness=0.6))
    base = s.mix(s.mul(streaks, 0.3), base, (dark[0] * 0.7, dark[1] * 0.7, dark[2] * 0.65))
    dirt = pk.cavity(s, 0.12)
    base = s.mix(s.mul(dirt, 0.7), base, (0.025, 0.024, 0.02))
    edge = pk.wear(s, 0.03)
    base = s.mix(s.mul(edge, 0.35), base, (light[0] * 1.25, light[1] * 1.25, light[2] * 1.2))
    rough = s.remap(fine, 0.3, 0.7, 0.76, 0.92)
    height = s.add(s.mul(mid, 0.8), s.mul(fine, 0.06))
    if moss > 0.0:
        up = s.smooth(1.0 - cover - 0.2, 1.0 - cover + 0.15, normal_z)
        patches = s.smooth(0.4, 0.56, s.add(s.noise(p, 2.4, detail=5.0, roughness=0.6), s.mul(s.sub(normal_z, 0.6), 0.25)))
        amount = s.mul(s.mul(up, patches), moss)
        tuft = s.noise(p, 30.0, detail=4.0)
        moss_tone = s.mix(s.add(s.mul(tuft, 0.6), s.mul(large, 0.4)), (0.03, 0.05, 0.012), (0.12, 0.17, 0.035))
        base = s.mix(amount, base, moss_tone)
        rough = s.lerp(amount, rough, 0.95)
        height = s.lerp(amount, height, s.add(0.45, s.mul(tuft, 0.4)))
    return s.finish(base, rough, height=height, bump=0.3, bump_distance=0.012, bake_size=bake_size)


def canopy_material(name, light=(0.03, 0.06, 0.016), dark=(0.005, 0.014, 0.005), bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    cells = s.voronoi(p, 16.0, 'F1', 1.0, 'Distance')
    patch = s.noise(p, 3.0, detail=4.0)
    tone = s.clamp01(s.add(s.mul(patch, 0.6), s.mul(s.sub(1.0, cells), 0.3)))
    base = s.mix(tone, dark, light)
    dirt = pk.cavity(s, 0.25)
    base = s.mix(s.mul(dirt, 0.8), base, (0.004, 0.01, 0.004))
    return s.finish(base, 0.7, height=s.sub(1.0, cells), bump=0.4, bump_distance=0.01, bake_size=bake_size)


def petal_material(name, tone, glow=0.0, bake_size=256):
    s = kit.Shader(name)
    p = pk.local(s)
    veins = s.wave(p, 140.0, kind='BANDS', direction='X', distortion=2.0, detail=1.0)
    vary = s.noise(p, 0.6, detail=1.0)
    base = s.mix(s.mul(vary, 0.5), (tone[0] * 0.8, tone[1] * 0.75, tone[2] * 0.75), (min(1.0, tone[0] * 1.15), min(1.0, tone[1] * 1.15), min(1.0, tone[2] * 1.15)))
    base = s.mix(s.mul(veins, 0.15), base, (tone[0] * 0.6, tone[1] * 0.55, tone[2] * 0.55))
    dirt = pk.cavity(s, 0.01)
    base = s.mix(s.mul(dirt, 0.5), base, (tone[0] * 0.3, tone[1] * 0.25, tone[2] * 0.25))
    emission = 0.6 if glow > 0.0 else None
    return s.finish(base, 0.55, height=veins, bump=0.05, bump_distance=0.001, emission=emission, emission_color=tone, emission_strength=glow, bake_size=bake_size)


def stem_material(name, light=(0.12, 0.22, 0.05), dark=(0.035, 0.08, 0.02), bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    streak = s.noise(s.stretch(p, (40.0, 40.0, 4.0)), 1.0, detail=3.0)
    up = s.smooth(-0.2, 0.9, s.separate(s.geometry('Normal'))[2])
    base = s.mix(s.clamp01(s.add(s.mul(streak, 0.5), s.mul(up, 0.5))), dark, light)
    dirt = pk.cavity(s, 0.03)
    base = s.mix(s.mul(dirt, 0.6), base, (0.01, 0.02, 0.006))
    return s.finish(base, 0.6, height=streak, bump=0.2, bump_distance=0.002, bake_size=bake_size)


def cap_material(name, tone=(0.02, 0.07, 0.1), glow=(0.25, 0.95, 0.85), strength=2.2, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    normal_z = s.separate(s.geometry('Normal'))[2]
    under = s.smooth(0.0, -0.5, normal_z)
    spots = s.sub(1.0, s.smooth(0.08, 0.14, s.voronoi(p, 70.0, 'F1', 1.0, 'Distance')))
    sparse = s.smooth(0.45, 0.6, s.noise(p, 20.0, detail=2.0))
    rim = s.mul(pk.wear(s, 0.008), 0.6)
    mottled = s.noise(p, 30.0, detail=3.0)
    base = s.mix(mottled, tone, (tone[0] * 1.8, tone[1] * 1.6, tone[2] * 1.5))
    light = s.clamp01(s.add(s.mul(s.mul(spots, sparse), s.sub(1.0, under)), s.add(s.mul(under, 0.85), rim)))
    base = s.mix(light, base, glow)
    return s.finish(base, 0.4, height=s.mul(spots, sparse), bump=0.15, bump_distance=0.002, emission=light, emission_color=glow, emission_strength=strength, bake_size=bake_size)


def build(target):
    rng = random.Random(83)
    m = {
        'bark': bark_material('Bark', bake_size=1024),
        'cards': spray_material('LeafCards', bake_size=2048),
        'canopy': canopy_material('Canopy', bake_size=512),
        'fronds': frond_material('FrondCards', bake_size=2048),
        'needles': canopy_material('PineCore', light=(0.02, 0.045, 0.03), dark=(0.004, 0.012, 0.009), bake_size=512),
        'stone': rock_material('Stone', moss=0.8, cover=0.22, bake_size=1024),
        'moss': rock_material('MossStone', light=(0.18, 0.175, 0.16), dark=(0.055, 0.053, 0.05), moss=1.0, cover=0.75, bake_size=1024),
        'stems': stem_material('Stems', bake_size=512),
        'ember': petal_material('PetalEmber', (0.78, 0.17, 0.07)),
        'gold': petal_material('PetalGold', (0.88, 0.6, 0.12)),
        'violet': petal_material('PetalViolet', (0.46, 0.24, 0.78), glow=1.2),
        'white': petal_material('PetalWhite', (0.86, 0.85, 0.8)),
        'heart': pk.glow_material('FlowerHeart', color=(1.0, 0.7, 0.25), strength=0.6, base=(0.85, 0.55, 0.12), bake_size=128),
        'cap': cap_material('MushroomCap'),
        'stem': pk.wax_material('MushroomStem', tone=(0.6, 0.66, 0.62), bake_size=256),
        'grass': grass_material('GrassCards', bake_size=1024),
    }
    trees = [oak('A', m, target, rng), oak('B', m, target, rng, height=1.6, crown=1.22, count=220), pine('A', m, target, rng), pine('B', m, target, rng, height=3.4, base=1.05, levels=6)]
    shrubs = [bush('A', m, target, rng), bush('B', m, target, rng, radius=0.5, height=0.6, lobes=5, count=48), bush('C', m, target, rng, radius=0.7, height=0.8, lobes=7, count=80)]
    shrubs += [roots('A', m, target, rng), roots('B', m, target, rng, strands=4, length=1.6, spread=0.2), roots('C', m, target, rng, strands=8, length=2.8, spread=0.36)]
    stones = [
        rock('Rock_A', (0.4, 0.3, 'stone', 9), m, target, rng),
        rock('Rock_B', (0.36, 0.34, 'stone', 10), m, target, rng),
        rock('Rock_C', (0.42, 0.26, 'stone', 8), m, target, rng),
        rock('Rock_D', (0.34, 0.3, 'stone', 9), m, target, rng),
        rock('Boulder_A', (0.82, 0.55, 'stone', 10), m, target, rng),
        rock_pair('Boulder_B', (0.72, 0.6, 'stone', 9), m, target, rng),
        rock('Boulder_C', (0.85, 0.48, 'stone', 11), m, target, rng),
        rock('MossBoulder_A', (0.98, 0.62, 'moss', 9), m, target, rng),
        rock_pair('MossBoulder_B', (0.9, 0.66, 'moss', 10), m, target, rng),
    ]
    blooms = [
        flowers('A', ['ember', 'gold'], m, target, rng),
        flowers('B', ['violet', 'white'], m, target, rng),
        flowers('C', ['white', 'violet', 'gold'], m, target, rng, heads=11),
        mushrooms('A', [(0.0, 0.0, 0.24, 0.13), (2.2, 0.17, 0.16, 0.09), (4.1, 0.15, 0.19, 0.1), (5.3, 0.22, 0.09, 0.055)], m, target, rng),
        mushrooms('B', [(0.5, 0.05, 0.14, 0.08), (1.6, 0.12, 0.1, 0.06), (2.9, 0.1, 0.12, 0.07), (3.9, 0.17, 0.07, 0.045), (5.0, 0.13, 0.09, 0.05), (6.0, 0.2, 0.06, 0.04)], m, target, rng),
        grass('A', m, target, rng),
        grass('B', m, target, rng, count=4, width=0.5, height=0.38),
        grass('C', m, target, rng, width=0.48, height=0.22),
    ]
    return trees, shrubs, stones, blooms


def stage(groups, active, spacing, columns, folder, name):
    for group in groups:
        for obj in group:
            obj.matrix_world = Matrix.Translation((0.0, 200.0, 0.0))
    for index, obj in enumerate(active):
        obj.matrix_world = Matrix.Translation(((index % columns) * spacing - (columns - 1) * spacing * 0.5, (index // columns) * spacing, 0.0))
    pk.preview(active, folder, name, fill=1.0)


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    trees, shrubs, stones, blooms = build(target)
    objects = trees + shrubs + stones + blooms
    pk.unwrap_all(objects, {'PetalEmber': 0.01, 'PetalGold': 0.01, 'PetalViolet': 0.01, 'PetalWhite': 0.01, 'FlowerHeart': 0.02, 'MushroomCap': 0.01, 'MushroomStem': 0.01, 'Stems': 0.006})
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.5)
    if args.get('--preview'):
        pk.ground_preview(16.0)
        groups = (trees, shrubs + stones, blooms)
        stage(groups, trees, 3.0, 4, args['--preview'], 'nature_trees')
        stage(groups, shrubs + stones, 2.2, 4, args['--preview'], 'nature_stones')
        stage(groups, blooms, 0.8, 5, args['--preview'], 'nature_blooms')


if __name__ == '__main__':
    main()
