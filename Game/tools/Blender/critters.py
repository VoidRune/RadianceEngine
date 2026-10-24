import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Quaternion, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

NAME = 'Critters'


def tilt(axis, degrees):
    return Quaternion(axis, math.radians(degrees))


def cat(target, fur, eyes, rng):
    body = pk.sculpt([
        pk.ellipsoid_bm((0.0, 0.035, 0.075), (0.1, 0.105, 0.078)),
        pk.ellipsoid_bm((0.0, -0.02, 0.15), (0.07, 0.072, 0.105), tilt((1.0, 0.0, 0.0), -12.0)),
        pk.ellipsoid_bm((0.0, -0.06, 0.19), (0.056, 0.045, 0.065)),
        pk.ellipsoid_bm((0.0, -0.06, 0.245), (0.044, 0.042, 0.045)),
        pk.ellipsoid_bm((0.072, 0.035, 0.07), (0.04, 0.072, 0.058)),
        pk.ellipsoid_bm((-0.072, 0.035, 0.07), (0.04, 0.072, 0.058)),
        pk.capsule_bm((0.032, -0.074, 0.165), (0.034, -0.088, 0.026), 0.021, 0.018),
        pk.capsule_bm((-0.032, -0.074, 0.165), (-0.034, -0.088, 0.026), 0.021, 0.018),
        pk.ellipsoid_bm((0.035, -0.1, 0.013), (0.022, 0.03, 0.014)),
        pk.ellipsoid_bm((-0.035, -0.1, 0.013), (0.022, 0.03, 0.014)),
        pk.ellipsoid_bm((0.072, -0.035, 0.014), (0.024, 0.045, 0.015)),
        pk.ellipsoid_bm((-0.072, -0.035, 0.014), (0.024, 0.045, 0.015)),
    ], target, fur, 'Cat_Body', voxel=0.005, smooth=6, smooth_factor=0.5, faces=2800, rng=rng)
    head = pk.sculpt([
        pk.ellipsoid_bm((0.0, -0.078, 0.29), (0.05, 0.046, 0.043)),
        pk.ellipsoid_bm((0.0, -0.113, 0.276), (0.024, 0.02, 0.017)),
        pk.ellipsoid_bm((0.025, -0.098, 0.277), (0.023, 0.022, 0.02)),
        pk.ellipsoid_bm((-0.025, -0.098, 0.277), (0.023, 0.022, 0.02)),
        pk.ellipsoid_bm((0.0, -0.065, 0.255), (0.04, 0.04, 0.03)),
    ], target, fur, 'Cat_Head', voxel=0.004, smooth=8, faces=1600, rng=rng)
    ears = []
    for side in (-1.0, 1.0):
        base = Vector((side * 0.029, -0.076, 0.316))
        tip = base + Vector((side * 0.012, 0.006, 0.04))
        ears.append(pk.part(f'Cat_Ear{int(side)}', pk.cone_bm(base, tip, 0.025, sides=7, flatten=0.5), target, fur, rng=rng, smooth=40.0))
    eye_parts = []
    for side in (-1.0, 1.0):
        eye = pk.part(f'Cat_Eye{int(side)}', pk.ellipsoid_bm((0.0, 0.0, 0.0), (0.0105, 0.006, 0.008), segments=12, rings=8), target, eyes, smooth=60.0)
        pk.tag(eye)
        eye.matrix_world = Matrix.Translation((side * 0.0195, -0.1185, 0.295)) @ Matrix.Rotation(side * 0.35, 4, 'Z') @ Matrix.Rotation(side * -0.25, 4, 'Y')
        eye_parts.append(eye)
    head = kit.join([head] + ears + eye_parts, 'Cat_Head')
    tail = pk.tube([(0.0, 0.11, 0.045), (0.055, 0.15, 0.024), (0.125, 0.11, 0.017), (0.15, 0.015, 0.016), (0.125, -0.065, 0.02), (0.085, -0.1, 0.036)], 0.017, target, fur, 'Cat_Tail', resolution=6, sides=8, radii=[1.0, 0.95, 0.88, 0.8, 0.7, 0.55], rng=rng)
    return [body, head, tail]


def duck(target, rubber, beak, gloss, rng):
    body = pk.sculpt([
        pk.ellipsoid_bm((0.0, 0.03, 0.165), (0.205, 0.285, 0.158)),
        pk.ellipsoid_bm((0.0, 0.25, 0.25), (0.075, 0.1, 0.07), tilt((1.0, 0.0, 0.0), 32.0)),
        pk.ellipsoid_bm((0.0, -0.14, 0.18), (0.165, 0.13, 0.148)),
        pk.capsule_bm((0.0, -0.13, 0.25), (0.0, -0.165, 0.33), 0.1, 0.095),
        pk.ellipsoid_bm((0.0, -0.17, 0.4), (0.128, 0.128, 0.122)),
        pk.ellipsoid_bm((0.168, 0.05, 0.2), (0.055, 0.155, 0.068), tilt((1.0, 0.0, 0.0), 10.0)),
        pk.ellipsoid_bm((-0.168, 0.05, 0.2), (0.055, 0.155, 0.068), tilt((1.0, 0.0, 0.0), 10.0)),
    ], target, rubber, 'Duck', voxel=0.008, smooth=10, faces=3000, rng=rng)
    bill = pk.sculpt([
        pk.ellipsoid_bm((0.0, -0.29, 0.378), (0.072, 0.07, 0.024), tilt((1.0, 0.0, 0.0), -6.0)),
        pk.ellipsoid_bm((0.0, -0.275, 0.355), (0.06, 0.055, 0.018), tilt((1.0, 0.0, 0.0), 8.0)),
    ], target, beak, 'Duck_Bill', voxel=0.005, smooth=6, faces=500, rng=rng)
    eye_parts = []
    for side in (-1.0, 1.0):
        eye_parts.append(pk.part(f'Duck_Eye{int(side)}', pk.ellipsoid_bm((side * 0.068, -0.262, 0.432), (0.024, 0.016, 0.026), segments=12, rings=8), target, gloss, rng=rng, smooth=60.0))
    return [kit.join([body, bill] + eye_parts, 'Duck')]


def balloon(target, red, rng):
    profile = [(0.0, -0.045), (0.02, -0.01), (0.026, 0.008), (0.018, 0.02), (0.03, 0.035), (0.09, 0.07), (0.17, 0.13), (0.25, 0.22), (0.31, 0.33), (0.345, 0.45), (0.35, 0.55), (0.33, 0.65), (0.28, 0.74), (0.2, 0.81), (0.1, 0.855), (0.0, 0.87)]
    shell = pk.part('Balloon', pk.lathe(profile, 28), target, red, rng=rng, smooth=50.0)
    return [shell]


def dummy(target, wood, burlap, rope, straw, rng):
    post_parts = [pk.part('Dummy_Pole', pk.cylinder(0.055, 1.32, 10), target, wood, rng=rng, smooth=40.0)]
    for angle in (0.0, math.pi * 0.5):
        foot = pk.part(f'Dummy_Foot{angle:.1f}', pk.chipped((0.74, 0.11, 0.07), rng, chips=2, depth=0.015, center=(0.0, 0.0, 0.035)), target, wood, frame=Matrix.Identity(4), rng=rng, bevel=(0.012, 2, 30.0))
        foot.matrix_world = Matrix.Rotation(angle + rng.uniform(-0.05, 0.05), 4, 'Z')
        post_parts.append(foot)
    for side in range(4):
        angle = side * math.pi * 0.5 + math.pi * 0.25
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        brace = pk.part(f'Dummy_Brace{side}', pk.rod(out * 0.2 + Vector((0.0, 0.0, 0.06)), out * 0.03 + Vector((0.0, 0.0, 0.3)), 0.022, 6), target, wood, rng=rng, smooth=40.0)
        post_parts.append(brace)
    post = kit.join(post_parts, 'Dummy_Post')

    torso_profile = [(0.0, 0.73), (0.17, 0.745), (0.235, 0.8), (0.262, 0.9), (0.27, 1.02), (0.262, 1.16), (0.235, 1.27), (0.18, 1.345), (0.1, 1.38), (0.0, 1.39)]
    torso = pk.part('Dummy_Torso', pk.lathe(torso_profile, 24), target, burlap, smooth=60.0)
    lumps = torso.modifiers.new('Lumps', 'DISPLACE')
    texture = bpy.data.textures.new('DummyLumps', 'CLOUDS')
    texture.noise_scale = 0.12
    lumps.texture = texture
    lumps.strength = 0.018
    lumps.texture_coords = 'LOCAL'
    pk.subdivide(torso, 1)
    kit.apply_modifiers(torso)
    pk.tag(torso)
    torso.data.shade_smooth()
    head_profile = [(0.0, 1.385), (0.07, 1.39), (0.1, 1.41), (0.16, 1.46), (0.195, 1.53), (0.2, 1.6), (0.185, 1.68), (0.14, 1.745), (0.07, 1.78), (0.0, 1.79)]
    head = pk.part('Dummy_Head', pk.lathe(head_profile, 20), target, burlap, smooth=60.0)
    knot = pk.part('Dummy_Knot', pk.cone_bm((0.0, 0.0, 1.765), (0.012, 0.01, 1.86), 0.05, sides=8), target, burlap, rng=rng, smooth=40.0)
    bands = [
        pk.part('Dummy_Band0', pk.torus(0.258, 0.022, 28, 6), target, rope, matrix=Matrix.Translation((0.0, 0.0, 0.86)), rng=rng, smooth=50.0),
        pk.part('Dummy_Band1', pk.torus(0.2, 0.02, 24, 6), target, rope, matrix=Matrix.Translation((0.0, 0.0, 1.31)), rng=rng, smooth=50.0),
        pk.part('Dummy_Collar', pk.torus(0.1, 0.02, 20, 6), target, rope, matrix=Matrix.Translation((0.0, 0.0, 1.405)), rng=rng, smooth=50.0),
    ]
    arm = pk.part('Dummy_ArmBar', pk.rod((-0.52, 0.0, 1.22), (0.52, 0.0, 1.22), 0.03, 8), target, wood, rng=rng, smooth=40.0)
    sleeves = []
    for side in (-1.0, 1.0):
        sleeve = pk.part(f'Dummy_Sleeve{int(side)}', pk.rod((side * 0.22, 0.0, 1.22), (side * 0.45, 0.0, 1.22), 0.062, 12, end_radius=0.055), target, burlap, rng=rng, smooth=50.0)
        cuff = pk.part(f'Dummy_Cuff{int(side)}', pk.torus(0.058, 0.014, 16, 6), target, rope, matrix=Matrix.Translation((side * 0.43, 0.0, 1.22)) @ Matrix.Rotation(math.pi * 0.5, 4, 'Y'), rng=rng, smooth=50.0)
        sleeves.extend([sleeve, cuff])
        for i in range(9):
            angle = 2.0 * math.pi * i / 9.0 + rng.uniform(-0.2, 0.2)
            spread = Vector((0.0, math.cos(angle), math.sin(angle))) * rng.uniform(0.02, 0.045)
            start = Vector((side * 0.45, 0.0, 1.22)) + spread * 0.6
            end = Vector((side * (0.56 + rng.uniform(0.0, 0.06)), 0.0, 1.22)) + spread * 2.2 + Vector((0.0, 0.0, -rng.uniform(0.0, 0.03)))
            sleeves.append(pk.part(f'Dummy_Straw{int(side)}_{i}', pk.rod(start, end, 0.009, 4, end_radius=0.003), target, straw, rng=rng, smooth=40.0))
    for i in range(10):
        angle = 2.0 * math.pi * i / 10.0 + rng.uniform(-0.2, 0.2)
        spread = Vector((math.cos(angle), math.sin(angle), 0.0))
        start = Vector((0.0, 0.0, 1.8)) + spread * 0.015
        end = Vector((0.0, 0.0, 1.86 + rng.uniform(0.0, 0.05))) + spread * rng.uniform(0.04, 0.08)
        sleeves.append(pk.part(f'Dummy_Tuft{i}', pk.rod(start, end, 0.008, 4, end_radius=0.003), target, straw, rng=rng, smooth=40.0))
    body = kit.join([torso, head, knot, arm] + bands + sleeves, 'Dummy_Body')
    return [post, body]


def build(target):
    rng = random.Random(11)
    fur = pk.fur_material('Fur')
    eyes = pk.eye_material('CatEye', strength=2.0)
    rubber = pk.rubber_material('DuckRubber', tone=(0.97, 0.7, 0.05))
    beak = pk.rubber_material('DuckBeak', tone=(0.95, 0.32, 0.03), bake_size=256)
    gloss = pk.rubber_material('DuckEye', tone=(0.012, 0.012, 0.014), bake_size=128, roughness=0.18)
    red = pk.rubber_material('BalloonRubber', tone=(0.72, 0.035, 0.03), roughness=0.24)
    wood = pk.wood_material('DummyWood', light=(0.36, 0.25, 0.14), dark=(0.17, 0.11, 0.06), weathered=0.35, bake_size=512)
    burlap = pk.burlap_material('Burlap', tone=(0.5, 0.38, 0.22), target=True)
    rope = pk.rope_material('Rope')
    straw = pk.straw_material('Straw')
    objects = []
    objects += cat(target, fur, eyes, rng)
    objects += duck(target, rubber, beak, gloss, rng)
    objects += balloon(target, red, rng)
    objects += dummy(target, wood, burlap, rope, straw, rng)
    return objects


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    loose = [obj for obj in bpy.data.objects if obj.type == 'MESH' and obj.users_collection and obj.users_collection[0] == target]
    pk.unwrap_all(loose, {'CatEye': 0.02, 'DuckEye': 0.02, 'DuckBeak': 0.01})
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.15)
    if args.get('--preview'):
        layout = {'Cat_Body': (0.0, 0.0), 'Cat_Head': (0.0, 0.0), 'Cat_Tail': (0.0, 0.0), 'Duck': (0.8, 0.0), 'Balloon': (1.6, 0.0), 'Dummy_Post': (2.6, 0.0), 'Dummy_Body': (2.6, 0.0)}
        for obj in objects:
            obj.matrix_world = Matrix.Translation((layout.get(obj.name, (0.0, 0.0))[0], layout.get(obj.name, (0.0, 0.0))[1], 0.0))
        pk.ground_preview(8.0)
        pk.preview([o for o in objects if o.name.startswith('Cat')], args['--preview'], 'cat', fill=1.1)
        pk.preview([o for o in objects if o.name.startswith('Duck')], args['--preview'], 'duck', fill=1.1)
        pk.preview([o for o in objects if o.name.startswith('Balloon')], args['--preview'], 'balloon', close=False, fill=1.2)
        pk.preview([o for o in objects if o.name.startswith('Dummy')], args['--preview'], 'dummy', fill=1.05)


if __name__ == '__main__':
    main()
