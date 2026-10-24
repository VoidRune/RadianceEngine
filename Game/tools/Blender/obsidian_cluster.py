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

NAME = 'ObsidianCluster'
WATERLINE = 0.0
BOTTOM = -0.45


def obsidian_material():
    s = kit.Shader('Obsidian')
    p = s.coords('Object')
    warp = s.noise(p, 1.6, detail=3.0, distortion=0.8)
    bands = s.wave(s.stretch(p, (0.6, 0.6, 1.0)), 1.4, kind='BANDS', direction='Z', distortion=6.0, detail=3.0, detail_scale=1.4)
    tone = s.madd(bands, 0.65, s.mul(warp, 0.35))
    base = s.mix(s.smooth(0.3, 0.95, tone), (0.009, 0.0095, 0.013), (0.022, 0.021, 0.03))
    sheen = s.mul(s.smooth(0.86, 0.98, bands), 0.55)
    base = s.mix(sheen, base, (0.028, 0.019, 0.010))
    smudge = s.noise(p, 3.2, detail=4.0, roughness=0.6)
    rough = s.remap(smudge, 0.3, 0.75, 0.045, 0.14)
    warped = s.offset(p, s.mul(s.noise_color(p, 3.0, detail=2.0), 0.18))
    cells = s.voronoi(warped, 1.9, 'F1', 1.0, 'Distance')
    ripples = s.math('SINE', s.mul(cells, 44.0))
    fade = s.sub(1.0, s.smooth(0.05, 0.45, cells))
    micro = s.noise(p, 38.0, detail=2.0)
    height = s.add(s.mul(s.mul(ripples, fade), 0.18), s.mul(micro, 0.05))
    fissures = s.voronoi(s.stretch(warped, (1.0, 1.0, 0.45)), 2.6, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    lines = s.sub(1.0, s.smooth(0.0, 0.02, fissures))
    breakup = s.smooth(0.4, 0.55, s.noise(p, 2.1, detail=2.0))
    _, _, z = s.separate(p)
    rising = s.sub(1.0, s.smooth(0.0, 1.4, z))
    veins = s.mul(s.mul(lines, breakup), rising)
    base = s.mix(veins, base, (0.08, 0.02, 0.006))
    return s.finish(base, rough, height=height, bump=0.1, bump_distance=0.004, emission=veins, emission_color=(1.0, 0.38, 0.08), emission_strength=3.5, bake_size=2048)


def basalt_material():
    s = kit.Shader('Basalt')
    p = s.coords('Object')
    large = s.noise(p, 2.2, detail=4.0, roughness=0.55)
    pores = s.voronoi(p, 24.0, 'F1', 1.0, 'Distance')
    pits = s.smooth(0.02, 0.3, pores)
    grit = s.noise(p, 55.0, detail=1.0)
    warped = s.offset(p, s.mul(s.noise_color(p, 2.5, detail=3.0), 0.25))
    crack_distance = s.voronoi(warped, 2.3, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    width = s.remap(s.noise(p, 4.0, detail=2.0), 0.3, 0.7, 0.008, 0.04)
    crack = s.sub(1.0, s.smooth(0.0, width, crack_distance))
    _, _, z = s.separate(p)
    heat = s.sub(1.0, s.smooth(-0.1, 0.12, z))
    breakup = s.smooth(0.42, 0.55, s.noise(p, 1.6, detail=2.0))
    glow = s.mul(s.mul(crack, heat), breakup)
    base = s.mix(s.smooth(0.3, 0.75, large), (0.024, 0.021, 0.019), (0.058, 0.051, 0.045))
    base = s.mix(s.mul(s.sub(1.0, pits), 0.65), base, (0.01, 0.009, 0.009))
    base = s.mix(s.mul(crack, 0.9), base, (0.05, 0.016, 0.008))
    base = s.mix(s.mul(heat, 0.55), base, (0.075, 0.03, 0.014))
    height = s.add(s.mul(pits, 0.15), s.add(s.mul(large, 0.8), s.mul(grit, 0.03)))
    height = s.sub(height, s.mul(crack, 0.5))
    rough = s.remap(grit, 0.3, 0.7, 0.8, 0.94)
    return s.finish(base, rough, height=height, bump=0.3, bump_distance=0.02, emission=glow, emission_color=(1.0, 0.3, 0.06), emission_strength=1.6, bake_size=1024)


def shard(rng, radius, height, sides, flatten, embed, style, chips):
    bm = bmesh.new()
    step = 2.0 * math.pi / sides
    angles = [i * step + rng.uniform(-0.3, 0.3) * step for i in range(sides)]
    radii = [radius * rng.uniform(0.7, 1.0) for _ in range(sides)]
    tip = rng.uniform(0.18, 0.32)
    taper = rng.uniform(0.78, 0.93)
    shaft = height * (1.0 - tip)
    shift = Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), 0.0)) * radius * 0.12

    def ring(z, scale, offset):
        return [bm.verts.new((math.cos(a) * r * scale + offset.x, math.sin(a) * r * scale * flatten + offset.y, z)) for a, r in zip(angles, radii)]

    bottom = ring(-embed, 1.0, Vector())
    top = ring(shaft, taper, shift)
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
    bm.faces.new(list(reversed(bottom)))
    heading = rng.uniform(0.0, 2.0 * math.pi)
    if style == 'chisel':
        spread = radius * taper * rng.uniform(0.35, 0.6)
        across = Vector((math.cos(heading), math.sin(heading) * flatten, 0.0))
        center = shift + Vector((-across.y, across.x, 0.0)) * radius * 0.15
        ends = [bm.verts.new((center.x + across.x * spread * side, center.y + across.y * spread * side, height - rng.uniform(0.0, 0.06) * height)) for side in (-1.0, 1.0)]
        ridge = []
        for i in range(sides):
            vertex = top[i].co
            ridge.append(ends[0] if (vertex.x - center.x) * across.x + (vertex.y - center.y) * across.y < 0.0 else ends[1])
        for i in range(sides):
            j = (i + 1) % sides
            if ridge[i] == ridge[j]:
                bm.faces.new((top[i], top[j], ridge[i]))
            else:
                bm.faces.new((top[i], top[j], ridge[j], ridge[i]))
        peak = (ends[0].co + ends[1].co) * 0.5
    else:
        apex = bm.verts.new((shift.x + math.cos(heading) * radius * rng.uniform(0.15, 0.5), shift.y + math.sin(heading) * radius * 0.4 * flatten, height))
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((top[i], top[j], apex))
        peak = apex.co.copy()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    lower = [vertex.co.copy() for vertex in bottom]
    upper = [vertex.co.copy() for vertex in top]

    if style == 'broken':
        tilt = math.radians(rng.uniform(25.0, 55.0))
        normal = Vector((math.cos(heading) * math.sin(tilt), math.sin(heading) * math.sin(tilt), math.cos(tilt)))
        kit.cut(bm, Vector((shift.x, shift.y, shaft * rng.uniform(0.82, 1.0))), normal)
    for _ in range(chips):
        if rng.random() < 0.3 and style != 'broken':
            tilt = math.radians(rng.uniform(35.0, 70.0))
            around = rng.uniform(0.0, 2.0 * math.pi)
            normal = Vector((math.cos(around) * math.sin(tilt), math.sin(around) * math.sin(tilt), math.cos(tilt)))
            point = peak - normal * (height * tip * rng.uniform(0.2, 0.45))
        else:
            index = rng.randrange(sides)
            along = lower[index].lerp(upper[index], rng.uniform(0.25, 0.95))
            outward = Vector((along.x, along.y, 0.0))
            if outward.length < 1e-4:
                continue
            outward.normalize()
            outward.rotate(Matrix.Rotation(rng.uniform(-0.4, 0.4), 3, 'Z'))
            rise = math.radians(rng.uniform(-6.0, 6.0))
            normal = outward * math.cos(rise) + Vector((0.0, 0.0, math.sin(rise)))
            point = along - normal * (radius * rng.uniform(0.12, 0.3))
        kit.cut(bm, point, normal.normalized())
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def rock(rng, radius, squash, top):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=3, radius=1.0)
    seed = Vector((rng.uniform(-40.0, 40.0), rng.uniform(-40.0, 40.0), rng.uniform(-40.0, 40.0)))
    middle = top - 0.45
    for vertex in bm.verts:
        direction = vertex.co.normalized()
        bulge = 1.0 + 0.2 * noise.fractal(direction * 1.3 + seed, 1.0, 2.0, 4) + 0.07 * noise.noise(direction * 3.8 + seed * 2.0)
        vertex.co = Vector((direction.x * radius * bulge, direction.y * radius * squash * bulge, middle + direction.z * 0.45 * bulge))
    kit.cut(bm, Vector((0.0, 0.0, BOTTOM)), Vector((0.0, 0.0, -1.0)))
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(4.0), verts=bm.verts[:], edges=bm.edges[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def chunk(rng, size):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=1, radius=1.0)
    seed = Vector((rng.uniform(-40.0, 40.0), rng.uniform(-40.0, 40.0), rng.uniform(-40.0, 40.0)))
    for vertex in bm.verts:
        direction = vertex.co.normalized()
        bulge = 1.0 + 0.3 * noise.noise(direction * 1.7 + seed)
        vertex.co = Vector((direction.x * size * bulge, direction.y * size * bulge * rng.uniform(0.7, 1.0), direction.z * size * 0.6 * bulge))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def surface_height(tree, x, y):
    hit = tree.ray_cast(Vector((x, y, 5.0)), Vector((0.0, 0.0, -1.0)))
    return hit[0].z if hit[0] is not None else 0.0


VARIANTS = {
    'A': dict(seed=101, rock=(0.72, 0.9, 0.16), chunks=3, shards=[
        (0.04, 0.0, 0.25, 2.25, 6.0, 6, 0.82, 'point'),
        (0.26, 125.0, 0.2, 1.5, 18.0, 5, 0.62, 'chisel'),
        (0.3, 235.0, 0.18, 1.2, 22.0, 6, 0.75, 'broken'),
        (0.22, 300.0, 0.15, 1.0, 26.0, 5, 0.55, 'point'),
        (0.42, 30.0, 0.12, 0.8, 34.0, 5, 0.6, 'chisel'),
        (0.48, 175.0, 0.1, 0.62, 40.0, 5, 0.7, 'point'),
        (0.5, 280.0, 0.1, 0.55, 44.0, 5, 0.58, 'broken'),
        (0.56, 80.0, 0.08, 0.42, 52.0, 5, 0.7, 'point'),
        (0.6, 210.0, 0.07, 0.34, 58.0, 4, 0.65, 'point'),
        (0.36, 345.0, 0.09, 0.5, 38.0, 5, 0.6, 'chisel'),
    ]),
    'B': dict(seed=211, rock=(0.7, 0.8, 0.15), chunks=4, shards=[
        (0.1, 150.0, 0.21, 1.55, 14.0, 6, 0.6, 'chisel'),
        (0.22, 40.0, 0.19, 1.3, 20.0, 5, 0.7, 'point'),
        (0.25, 260.0, 0.17, 1.1, 26.0, 6, 0.55, 'broken'),
        (0.18, 330.0, 0.15, 0.95, 30.0, 5, 0.75, 'point'),
        (0.42, 100.0, 0.12, 0.72, 38.0, 5, 0.6, 'chisel'),
        (0.45, 200.0, 0.11, 0.64, 42.0, 5, 0.68, 'point'),
        (0.5, 0.0, 0.1, 0.5, 48.0, 5, 0.6, 'broken'),
        (0.55, 290.0, 0.08, 0.4, 54.0, 4, 0.7, 'point'),
        (0.36, 230.0, 0.08, 0.44, 40.0, 5, 0.62, 'point'),
    ]),
    'C': dict(seed=307, rock=(0.62, 0.95, 0.12), chunks=3, shards=[
        (0.05, 0.0, 0.15, 0.95, 8.0, 6, 0.75, 'point'),
        (0.22, 20.0, 0.15, 0.9, 30.0, 6, 0.6, 'chisel'),
        (0.24, 95.0, 0.14, 0.82, 34.0, 5, 0.72, 'point'),
        (0.22, 165.0, 0.14, 0.88, 28.0, 6, 0.62, 'broken'),
        (0.25, 240.0, 0.15, 0.94, 32.0, 6, 0.7, 'point'),
        (0.24, 310.0, 0.12, 0.7, 40.0, 5, 0.58, 'chisel'),
        (0.48, 60.0, 0.08, 0.42, 52.0, 5, 0.7, 'point'),
        (0.5, 200.0, 0.085, 0.46, 50.0, 5, 0.66, 'point'),
        (0.52, 275.0, 0.07, 0.36, 58.0, 4, 0.7, 'broken'),
    ]),
}


def build_variant(key, spec, glass, crust, offset, target):
    rng = random.Random(spec['seed'])
    radius, squash, top = spec['rock']
    rock_bm = rock(rng, radius, squash, top)
    tree = BVHTree.FromBMesh(rock_bm)
    roots = []
    for distance, around, *_ in spec['shards']:
        angle = math.radians(around + rng.uniform(-8.0, 8.0))
        root = Vector((math.cos(angle) * distance * radius, math.sin(angle) * distance * radius * squash, 0.0))
        root.z = surface_height(tree, root.x, root.y) - 0.05
        roots.append((angle, root))
    base = kit.mesh_object(f'{NAME}_{key}_Rock', rock_bm, target, crust)
    kit.bevel(base, 0.025, segments=2, angle=35.0)
    parts = [base]
    for index in range(spec['chunks']):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        size = rng.uniform(0.1, 0.18)
        obj = kit.mesh_object(f'{NAME}_{key}_Chunk{index}', chunk(rng, size), target, crust)
        kit.bevel(obj, 0.018, segments=2, angle=35.0)
        distance = radius * rng.uniform(0.95, 1.15)
        obj.matrix_world = Matrix.Translation((math.cos(angle) * distance, math.sin(angle) * distance * squash, rng.uniform(-0.06, 0.0))) @ Matrix.Rotation(rng.uniform(0.0, 6.28), 4, 'Z')
        parts.append(obj)
    for index, ((distance, around, shard_radius, height, lean, sides, flatten, style), (angle, root)) in enumerate(zip(spec['shards'], roots)):
        bm = shard(rng, shard_radius, height, sides, flatten, embed=0.2 + 0.12 * shard_radius / 0.25, style=style, chips=rng.randint(2, 4) + (1 if height > 1.0 else 0))
        obj = kit.mesh_object(f'{NAME}_{key}_Shard{index}', bm, target, glass)
        kit.bevel(obj, max(0.01, shard_radius * 0.09), segments=2, angle=25.0)
        if distance < 0.06:
            angle = math.radians(rng.uniform(0.0, 360.0))
        outward = Vector((math.cos(angle), math.sin(angle), 0.0))
        axis = Vector((0.0, 0.0, 1.0)).cross(outward).normalized()
        tilt = Matrix.Rotation(math.radians(lean + rng.uniform(-4.0, 4.0)), 4, axis)
        spin = Matrix.Rotation(rng.uniform(0.0, 2.0 * math.pi), 4, 'Z')
        obj.matrix_world = Matrix.Translation(root) @ tilt @ spin
        parts.append(obj)
    for obj in parts:
        obj.matrix_world = Matrix.Translation(offset) @ obj.matrix_world
    return parts


def build(target):
    glass = obsidian_material()
    crust = basalt_material()
    variants = {}
    for index, (key, spec) in enumerate(VARIANTS.items()):
        variants[key] = build_variant(key, spec, glass, crust, Vector((index * 3.5, 0.0, 0.0)), target)
    return variants


def finalize(variants):
    glass = [obj for parts in variants.values() for obj in parts if 'Shard' in obj.name]
    stone = [obj for parts in variants.values() for obj in parts if 'Shard' not in obj.name]
    kit.unwrap(glass, angle=55.0, margin=0.003)
    kit.unwrap(stone, angle=55.0, margin=0.004)
    return [kit.join(parts, f'{NAME}_{key}') for key, parts in variants.items()]


def lava_plane(target):
    s = kit.Shader('PreviewLava')
    p = s.coords('Object')
    crust = s.voronoi(p, 0.9, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    hot = s.sub(1.0, s.smooth(0.0, 0.07, crust))
    glow = s.add(s.mul(hot, 0.9), 0.04)
    material = s.finish((0.03, 0.02, 0.015), 0.7, emission=glow, emission_color=(1.0, 0.32, 0.06), emission_strength=4.0)
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=7.0)
    plane = kit.mesh_object('PreviewLava', bm, target, material)
    plane.location = (3.5, 0.0, WATERLINE)
    return plane


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    options = dict(zip(argv[::2], argv[1::2]))
    output = options.get('--out')
    preview = options.get('--preview')
    scene = kit.reset()
    target = kit.collection('Export')
    variants = build(target)
    objects = finalize(variants)
    scene['ao_distance'] = 0.6
    if output:
        os.makedirs(output, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(output, NAME + '.blend'), compress=True)
        kit.bake_and_export(objects, output, NAME, ao_distance=scene['ao_distance'])
    if preview:
        os.makedirs(preview, exist_ok=True)
        scene.view_settings.view_transform = 'Standard'
        stage = kit.collection('Preview')
        lava_plane(stage)
        kit.environment('night.exr', strength=0.5)
        for obj in objects:
            key = obj.name[-1]
            kit.render(os.path.join(preview, f'obsidian_{key}_game.png'), [obj], yaw=30.0, pitch=57.0, fill=1.6)
            kit.render(os.path.join(preview, f'obsidian_{key}_close.png'), [obj], yaw=-25.0, pitch=16.0, fill=1.0)
        kit.environment('forest.exr', strength=1.0)
        kit.render(os.path.join(preview, 'obsidian_all_day.png'), objects, yaw=10.0, pitch=35.0, fill=0.95)


if __name__ == '__main__':
    main()
