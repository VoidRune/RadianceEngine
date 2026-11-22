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

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.append(HERE)
import assetkit as kit
import propkit as pk
import emberisland as ei
import portalterrace as pt

NAME = 'EmberHeart'
H = ei.H
GLASS = H['EmberGlassRadius']
RING_OUT = H['EmberRingOuter']
SURROUND = H['EmberSurroundRadius']
HEART = Vector((0.0, 0.0, -H['EmberHeartDepth']))
STAR = H['EmberHeartRadius']
HARNESS = H['EmberHarnessRadius']
CHAIN_DEPTH = H['EmberChainDepth']
FLOOR = H['EmberCavernDepths'][-1]
VIEW = Vector((0.0, -0.54, 0.84)).normalized()

TOP = 0.014
GLASS_TOP = -0.012
GLASS_BOTTOM = -0.3
GLASS_EDGE = 4.62
LIP_IN = 4.3
COLLAR_BOTTOM = -0.95
CARDINAL = (0.0, 0.5 * math.pi, math.pi, 1.5 * math.pi)
DIAGONAL = tuple(a + 0.25 * math.pi for a in CARDINAL)
STRIP = (0.032, 0.018)
CHANNEL = (5.84, 5.9)
BAND = (6.72, 7.1)
OUTER = (7.6, 7.72)
INNER_DIAMOND = (4.74, 0.44, 0.34)
OUTER_DIAMOND = (7.66, 0.3, 0.3)
COURSES = (
    (H['EmberRuneInner'], H['EmberRuneOuter'], 'basalt', int(H['EmberRuneTiles'])),
    (CHANNEL[1] + 0.008, 6.3, 'lime', (0.42, 0.78)),
    (6.3 + 0.014, BAND[0] - 0.008, 'lime', (0.45, 0.85)),
    (BAND[1] + 0.008, OUTER[0] - 0.008, 'lime', (0.6, 1.1)),
)
JOINT = (0.008, 0.016)
LINK = (0.46, 0.28, 0.062)
CHAIN_SAG = 0.22
BLOCK_HEIGHT = (0.7, 1.6)
BLOCK_ARC = (1.1, 2.6)
WALL_BACK = 1.05
AO_DISTANCE = 0.5
SEED = 23


def polar(r, a, z=0.0):
    return Vector((r * math.cos(a), r * math.sin(a), z))


def arc(r, a0, a1, step=0.12):
    count = max(2, int(math.ceil(abs(a1 - a0) * r / step)) + 1)
    return [(r * math.cos(a0 + (a1 - a0) * i / (count - 1)), r * math.sin(a0 + (a1 - a0) * i / (count - 1))) for i in range(count)]


def circle(r, step=0.08):
    count = max(24, int(2.0 * math.pi * r / step))
    return [(r * math.cos(2.0 * math.pi * i / count), r * math.sin(2.0 * math.pi * i / count)) for i in range(count)]


def annulus(r0, r1, z0, z1, step=0.06):
    count = max(48, int(2.0 * math.pi * r1 / step))
    outer = [(r1 * math.cos(2.0 * math.pi * i / count), r1 * math.sin(2.0 * math.pi * i / count)) for i in range(count)]
    inner = [(r0 * math.cos(2.0 * math.pi * i / count), r0 * math.sin(2.0 * math.pi * i / count)) for i in range(count)]
    return pt.ring_prism(outer, inner, z0, z1)


def sector_poly(r0, r1, a0, a1, step=0.1):
    return arc(r0, a0, a1, step) + arc(r1, a1, a0, step)


def wrap(a):
    return a % (2.0 * math.pi)


def strips():
    return [(a, STRIP[0]) for a in CARDINAL] + [(a, STRIP[1]) for a in DIAGONAL]


def strip_clip(poly, a, half, sign):
    side = Vector((-math.sin(a), math.cos(a))) * sign
    return ei.clip_polygon(poly, side * (half + 0.007), -side)


def diamond_points(r, a, half_r, half_t, scale=1.0):
    out = Vector((math.cos(a), math.sin(a)))
    side = Vector((-out.y, out.x))
    center = out * r
    return [center + out * (half_r * scale), center + side * (half_t * scale), center - out * (half_r * scale), center - side * (half_t * scale)]


def diamonds():
    return [diamond_points(INNER_DIAMOND[0], a, INNER_DIAMOND[1], INNER_DIAMOND[2]) for a in CARDINAL] + [diamond_points(OUTER_DIAMOND[0], a, OUTER_DIAMOND[1], OUTER_DIAMOND[2]) for a in CARDINAL]


def diamond_clip(poly):
    for shape in diamonds():
        center = sum(shape, Vector((0.0, 0.0))) / 4.0
        if (ei.centroid(poly) - center).length > 2.0:
            continue
        edges = []
        for i in range(4):
            p, q = shape[i], shape[(i + 1) % 4]
            n = Vector((q.y - p.y, p.x - q.x)).normalized()
            if n.dot(p - center) < 0.0:
                n = -n
            edges.append((p, n))
        inside = any(all((v - p).dot(n) < 0.0 for p, n in edges) for v in poly)
        if not inside:
            continue
        c = ei.centroid(poly)
        p, n = max(edges, key=lambda e: (c - e[0]).dot(e[1]))
        poly = ei.clip_polygon(poly, p + n * 0.008, -n)
        if len(poly) < 3:
            return []
    return poly


def slab(poly, rng, top, depth=0.07, chips=(0, 2)):
    bm = pt.prism(poly, top - depth, top)
    pt.chip(bm, rng, rng.randint(*chips), rng.uniform(0.025, 0.05))
    pt.settle(bm, rng, top - 0.01, 0.006)
    return bm


def lay_course(builder, mats, r0, r1, kind, lengths, rng):
    marks = sorted(strips())
    mid = 0.5 * (r0 + r1)
    for i, (sa, ha) in enumerate(marks):
        sb, hb = marks[(i + 1) % len(marks)]
        if sb <= sa:
            sb += 2.0 * math.pi
        a0, a1 = sa, sb
        cursor = a0
        while cursor < a1 - 1e-4:
            if isinstance(lengths, int):
                nxt = min(cursor + (a1 - a0) / lengths, a1)
                if a1 - nxt < 1e-3:
                    nxt = a1
            else:
                nxt = min(cursor + rng.uniform(*lengths) / mid, a1)
                if a1 - nxt < 0.3 / mid:
                    nxt = a1
            joint = rng.uniform(*JOINT) / mid
            s0 = cursor + (joint * 0.5 if cursor > a0 else 0.0)
            s1 = nxt - (joint * 0.5 if nxt < a1 else 0.0)
            ri = r0 + rng.uniform(-0.004, 0.004)
            ro = r1 + rng.uniform(-0.004, 0.004)
            poly = [Vector((x + rng.uniform(-0.005, 0.005), y + rng.uniform(-0.005, 0.005))) for x, y in sector_poly(ri, ro, s0, s1)]
            if cursor <= a0 + 1e-6:
                poly = strip_clip(poly, sa, ha, 1.0)
            if nxt >= a1 - 1e-6 and len(poly) >= 3:
                poly = strip_clip(poly, sb, hb, -1.0)
            if len(poly) >= 3:
                poly = diamond_clip(poly)
            if len(poly) >= 3 and abs(ei.area(poly)) > 0.01:
                material = mats['basalt'] if kind == 'basalt' else (mats['basalt'] if rng.random() < 0.12 else mats['lime'])
                top = TOP + rng.uniform(-0.003, 0.004)
                builder.add('Slabs', slab([tuple(p) for p in poly], rng, top), material, bevel=0.012, angle=30.0)
            cursor = nxt


def diamond(builder, mats, r, a, half_r, half_t, z, rng):
    center = Vector((math.cos(a), math.sin(a))) * r

    def points(scale):
        return [tuple(p) for p in diamond_points(r, a, half_r, half_t, scale)]

    builder.add('Brass', pt.ring_prism(points(1.0), points(0.72), z - 0.05, z), mats['brass'], bevel=0.006, angle=30.0)
    builder.add('Inlay', pt.prism(points(0.74), z - 0.05, z - 0.006), mats['inlay'], bevel=0.0)
    builder.add('Brass', pt.ring_prism(points(0.58), points(0.34), z - 0.04, z - 0.001), mats['brass'], bevel=0.004, angle=30.0)
    builder.add('Brass', pt.ring_prism(points(0.66), points(0.62), z - 0.04, z - 0.002), mats['brass'], bevel=0.0)
    gem = bmesh.new()
    rim = [gem.verts.new((p[0], p[1], z - 0.012)) for p in points(0.3)]
    tip = gem.verts.new((center.x, center.y, z + 0.004))
    for i in range(4):
        gem.faces.new((rim[i], rim[(i + 1) % 4], tip))
    gem.faces.new(rim[::-1])
    bmesh.ops.recalc_face_normals(gem, faces=gem.faces[:])
    builder.add('Lights', gem, mats['amber'], bevel=0.0)
    for k in range(4):
        corner = Vector(points(0.68)[k])
        bolt = pk.lathe([(0.0, z + 0.001), (0.014, z), (0.02, z - 0.006)], 6)
        bmesh.ops.translate(bolt, vec=(corner.x, corner.y, 0.0), verts=bolt.verts)
        builder.add('Brass', bolt, mats['brass'], bevel=0.0)


def retaining_ring(builder, mats, rng):
    profile = [
        (GLASS, GLASS_TOP), (GLASS, TOP - 0.008), (GLASS + 0.014, TOP), (4.6, TOP), (4.6, TOP - 0.012),
        (4.67, TOP - 0.012), (4.67, TOP), (4.93, TOP), (4.935, TOP - 0.006), (4.965, TOP - 0.006),
        (4.97, TOP - 0.002), (RING_OUT, TOP - 0.002), (RING_OUT, -0.12), (GLASS_EDGE + 0.004, -0.12),
        (GLASS_EDGE + 0.004, GLASS_TOP),
    ]
    builder.add('Brass', ei.loop_lathe([(r, z) for r, z in profile], 320), mats['brass'], bevel=0.0)
    for radius, width in ((4.785, 0.006), (4.81, 0.004)):
        groove = annulus(radius - width, radius + width, TOP - 0.004, TOP + 0.0005, step=0.05)
        builder.add('Inlay', groove, mats['inlay'], bevel=0.0)
    for k in range(72):
        a = (k + 0.5) * 2.0 * math.pi / 72.0
        bolt = pk.lathe([(0.0, TOP - 0.004), (0.017, TOP - 0.006), (0.024, TOP - 0.012)], 6, phase=rng.uniform(0.0, 1.0))
        bmesh.ops.translate(bolt, vec=polar(4.635, a), verts=bolt.verts)
        builder.add('Brass', bolt, mats['brass'], bevel=0.0)
    for k in range(48):
        a = (k + 0.25) * 2.0 * math.pi / 48.0
        rivet = pk.lathe([(0.0, TOP + 0.0), (0.012, TOP - 0.002), (0.016, TOP - 0.006)], 8)
        bmesh.ops.translate(rivet, vec=polar(5.01, a), verts=rivet.verts)
        builder.add('Brass', rivet, mats['brass'], bevel=0.0)
    collar = [
        (GLASS_EDGE + 0.004, -0.12), (RING_OUT, -0.12), (RING_OUT - 0.02, -0.45), (5.1, -0.47), (5.1, -0.62),
        (4.98, -0.64), (4.98, COLLAR_BOTTOM), (4.62, COLLAR_BOTTOM), (4.62, -0.8), (4.5, -0.78), (4.5, -0.6),
        (4.38, -0.58), (4.38, -0.42), (LIP_IN, -0.4), (LIP_IN, GLASS_BOTTOM), (GLASS_EDGE + 0.004, GLASS_BOTTOM),
    ]
    builder.add('Collar', ei.loop_lathe(collar, 240), mats['bronze'], bevel=0.0)
    seam = [(LIP_IN - 0.012, -0.375), (LIP_IN + 0.002, -0.375), (LIP_IN + 0.002, -0.345), (LIP_IN - 0.012, -0.345)]
    builder.add('Lights', ei.loop_lathe(seam, 240), mats['amber'], bevel=0.0)
    for k in range(60):
        a = (k + 0.5) * 2.0 * math.pi / 60.0
        lens = pk.lathe([(0.0, 0.012), (0.018, 0.006), (0.022, 0.0)], 10)
        bmesh.ops.rotate(lens, cent=(0.0, 0.0, 0.0), matrix=Matrix.Rotation(-0.5 * math.pi, 3, 'Y'), verts=lens.verts)
        bmesh.ops.rotate(lens, cent=(0.0, 0.0, 0.0), matrix=Matrix.Rotation(a, 3, 'Z'), verts=lens.verts)
        bmesh.ops.translate(lens, vec=polar(4.375, a, -0.5), verts=lens.verts)
        builder.add('Lights', lens, mats['amber'], bevel=0.0)
    for k in range(36):
        a = (k + 0.5) * 2.0 * math.pi / 36.0
        bolt = pk.lathe([(0.0, -0.43), (0.03, -0.42), (0.034, -0.4)], 6)
        bmesh.ops.translate(bolt, vec=polar(4.45, a), verts=bolt.verts)
        builder.add('Collar', bolt, mats['iron'], bevel=0.0)
    for k in range(24):
        a = (k + 0.5) * 2.0 * math.pi / 24.0
        rib = pt.prism([tuple(polar(4.52, a - 0.012)[:2]), tuple(polar(4.95, a - 0.012)[:2]), tuple(polar(4.95, a + 0.012)[:2]), tuple(polar(4.52, a + 0.012)[:2])], -0.93, -0.6)
        builder.add('Collar', rib, mats['bronze'], bevel=0.008, angle=30.0)
    for a in CARDINAL:
        diamond(builder, mats, INNER_DIAMOND[0], a, INNER_DIAMOND[1], INNER_DIAMOND[2], TOP + 0.003, rng)


def glass_ribs(builder, mats):
    for a in CARDINAL:
        out = Vector((math.cos(a), math.sin(a)))
        side = Vector((-out.y, out.x))
        start, end = 0.58, GLASS + 0.01
        poly = [tuple(out * start - side * 0.03), tuple(out * end - side * 0.03), tuple(out * end + side * 0.03), tuple(out * start + side * 0.03)]
        builder.add('Brass', pt.prism(poly, GLASS_TOP - 0.03, TOP - 0.002), mats['brass'], bevel=0.005, angle=30.0)
    builder.add('Brass', annulus(0.5, 0.6, GLASS_TOP - 0.03, TOP - 0.002, step=0.05), mats['brass'], bevel=0.005, angle=30.0)


def glass_disc():
    profile = [(0.0, GLASS_BOTTOM), (GLASS_EDGE - 0.04, GLASS_BOTTOM), (GLASS_EDGE, GLASS_BOTTOM + 0.04), (GLASS_EDGE, GLASS_TOP - 0.03), (GLASS_EDGE - 0.03, GLASS_TOP), (0.0, GLASS_TOP)]
    return pk.lathe(profile, 160)


def surround(builder, mats, rng):
    for r0, r1, kind, lengths in COURSES:
        lay_course(builder, mats, r0, r1, kind, lengths, rng)
    builder.add('Brass', annulus(CHANNEL[0], CHANNEL[1], TOP - 0.06, TOP - 0.01), mats['brass'], bevel=0.0)
    for k in range(32):
        a = (k + 0.5) * 2.0 * math.pi / 32.0
        if any(abs(math.remainder(a - s, 2.0 * math.pi)) < 0.03 for s, _ in strips()):
            continue
        c = polar(0.5 * (CHANNEL[0] + CHANNEL[1]), a)
        bezel = pk.lathe([(0.012, TOP - 0.006), (0.024, TOP - 0.004), (0.026, TOP - 0.012)], 12)
        bmesh.ops.translate(bezel, vec=(c.x, c.y, 0.0), verts=bezel.verts)
        builder.add('Brass', bezel, mats['brass'], bevel=0.0)
        lens = pk.lathe([(0.0, TOP - 0.005), (0.013, TOP - 0.008), (0.014, TOP - 0.012)], 10)
        bmesh.ops.translate(lens, vec=(c.x, c.y, 0.0), verts=lens.verts)
        builder.add('Lights', lens, mats['amber'], bevel=0.0)
    for r0, r1 in ((BAND[0], BAND[0] + 0.03), (BAND[1] - 0.03, BAND[1])):
        builder.add('Brass', annulus(r0, r1, TOP - 0.05, TOP), mats['brass'], bevel=0.0)
    engraved_band(builder, mats, rng)
    builder.add('Brass', annulus(OUTER[0], OUTER[1], TOP - 0.06, TOP), mats['brass'], bevel=0.0)
    builder.add('Inlay', annulus(0.5 * (OUTER[0] + OUTER[1]) - 0.007, 0.5 * (OUTER[0] + OUTER[1]) + 0.007, TOP - 0.004, TOP + 0.0005), mats['inlay'], bevel=0.0)
    for k in range(16):
        a = (k + 0.5) * 2.0 * math.pi / 16.0
        c = polar(0.5 * (OUTER[0] + OUTER[1]), a)
        lens = pk.lathe([(0.0, TOP + 0.001), (0.016, TOP - 0.002), (0.018, TOP - 0.01)], 10)
        bmesh.ops.translate(lens, vec=(c.x, c.y, 0.0), verts=lens.verts)
        builder.add('Lights', lens, mats['amber'], bevel=0.0)
    for a, half in strips():
        out = Vector((math.cos(a), math.sin(a)))
        side = Vector((-out.y, out.x))
        start, end = RING_OUT - 0.01, OUTER[1]
        poly = [tuple(out * start - side * half), tuple(out * end - side * half), tuple(out * end + side * half), tuple(out * start + side * half)]
        builder.add('Brass', pt.prism(poly, TOP - 0.05, TOP - 0.0005), mats['brass'], bevel=0.004, angle=30.0)
    for a in CARDINAL:
        diamond(builder, mats, OUTER_DIAMOND[0], a, OUTER_DIAMOND[1], OUTER_DIAMOND[2], TOP + 0.003, rng)
    builder.add('Bed', annulus(RING_OUT - 0.02, SURROUND, -0.5, -0.004, step=0.1), mats['mortar'], bevel=0.0)


def engraved_band(builder, mats, rng):
    r0, r1 = BAND[0] + 0.03, BAND[1] - 0.03
    panels = 16
    for k in range(panels):
        a0 = k * 2.0 * math.pi / panels
        a1 = (k + 1) * 2.0 * math.pi / panels
        inset = 0.006 / r0
        builder.add('Inlay', pt.prism(sector_poly(r0 + 0.004, r1 - 0.004, a0 + inset, a1 - inset, 0.08), TOP - 0.05, TOP - 0.004), mats['inlay'], bevel=0.003, angle=30.0)
        mid = 0.5 * (r0 + r1)
        orbit = mid + rng.uniform(-0.05, 0.05)
        b0, b1 = a0 + 0.05, a1 - 0.05
        line = arc(orbit, b0, b1, 0.04)
        builder.add('Brass', pt.prism(pt.stroke(line, 0.012), TOP - 0.02, TOP - 0.0015), mats['brass'], bevel=0.0)
        t = rng.uniform(0.25, 0.75)
        body = polar(orbit, b0 + (b1 - b0) * t)
        size = rng.uniform(0.032, 0.05)
        ring = pt.ring_prism(circle(size, 1.0), circle(size - 0.01, 1.0), TOP - 0.02, TOP - 0.0012)
        bmesh.ops.translate(ring, vec=(body.x, body.y, 0.0), verts=ring.verts)
        builder.add('Brass', ring, mats['brass'], bevel=0.0)
        if k % 2 == 0:
            disc = pt.prism(circle(size * 0.45, 0.01), TOP - 0.02, TOP - 0.0012)
            bmesh.ops.translate(disc, vec=(body.x, body.y, 0.0), verts=disc.verts)
            builder.add('Lights', disc, mats['amber'], bevel=0.0)
        if k % 4 == 1:
            c = polar(mid + rng.uniform(-0.04, 0.04), b0 + (b1 - b0) * (1.0 - t))
            moon = [tuple(Vector(p) + c.to_2d()) for p in arc(0.04, 0.4, 0.4 + math.pi * 1.25, 0.012)]
            builder.add('Brass', pt.prism(pt.stroke(moon, 0.01), TOP - 0.02, TOP - 0.0015), mats['brass'], bevel=0.0)


def wall_point(depth, angle):
    return ei.cavern_wall(depth, angle)


def hull_block(points, rng, size):
    bm = bmesh.new()
    for p in points:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=bm.verts[:])
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(3.0), verts=bm.verts[:], edges=bm.edges[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    if len(bm.faces) < 4:
        bm.free()
        return None
    ei.fracture(bm, rng, size, rng.randint(1, 3), rng.randint(0, 1))
    if not bm.verts:
        bm.free()
        return None
    ei.finish(bm, rng, size, rng.random() < 0.3)
    return bm


def clear_of_heart(p, margin):
    return (p - HEART).length > HARNESS + margin


def chain_line(azimuth):
    return polar(1.0, math.radians(azimuth))


def near_chain(angle, depth):
    for azimuth in H['EmberChainAzimuths']:
        if abs(math.remainder(angle - math.radians(azimuth), 2.0 * math.pi)) < 0.2 and depth < 4.5:
            return True
    return False


WALL_REF = 7.0
BACK_SHELL = 0.42
WALL_SQUASH = 1.6
LIME_BANDS = ((3.4, 4.2), (7.6, 8.3), (11.2, 11.7))


def wall_cells(rng):
    period = 2.0 * math.pi * WALL_REF
    top, bottom = 0.45, FLOOR - 0.2
    points = []
    for _ in range(9000):
        u = rng.uniform(0.0, period)
        v = rng.uniform(top, bottom)
        spacing = rng.uniform(1.5, 2.6) * (1.35 if ei.fbm2(u, v, 0.08, 3.3) > 0.15 else 1.0)
        iso = Vector((u, v * WALL_SQUASH))
        clear = True
        for q, d in points:
            du = abs(q.x - iso.x)
            du = min(du, period - du)
            if du * du + (q.y - iso.y) ** 2 < (0.5 * (spacing + d)) ** 2:
                clear = False
                break
        if clear:
            points.append((iso, spacing))
    cells = []
    for i, (p, d) in enumerate(points):
        poly = [p + Vector((x, y)) for x, y in ((-4.0, -4.0), (4.0, -4.0), (4.0, 4.0), (-4.0, 4.0))]
        for j, (q, _) in enumerate(points):
            if j == i:
                continue
            for shift in (-period, 0.0, period):
                qq = Vector((q.x + shift, q.y))
                if (qq - p).length > 8.0:
                    continue
                poly = ei.clip_polygon(poly, (p + qq) * 0.5, qq - p)
        top_iso, bottom_iso = top * WALL_SQUASH, bottom * WALL_SQUASH
        poly = ei.clip_polygon(poly, Vector((0.0, top_iso - 0.3)), Vector((0.0, -1.0)))
        if len(poly) >= 3:
            poly = ei.clip_polygon(poly, Vector((0.0, bottom_iso + 0.5)), Vector((0.0, 1.0)))
        if len(poly) >= 3:
            cells.append((p, poly))
    return cells


def backing(sink, rng):
    columns = 56
    depths = list(np.arange(0.4, FLOOR + 0.6, 0.75))
    bm = bmesh.new()
    grid = []
    for j, depth in enumerate(depths):
        row = []
        for i in range(columns):
            angle = 2.0 * math.pi * (i + 0.5 * (j % 2)) / columns
            wall = wall_point(min(depth, FLOOR), angle)
            if wall <= 0.0:
                wall = 3.0
            r = wall + BACK_SHELL + rng.uniform(-0.12, 0.12)
            row.append(bm.verts.new(polar(r, angle + rng.uniform(-0.02, 0.02), -depth + rng.uniform(-0.1, 0.1))))
        grid.append(row)
    for j in range(len(depths) - 1):
        a, b = grid[j], grid[j + 1]
        for i in range(columns):
            n = (i + 1) % columns
            if j % 2 == 0:
                bm.faces.new((a[i], b[i], a[n]))
                bm.faces.new((a[n], b[i], b[n]))
            else:
                bm.faces.new((a[i], b[i], b[n]))
                bm.faces.new((a[i], b[n], a[n]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    for f in bm.faces:
        if f.normal.dot(f.calc_center_median() * -1.0 + Vector((0.0, 0.0, f.calc_center_median().z))) < 0.0:
            f.normal_flip()
    sink.add(bm, ei.BASALT, 'Cavern')


MONOLITHS = [math.radians(a) for a in H['EmberMonolithAzimuths']]
MONO_DEPTH = H['EmberMonolithDepth']
MONO_WIDTH = H['EmberMonolithWidth']
MONO_HEIGHT = H['EmberMonolithHeight']
MONO_THICK = H['EmberMonolithThickness']
FALLS = [math.radians(a) for a in H['EmberLavaFalls']]
SPOUT_DEPTH = H['EmberSpoutDepth']
FALL_REACH = 6.5
SCORCHED = (2.2, 5.8)


def in_monolith(angle, depth, margin):
    for a in MONOLITHS:
        wall = wall_point(MONO_DEPTH, a)
        if abs(math.remainder(angle - a, 2.0 * math.pi)) * wall < 0.5 * MONO_WIDTH + margin and abs(depth - MONO_DEPTH) < 0.5 * MONO_HEIGHT + margin:
            return True
    return False


def in_fall(angle, depth, margin):
    return depth > SPOUT_DEPTH - 0.8 and any(abs(math.remainder(angle - a, 2.0 * math.pi)) * FALL_REACH < margin for a in FALLS)


def inner_limit(depth):
    if depth > 9.0:
        return 2.4
    return float(np.interp(depth, (0.0, 1.3, 2.6), (RING_OUT + 0.15, RING_OUT + 0.05, 3.4)))


def cavern(sink, rng):
    backing(sink, rng)
    blocks = 0
    for seed, poly in wall_cells(rng):
        poly = ei.shrink(poly, rng.uniform(0.015, 0.04))
        if len(poly) < 3:
            continue
        u0, v0 = seed.x, seed.y / WALL_SQUASH
        if in_monolith(u0 / WALL_REF, v0, 0.45) or in_fall(u0 / WALL_REF, v0, 0.9):
            continue
        lean = 0.32 * ei.fbm2(u0, v0, 0.11, 7.1) + 0.18 * ei.fbm2(u0, v0, 0.35, 2.9)
        jut = -0.35 + 1.25 * lean + rng.uniform(-0.18, 0.12)
        if rng.random() < 0.05 and v0 > 2.0:
            jut += rng.uniform(0.35, 0.7)
        jut = min(max(jut, -1.1), BACK_SHELL - 0.08)
        inner_points = []
        outer_points = []
        for q in poly:
            angle = q.x / WALL_REF
            depth = min(max(q.y / WALL_SQUASH, 0.0), FLOOR)
            wall = wall_point(depth, angle)
            if wall <= 0.0:
                wall = 2.4
            inner = max(wall + jut + rng.uniform(-0.07, 0.07), inner_limit(depth))
            inner_points.append(polar(inner, angle, -depth))
            outer_points.append(polar(wall + WALL_BACK, angle, -depth))
        center_angle = u0 / WALL_REF
        center_wall = wall_point(min(v0, FLOOR), center_angle)
        if center_wall <= 0.0:
            continue
        bulge = polar(max(center_wall + jut - rng.uniform(0.05, 0.25), inner_limit(v0) - 0.1), center_angle, -v0)
        if v0 < 6.0 and not clear_of_heart(bulge, 1.2):
            continue
        size = max((Vector(inner_points[0]) - Vector(inner_points[len(inner_points) // 2])).length, 0.8)
        bm = hull_block(inner_points + outer_points + [bulge], rng, size)
        if bm is None:
            continue
        inward = -polar(1.0, center_angle)
        for _ in range(rng.randint(1, 2)):
            tilt = Vector((0.0, 0.0, rng.uniform(-0.75, 0.6)))
            normal = (inward + ei.random_unit(rng) * 0.45 + tilt).normalized()
            kit.cut(bm, bulge - inward * rng.uniform(0.05, 0.3), normal)
        if len(bm.faces) < 4:
            bm.free()
            continue
        ei.finish(bm, rng, size, rng.random() < 0.25)
        lime = any(a <= v0 <= b for a, b in LIME_BANDS)
        tone = ei.LIMESTONE if (lime and rng.random() < 0.8) or rng.random() < 0.05 else ei.BASALT
        if SCORCHED[0] < v0 < SCORCHED[1]:
            tone = ei.BASALT
        sink.add(bm, tone, 'Cavern')
        blocks += 1
    spurs = 0
    attempts = 0
    while spurs < 14 and attempts < 400:
        attempts += 1
        angle = rng.uniform(0.0, 2.0 * math.pi)
        depth = rng.uniform(2.2, 11.5)
        if near_chain(angle, depth) or in_monolith(angle, depth, 1.0) or in_fall(angle, depth, 1.4):
            continue
        wall = wall_point(depth, angle)
        if wall <= 0.0:
            continue
        out = polar(1.0, angle)
        side = Vector((-out.y, out.x, 0.0))
        root = polar(wall + 0.4, angle, -depth)
        length = rng.uniform(1.4, 3.2)
        droop = rng.uniform(-0.25, 0.55)
        direction = (-out + Vector((0.0, 0.0, -droop))).normalized()
        tip = root + direction * length
        if tip.to_2d().length < (4.9 if depth < 5.5 else 3.4) or not clear_of_heart(tip, 1.3):
            continue
        width = rng.uniform(0.55, 1.1)
        points = []
        for t, scale in ((0.0, 1.0), (0.45, rng.uniform(0.7, 0.95)), (1.0, rng.uniform(0.12, 0.3))):
            center = root + (tip - root) * t
            for k in range(5):
                phi = 2.0 * math.pi * k / 5.0 + rng.uniform(-0.3, 0.3)
                up = direction.cross(side).normalized()
                points.append(center + (side * math.cos(phi) + up * math.sin(phi) * 0.8) * width * scale * rng.uniform(0.75, 1.1))
        bm = hull_block(points, rng, length)
        if bm is not None:
            sink.add(bm, ei.BASALT if rng.random() < 0.8 else ei.LIMESTONE, 'Cavern')
            spurs += 1
    hangs = 0
    for _ in range(300):
        if hangs >= 10:
            break
        angle = rng.uniform(0.0, 2.0 * math.pi)
        r = rng.uniform(5.2, 7.2) + 0.45
        depth = 0.9
        if r > wall_point(1.6, angle) - 0.2 or near_chain(angle, 2.0):
            continue
        root = polar(r, angle, -0.7)
        length = rng.uniform(1.0, 2.3)
        poly = [Vector((root.x, root.y)) + Vector((math.cos(2.0 * math.pi * k / 5.0 + rng.uniform(-0.3, 0.3)), math.sin(2.0 * math.pi * k / 5.0 + rng.uniform(-0.3, 0.3)))) * rng.uniform(0.35, 0.6) for k in range(5)]
        bm = ei.tip_bm(poly, root.z, length, rng)
        ei.finish(bm, rng, length, False)
        sink.add(bm, ei.BASALT, 'Cavern')
        hangs += 1
    rubble = 0
    for _ in range(90):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        depth = rng.uniform(FLOOR - 2.6, FLOOR - 0.6)
        wall = wall_point(depth, angle)
        if wall <= 0.0:
            continue
        r = rng.uniform(0.0, wall + 0.2)
        center = polar(r, angle, -depth)
        size = rng.uniform(0.5, 1.4)
        points = [center + ei.random_unit(rng) * size * rng.uniform(0.6, 1.0) for _ in range(10)]
        bm = hull_block(points, rng, size)
        if bm is not None:
            sink.add(bm, ei.BASALT if rng.random() < 0.85 else ei.LIMESTONE, 'Cavern')
            rubble += 1
    floor = bmesh.new()
    bmesh.ops.create_circle(floor, cap_ends=True, radius=4.0, segments=40)
    bmesh.ops.translate(floor, vec=(0.0, 0.0, -FLOOR - 0.5), verts=floor.verts)
    sink.add(floor, ei.HIDDEN, 'Cavern')
    print(f'cavern blocks {blocks} spurs {spurs} hangs {hangs} rubble {rubble}')


GEODE_CLUSTERS = 220
GEODE_BAND = (1.9, 8.4)
GEODE_PEAK = 4.4
GEODE_SPREAD = 2.4
DRIFTS = 6


def crystal(bm, base, axis, length, radius, rng):
    axis = axis.normalized()
    u = axis.orthogonal().normalized()
    v = axis.cross(u)
    twist = rng.uniform(0.0, 2.0 * math.pi)
    shoulder = rng.uniform(0.6, 0.8)
    taper = rng.uniform(0.72, 0.95)
    lower, upper = [], []
    for k in range(6):
        a = twist + 2.0 * math.pi * k / 6.0
        d = u * math.cos(a) + v * math.sin(a)
        jitter = rng.uniform(0.85, 1.15)
        lower.append(bm.verts.new(base + d * (radius * jitter)))
        upper.append(bm.verts.new(base + axis * (length * shoulder) + d * (radius * taper * jitter)))
    apex = bm.verts.new(base + axis * length + (u * rng.uniform(-1.0, 1.0) + v * rng.uniform(-1.0, 1.0)) * (radius * 0.15))
    for k in range(6):
        n = (k + 1) % 6
        bm.faces.new((lower[k], lower[n], upper[n], upper[k]))
        bm.faces.new((upper[k], upper[n], apex))
    bm.faces.new(list(reversed(lower)))


def cluster(base, axis, size, rng, count):
    bm = bmesh.new()
    axis = axis.normalized()
    u = axis.orthogonal().normalized()
    v = axis.cross(u)
    crystal(bm, base, axis, size, size * rng.uniform(0.13, 0.19), rng)
    for _ in range(count - 1):
        a = rng.uniform(0.0, 2.0 * math.pi)
        offset = (u * math.cos(a) + v * math.sin(a)) * (size * rng.uniform(0.08, 0.3))
        direction = (axis + ei.random_unit(rng) * rng.uniform(0.25, 0.65)).normalized()
        length = size * rng.uniform(0.3, 0.8)
        crystal(bm, base + offset - axis * 0.04, direction, length, length * rng.uniform(0.13, 0.2), rng)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def geode(builder, mats, tree, rng):
    placed = 0
    for _ in range(GEODE_CLUSTERS * 6):
        if placed >= GEODE_CLUSTERS:
            break
        azimuth = rng.uniform(0.0, 2.0 * math.pi)
        elevation = rng.uniform(-0.95, 0.5)
        direction = Vector((math.cos(azimuth) * math.cos(elevation), math.sin(azimuth) * math.cos(elevation), math.sin(elevation)))
        hit, normal, _, distance = tree.ray_cast(HEART, direction, 20.0)
        if hit is None or distance < 3.6:
            continue
        depth = -hit.z
        angle = math.atan2(hit.y, hit.x)
        if not GEODE_BAND[0] <= depth <= GEODE_BAND[1] or near_chain(angle, depth) or in_monolith(angle, depth, 0.35) or in_fall(angle, depth, 1.1):
            continue
        weight = math.exp(-((depth - GEODE_PEAK) / GEODE_SPREAD) ** 2)
        if rng.random() > 0.15 + 0.85 * weight:
            continue
        inward = -direction
        if normal.dot(inward) < 0.2:
            continue
        axis = (inward * 0.6 + normal * 0.4 + ei.random_unit(rng) * 0.2).normalized()
        size = rng.uniform(0.55, 1.15) * (0.6 + 0.8 * weight)
        builder.add('Geode', cluster(hit - axis * 0.12, axis, size, rng, rng.randint(3, 7)), mats['crystal'], bevel=0.0)
        placed += 1
    print(f'geode clusters {placed}')


def drifts(builder, mats, rng):
    for k in range(DRIFTS):
        size = rng.uniform(0.32, 0.55)
        points = [Vector((ei.random_unit(rng).x * size * 1.25, ei.random_unit(rng).y * size, ei.random_unit(rng).z * size * 0.85)) for _ in range(16)]
        bm = hull_block(points, rng, size)
        if bm is None:
            continue
        builder.add(f'Drift{k}', bm, mats['rock'], bevel=0.0)
        for _ in range(rng.randint(1, 2)):
            out = ei.random_unit(rng)
            builder.add(f'Drift{k}', cluster(out * (size * 0.55), out, rng.uniform(0.28, 0.5), rng, rng.randint(2, 4)), mats['crystal'], bevel=0.0)


def monoliths(builder, mats, rng):
    up = Vector((0.0, 0.0, 1.0))
    hw, hh = 0.5 * MONO_WIDTH, 0.5 * MONO_HEIGHT
    for k, a in enumerate(MONOLITHS):
        front = wall_point(MONO_DEPTH, a) - 0.5
        out = Vector((math.cos(a), math.sin(a), 0.0))
        side = Vector((-out.y, out.x, 0.0))
        points = []
        for r, scale in ((front, 1.0), (front + MONO_THICK, 1.12)):
            for sx, sz in ((-1.0, -1.0), (1.0, -1.0), (1.0, 1.0), (-1.0, 1.0)):
                taper = 0.84 if sz > 0.0 else 1.0
                drop = rng.uniform(-0.16, 0.0) if sz > 0.0 else 0.0
                points.append(out * r + side * (sx * hw * scale * taper) + up * (-MONO_DEPTH + sz * hh * scale + drop))
        points.append(out * (front + 0.35) + side * rng.uniform(-0.18, 0.18) + up * (-MONO_DEPTH + hh + rng.uniform(0.06, 0.16)))
        bm = bmesh.new()
        for p in points:
            bm.verts.new(p)
        bmesh.ops.convex_hull(bm, input=bm.verts[:])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        ei.finish(bm, rng, MONO_HEIGHT, False)
        builder.add(f'Monolith{k}', bm, mats['basalt'], bevel=0.0)
        for _ in range(rng.randint(2, 4)):
            base = out * (front + rng.uniform(0.1, 0.6)) + side * rng.uniform(-hw - 0.3, hw + 0.3) + up * (-MONO_DEPTH - hh - rng.uniform(0.0, 0.12))
            size = rng.uniform(0.18, 0.34)
            rubble = hull_block([base + ei.random_unit(rng) * size for _ in range(9)], rng, size)
            if rubble is not None:
                builder.add('Footing', rubble, mats['rock'], bevel=0.0)
    print(f'monoliths {len(MONOLITHS)}')


def spouts(builder, mats, rng):
    up = Vector((0.0, 0.0, 1.0))
    for k, a in enumerate(FALLS):
        wall = wall_point(SPOUT_DEPTH, a)
        out = Vector((math.cos(a), math.sin(a), 0.0))
        side = Vector((-out.y, out.x, 0.0))
        points = [out * rng.uniform(wall - 0.3, wall + 0.55) + side * rng.uniform(-0.42, 0.42) + up * (-SPOUT_DEPTH + rng.uniform(-0.28, 0.16)) for _ in range(16)]
        points += [out * (wall - 0.3) + side * s + up * (-SPOUT_DEPTH + 0.16) for s in (-0.42, 0.42)]
        points += [out * (wall + 0.55) + side * s + up * (-SPOUT_DEPTH + z) for s in (-0.42, 0.42) for z in (-0.28, 0.16)]
        bm = hull_block(points, rng, 0.7)
        if bm is not None:
            builder.add(f'Spout{k}', bm, mats['rock'], bevel=0.0)


def fringe(builder, mats, rng):
    down = Vector((0.0, 0.0, -1.0))
    chains = [math.radians(a) for a in H['EmberChainAzimuths']]
    placed = 0
    for k in range(150):
        a = 2.0 * math.pi * (k + rng.uniform(-0.35, 0.35)) / 150.0
        if any(abs(math.remainder(a - c, 2.0 * math.pi)) < 0.09 for c in chains) or rng.random() < 0.2:
            continue
        inward = -polar(1.0, a)
        base = polar(rng.uniform(4.68, 4.94), a, COLLAR_BOTTOM + 0.06)
        axis = (down + inward * rng.uniform(0.15, 0.5) + ei.random_unit(rng) * 0.2).normalized()
        size = rng.uniform(0.18, 0.5) * (1.7 if rng.random() < 0.12 else 1.0)
        builder.add('Fringe', cluster(base, axis, size, rng, rng.randint(2, 4)), mats['fringe'], bevel=0.0)
        placed += 1
    for k in range(110):
        a = 2.0 * math.pi * (k + rng.uniform(-0.35, 0.35)) / 110.0
        if rng.random() < 0.3:
            continue
        inward = -polar(1.0, a)
        base = polar(4.53, a, -0.74)
        axis = (inward * 0.7 + down * 0.55 + ei.random_unit(rng) * 0.2).normalized()
        builder.add('Fringe', cluster(base, axis, rng.uniform(0.1, 0.26), rng, rng.randint(1, 3)), mats['fringe'], bevel=0.0)
        placed += 1
    print(f'fringe clusters {placed}')


def sweep(points, frames, profile, closed):
    bm = bmesh.new()
    rings = [[bm.verts.new(c + fx * px + fy * py) for px, py in profile] for c, (fx, fy) in zip(points, frames)]
    n = len(profile)
    count = len(rings)
    for i in range(count if closed else count - 1):
        a, b = rings[i], rings[(i + 1) % count]
        for j in range(n):
            k = (j + 1) % n
            bm.faces.new((a[j], a[k], b[k], b[j]))
    if not closed:
        bm.faces.new(rings[0])
        bm.faces.new(rings[-1][::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def rect(half_r, half_a, chamfer, offset=(0.0, 0.0)):
    ox, oy = offset
    c = chamfer
    return [(ox + half_r, oy - half_a + c), (ox + half_r, oy + half_a - c), (ox + half_r - c, oy + half_a), (ox - half_r + c, oy + half_a),
            (ox - half_r, oy + half_a - c), (ox - half_r, oy - half_a + c), (ox - half_r + c, oy - half_a), (ox + half_r - c, oy - half_a)]


class Orbit:
    def __init__(self, radius, normal, phase):
        self.radius = radius
        self.normal = normal.normalized()
        u = self.normal.cross(Vector((0.0, 0.0, 1.0)))
        if u.length < 1e-3:
            u = Vector((1.0, 0.0, 0.0))
        self.u = u.normalized()
        self.v = self.normal.cross(self.u)
        self.phase = phase

    def direction(self, t):
        return self.u * math.cos(t + self.phase) + self.v * math.sin(t + self.phase)

    def point(self, t, extra=0.0):
        return HEART + self.direction(t) * (self.radius + extra)


def orbit_spans(orbit, rng, cut):
    steps = 360
    keep = []
    for i in range(steps):
        t = 2.0 * math.pi * i / steps
        keep.append(orbit.direction(t).dot(VIEW) < cut)
    blocked = rng.uniform(0.0, 2.0 * math.pi)
    width = rng.uniform(0.25, 0.5)
    for i in range(steps):
        t = 2.0 * math.pi * i / steps
        if abs(math.remainder(t - blocked, 2.0 * math.pi)) < width:
            keep[i] = False
    if all(keep):
        return [(0.0, 2.0 * math.pi, True)]
    spans = []
    start = None
    first = keep.index(False)
    for k in range(steps + 1):
        i = (first + k) % steps
        if keep[i] and start is None:
            start = first + k
        if not keep[i] and start is not None:
            spans.append((2.0 * math.pi * start / steps, 2.0 * math.pi * (first + k) / steps, False))
            start = None
    return [s for s in spans if s[1] - s[0] > 0.6]


def orbit_band(orbit, t0, t1, closed, profile_fn):
    count = max(8, int((t1 - t0) * orbit.radius / 0.06))
    points, frames = [], []
    for i in range(count if closed else count + 1):
        t = t0 + (t1 - t0) * i / count
        d = orbit.direction(t)
        points.append(HEART + d * orbit.radius)
        frames.append((d, orbit.normal))
    return sweep(points, frames, profile_fn, closed)


def harness(builder, mats, rng):
    orbits = [
        Orbit(HARNESS, Vector((0.12, -0.06, 1.0)), 0.3),
        Orbit(HARNESS + 0.13, Matrix.Rotation(math.radians(64.0), 3, Vector((math.cos(0.6), math.sin(0.6), 0.0))) @ Vector((0.0, 0.0, 1.0)), 1.1),
        Orbit(HARNESS + 0.26, Matrix.Rotation(math.radians(-58.0), 3, Vector((math.cos(2.3), math.sin(2.3), 0.0))) @ Vector((0.0, 0.0, 1.0)), 2.2),
    ]
    band = rect(0.045, 0.085, 0.014)
    edge = rect(0.032, 0.013, 0.006)
    accent = rect(0.004, 0.011, 0.002, (0.045, 0.0))
    ends = []
    for index, orbit in enumerate(orbits):
        spans = [(0.0, 2.0 * math.pi, True)] if index == 0 else orbit_spans(orbit, rng, 0.42)
        for t0, t1, closed in spans:
            builder.add('Harness', orbit_band(orbit, t0, t1, closed, band), mats['brass'], bevel=0.0)
            for side in (-1.0, 1.0):
                builder.add('Harness', orbit_band(orbit, t0, t1, closed, [(x, y + side * 0.098) for x, y in edge]), mats['iron'], bevel=0.0)
            builder.add('Harness', orbit_band(orbit, t0, t1, closed, accent), mats['amber'], bevel=0.0)
            if not closed:
                ends.append((orbit, t0, -1.0))
                ends.append((orbit, t1, 1.0))
            count = max(1, int((t1 - t0) / 0.9))
            for k in range(count):
                t = t0 + (t1 - t0) * (k + 0.5) / count
                clamp_frame = orbit_frame(orbit, t)
                clamp = pt.transformed(pk.chipped((0.13, 0.12, 0.26), rng, chips=1, depth=0.01), clamp_frame)
                builder.add('Harness', clamp, mats['iron'], bevel=0.008, angle=30.0)
                for sy in (-0.07, 0.07):
                    bolt = pk.lathe([(0.0, 0.03), (0.018, 0.024), (0.022, 0.0)], 6)
                    bolt = pt.transformed(bolt, clamp_frame @ Matrix.Translation((0.065, 0.0, sy)) @ Matrix.Rotation(math.radians(90.0), 4, 'Y'))
                    builder.add('Harness', bolt, mats['bronze'], bevel=0.0)
    for orbit, t, sign in ends:
        frame = orbit_frame(orbit, t)
        cap = pt.transformed(pk.chipped((0.16, 0.2, 0.22), rng, chips=1, depth=0.012), frame)
        builder.add('Harness', cap, mats['bronze'], bevel=0.01, angle=30.0)
        tip = pk.cone_bm(Vector((0.0, 0.0, 0.0)), Vector((0.0, 0.0, 0.18)), 0.06, sides=6)
        tangent_turn = Matrix.Rotation(math.radians(-90.0 * sign), 4, 'X')
        builder.add('Harness', pt.transformed(tip, frame @ Matrix.Translation((0.0, 0.0, 0.0)) @ tangent_turn @ Matrix.Translation((0.0, 0.0, 0.08))), mats['iron'], bevel=0.0)
    lugs = []
    equator = orbits[0]
    for azimuth in H['EmberChainAzimuths']:
        a = math.radians(azimuth)
        target = Vector((math.cos(a), math.sin(a), 0.0))
        t = max((2.0 * math.pi * i / 720.0 for i in range(720)), key=lambda s: equator.direction(s).dot(target))
        d = equator.direction(t)
        frame = orbit_frame(equator, t)
        block = pt.transformed(pk.chipped((0.2, 0.22, 0.3), rng, chips=2, depth=0.015), frame @ Matrix.Translation((0.06, 0.0, 0.0)))
        builder.add('Harness', block, mats['iron'], bevel=0.012, angle=30.0)
        anchor = HEART + d * (equator.radius + 0.2) + Vector((0.0, 0.0, 0.1))
        lugs.append(anchor)
    return lugs


def orbit_frame(orbit, t):
    d = orbit.direction(t)
    tangent = orbit.normal.cross(d)
    frame = Matrix.Identity(4)
    frame.col[0] = d.to_4d()
    frame.col[1] = tangent.to_4d()
    frame.col[2] = orbit.normal.to_4d()
    frame.col[0][3] = frame.col[1][3] = frame.col[2][3] = 0.0
    frame.col[3] = (HEART + d * orbit.radius).to_4d()
    frame.col[3][3] = 1.0
    return frame


def link_bm(length, width, wire, sides=7, path=20):
    straight = length - width
    radius = 0.5 * width - wire
    points = []
    half = path // 2
    for k in range(half):
        t = math.pi * k / (half - 1) - math.pi * 0.5
        points.append(Vector((0.5 * straight + radius * math.cos(t), radius * math.sin(t), 0.0)))
    for k in range(half):
        t = math.pi * k / (half - 1) + math.pi * 0.5
        points.append(Vector((-0.5 * straight + radius * math.cos(t), radius * math.sin(t), 0.0)))
    bm = bmesh.new()
    rings = []
    count = len(points)
    for i, p in enumerate(points):
        tangent = (points[(i + 1) % count] - points[i - 1]).normalized()
        normal = tangent.cross(Vector((0.0, 0.0, 1.0))).normalized()
        rings.append([bm.verts.new(p + (normal * math.cos(2.0 * math.pi * j / sides) + Vector((0.0, 0.0, 1.0)) * math.sin(2.0 * math.pi * j / sides)) * wire) for j in range(sides)])
    for i in range(count):
        a, b = rings[i], rings[(i + 1) % count]
        for j in range(sides):
            bm.faces.new((a[j], b[j], b[(j + 1) % sides], a[(j + 1) % sides]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def chain_bm(start, end, rng):
    length, width, wire = LINK
    pitch = length - 2.0 * wire * 1.05

    def point(t):
        return start + (end - start) * t + Vector((0.0, 0.0, -4.0 * CHAIN_SAG * t * (1.0 - t)))

    samples = [point(i / 400.0) for i in range(401)]
    distances = [0.0]
    for i in range(1, len(samples)):
        distances.append(distances[-1] + (samples[i] - samples[i - 1]).length)
    total = distances[-1]
    count = max(2, int(total / pitch))
    parts = []
    template = link_bm(length, width, wire)
    roll = rng.uniform(0.0, math.pi)
    for k in range(count):
        target = (k + 0.5) * total / count
        index = min(range(len(distances)), key=lambda i: abs(distances[i] - target))
        a = samples[max(index - 2, 0)]
        b = samples[min(index + 2, len(samples) - 1)]
        tangent = (b - a).normalized()
        side = tangent.cross(Vector((0.0, 0.0, 1.0)))
        side = side.normalized() if side.length > 1e-3 else Vector((1.0, 0.0, 0.0))
        up = side.cross(tangent)
        angle = roll + (k % 2) * math.pi * 0.5 + rng.uniform(-0.1, 0.1)
        y = side * math.cos(angle) + up * math.sin(angle)
        z = tangent.cross(y)
        link = template.copy()
        bmesh.ops.transform(link, matrix=Matrix.Translation(samples[index]) @ Matrix((tangent, y, z)).transposed().to_4x4(), verts=link.verts)
        parts.append(link)
    template.free()
    return pk.merge_bm(parts)


def anchors(builder, mats, rocks_tree, lugs, rng):
    eyes = []
    for azimuth, lug in zip(H['EmberChainAzimuths'], lugs):
        a = math.radians(azimuth)
        out = polar(1.0, a)
        origin = Vector((0.0, 0.0, -CHAIN_DEPTH))
        hit, normal, _, _ = rocks_tree.ray_cast(origin, out, 30.0)
        if hit is None:
            hit = polar(ei.cavern_wall(CHAIN_DEPTH, a), a, -CHAIN_DEPTH)
        wall = hit.copy()
        side = Vector((-out.y, out.x, 0.0))
        root = wall + out * 0.5
        face = wall - out * 0.7
        points = []
        for base, half_w, half_h in ((root, 0.78, 0.7), (face, 0.6, 0.55)):
            for sx in (-1.0, 1.0):
                for sz in (-1.0, 1.0):
                    points.append(base + side * (sx * half_w * rng.uniform(0.85, 1.1)) + Vector((0.0, 0.0, sz * half_h * rng.uniform(0.85, 1.1))))
        points.append(face + Vector((0.0, 0.0, -0.95)) + out * 0.35)
        stone = hull_block(points, rng, 1.2)
        if stone is not None:
            builder.add('Anchors', stone, mats['rock'], bevel=0.0, local=False)
        x = -out
        frame = Matrix.Identity(4)
        frame.col[0] = side.to_4d()
        frame.col[1] = Vector((0.0, 0.0, 1.0)).to_4d()
        frame.col[2] = x.to_4d()
        frame.col[0][3] = frame.col[1][3] = frame.col[2][3] = 0.0
        frame.col[3] = (face - out * 0.02).to_4d()
        frame.col[3][3] = 1.0
        plate = pt.transformed(pk.chipped((0.9, 0.8, 0.09), rng, chips=1, depth=0.012), frame)
        builder.add('Anchors', plate, mats['bronze'], bevel=0.012, angle=30.0)
        inner = pt.transformed(pk.chipped((0.66, 0.56, 0.05), rng, chips=0, depth=0.01), frame @ Matrix.Translation((0.0, 0.0, 0.065)))
        builder.add('Anchors', inner, mats['iron'], bevel=0.008, angle=30.0)
        for sx in (-0.36, 0.36):
            for sy in (-0.31, 0.31):
                bolt = pk.lathe([(0.0, 0.06), (0.03, 0.05), (0.04, 0.0)], 6)
                builder.add('Anchors', pt.transformed(bolt, frame @ Matrix.Translation((sx, sy, 0.04))), mats['iron'], bevel=0.0)
        glow = pt.transformed(pk.box((0.5, 0.025, 0.012)), frame @ Matrix.Translation((0.0, -0.2, 0.09)))
        builder.add('Anchors', glow, mats['amber'], bevel=0.0)
        boss = pk.lathe([(0.0, 0.2), (0.12, 0.18), (0.16, 0.08), (0.17, 0.0)], 12)
        builder.add('Anchors', pt.transformed(boss, frame @ Matrix.Translation((0.0, 0.05, 0.08))), mats['iron'], bevel=0.0)
        pull = (lug - (face - out * 0.32)).normalized()
        eye = pk.torus(0.17, 0.055, steps=18, sides=8)
        ey = pull.cross(side).normalized()
        eye_frame = Matrix((pull, ey, pull.cross(ey))).transposed().to_4x4()
        eye_center = face - out * 0.3 + Vector((0.0, 0.0, 0.05)) + pull * 0.1
        builder.add('Anchors', pt.transformed(eye, Matrix.Translation(eye_center) @ eye_frame), mats['iron'], bevel=0.0)
        eyes.append(eye_center + pull * 0.17)
    return eyes


def chains(builder, mats, lugs, eyes, rng):
    for lug, eye in zip(lugs, eyes):
        pull = (eye - lug).normalized()
        shackle = pk.torus(0.15, 0.05, steps=16, sides=7)
        side = pull.cross(Vector((0.0, 0.0, 1.0))).normalized()
        frame = Matrix((pull, side.cross(pull), side)).transposed().to_4x4()
        builder.add('Chains', pt.transformed(shackle, Matrix.Translation(lug + pull * 0.05) @ frame), mats['iron'], bevel=0.0)
        builder.add('Chains', chain_bm(lug + pull * 0.2, eye - pull * 0.04, rng), mats['iron'], bevel=0.0)


def basalt_slab_material(name='HeartBasalt', size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    island = s.geometry('Random Per Island')
    _, _, nz = s.separate(s.geometry('Normal'))
    mid = s.noise(p, 3.2, detail=5.0, roughness=0.55)
    fine = s.noise(p, 18.0, detail=3.0)
    broad = s.noise(p, 0.9, detail=3.0)
    tone = s.clamp01(s.add(s.mul(mid, 0.35), s.add(s.mul(island, 0.45), s.mul(broad, 0.2))))
    base = s.mix(tone, (0.03, 0.028, 0.027), (0.105, 0.094, 0.086))
    warm = s.mul(s.smooth(0.55, 0.62, island), s.sub(1.0, s.smooth(0.8, 0.86, island)))
    base = s.mix(s.mul(warm, 0.6), base, (0.13, 0.098, 0.072))
    speck = s.mul(s.sub(1.0, s.smooth(0.0, 0.2, s.voronoi(p, 60.0, 'F1', 1.0, 'Distance'))), s.smooth(0.45, 0.7, s.noise(p, 7.0, detail=2.0)))
    base = s.mix(s.mul(speck, 0.35), base, (0.2, 0.19, 0.18))
    edge = pk.wear(s, 0.02)
    chips = s.mul(edge, s.smooth(0.4, 0.6, s.noise(p, 7.0, detail=4.0)))
    base = s.mix(s.mul(chips, 0.6), base, (0.19, 0.175, 0.16))
    dirt = pk.cavity(s, 0.08)
    base = s.mix(s.mul(dirt, 0.7), base, (0.012, 0.01, 0.009))
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.014, s.voronoi(s.offset(p, s.mul(s.noise_color(p, 2.0, detail=2.0), 0.2)), 1.7, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.56, 0.68, s.noise(p, 1.1, detail=2.0)))
    base = s.mix(s.mul(cracks, 0.8), base, (0.006, 0.005, 0.005))
    up = s.smooth(0.5, 0.9, nz)
    scuff = s.mul(s.smooth(0.4, 0.75, s.noise(p, 2.4, detail=5.0)), up)
    rough = s.remap(fine, 0.3, 0.7, 0.5, 0.74)
    rough = s.lerp(s.mul(scuff, 0.8), rough, 0.3)
    rough = s.lerp(chips, rough, 0.7)
    rough = s.lerp(s.clamp01(s.add(dirt, cracks)), rough, 0.9)
    height = s.sub(s.add(s.mul(mid, 0.45), s.mul(fine, 0.12)), s.add(s.mul(cracks, 0.7), s.add(s.mul(chips, 0.5), s.mul(speck, 0.08))))
    return s.finish(base, rough, height=height, bump=0.6, bump_distance=0.012, bake_size=size)


def brass_material(name, tone, polish, tarnish, size=2048):
    s = kit.Shader(name)
    p = pk.local(s)
    noise = s.noise(p, 9.0, detail=4.0)
    broad = s.noise(p, 1.4, detail=3.0)
    scratches = s.smooth(0.64, 0.72, s.noise(s.stretch(p, (70.0, 2.5, 2.5)), 2.6, detail=2.0))
    dents = s.smooth(0.0, 0.38, s.voronoi(p, 26.0, 'F1', 1.0, 'Distance'))
    bright = (min(1.0, tone[0] * 1.08), min(1.0, tone[1] * 1.14), min(1.0, tone[2] * 1.4))
    base = s.mix(s.add(s.mul(noise, 0.6), s.mul(broad, 0.4)), (tone[0] * 0.8, tone[1] * 0.74, tone[2] * 0.66), tone)
    pits = pk.cavity(s, 0.035)
    patina = s.clamp01(s.mul(pits, s.add(0.45, s.mul(s.smooth(0.4, 0.75, broad), 0.5))))
    base = s.mix(s.mul(patina, tarnish), base, (0.08, 0.055, 0.03))
    verdigris = s.mul(s.mul(pits, s.smooth(0.62, 0.76, s.noise(p, 3.5, detail=3.0))), 0.45 * tarnish)
    base = s.mix(verdigris, base, (0.07, 0.13, 0.1))
    edge = pk.wear(s, 0.01)
    base = s.mix(s.mul(edge, 0.7), base, bright)
    base = s.mix(s.mul(scratches, 0.25), base, bright)
    rough = s.remap(noise, 0.3, 0.7, polish[0], polish[1])
    rough = s.lerp(patina, rough, 0.6)
    rough = s.lerp(s.mul(edge, 0.6), rough, 0.1)
    metal = s.lerp(s.clamp01(s.add(patina, verdigris)), 1.0, 0.5)
    height = s.sub(s.add(s.add(s.mul(noise, 0.25), s.mul(broad, 0.15)), s.mul(dents, 0.4)), s.mul(scratches, 0.12))
    return s.finish(base, rough, height=height, bump=0.07, bump_distance=0.002, metallic=metal, bake_size=size)


def mortar_material(name='HeartMortar'):
    s = kit.Shader(name)
    p = s.coords('Object')
    grain = s.noise(p, 30.0, detail=3.0)
    patches = s.noise(p, 2.0, detail=3.0)
    base = s.mix(s.add(s.mul(grain, 0.5), s.mul(patches, 0.5)), (0.025, 0.022, 0.02), (0.07, 0.062, 0.054))
    return s.finish(base, 0.92, height=grain, bump=0.3, bump_distance=0.004, bake_size=512)


def amber_material(name='HeartAmber'):
    s = kit.Shader(name)
    p = pk.local(s)
    mottled = s.noise(p, 40.0, detail=3.0)
    glow = s.remap(mottled, 0.2, 0.8, 0.7, 1.0)
    return s.finish((0.9, 0.5, 0.16), 0.2, height=mottled, bump=0.05, bump_distance=0.002, emission=glow, emission_color=(1.0, 0.55, 0.2), emission_strength=3.6, bake_size=256)


def glass_material(name='HeartGlass'):
    s = kit.Shader(name)
    p = s.coords('Object')
    smudge = s.smooth(0.5, 0.8, s.noise(p, 1.4, detail=4.0))
    scratches = s.smooth(0.74, 0.8, s.noise(s.stretch(p, (40.0, 1.0, 1.0)), 2.0, detail=2.0))
    seeds = s.sub(1.0, s.smooth(0.0, 0.05, s.voronoi(p, 22.0, 'F1', 1.0, 'Distance')))
    seeds = s.mul(seeds, s.smooth(0.55, 0.7, s.noise(p, 3.0, detail=2.0)))
    rough = s.clamp01(s.add(0.02, s.add(s.mul(smudge, 0.04), s.add(s.mul(scratches, 0.06), s.mul(seeds, 0.1)))))
    base = s.mix(s.mul(smudge, 0.3), (0.026, 0.018, 0.01), (0.034, 0.024, 0.013))
    material = s.finish(base, rough, height=s.sub(0.0, s.add(s.mul(scratches, 0.15), s.mul(seeds, 0.3))), bump=0.2, bump_distance=0.004, transmission=1.0, ior=1.52, attenuation=((1.0, 0.62, 0.28), 0.8), bake_size=2048)
    bsdf = material.node_tree.nodes['Principled BSDF']
    bsdf.inputs['Transmission Weight'].default_value = 1.0
    bsdf.inputs['IOR'].default_value = 1.52
    return material


def cavern_material(name, dark, light, glow, size):
    s = kit.Shader(name)
    p = s.coords('Object')
    _, _, z = s.separate(p)
    island = s.geometry('Random Per Island')
    q = s.offset(p, s.combine(s.mul(island, 37.0), s.mul(island, 53.0), s.mul(island, 71.0)))
    large = s.noise(p, 0.3, detail=3.0)
    mid = s.noise(q, 1.6, detail=5.0, roughness=0.55)
    fine = s.noise(q, 9.0, detail=3.0)
    tone = s.clamp01(s.add(s.mul(large, 0.4), s.add(s.mul(mid, 0.3), s.mul(island, 0.35))))
    base = s.mix(tone, dark, light)
    strata = s.smooth(0.45, 0.75, s.noise(s.stretch(p, (0.25, 0.25, 4.0)), 1.0, detail=3.0))
    base = s.mix(s.mul(strata, 0.35), base, (light[0] * 1.2, light[1] * 1.1, light[2] * 0.95))
    warped = s.offset(q, s.mul(s.noise_color(q, 1.1, detail=2.0), 0.35))
    cracks = s.mul(s.sub(1.0, s.smooth(0.0, 0.016, s.voronoi(warped, 0.7, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.52, 0.64, s.noise(q, 0.45, detail=3.0)))
    hair = s.mul(s.sub(1.0, s.smooth(0.0, 0.012, s.voronoi(warped, 1.8, 'DISTANCE_TO_EDGE', 1.0, 'Distance'))), s.smooth(0.62, 0.76, s.noise(q, 1.0, detail=2.0)))
    pits = s.mul(s.sub(1.0, s.smooth(0.0, 0.22, s.voronoi(q, 26.0, 'F1', 1.0, 'Distance'))), s.smooth(0.45, 0.7, s.noise(q, 2.4, detail=2.0)))
    edge = pk.wear(s, 0.04)
    chipped = s.mul(edge, s.smooth(0.4, 0.6, s.noise(q, 6.0, detail=3.0)))
    dirt = pk.cavity(s, 0.3)
    base = s.mix(s.mul(pits, 0.6), base, (dark[0] * 0.5, dark[1] * 0.5, dark[2] * 0.5))
    base = s.mix(s.mul(chipped, 0.6), base, (light[0] * 1.2, light[1] * 1.15, light[2] * 1.1))
    base = s.mix(s.mul(s.add(cracks, s.mul(hair, 0.6)), 0.85), base, (0.01, 0.008, 0.007))
    base = s.mix(s.mul(dirt, 0.8), base, (0.012, 0.01, 0.008))
    soot = s.mul(s.smooth(-3.0, -0.6, z), 0.45)
    base = s.mix(soot, base, (0.02, 0.016, 0.014))
    rough = s.remap(fine, 0.3, 0.7, 0.7, 0.9)
    rough = s.lerp(s.mul(chipped, 0.5), rough, 0.6)
    height = s.add(s.mul(mid, 0.5), s.mul(fine, 0.12))
    height = s.sub(height, s.add(s.mul(cracks, 0.6), s.add(s.mul(hair, 0.2), s.add(s.mul(chipped, 0.3), s.mul(pits, 0.2)))))
    emission = None
    if glow > 0.0:
        veins_warp = s.offset(p, s.mul(s.noise_color(p, 0.6, detail=3.0), 0.8))
        veins = s.sub(1.0, s.smooth(0.0, 0.011, s.voronoi(veins_warp, 0.6, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
        field = s.noise(p, 0.18, detail=2.0)
        mask = s.smooth(0.63, 0.69, field)
        deep = s.smooth(1.5, 4.0, s.sub(0.0, z))
        emission = s.clamp01(s.mul(s.add(s.mul(cracks, 0.5), veins), s.mul(mask, deep)))
        base = s.mix(s.mul(emission, 0.8), base, (0.05, 0.015, 0.005))
        height = s.sub(height, s.mul(veins, s.mul(mask, 0.4)))
    return s.finish(base, rough, height=height, bump=0.9, bump_distance=0.04, emission=emission, emission_color=(1.0, 0.32, 0.06), emission_strength=glow, bake_size=size)


def geode_material(name='HeartCrystal', floor=0.0):
    s = kit.Shader(name)
    p = pk.local(s)
    island = s.geometry('Random Per Island')
    depth = s.smooth(0.25, 0.9, s.occlusion(0.2, samples=24, inside=True))
    edges = s.sub(1.0, s.smooth(0.0, 0.04, s.voronoi(p, 6.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance')))
    lit = s.smooth(0.45, 0.95, island)
    glow = s.clamp01(s.add(s.add(s.mul(lit, s.mul(depth, 0.75)), s.mul(edges, 0.18)), s.mul(depth, floor)))
    base = s.mix(island, (0.06, 0.02, 0.008), (0.15, 0.055, 0.018))
    return s.finish(base, 0.1, height=edges, bump=0.06, bump_distance=0.003, emission=glow, emission_color=(1.0, 0.42, 0.1), emission_strength=0.9, specular=0.7, bake_size=1024, translucency=0.5, thickness=0.4)


def materials():
    return {
        'basalt': basalt_slab_material(),
        'lime': pt.stone_material('HeartLimestone', 2048),
        'brass': brass_material('HeartBrass', (0.92, 0.66, 0.3), (0.16, 0.3), 0.75),
        'bronze': brass_material('HeartBronze', (0.5, 0.32, 0.15), (0.3, 0.46), 1.0),
        'iron': pk.iron_material('HeartIron', tone=(0.04, 0.038, 0.036), rust=0.5, bake_size=1024),
        'inlay': pt.inlay_material('HeartInlay', 1024),
        'amber': amber_material(),
        'mortar': mortar_material(),
        'glass': glass_material(),
        'rock': cavern_material('HeartRock', (0.03, 0.026, 0.023), (0.11, 0.092, 0.078), 1.5, 2048),
        'limerock': cavern_material('HeartLimeRock', (0.12, 0.11, 0.095), (0.34, 0.31, 0.27), 0.0, 1024),
        'crystal': geode_material(),
        'fringe': geode_material('HeartFringe', 0.35),
        'hidden': ei.hidden_material(),
    }


def build(target, rng):
    started = time.time()
    mats = materials()
    builder = pt.Builder(target, SEED)
    builder.rng = rng
    retaining_ring(builder, mats, rng)
    surround(builder, mats, rng)
    print(f'surround in {time.time() - started:.0f} s')
    sink = ei.Sink()
    cavern(sink, rng)
    rock_mats = {ei.BASALT: mats['rock'], ei.LIMESTONE: mats['limerock'], ei.HIDDEN: mats['hidden']}
    rocks = sink.build(target, rock_mats)
    tree = BVHTree.FromObject(rocks[0], bpy.context.evaluated_depsgraph_get())
    lugs = harness(builder, mats, rng)
    eyes = anchors(builder, mats, tree, lugs, rng)
    chains(builder, mats, lugs, eyes, rng)
    geode(builder, mats, tree, rng)
    drifts(builder, mats, rng)
    monoliths(builder, mats, rng)
    spouts(builder, mats, rng)
    fringe(builder, mats, rng)
    groups = builder.finish()
    pane = kit.mesh_object('Glass', glass_disc(), target, mats['glass'])
    pane.data.shade_smooth()
    pane.data.set_sharp_from_angle(angle=math.radians(40.0))
    ei.planar_uv(pane, GLASS_EDGE + 0.05)
    pane['keep_uv'] = True
    objects = rocks + list(groups.values()) + [pane]
    for obj in objects:
        print('TRIS', obj.name, sum(len(p.vertices) - 2 for p in obj.data.polygons))
    print('TRIS total', sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in objects))
    print(f'built in {time.time() - started:.0f} s')
    return objects, mats


def preview_lights():
    light = bpy.data.lights.new('Star', 'POINT')
    light.energy = 7000.0
    light.color = (1.0, 0.62, 0.3)
    light.shadow_soft_size = STAR
    holder = bpy.data.objects.new('Star', light)
    bpy.context.scene.collection.objects.link(holder)
    holder.location = HEART
    sun = bpy.data.lights.new('Sky', 'SUN')
    sun.energy = 2.2
    obj = bpy.data.objects.new('Sky', sun)
    bpy.context.scene.collection.objects.link(obj)
    obj.rotation_euler = (math.radians(45), 0, math.radians(35))
    sphere = bmesh.new()
    bmesh.ops.create_icosphere(sphere, subdivisions=4, radius=STAR)
    bmesh.ops.translate(sphere, vec=HEART, verts=sphere.verts)
    material = bpy.data.materials.new('StarPreview')
    material.use_nodes = True
    bsdf = material.node_tree.nodes['Principled BSDF']
    bsdf.inputs['Emission Color'].default_value = (1.0, 0.7, 0.3, 1.0)
    bsdf.inputs['Emission Strength'].default_value = 30.0
    star = kit.mesh_object('StarPreview', sphere, bpy.context.scene.collection, material)
    star.visible_shadow = False


def preview(folder, objects, views):
    kit.enable_gpu()
    kit.environment('night.exr', 0.25)
    preview_lights()
    shots = {
        'game': ((0, 0, -1.0), 0, 57, 15.0, None, 40.0),
        'q45': ((0, 0, -1.2), 30, 38, 15.0, None, 40.0),
        'top': ((0, 0, -2.0), 0, 89.9, 200, 17.5, 40.0),
        'rim': ((0, -4.8, 0.0), 20, 30, 4.5, None, 50.0),
        'inside': (tuple(HEART), 15, 8, 6.5, None, 80.0),
        'deep': ((0, 2, -8.0), 0, 30, 9.0, None, 75.0),
        'side': ((0, 0, -6.0), 0, 0, 200, 24.0, 40.0),
    }
    for view in views:
        focus, yaw, pitch, dist, ortho, fov = shots[view]
        hidden = []
        if view in ('side', 'inside', 'deep'):
            for obj in objects:
                if obj.name in ('Slabs', 'Bed', 'Glass', 'Brass', 'Inlay', 'Lights'):
                    obj.hide_render = True
                    hidden.append(obj)
        if view == 'side':
            cutter(objects, True)
        ei.camera_shot(os.path.join(folder, f'heart_{view}.png'), focus, yaw, pitch, dist, ortho=ortho, fov=fov, samples=40)
        if view == 'side':
            cutter(objects, False)
        for obj in hidden:
            obj.hide_render = False


def cutter(objects, enable):
    for obj in objects:
        if obj.name.startswith('Cavern') or obj.name == 'Collar':
            if enable:
                mod = obj.modifiers.new('Cut', 'BOOLEAN')
                box = bpy.data.objects.get('CutBox')
                if box is None:
                    bm = bmesh.new()
                    bmesh.ops.create_cube(bm, size=1.0)
                    bmesh.ops.scale(bm, vec=(60.0, 30.0, 60.0), verts=bm.verts)
                    bmesh.ops.translate(bm, vec=(0.0, -15.0, 0.0), verts=bm.verts)
                    box = kit.mesh_object('CutBox', bm, bpy.context.scene.collection, None)
                    box.hide_render = True
                mod.object = box
                mod.operation = 'DIFFERENCE'
            else:
                for mod in list(obj.modifiers):
                    if mod.name == 'Cut':
                        obj.modifiers.remove(mod)


def export(objects, folder):
    os.makedirs(folder, exist_ok=True)
    pk.unwrap_all([o for o in objects if not o.get('keep_uv')], {'HeartRock': 0.002, 'HeartLimeRock': 0.002, 'EmberHidden': 0.0005})
    materials = []
    for obj in objects:
        for slot in obj.material_slots:
            if slot.material and slot.material not in materials:
                materials.append(slot.material)
    baked, emissive, clear = {}, {}, {}
    for material in materials:
        started = time.time()
        users = [obj for obj in objects if material.name in obj.data.materials]
        files = kit.bake_material(material, users, folder, NAME, ao_distance=AO_DISTANCE)
        if 'emit' in files:
            emissive[material.name] = (tuple(material['emit_color']), float(material['emit_strength']))
        if material.get('transmission', 0.0) > 0.0:
            clear[material.name] = (float(material['transmission']), float(material['ior']), [float(v) for v in material['attenuation']])
        baked[material.name] = kit.baked_material(material, files)
        print(f'baked {material.name} in {time.time() - started:.0f} s')
    for obj in objects:
        for slot in obj.material_slots:
            slot.material = baked[slot.material.name]
    for obj in objects:
        kit.select_only([obj])
        with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
            bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    kit.export_gltf(objects, os.path.join(folder, NAME + '.gltf'), emissive, None, None, False, None, clear)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    opts = {}
    for i, arg in enumerate(argv):
        if arg.startswith('--'):
            opts[arg] = argv[i + 1] if i + 1 < len(argv) and not argv[i + 1].startswith('--') else '1'
    kit.reset()
    bpy.context.preferences.filepaths.save_version = 0
    rng = random.Random(int(opts.get('--seed', SEED)))
    target = kit.collection(NAME)
    objects, _ = build(target, rng)
    if opts.get('--preview'):
        folder = os.path.abspath(opts['--preview'])
        os.makedirs(folder, exist_ok=True)
        preview(folder, objects, opts.get('--views', 'game,q45,top').split(','))
    out = opts.get('--out')
    if out:
        for obj in objects:
            ei.mirror(obj)
        out = os.path.abspath(out)
        export(objects, out)
        bpy.ops.outliner.orphans_purge(do_recursive=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, NAME + '.blend'), compress=True)


if __name__ == '__main__':
    main()
