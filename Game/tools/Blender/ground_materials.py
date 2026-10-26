import math
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit


def paving_stone():
    s = kit.Shader('PavingStone')
    p, w = s.torus(1.0)
    mottle = s.noise(p, 2.4, detail=4.0, roughness=0.55, w=w)
    tone = s.noise(p, 7.0, detail=3.0, w=w)
    middle = s.noise(p, 19.0, detail=3.0, roughness=0.6, w=w)
    grain = s.noise(p, 45.0, detail=2.0, w=w)
    dark = s.smooth(0.64, 0.72, s.noise(p, 95.0, detail=1.0, w=w))
    light = s.smooth(0.66, 0.74, s.noise(p, 80.0, detail=1.0, w=s.add(w, 5.0)))
    base = s.mix(s.smooth(0.3, 0.75, mottle), (0.2, 0.192, 0.18), (0.31, 0.3, 0.28))
    base = s.mix(s.mul(s.smooth(0.55, 0.8, tone), 0.4), base, (0.3, 0.25, 0.2))
    base = s.mix(s.mul(s.smooth(0.42, 0.7, middle), 0.45), base, (0.15, 0.145, 0.137))
    base = s.mix(s.mul(dark, 0.65), base, (0.075, 0.072, 0.07))
    base = s.mix(s.mul(light, 0.5), base, (0.45, 0.44, 0.42))
    base = s.mix(s.mul(grain, 0.2), base, (0.16, 0.155, 0.148))
    pits = s.smooth(0.0, 0.2, s.voronoi(p, 8.0, 'F1', 1.0, 'Distance', w=s.add(w, 3.1)))
    height = s.add(s.mul(mottle, 0.55), s.add(s.mul(pits, 0.25), s.mul(tone, 0.2)))
    rough = s.remap(tone, 0.3, 0.7, 0.68, 0.86)
    return s.finish(base, rough, height=height), 1.0, 0.004, 2.5


def paving_bed():
    s = kit.Shader('PavingBed')
    p, w = s.torus(1.0)
    dirt = s.noise(p, 4.0, detail=5.0, roughness=0.6, w=w)
    grit = s.noise(p, 70.0, detail=1.0, w=w)
    pebble = s.sub(1.0, s.smooth(0.12, 0.24, s.voronoi(p, 32.0, 'F1', 1.0, 'Distance', w=w)))
    shade, _, _ = s.separate(s.voronoi(p, 32.0, 'F1', 1.0, 'Color', w=w))
    base = s.mix(s.smooth(0.3, 0.7, dirt), (0.04, 0.036, 0.031), (0.072, 0.064, 0.054))
    base = s.mix(s.mul(grit, 0.3), base, (0.1, 0.095, 0.085))
    base = s.mix(s.mul(pebble, 0.85), base, s.mix(shade, (0.075, 0.072, 0.068), (0.17, 0.162, 0.15)))
    height = s.add(s.mul(dirt, 0.4), s.mul(pebble, 0.6))
    rough = s.sub(0.95, s.mul(pebble, 0.12))
    return s.finish(base, rough, height=height), 1.0, 0.006, 3.0


def ground_grass():
    s = kit.Shader('GroundGrass')
    p, w = s.torus(2.0)
    patches = s.noise(p, 0.9, detail=3.0, w=w)
    tone = s.noise(p, 3.5, detail=4.0, w=w)
    clumps = s.voronoi(p, 7.0, 'F1', 1.0, 'Distance', w=w)
    variety, _, _ = s.separate(s.voronoi(p, 7.0, 'F1', 1.0, 'Color', w=w))
    blades = s.noise(p, 55.0, detail=2.0, w=w)
    soil = s.smooth(0.64, 0.74, s.noise(p, 2.2, detail=3.0, w=s.add(w, 3.0)))
    flowers = s.smooth(0.75, 0.8, s.noise(p, 40.0, detail=1.0, w=s.add(w, 9.0)))
    base = s.mix(s.smooth(0.3, 0.7, tone), (0.04, 0.078, 0.023), (0.075, 0.125, 0.033))
    base = s.mix(s.mul(s.smooth(0.55, 0.75, patches), 0.75), base, (0.14, 0.13, 0.05))
    base = s.mix(s.mul(s.smooth(0.25, 0.75, variety), 0.55), base, (0.05, 0.09, 0.023))
    base = s.mix(s.mul(s.smooth(0.45, 0.7, blades), 0.45), base, (0.023, 0.045, 0.014))
    base = s.mix(s.mul(soil, 0.85), base, (0.07, 0.052, 0.035))
    base = s.mix(s.mul(flowers, 0.6), base, (0.35, 0.34, 0.22))
    height = s.add(s.mul(s.sub(1.0, s.smooth(0.0, 0.5, clumps)), 0.45), s.add(s.mul(blades, 0.3), s.mul(s.sub(1.0, soil), 0.45)))
    rough = s.remap(blades, 0.3, 0.7, 0.82, 0.95)
    return s.finish(base, rough, height=height), 2.0, 0.008, 2.0


def ground_dirt():
    s = kit.Shader('GroundDirt')
    p, w = s.torus(2.0)
    large = s.noise(p, 1.2, detail=3.0, w=w)
    tone = s.noise(p, 5.0, detail=4.0, roughness=0.6, w=w)
    grit = s.noise(p, 70.0, detail=1.0, w=w)
    pebble = s.sub(1.0, s.smooth(0.1, 0.22, s.voronoi(p, 22.0, 'F1', 1.0, 'Distance', w=w)))
    shade, _, _ = s.separate(s.voronoi(p, 22.0, 'F1', 1.0, 'Color', w=w))
    cracks = s.sub(1.0, s.smooth(0.0, 0.02, s.voronoi(p, 3.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance', w=s.add(w, 4.0))))
    dry = s.mul(s.mul(cracks, s.smooth(0.62, 0.75, s.noise(p, 2.4, detail=2.0, w=s.add(w, 6.0)))), 0.6)
    damp = s.smooth(0.5, 0.75, s.noise(p, 0.7, detail=3.0, w=s.add(w, 13.0)))
    base = s.mix(s.smooth(0.3, 0.7, large), (0.075, 0.056, 0.038), (0.13, 0.1, 0.07))
    base = s.mix(s.mul(damp, 0.45), base, (0.05, 0.038, 0.028))
    base = s.mix(s.mul(s.smooth(0.5, 0.8, tone), 0.4), base, (0.16, 0.13, 0.095))
    base = s.mix(s.mul(grit, 0.25), base, (0.05, 0.038, 0.027))
    base = s.mix(s.mul(pebble, 0.8), base, s.mix(shade, (0.09, 0.085, 0.078), (0.2, 0.19, 0.17)))
    base = s.mix(s.mul(dry, 0.6), base, (0.035, 0.027, 0.02))
    height = s.add(s.mul(large, 0.35), s.add(s.mul(pebble, 0.5), s.mul(tone, 0.15)))
    height = s.sub(height, s.mul(dry, 0.3))
    rough = s.sub(0.93, s.mul(pebble, 0.15))
    return s.finish(base, rough, height=height), 2.0, 0.008, 2.5


def ground_rock():
    s = kit.Shader('GroundRock')
    p, w = s.torus(3.0)
    tone = s.noise(p, 4.0, detail=4.0, w=w)
    broad = s.noise(p, 0.8, detail=3.0, w=s.add(w, 5.0))
    edges = s.voronoi(p, 1.3, 'DISTANCE_TO_EDGE', 1.0, 'Distance', w=s.add(w, 2.0))
    plate, _, _ = s.separate(s.voronoi(p, 1.3, 'F1', 1.0, 'Color', w=s.add(w, 2.0)))
    fine = s.voronoi(p, 4.5, 'DISTANCE_TO_EDGE', 1.0, 'Distance', w=s.add(w, 7.0))
    seams = s.sub(1.0, s.smooth(0.0, 0.025, edges))
    hairline = s.mul(s.sub(1.0, s.smooth(0.0, 0.012, fine)), s.smooth(0.45, 0.6, s.noise(p, 1.7, detail=2.0, w=s.add(w, 11.0))))
    slab = s.smooth(0.0, 0.22, edges)
    lichen = s.smooth(0.66, 0.74, s.noise(p, 6.0, detail=3.0, w=s.add(w, 8.0)))
    base = s.mix(s.smooth(0.3, 0.75, tone), (0.085, 0.08, 0.072), (0.15, 0.14, 0.125))
    base = s.mix(s.mul(s.smooth(0.2, 0.8, plate), 0.45), base, (0.17, 0.15, 0.125))
    base = s.mix(s.mul(s.smooth(0.35, 0.7, broad), 0.4), base, (0.07, 0.068, 0.065))
    base = s.mix(s.mul(seams, 0.55), base, (0.035, 0.032, 0.03))
    base = s.mix(s.mul(hairline, 0.6), base, (0.045, 0.042, 0.04))
    base = s.mix(s.mul(lichen, 0.45), base, (0.11, 0.12, 0.065))
    height = s.add(s.mul(slab, 0.45), s.add(s.mul(tone, 0.35), s.mul(plate, 0.2)))
    height = s.sub(height, s.mul(hairline, 0.15))
    rough = s.remap(tone, 0.3, 0.7, 0.72, 0.88)
    return s.finish(base, rough, height=height), 3.0, 0.02, 2.5


def ground_sand():
    s = kit.Shader('GroundSand')
    p, w = s.torus(2.0)
    _, v, _ = s.separate(s.coords('UV'))
    warp = s.noise(p, 0.9, detail=3.0, w=w)
    ripples = s.smooth(-1.0, 1.0, s.math('SINE', s.add(s.mul(v, 2.0 * math.pi * 16.0), s.mul(warp, 12.0))))
    tone = s.noise(p, 3.0, detail=4.0, w=w)
    grit = s.noise(p, 80.0, detail=1.0, w=w)
    base = s.mix(s.smooth(0.3, 0.7, tone), (0.27, 0.22, 0.155), (0.37, 0.31, 0.22))
    base = s.mix(s.mul(grit, 0.3), base, (0.18, 0.15, 0.105))
    height = s.add(s.mul(ripples, 0.3), s.add(s.mul(tone, 0.55), s.mul(grit, 0.1)))
    rough = s.remap(grit, 0.3, 0.7, 0.86, 0.95)
    return s.finish(base, rough, height=height), 2.0, 0.006, 2.0


def ground_snow():
    s = kit.Shader('GroundSnow')
    p, w = s.torus(2.0)
    _, v, _ = s.separate(s.coords('UV'))
    drift = s.noise(p, 0.8, detail=3.0, w=w)
    tone = s.noise(p, 3.2, detail=4.0, roughness=0.55, w=w)
    warp = s.noise(p, 1.4, detail=2.0, w=s.add(w, 3.0))
    ripples = s.smooth(-1.0, 1.0, s.math('SINE', s.add(s.mul(v, 2.0 * math.pi * 9.0), s.mul(warp, 9.0))))
    grain = s.noise(p, 90.0, detail=1.0, w=w)
    crust = s.smooth(0.62, 0.72, s.noise(p, 5.0, detail=3.0, w=s.add(w, 7.0)))
    base = s.mix(s.smooth(0.3, 0.75, tone), (0.5, 0.56, 0.64), (0.66, 0.71, 0.77))
    base = s.mix(s.mul(s.smooth(0.4, 0.75, drift), 0.5), base, (0.74, 0.78, 0.83))
    base = s.mix(s.mul(crust, 0.35), base, (0.46, 0.54, 0.63))
    base = s.mix(s.mul(grain, 0.15), base, (0.8, 0.83, 0.87))
    height = s.add(s.mul(drift, 0.55), s.add(s.mul(ripples, 0.15), s.add(s.mul(tone, 0.25), s.mul(grain, 0.05))))
    rough = s.remap(grain, 0.3, 0.7, 0.7, 0.9)
    return s.finish(base, rough, height=height), 2.0, 0.006, 1.0


def stretched_torus(s, period, squash):
    u, v, _ = s.separate(s.coords('UV'))
    radius = period / (2.0 * math.pi)
    around = s.mul(u, 2.0 * math.pi)
    along = s.mul(v, 2.0 * math.pi)
    vector = s.combine(s.mul(s.math('COSINE', around), radius), s.mul(s.math('SINE', around), radius), s.mul(s.math('COSINE', along), radius * squash))
    return vector, s.mul(s.math('SINE', along), radius * squash)


def ground_cliff():
    s = kit.Shader('GroundCliff')
    p, w = s.torus(3.0)
    tall, tall_w = stretched_torus(s, 3.0, 0.45)
    _, v, _ = s.separate(s.coords('UV'))
    jitter = s.vector('SUBTRACT', s.noise_color(p, 1.6, detail=3.0, w=s.add(w, 41.0)), (0.5, 0.5, 0.5))
    bent = s.vector('ADD', tall, s.vector('SCALE', jitter, scale=0.5))
    rough_p = s.vector('ADD', p, s.vector('SCALE', jitter, scale=0.35))
    warp = s.noise(p, 0.9, detail=3.0, w=w)
    edges = s.voronoi(bent, 1.4, 'DISTANCE_TO_EDGE', 1.0, 'Distance', w=tall_w)
    block, warm, _ = s.separate(s.voronoi(bent, 1.4, 'F1', 1.0, 'Color', w=tall_w))
    fracture = s.voronoi(rough_p, 3.6, 'DISTANCE_TO_EDGE', 1.0, 'Distance', w=s.add(w, 3.0))
    width = s.add(0.012, s.mul(s.noise(p, 2.5, detail=2.0, w=s.add(w, 5.0)), 0.05))
    joints = s.sub(1.0, s.smooth(0.0, width, edges))
    face = s.smooth(0.0, 0.1, edges)
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.02, fracture)), s.smooth(0.42, 0.62, s.noise(p, 1.6, detail=2.0, w=s.add(w, 7.0))))
    ridge = s.mul(s.smooth(0.0, 0.08, fracture), s.mul(s.sub(1.0, s.smooth(0.08, 0.2, fracture)), s.smooth(0.5, 0.7, s.noise(p, 1.6, detail=2.0, w=s.add(w, 7.0)))))
    bedding = s.mul(s.smooth(0.9, 0.995, s.math('SINE', s.add(s.mul(v, 2.0 * math.pi * 4.0), s.mul(warp, 8.0)))), s.smooth(0.35, 0.6, s.noise(p, 1.2, detail=2.0, w=s.add(w, 13.0))))
    crust = s.noise(p, 2.2, detail=6.0, roughness=0.66, distortion=0.9, w=w)
    mottle = s.noise(p, 8.0, detail=4.0, roughness=0.65, distortion=0.5, w=s.add(w, 17.0))
    grain = s.noise(p, 55.0, detail=4.0, roughness=0.78, w=w)
    specks = s.sub(1.0, s.smooth(0.0, 0.12, s.voronoi(p, 38.0, 'F1', 1.0, 'Distance', w=s.add(w, 9.0))))
    streak = s.smooth(0.55, 0.85, s.noise(tall, 3.0, detail=3.0, w=s.add(tall_w, 4.0)))
    lichen = s.mul(s.smooth(0.7, 0.78, s.noise(p, 9.0, detail=3.0, w=s.add(w, 21.0))), s.smooth(0.52, 0.64, s.noise(p, 1.3, detail=2.0, w=s.add(w, 15.0))))
    rust = s.mul(s.smooth(0.72, 0.8, s.noise(p, 6.0, detail=3.0, w=s.add(w, 31.0))), s.smooth(0.56, 0.7, s.noise(p, 1.1, detail=2.0, w=s.add(w, 33.0))))
    base = s.mix(s.smooth(0.2, 0.85, crust), (0.07, 0.074, 0.08), (0.2, 0.198, 0.192))
    base = s.mix(s.mul(s.smooth(0.2, 0.85, block), 0.35), base, (0.24, 0.236, 0.228))
    base = s.mix(s.mul(s.smooth(0.65, 0.95, warm), 0.3), base, (0.16, 0.135, 0.11))
    base = s.mix(s.mul(s.smooth(0.35, 0.75, mottle), 0.4), base, (0.055, 0.057, 0.062))
    base = s.mix(s.mul(grain, 0.45), base, (0.05, 0.052, 0.055))
    base = s.mix(s.mul(specks, 0.35), base, (0.03, 0.03, 0.033))
    base = s.mix(s.mul(streak, 0.45), base, (0.042, 0.044, 0.05))
    base = s.mix(s.mul(ridge, 0.35), base, (0.27, 0.27, 0.265))
    base = s.mix(s.mul(bedding, 0.55), base, (0.038, 0.038, 0.041))
    base = s.mix(s.mul(cracks, 0.7), base, (0.028, 0.028, 0.031))
    base = s.mix(s.mul(joints, 0.94), base, (0.014, 0.014, 0.016))
    base = s.mix(s.mul(lichen, 0.7), base, (0.24, 0.24, 0.14))
    base = s.mix(s.mul(rust, 0.5), base, (0.26, 0.145, 0.065))
    height = s.add(s.mul(face, 0.35), s.add(s.mul(block, 0.22), s.add(s.mul(crust, 0.3), s.add(s.mul(mottle, 0.1), s.mul(grain, 0.06)))))
    height = s.sub(height, s.add(s.mul(joints, 0.35), s.add(s.mul(bedding, 0.12), s.mul(cracks, 0.12))))
    rough = s.remap(crust, 0.3, 0.7, 0.74, 0.9)
    return s.finish(base, rough, height=height), 3.0, 0.04, 3.0


def ground_scree():
    s = kit.Shader('GroundScree')
    p, w = s.torus(2.0)
    large = s.noise(p, 1.0, detail=3.0, w=w)
    grit = s.noise(p, 90.0, detail=2.0, w=w)
    dust = s.noise(p, 12.0, detail=3.0, w=s.add(w, 5.0))
    stones = s.voronoi(p, 5.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance', w=w)
    shade, warm, keep = s.separate(s.voronoi(p, 5.0, 'F1', 1.0, 'Color', w=w))
    gravel = s.voronoi(p, 14.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance', w=s.add(w, 3.0))
    fleck, tint, there = s.separate(s.voronoi(p, 14.0, 'F1', 1.0, 'Color', w=s.add(w, 3.0)))
    rock = s.mul(s.smooth(0.03, 0.2, stones), s.smooth(0.5, 0.62, keep))
    pebble = s.mul(s.mul(s.smooth(0.03, 0.17, gravel), s.smooth(0.35, 0.5, there)), s.sub(1.0, rock))
    base = s.mix(s.smooth(0.3, 0.7, large), (0.035, 0.032, 0.029), (0.06, 0.054, 0.048))
    base = s.mix(s.mul(s.smooth(0.4, 0.7, dust), 0.5), base, (0.075, 0.068, 0.06))
    base = s.mix(s.mul(grit, 0.4), base, (0.09, 0.085, 0.08))
    gravel_color = s.mix(fleck, (0.08, 0.08, 0.082), (0.2, 0.19, 0.18))
    gravel_color = s.mix(s.mul(s.smooth(0.65, 0.9, tint), 0.4), gravel_color, (0.17, 0.14, 0.11))
    base = s.mix(s.mul(pebble, 0.95), base, gravel_color)
    stone = s.mix(shade, (0.1, 0.102, 0.108), (0.25, 0.243, 0.232))
    stone = s.mix(s.mul(s.smooth(0.6, 0.9, warm), 0.4), stone, (0.2, 0.17, 0.13))
    base = s.mix(s.mul(rock, 0.97), base, stone)
    base = s.mix(s.mul(s.mul(grit, s.add(rock, pebble)), 0.25), base, (0.06, 0.06, 0.06))
    height = s.add(s.mul(rock, s.add(0.5, s.mul(shade, 0.3))), s.add(s.mul(pebble, s.add(0.25, s.mul(fleck, 0.15))), s.mul(grit, 0.04)))
    rough = s.sub(0.95, s.mul(rock, 0.12))
    return s.finish(base, rough, height=height), 2.0, 0.014, 3.0


def ground_alpine():
    s = kit.Shader('GroundAlpine')
    p, w = s.torus(2.0)
    patches = s.noise(p, 0.9, detail=3.0, w=w)
    tone = s.noise(p, 3.5, detail=4.0, w=w)
    clumps = s.voronoi(p, 7.0, 'F1', 1.0, 'Distance', w=w)
    variety, _, _ = s.separate(s.voronoi(p, 7.0, 'F1', 1.0, 'Color', w=w))
    blades = s.noise(p, 55.0, detail=2.0, w=w)
    soil = s.smooth(0.66, 0.76, s.noise(p, 2.2, detail=3.0, w=s.add(w, 3.0)))
    stones = s.mul(s.sub(1.0, s.smooth(0.05, 0.16, s.voronoi(p, 9.0, 'F1', 1.0, 'Distance', w=s.add(w, 9.0)))), s.smooth(0.68, 0.76, s.noise(p, 2.0, detail=2.0, w=s.add(w, 11.0))))
    base = s.mix(s.smooth(0.3, 0.7, tone), (0.06, 0.068, 0.03), (0.115, 0.115, 0.05))
    base = s.mix(s.mul(s.smooth(0.5, 0.75, patches), 0.7), base, (0.2, 0.172, 0.09))
    base = s.mix(s.mul(s.smooth(0.25, 0.75, variety), 0.5), base, (0.05, 0.08, 0.028))
    base = s.mix(s.mul(s.smooth(0.45, 0.7, blades), 0.45), base, (0.026, 0.034, 0.016))
    base = s.mix(s.mul(soil, 0.85), base, (0.068, 0.054, 0.04))
    base = s.mix(s.mul(stones, 0.85), base, (0.18, 0.178, 0.17))
    height = s.add(s.mul(s.sub(1.0, s.smooth(0.0, 0.5, clumps)), 0.45), s.add(s.mul(blades, 0.3), s.add(s.mul(s.sub(1.0, soil), 0.4), s.mul(stones, 0.5))))
    rough = s.remap(blades, 0.3, 0.7, 0.82, 0.95)
    return s.finish(base, rough, height=height), 2.0, 0.008, 2.0


def ground_water():
    s = kit.Shader('GroundWater')
    p, w = s.torus(2.0)
    swell = s.noise(p, 2.5, detail=3.0, w=w)
    chop = s.noise(p, 9.0, detail=2.0, w=s.add(w, 4.0))
    base = s.mix(s.smooth(0.35, 0.65, swell), (0.07, 0.1, 0.1), (0.095, 0.125, 0.12))
    height = s.add(s.mul(swell, 0.6), s.mul(chop, 0.4))
    rough = s.remap(chop, 0.3, 0.7, 0.04, 0.08)
    return s.finish(base, rough, height=height), 2.0, 0.003, 0.0


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    options = dict(zip(argv[::2], argv[1::2]))
    output = options['--out']
    preview = options.get('--preview')
    kit.reset()
    plane = kit.tileable_plane()
    only = options.get('--only')
    for build in (paving_stone, paving_bed, ground_grass, ground_dirt, ground_rock, ground_sand, ground_snow, ground_water, ground_cliff, ground_scree, ground_alpine):
        if only and build.__name__ != only:
            continue
        material, period, depth, cavity = build()
        kit.export_tileable(material, plane, os.path.join(output, material.name), period, depth, cavity, preview=preview)


if __name__ == '__main__':
    main()
