import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import stations

NAME = 'Glacier'


def ice_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(s.coords('Object'))
    up = s.smooth(0.5, 0.85, s.separate(s.geometry('Normal'))[2])
    depth = s.smooth(0.2, 0.85, s.occlusion(0.35, samples=24, inside=True))
    reach = s.smooth(0.45, 0.62, s.noise(p, 1.3, detail=2.0))
    planes = s.mul(s.sub(1.0, s.smooth(0.0, 0.018, s.voronoi(p, 1.7, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), reach)
    hairline = s.mul(s.sub(1.0, s.smooth(0.0, 0.008, s.voronoi(p, 5.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.55, 0.7, s.noise(p, 2.4, detail=2.0)))
    bubbles = s.sub(1.0, s.smooth(0.0, 0.1, s.voronoi(p, 40.0, 'F1', 1.0, 'Distance')))
    edge = pk.wear(s, 0.03)
    tone = s.noise(p, 2.2, detail=3.0)
    base = s.mix(s.smooth(0.3, 0.8, tone), (0.1, 0.3, 0.45), (0.18, 0.45, 0.62))
    base = s.mix(s.sub(1.0, depth), base, (0.32, 0.6, 0.76))
    base = s.mix(s.mul(edge, 0.8), base, (0.52, 0.76, 0.88))
    base = s.mix(s.mul(bubbles, 0.2), base, (0.4, 0.62, 0.76))
    base = s.mix(planes, base, (0.66, 0.84, 0.93))
    base = s.mix(s.mul(hairline, 0.6), base, (0.56, 0.76, 0.88))
    low = s.mul(s.sub(1.0, s.smooth(0.0, 0.35, z)), s.smooth(0.4, 0.6, s.noise(p, 5.0, detail=3.0)))
    frost = s.clamp01(s.add(s.mul(up, s.smooth(0.35, 0.6, s.noise(p, 3.0, detail=3.0))), low))
    base = s.mix(frost, base, (0.7, 0.78, 0.86))
    rough = s.remap(frost, 0.0, 1.0, 0.06, 0.7)
    glow = s.mul(s.clamp01(s.add(s.mul(planes, 0.7), s.mul(edge, 0.25))), s.sub(1.0, frost))
    height = s.add(s.mul(planes, 0.5), s.add(s.mul(frost, 0.4), s.mul(hairline, 0.1)))
    return s.finish(base, rough, height=height, bump=0.1, bump_distance=0.003, emission=glow, emission_color=(0.3, 0.7, 1.0), emission_strength=0.9, specular=0.8, bake_size=bake_size, translucency=0.9, thickness=0.45, thin=s.sub(1.0, s.mul(frost, 0.7)))


def snow_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    drift = s.noise(p, 2.6, detail=3.0)
    grain = s.noise(p, 55.0, detail=1.0)
    crust = s.smooth(0.6, 0.72, s.noise(p, 7.0, detail=2.0))
    base = s.mix(s.smooth(0.3, 0.75, drift), (0.6, 0.68, 0.78), (0.76, 0.82, 0.9))
    base = s.mix(s.mul(crust, 0.4), base, (0.55, 0.64, 0.76))
    base = s.mix(s.mul(grain, 0.2), base, (0.85, 0.89, 0.94))
    rough = s.remap(grain, 0.3, 0.7, 0.7, 0.9)
    return s.finish(base, rough, height=s.add(s.mul(drift, 0.7), s.mul(grain, 0.1)), bump=0.1, bump_distance=0.004, bake_size=bake_size)


def snowy_rock_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    large = s.noise(p, 1.1, detail=3.0)
    mid = s.noise(p, 4.5, detail=5.0, roughness=0.55)
    island = s.geometry('Random Per Island')
    tone = s.madd(s.smooth(0.3, 0.7, large), 0.45, s.add(s.mul(mid, 0.3), s.mul(island, 0.25)))
    base = s.mix(tone, (0.09, 0.1, 0.12), (0.25, 0.26, 0.29))
    base = s.mix(s.mul(pk.cavity(s, 0.12), 0.6), base, (0.035, 0.04, 0.045))
    base = s.mix(s.mul(pk.wear(s, 0.025), 0.3), base, (0.3, 0.31, 0.34))
    up = s.smooth(0.5, 0.88, s.separate(s.geometry('Normal'))[2])
    drift = s.smooth(0.3, 0.55, s.noise(p, 2.4, detail=4.0, roughness=0.6))
    snow = s.smooth(0.35, 0.6, s.clamp01(s.mul(up, s.add(0.35, s.mul(drift, 0.9)))))
    base = s.mix(snow, base, (0.66, 0.71, 0.77))
    rough = s.remap(snow, 0.0, 1.0, 0.8, 0.9)
    height = s.add(s.mul(mid, 0.8), s.mul(snow, 0.5))
    return s.finish(base, rough, height=height, bump=0.2, bump_distance=0.01, bake_size=bake_size)


def lump(radius, height, rng, sink=-0.1, wobble=0.12, subdivisions=3):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdivisions, radius=1.0)
    phase = (rng.uniform(0.0, 6.0), rng.uniform(0.0, 6.0))
    for vertex in bm.verts:
        d = vertex.co.normalized()
        bump = 1.0 + wobble * math.sin(d.x * 4.0 + phase[0]) * math.cos(d.y * 3.5 + phase[1]) + rng.uniform(-0.03, 0.03)
        vertex.co = Vector((d.x * radius * bump, d.y * radius * bump, max(d.z, sink) * height * bump))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def ice_pillar(key, spec, m, target, rng):
    parts = [pk.part(f'IcePillar{key}_Mound', lump(spec['mound'], spec['mound'] * 0.32, rng), target, m['snow'], rng=rng, smooth=40.0)]
    for index, (angle, distance, radius, height, lean) in enumerate(spec['shards']):
        shard = pk.part(f'IcePillar{key}_Shard{index}', stations.crystal_shard(radius, height, sides=rng.choice((5, 6, 7)), tip=rng.uniform(0.18, 0.3), rng=rng), target, m['ice'], rng=rng, bevel=(0.03, 2, 30.0))
        out = Vector((math.cos(angle), math.sin(angle), 0.0))
        axis = Vector((0.0, 0.0, 1.0)).cross(out).normalized() if distance > 0.01 else Vector((1.0, 0.0, 0.0))
        shard.matrix_world = Matrix.Translation(out * distance) @ Matrix.Rotation(math.radians(lean), 4, axis) @ Matrix.Rotation(rng.uniform(0.0, 6.28), 4, 'Z')
        parts.append(shard)
    return kit.join(parts, f'IcePillar_{key}')


def rock_lump(radius, height, rng):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
    waves = [(Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(-0.4, 0.4))).normalized(), rng.uniform(2.0, 5.0), rng.uniform(0.0, 6.28), rng.uniform(0.05, 0.12)) for _ in range(5)]
    stretch = rng.uniform(0.75, 0.95)
    for vertex in bm.verts:
        d = vertex.co.normalized()
        bump = 1.0 + sum(amount * math.sin(d.dot(axis) * frequency + phase) for axis, frequency, phase, amount in waves) + rng.uniform(-0.03, 0.03)
        vertex.co = Vector((d.x * radius * bump, d.y * radius * bump * stretch, max(d.z, -0.15) * height * bump))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def snow_rock(key, radius, height, m, target, rng):
    return pk.part(f'SnowRock_{key}', rock_lump(radius, height, rng), target, m['rock'], rng=rng, smooth=25.0)


def build(target):
    rng = random.Random(61)
    m = {
        'ice': ice_material('GlacierIce'),
        'snow': snow_material('GlacierSnow'),
        'rock': snowy_rock_material('GlacierRock'),
    }
    pillars = {
        'A': {'mound': 0.78, 'shards': [(0.0, 0.0, 0.4, 2.7, 3.0), (0.6, 0.38, 0.2, 1.4, 22.0), (2.4, 0.4, 0.17, 1.05, 28.0), (4.2, 0.36, 0.15, 0.8, 34.0)]},
        'B': {'mound': 0.82, 'shards': [(0.0, 0.05, 0.36, 2.3, 8.0), (1.4, 0.3, 0.3, 1.9, 14.0), (3.4, 0.42, 0.18, 1.0, 30.0), (5.2, 0.4, 0.14, 0.7, 38.0)]},
        'C': {'mound': 0.72, 'shards': [(0.0, 0.0, 0.46, 3.0, 2.0), (2.0, 0.42, 0.16, 0.9, 30.0), (4.4, 0.38, 0.13, 0.65, 36.0)]},
    }
    objects = [ice_pillar(key, spec, m, target, rng) for key, spec in pillars.items()]
    objects += [snow_rock('A', 0.9, 0.62, m, target, rng), snow_rock('B', 0.6, 0.5, m, target, rng), snow_rock('C', 0.42, 0.34, m, target, rng)]
    return objects


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    pk.unwrap_all(objects, {'GlacierIce': 0.008, 'GlacierSnow': 0.01})
    for index, obj in enumerate(objects):
        obj.matrix_world = Matrix.Translation((index * 4.0, 0.0, 0.0)) @ obj.matrix_world
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.4)
    if args.get('--preview'):
        pk.ground_preview(12.0)
        for index, obj in enumerate(objects):
            obj.matrix_world = Matrix.Translation(((index % 3) * 2.4 - 2.4, (index // 3) * 2.8 - 1.4, 0.0))
        pk.preview(objects[:3], args['--preview'], 'glacier_a', fill=0.95)
        pk.preview(objects[3:], args['--preview'], 'glacier_b', fill=0.95)


if __name__ == '__main__':
    main()
