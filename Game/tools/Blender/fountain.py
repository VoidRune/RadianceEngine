import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit

NAME = 'Fountain'
OCTAGON = math.cos(math.pi / 8.0)
BASIN_WATER = 0.47
BOWL_WATER = 1.262


def stone_material():
    s = kit.Shader('Stone')
    p = s.coords('Object')
    x, y, z = s.separate(p)
    radial = s.vector('LENGTH', s.stretch(p, (1.0, 1.0, 0.0)))
    large = s.noise(p, 1.1, detail=3.0)
    mid = s.noise(p, 4.5, detail=5.0, roughness=0.55)
    fine = s.noise(p, 38.0, detail=2.0)
    specks = s.sub(1.0, s.smooth(0.0, 0.14, s.voronoi(p, 85.0, 'F1', 1.0, 'Distance')))
    island = s.geometry('Random Per Island')
    tone = s.madd(s.smooth(0.3, 0.7, large), 0.45, s.add(s.mul(mid, 0.3), s.mul(island, 0.25)))
    base = s.mix(tone, (0.2, 0.185, 0.155), (0.38, 0.355, 0.31))
    base = s.mix(s.mul(s.smooth(0.55, 0.75, s.noise(p, 2.6, detail=3.0)), 0.3), base, (0.34, 0.27, 0.19))
    base = s.mix(s.mul(s.smooth(0.6, 0.9, island), 0.35), base, (0.28, 0.28, 0.27))
    base = s.mix(s.mul(specks, 0.3), base, (0.07, 0.066, 0.062))
    cavity = s.occlusion(0.12, samples=24)
    dirt = s.sub(1.0, s.smooth(0.35, 0.95, cavity))
    base = s.mix(s.mul(dirt, 0.7), base, (0.05, 0.045, 0.036))
    streaks = s.smooth(0.5, 0.72, s.noise(s.stretch(p, (14.0, 14.0, 0.8)), 1.0, detail=3.0, roughness=0.6))
    hanging = s.sub(1.0, s.smooth(0.3, 0.55, z))
    base = s.mix(s.mul(s.mul(streaks, hanging), 0.4), base, (0.07, 0.068, 0.058))
    grime = s.mul(s.sub(1.0, s.smooth(0.04, 0.3, z)), s.smooth(0.3, 0.6, s.noise(p, 3.5, detail=4.0)))
    base = s.mix(s.mul(grime, 0.55), base, (0.045, 0.05, 0.035))
    edges = s.occlusion(0.025, samples=24, inside=True)
    wear = s.sub(1.0, s.smooth(0.5, 0.9, edges))
    base = s.mix(s.mul(wear, 0.3), base, (0.36, 0.34, 0.3))
    basin = s.mul(s.smooth(1.25, 1.4, radial), s.sub(1.0, s.smooth(1.6, 1.68, radial)))
    soaked = s.mul(s.sub(1.0, s.smooth(BASIN_WATER - 0.01, BASIN_WATER + 0.09, z)), basin)
    algae = s.mul(s.mul(s.sub(1.0, s.smooth(0.0, 0.035, s.math('ABSOLUTE', s.sub(z, BASIN_WATER + 0.012)))), basin), s.smooth(0.35, 0.6, s.noise(p, 7.0, detail=3.0)))
    drip_noise = s.noise(s.stretch(p, (9.0, 9.0, 0.7)), 1.0, detail=3.0, roughness=0.6)
    under_bowl = s.mul(s.sub(1.0, s.smooth(0.82, 0.9, radial)), s.mul(s.smooth(0.55, 0.75, z), s.sub(1.0, s.smooth(1.22, 1.27, z))))
    drips = s.mul(s.smooth(0.52, 0.7, drip_noise), under_bowl)
    splash = s.mul(s.mul(s.smooth(1.5, 1.62, radial), s.sub(1.0, s.smooth(1.66, 1.75, radial))), s.mul(s.smooth(0.5, 0.62, z), s.smooth(0.45, 0.6, s.noise(p, 3.0, detail=2.0))))
    wet = s.clamp01(s.add(s.add(soaked, s.mul(drips, 0.8)), s.mul(splash, 0.6)))
    base = s.mix(s.mul(wet, 0.55), base, (0.1, 0.1, 0.09))
    base = s.mix(s.mul(algae, 0.75), base, (0.06, 0.08, 0.035))
    rough = s.remap(fine, 0.3, 0.7, 0.72, 0.88)
    rough = s.lerp(s.mul(wear, 0.4), rough, 0.92)
    rough = s.lerp(wet, rough, 0.38)
    chisel = s.wave(s.stretch(p, (1.0, 1.0, 0.4)), 14.0, kind='BANDS', direction='X', distortion=4.0, detail=2.0)
    pits = s.smooth(0.02, 0.18, s.voronoi(p, 30.0, 'F1', 1.0, 'Distance'))
    height = s.add(s.mul(mid, 0.8), s.add(s.mul(chisel, 0.1), s.add(s.mul(pits, 0.05), s.mul(fine, 0.03))))
    return s.finish(base, rough, height=height, bump=0.22, bump_distance=0.012, bake_size=2048)


def water_material():
    s = kit.Shader('Water')
    p = s.coords('Object')
    rings = s.wave(p, 4.0, kind='RINGS', direction='SPHERICAL', distortion=3.5, detail=3.0, detail_scale=1.5)
    chop = s.noise(p, 6.0, detail=4.0, roughness=0.55)
    height = s.add(s.mul(rings, 0.16), s.mul(chop, 0.84))
    radial = s.vector('LENGTH', s.stretch(p, (1.0, 1.0, 0.0)))
    depth = s.mul(s.smooth(0.25, 1.1, radial), s.sub(1.0, s.smooth(1.25, 1.6, radial)))
    base = s.mix(depth, (0.03, 0.075, 0.075), (0.018, 0.05, 0.056))
    warped = s.offset(p, s.mul(s.noise_color(p, 2.0, detail=2.0), 0.3))
    caustic = s.sub(1.0, s.smooth(0.0, 0.05, s.voronoi(warped, 7.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
    base = s.mix(s.mul(caustic, 0.35), base, (0.09, 0.17, 0.15))
    return s.finish(base, 0.035, height=height, bump=0.22, bump_distance=0.01, bake_size=512)


def ember_material():
    s = kit.Shader('Ember')
    p = s.coords('Object')
    grain = s.noise(p, 16.0, detail=3.0)
    base = s.mix(grain, (0.3, 0.05, 0.01), (0.55, 0.15, 0.03))
    glow = s.remap(grain, 0.3, 0.7, 0.55, 1.0)
    return s.finish(base, 0.28, height=grain, bump=0.08, bump_distance=0.004, emission=glow, emission_color=(1.0, 0.42, 0.12), emission_strength=4.0, bake_size=256)


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
    return bm


def octagon(apothem):
    return apothem / OCTAGON


def block(length, depth, height, rng=None, chips=0, chip_depth=0.03):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for vertex in bm.verts:
        vertex.co = Vector((vertex.co.x * length, vertex.co.y * depth, (vertex.co.z + 0.5) * height))
    for _ in range(chips):
        sx, sy = rng.choice((-1.0, 1.0)), rng.choice((-1.0, 1.0))
        kind = rng.random()
        if kind < 0.35:
            point = Vector((sx * length * 0.5, sy * depth * 0.5, height))
            normal = Vector((sx * rng.uniform(0.5, 1.0), sy * rng.uniform(0.5, 1.0), rng.uniform(0.6, 1.0)))
        elif kind < 0.75:
            point = Vector((rng.uniform(-0.4, 0.4) * length, sy * depth * 0.5, height))
            normal = Vector((rng.uniform(-0.25, 0.25), sy, rng.uniform(0.7, 1.3)))
        else:
            point = Vector((sx * length * 0.5, sy * depth * 0.5, rng.uniform(0.2, 0.8) * height))
            normal = Vector((sx, sy * rng.uniform(0.6, 1.4), rng.uniform(-0.3, 0.3)))
        normal.normalize()
        kit.cut(bm, point - normal * chip_depth * rng.uniform(0.6, 1.3), normal)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def capstone(size, height, peak, top):
    bm = bmesh.new()
    half = size * 0.5
    ring = [bm.verts.new((sx * half, sy * half, 0.0)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    upper = [bm.verts.new((sx * half, sy * half, height)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    tip = [bm.verts.new((sx * top * 0.5, sy * top * 0.5, height + peak)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    bm.faces.new(list(reversed(ring)))
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((ring[i], ring[j], upper[j], upper[i]))
        bm.faces.new((upper[i], upper[j], tip[j], tip[i]))
    bm.faces.new(tip)
    return bm


def bipyramid(radius, below, above, sides=6):
    bm = bmesh.new()
    ring = [bm.verts.new((math.cos(2.0 * math.pi * i / sides) * radius, math.sin(2.0 * math.pi * i / sides) * radius, 0.0)) for i in range(sides)]
    upper = [bm.verts.new((math.cos(2.0 * math.pi * (i + 0.5) / sides) * radius * 0.62, math.sin(2.0 * math.pi * (i + 0.5) / sides) * radius * 0.62, above * 0.55)) for i in range(sides)]
    top = bm.verts.new((0.0, 0.0, above))
    bottom = bm.verts.new((0.0, 0.0, -below))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((ring[i], ring[j], upper[i]))
        bm.faces.new((upper[i], ring[j], upper[j]))
        bm.faces.new((upper[i], upper[j], top))
        bm.faces.new((ring[j], ring[i], bottom))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def disc(radius, z, steps, phase=0.0):
    bm = lathe([(radius, z), (0.0, z)], steps, phase)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    for face in bm.faces:
        if face.normal.z < 0.0:
            face.normal_flip()
    return bm


def place(obj, matrix):
    obj.matrix_world = matrix
    return obj


def build(target):
    stone = stone_material()
    water = water_material()
    ember = ember_material()
    rng = random.Random(5)
    parts = []
    eighth = math.pi / 8.0

    plinth = kit.mesh_object('Plinth', lathe([(octagon(1.98), 0.0), (octagon(1.98), 0.075), (octagon(1.94), 0.11), (octagon(1.66), 0.11)], 8, eighth), target, stone)
    kit.bevel(plinth, 0.03, segments=3, angle=30.0)
    parts.append(plinth)

    inner, outer = 1.55, 1.78
    middle = (inner + outer) * 0.5
    side = 2.0 * middle * math.tan(eighth)
    for k in range(8):
        angle = k * math.pi / 4.0
        rotation = Matrix.Rotation(angle + math.pi / 2.0 + rng.uniform(-0.006, 0.006), 4, 'Z')
        wall = kit.mesh_object(f'Wall{k}', block(side, outer - inner + rng.uniform(-0.01, 0.01), 0.42 + rng.uniform(-0.008, 0.004), rng, chips=rng.randint(0, 2), chip_depth=0.022), target, stone)
        kit.bevel(wall, 0.03, segments=3, angle=30.0)
        parts.append(place(wall, Matrix.Translation((math.cos(angle) * middle, math.sin(angle) * middle, 0.105)) @ rotation))
        coping = kit.mesh_object(f'Coping{k}', block(side - 0.1 + rng.uniform(-0.02, 0.0), 0.33 + rng.uniform(-0.012, 0.008), 0.1 + rng.uniform(-0.006, 0.006), rng, chips=rng.randint(1, 3), chip_depth=0.03), target, stone)
        kit.bevel(coping, 0.04, segments=4, angle=30.0)
        tilt = Matrix.Rotation(rng.uniform(-0.008, 0.008), 4, 'X') @ Matrix.Rotation(rng.uniform(-0.005, 0.005), 4, 'Y')
        parts.append(place(coping, Matrix.Translation((math.cos(angle) * middle, math.sin(angle) * middle, 0.522)) @ rotation @ tilt))
        corner = angle + eighth
        radius = octagon(middle)
        post_rotation = Matrix.Rotation(corner + rng.uniform(-0.03, 0.03), 4, 'Z')
        post = kit.mesh_object(f'Post{k}', block(0.3, 0.3, 0.56 + rng.uniform(-0.01, 0.01), rng, chips=rng.randint(0, 2), chip_depth=0.025), target, stone)
        kit.bevel(post, 0.032, segments=3, angle=30.0)
        parts.append(place(post, Matrix.Translation((math.cos(corner) * radius, math.sin(corner) * radius, 0.105)) @ post_rotation))
        cap = kit.mesh_object(f'Cap{k}', capstone(0.38, 0.06, 0.08, 0.12), target, stone)
        kit.bevel(cap, 0.026, segments=3, angle=25.0)
        parts.append(place(cap, Matrix.Translation((math.cos(corner) * radius, math.sin(corner) * radius, 0.665 + rng.uniform(-0.01, 0.01))) @ post_rotation @ Matrix.Rotation(rng.uniform(-0.012, 0.012), 4, 'X')))

    pedestal = kit.mesh_object('Pedestal', lathe([
        (octagon(0.5), 0.4), (octagon(0.5), 0.52), (octagon(0.45), 0.56), (octagon(0.42), 0.6), (octagon(0.36), 0.62),
        (octagon(0.3), 0.66), (octagon(0.27), 0.95), (octagon(0.31), 0.99), (octagon(0.35), 1.03), (octagon(0.36), 1.07),
    ], 8, eighth), target, stone)
    kit.bevel(pedestal, 0.028, segments=3, angle=30.0)
    parts.append(pedestal)

    bowl = kit.mesh_object('Bowl', lathe([
        (0.3, 1.04), (0.46, 1.065), (0.62, 1.12), (0.75, 1.19), (0.845, 1.252), (0.862, 1.284),
        (0.85, 1.305), (0.81, 1.31), (0.78, 1.298), (0.765, 1.275), (0.74, 1.25),
    ], 32), target, stone)
    kit.smooth_by_angle(bowl, 40.0)
    parts.append(bowl)

    stem = kit.mesh_object('Stem', lathe([
        (0.15, 1.2), (0.15, 1.31), (0.125, 1.35), (0.1, 1.39), (0.085, 1.42), (0.08, 1.56), (0.095, 1.59), (0.12, 1.61),
    ], 16), target, stone)
    kit.smooth_by_angle(stem, 40.0)
    parts.append(stem)

    dish = kit.mesh_object('Dish', lathe([
        (0.11, 1.6), (0.2, 1.635), (0.255, 1.68), (0.268, 1.71), (0.255, 1.728), (0.21, 1.718), (0.12, 1.7), (0.0, 1.695),
    ], 16), target, stone)
    kit.smooth_by_angle(dish, 40.0)
    parts.append(dish)

    gem = kit.mesh_object('EmberStone', bipyramid(0.1, 0.14, 0.26), target, ember)
    kit.bevel(gem, 0.004, segments=1, angle=20.0)
    parts.append(place(gem, Matrix.Translation((0.0, 0.0, 1.8)) @ Matrix.Rotation(0.3, 4, 'Z')))

    basin_water = kit.mesh_object('BasinWater', disc(octagon(1.57), BASIN_WATER, 8, eighth), target, water)
    parts.append(basin_water)
    bowl_water = kit.mesh_object('BowlWater', disc(0.775, BOWL_WATER, 32), target, water)
    parts.append(bowl_water)
    return parts


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    options = dict(zip(argv[::2], argv[1::2]))
    output = options.get('--out')
    preview = options.get('--preview')
    scene = kit.reset()
    target = kit.collection('Export')
    parts = build(target)
    kit.unwrap([obj for obj in parts if obj.data.materials[0].name == 'Stone'], angle=50.0, margin=0.003)
    kit.unwrap([obj for obj in parts if obj.data.materials[0].name == 'Water'], angle=50.0, margin=0.01)
    kit.unwrap([obj for obj in parts if obj.data.materials[0].name == 'Ember'], angle=50.0, margin=0.02)
    fountain = kit.join(parts, NAME)
    scene['ao_distance'] = 0.5
    if output:
        os.makedirs(output, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(output, NAME + '.blend'), compress=True)
        kit.bake_and_export([fountain], output, NAME, ao_distance=scene['ao_distance'])
    if preview:
        os.makedirs(preview, exist_ok=True)
        scene.view_settings.view_transform = 'Standard'
        bm = bmesh.new()
        bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=6.0)
        ground_shader = kit.Shader('PreviewGround')
        ground = kit.mesh_object('PreviewGround', bm, kit.collection('Preview'), ground_shader.finish((0.18, 0.17, 0.16), 0.9))
        kit.environment('forest.exr', strength=1.0, rotation=1.2)
        kit.render(os.path.join(preview, 'fountain_game.png'), [fountain], yaw=20.0, pitch=57.0, fill=1.2)
        kit.render(os.path.join(preview, 'fountain_close.png'), [fountain], yaw=-30.0, pitch=22.0, fill=0.85)
        kit.environment('sunset.exr', strength=0.9, rotation=2.4)
        kit.render(os.path.join(preview, 'fountain_dusk.png'), [fountain], yaw=140.0, pitch=35.0, fill=0.95)


if __name__ == '__main__':
    main()
