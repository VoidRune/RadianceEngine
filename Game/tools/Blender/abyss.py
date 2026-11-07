import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk

NAME = 'Abyss'


def serac_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    _, _, z = s.separate(p)
    up = s.smooth(0.42, 0.72, s.separate(s.geometry('Normal'))[2])
    strata = s.noise(s.stretch(p, (0.09, 0.09, 1.1)), 1.0, detail=5.0, roughness=0.6)
    tone = s.noise(p, 0.35, detail=5.0, roughness=0.55)
    fine = s.noise(p, 6.0, detail=4.0)
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.008, s.voronoi(p, 0.4, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.5, 0.66, s.noise(p, 0.5, detail=2.0)))
    base = s.mix(s.smooth(0.3, 0.72, tone), (0.012, 0.08, 0.17), (0.05, 0.27, 0.42))
    base = s.mix(s.mul(s.smooth(0.5, 0.82, strata), 0.34), base, (0.42, 0.68, 0.78))
    base = s.mix(s.mul(s.sub(1.0, s.occlusion(1.1, samples=16)), 0.8), base, (0.008, 0.06, 0.12))
    base = s.mix(s.mul(cracks, 0.5), base, (0.6, 0.82, 0.9))
    drift = s.smooth(0.3, 0.55, s.noise(p, 0.9, detail=4.0, roughness=0.6))
    snow = s.clamp01(s.mul(up, s.add(0.55, s.mul(drift, 0.8))))
    base = s.mix(snow, base, (0.78, 0.84, 0.92))
    rough = s.remap(snow, 0.0, 1.0, 0.16, 0.88)
    height = s.add(s.mul(strata, 0.3), s.add(s.mul(fine, 0.2), s.mul(snow, 0.45)))
    glow = s.mul(s.sub(1.0, snow), s.mul(s.smooth(0.3, 0.8, tone), 0.35))
    return s.finish(base, rough, height=height, bump=0.5, bump_distance=0.08, emission=glow, emission_color=(0.04, 0.26, 0.4), emission_strength=0.3, bake_size=bake_size)


def slab_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    up = s.smooth(0.55, 0.85, s.separate(s.geometry('Normal'))[2])
    tone = s.noise(p, 1.2, detail=4.0)
    planes = s.sub(1.0, s.smooth(0.0, 0.02, s.voronoi(p, 1.3, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
    reach = s.smooth(0.4, 0.6, s.noise(p, 1.1, detail=2.0))
    planes = s.mul(planes, reach)
    base = s.mix(s.smooth(0.2, 0.75, tone), (0.015, 0.2, 0.26), (0.14, 0.56, 0.6))
    base = s.mix(planes, base, (0.76, 0.96, 0.98))
    drift = s.smooth(0.34, 0.62, s.noise(p, 1.8, detail=5.0, roughness=0.6))
    snow = s.clamp01(s.mul(up, s.add(0.45, s.mul(drift, 0.9))))
    base = s.mix(snow, base, (0.8, 0.86, 0.94))
    rough = s.remap(snow, 0.0, 1.0, 0.1, 0.86)
    glow = s.mul(s.sub(1.0, snow), s.add(0.28, s.mul(tone, 0.5)))
    height = s.add(s.mul(tone, 0.2), s.add(s.mul(snow, 0.5), s.mul(planes, 0.2)))
    return s.finish(base, rough, height=height, bump=0.25, bump_distance=0.025, emission=glow, emission_color=(0.05, 0.42, 0.44), emission_strength=0.3, bake_size=bake_size)


def floe_material(name, bake_size=512):
    s = kit.Shader(name)
    p = pk.local(s)
    tone = s.noise(p, 1.6, detail=4.0)
    planes = s.mul(s.sub(1.0, s.smooth(0.0, 0.016, s.voronoi(p, 1.8, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.42, 0.6, s.noise(p, 1.4, detail=2.0)))
    up = s.smooth(0.6, 0.9, s.separate(s.geometry('Normal'))[2])
    rime = s.mul(up, s.smooth(0.52, 0.72, s.noise(p, 3.4, detail=4.0)))
    base = s.mix(s.smooth(0.2, 0.75, tone), (0.012, 0.11, 0.15), (0.05, 0.3, 0.36))
    base = s.mix(s.mul(planes, 0.7), base, (0.42, 0.7, 0.76))
    base = s.mix(s.mul(rime, 0.5), base, (0.46, 0.62, 0.72))
    rough = s.remap(rime, 0.0, 1.0, 0.07, 0.42)
    return s.finish(base, rough, height=s.add(s.mul(tone, 0.2), s.mul(rime, 0.12)), bump=0.12, bump_distance=0.015, emission=s.mul(s.sub(1.0, rime), s.mul(tone, 0.4)), emission_color=(0.03, 0.26, 0.3), emission_strength=0.25, bake_size=bake_size)


def cut_block(size, rng, cuts=14, depth=(0.08, 0.3), center=(0.0, 0.0, 0.0), upward=0.0):
    bm = pk.box(size, center)
    half = Vector((size[0] * 0.5, size[1] * 0.5, size[2] * 0.5))
    middle = Vector(center)
    for _ in range(cuts):
        normal = Vector((rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0), rng.uniform(-0.6 + upward, 1.0))).normalized()
        reach = abs(normal.x) * half.x + abs(normal.y) * half.y + abs(normal.z) * half.z
        kit.cut(bm, middle + normal * (reach * (1.0 - rng.uniform(depth[0], depth[1]))), normal)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def serac(key, size, m, target, rng):
    bm = cut_block(size, rng, cuts=30, depth=(0.03, 0.34), center=(0.0, 0.0, size[2] * 0.5 - 1.5), upward=0.25)
    parts = [pk.part(f'Serac_{key}_Core', bm, target, m['serac'], rng=rng)]
    for k in range(8):
        w = rng.uniform(0.28, 0.5)
        s = (size[0] * w, size[1] * w, size[2] * rng.uniform(0.3, 0.72))
        c = (rng.uniform(-0.55, 0.55) * size[0], -size[1] * rng.uniform(0.2, 0.55), s[2] * 0.5 - 1.5)
        piece = cut_block(s, rng, cuts=14, depth=(0.04, 0.32), center=c, upward=0.25)
        parts.append(pk.part(f'Serac_{key}_Spur{k}', piece, target, m['serac'], rng=rng))
    return kit.join(parts, f'Serac_{key}')


def ice_fall(m, target, rng):
    shapes = []
    for k in range(46):
        x = rng.uniform(-4.4, 4.4)
        top = rng.uniform(7.0, 12.0) * (1.0 - 0.3 * abs(x) / 4.4)
        y = rng.uniform(-0.6, 0.6) + 0.1 * top * 0.0
        r = rng.uniform(0.2, 0.55)
        foot = rng.uniform(-1.2, 1.4)
        shapes.append(pk.capsule_bm((x + rng.uniform(-0.2, 0.2), y - 0.4, foot), (x, y + 0.7, top), r * rng.uniform(1.2, 1.7), r * rng.uniform(0.2, 0.5), segments=10, rings=3))
    return pk.sculpt(shapes, target, m['serac'], 'IceFall', 0.1, smooth=3, smooth_factor=0.4, faces=14000, rng=rng)


def slab(key, size, m, target, rng):
    return pk.part(f'IceSlab_{key}', cut_block(size, rng, cuts=9, depth=(0.03, 0.18)), target, m['slab'], rng=rng)


def floe(key, radius, thickness, m, target, rng):
    bm = bmesh.new()
    sides = rng.randrange(20, 30)
    waves = [(rng.randrange(2, 5), rng.uniform(0.0, 6.28), rng.uniform(0.1, 0.22)), (rng.randrange(5, 9), rng.uniform(0.0, 6.28), rng.uniform(0.05, 0.12))]
    outline = []
    for i in range(sides):
        a = 2.0 * math.pi * i / sides
        r = radius * (0.86 + sum(w * math.sin(n * a + p) for n, p, w in waves) + rng.uniform(-0.1, 0.1))
        outline.append((math.cos(a) * r, math.sin(a) * r, thickness * rng.uniform(0.18, 0.42)))
    centre = bm.verts.new((rng.uniform(-0.1, 0.1) * radius, rng.uniform(-0.1, 0.1) * radius, thickness * 0.42))
    middle = [bm.verts.new((x * 0.62 + rng.uniform(-0.04, 0.04), y * 0.62 + rng.uniform(-0.04, 0.04), thickness * rng.uniform(0.3, 0.48))) for x, y, _ in outline]
    top = [bm.verts.new(co) for co in outline]
    water = [bm.verts.new((x * rng.uniform(1.0, 1.12), y * rng.uniform(1.0, 1.12), -thickness * rng.uniform(0.1, 0.28))) for x, y, _ in outline]
    low = [bm.verts.new((x * rng.uniform(0.7, 0.95), y * rng.uniform(0.7, 0.95), -thickness * rng.uniform(0.6, 1.1))) for x, y, _ in outline]
    bm.faces.new(list(reversed(low)))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((centre, middle[i], middle[j]))
        bm.faces.new((middle[i], top[i], top[j], middle[j]))
        bm.faces.new((top[i], water[i], water[j], top[j]))
        bm.faces.new((water[i], low[i], low[j], water[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return pk.part(f'Floe_{key}', bm, target, m['floe'], rng=rng, smooth=38.0)


def build(target):
    m = {
        'serac': serac_material('AbyssSerac'),
        'slab': slab_material('AbyssSlab'),
        'floe': floe_material('AbyssFloe'),
    }
    rng = random.Random(911)
    objects = [serac('A', (9.0, 7.0, 18.0), m, target, random.Random(918))]
    objects.append(serac('B', (7.0, 6.0, 14.0), m, target, random.Random(919)))
    objects.append(serac('C', (11.0, 7.0, 22.0), m, target, random.Random(920)))
    objects.append(ice_fall(m, target, random.Random(921)))
    objects.append(slab('A', (2.8, 1.8, 0.62), m, target, rng))
    objects.append(slab('B', (2.1, 2.3, 0.5), m, target, rng))
    objects.append(slab('C', (1.5, 1.0, 0.42), m, target, rng))
    objects.append(floe('A', 2.1, 0.55, m, target, rng))
    objects.append(floe('B', 1.5, 0.45, m, target, rng))
    objects.append(floe('C', 0.95, 0.38, m, target, rng))
    objects.append(floe('D', 0.55, 0.3, m, target, rng))
    return objects


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    for obj in objects:
        print(f'{obj.name}: {len(obj.data.polygons)} faces')
    pk.unwrap_all(objects, {'AbyssSerac': 0.004, 'AbyssSlab': 0.006, 'AbyssFloe': 0.008}, angle=52.0)
    x = 0.0
    for obj in objects:
        obj.matrix_world = Matrix.Translation((x, 0.0, 0.0))
        x += obj.dimensions.x * 0.5 + 16.0
    if args.get('--preview'):
        kit.reset_preview = None
        pk.ground_preview(60.0)
        for name, group, fill in (('seracs', ['Serac_A', 'Serac_C', 'IceFall'], 1.0), ('slabs', ['IceSlab_A', 'IceSlab_B', 'Floe_A', 'Floe_B'], 1.0)):
            shown = [obj for obj in objects if obj.name in group]
            for obj in objects:
                obj.hide_render = obj not in shown
            for k, obj in enumerate(shown):
                obj.matrix_world = Matrix.Translation((k * (shown[0].dimensions.x + 2.0) - 0.5 * (len(shown) - 1) * shown[0].dimensions.x, 0.0,  4.0))
            pk.preview(shown, args['--preview'], name, fill=fill)
        x = 0.0
        for obj in objects:
            obj.hide_render = False
            obj.matrix_world = Matrix.Translation((x, 0.0, 0.0))
            x += obj.dimensions.x * 0.5 + 16.0
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.5)


if __name__ == '__main__':
    main()
