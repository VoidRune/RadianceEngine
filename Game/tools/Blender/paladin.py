import bpy
import bmesh
import math
import os
import random
import sys
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import heroanim
import hero
import magegear as gear
import paladinkit as pkit
from paladinkit import look, smoothstep, unit, orth, Ring, Tube, Grid, Piece, patch, sample_path, bar, edge_path, gem, star

NAME = 'Hero'
SIDES = (('l', 1.0), ('r', -1.0))
GLOW = ((1.0, 0.5, 0.12), 5.0)
SIZES = {'Coat': 2048, 'Gear': 2048, 'Cape': 2048}
NODES = {'Coat': 'Wizard_Garb', 'Gear': 'Wizard_Garb', 'Cape': 'Wizard_Cape', 'Head': 'Hero_Head'}
METALS = {'gold': (0.62, 0.38, 0.12), 'silver': (0.6, 0.61, 0.64)}
METAL = 'gold'
L = {}

gear.SHOULDERS = {'l': (0.172, 0.012, 1.468), 'r': (-0.172, 0.012, 1.468)}
gear.CLAVICLES = {'l': (0.03, 0.0, 1.505), 'r': (-0.03, 0.0, 1.505)}
gear.ARM_POSE = {'l': (40.0, 4.0, 12.0), 'r': (40.0, 4.0, 12.0)}
gear.UPPER, gear.FORE = 0.28, 0.25
ARMS = {}
JOINTS = {}

LEG_LEFT = ((0.095, 0.0, 0.97), (0.158, -0.02, 0.53), (0.232, 0.01, 0.1), (0.25, -0.095, 0.032), (0.262, -0.18, 0.03))
LEGS = {'l': [np.array(p) for p in LEG_LEFT], 'r': [np.array((-p[0], p[1], p[2])) for p in LEG_LEFT]}
HEAD_FRAME = Matrix.Translation((0.0, 0.012, 1.4615)) @ Matrix.Scale(0.85, 4)

TORSO = Ring((
    (0.95, 0.172, 0.122, 0.128, 2.4, 0.0),
    (1.04, 0.162, 0.116, 0.12, 2.4, 0.0),
    (1.13, 0.152, 0.11, 0.112, 2.4, 0.0),
    (1.24, 0.176, 0.126, 0.114, 2.5, 0.0),
    (1.36, 0.196, 0.136, 0.12, 2.6, 0.0),
    (1.45, 0.198, 0.132, 0.122, 2.6, 0.005),
    (1.51, 0.168, 0.112, 0.108, 2.5, 0.008),
    (1.555, 0.115, 0.086, 0.088, 2.2, 0.01),
    (1.585, 0.072, 0.066, 0.07, 2.0, 0.01),
    (1.62, 0.06, 0.058, 0.062, 2.0, 0.012),
))
SKIRT = Ring((
    (0.2, 0.38, 0.15, 0.185, 2.0, 0.0),
    (0.4, 0.365, 0.146, 0.175, 2.0, 0.0),
    (0.55, 0.31, 0.144, 0.165, 2.1, 0.0),
    (0.7, 0.268, 0.14, 0.155, 2.2, 0.0),
    (0.85, 0.222, 0.136, 0.145, 2.3, 0.0),
    (1.0, 0.19, 0.132, 0.138, 2.4, 0.0),
    (1.12, 0.166, 0.124, 0.126, 2.4, 0.0),
    (1.18, 0.162, 0.12, 0.122, 2.4, 0.0),
))
HOOD = Ring((
    (1.44, 0.2, 0.15, 0.172, 2.0, 0.03),
    (1.5, 0.19, 0.148, 0.168, 2.0, 0.025),
    (1.56, 0.172, 0.14, 0.16, 2.0, 0.02),
    (1.62, 0.15, 0.132, 0.152, 2.0, 0.015),
    (1.67, 0.122, 0.13, 0.135, 2.0, 0.012),
    (1.72, 0.11, 0.13, 0.125, 2.0, 0.01),
    (1.76, 0.1, 0.125, 0.118, 2.0, 0.005),
    (1.8, 0.08, 0.115, 0.095, 2.0, -0.005),
    (1.835, 0.042, 0.065, 0.055, 2.0, -0.02),
    (1.865, 0.0, 0.0, 0.0, 2.0, -0.03),
))
HOOD_OPEN = ((1.44, 0.16), (1.52, 0.115), (1.58, 0.097), (1.62, 0.1), (1.68, 0.105), (1.73, 0.102), (1.77, 0.085), (1.795, 0.05), (1.815, 0.0))
COLLAR = Ring((
    (1.54, 0.086, 0.083, 0.086, 2.0, 0.012),
    (1.6, 0.079, 0.076, 0.081, 2.0, 0.015),
    (1.665, 0.09, 0.087, 0.087, 2.0, 0.018),
))
BELT = (1.095, 1.17)
SKIRT_TOP = 1.15
ROBE_SIGMA = (0.0, 0.25, 0.5, 0.75, 1.0, -0.75, -0.5, -0.25)
ROBE_Z = (1.02, 0.7, 0.36)
CAPE_COLUMNS = (-0.75, -0.375, 0.0, 0.375, 0.75)
CAPE_ROWS = (1.44, 1.16, 0.88, 0.6)
CAPE_WIDTH = ((1.52, 0.17), (1.4, 0.22), (1.1, 0.27), (0.7, 0.33), (0.4, 0.37), (0.15, 0.39))
CAPE_SPRINGS = []
ROBE_SPRINGS = []


def make_looks(metal):
    tone = METALS[metal]
    return {
        'coat': look((0.048, 0.026, 0.02), 0.82, fabric=1.0, veins=1.0),
        'coat_plain': look((0.048, 0.026, 0.02), 0.84, fabric=1.0),
        'black': look((0.013, 0.01, 0.009), 0.72, fabric=1.0),
        'cream': look((0.5, 0.39, 0.25), 0.74, fabric=0.6),
        'plate': look((0.56, 0.46, 0.32), 0.42, plate=1.0),
        'metal': look(tone, 0.3, metal=1.0, trim=1.0),
        'leather': look((0.06, 0.034, 0.022), 0.5, fabric=0.3, leather=1.0),
        'dark_plate': look((0.035, 0.022, 0.018), 0.4, plate=1.0, veins=0.8),
        'glove': look((0.03, 0.021, 0.018), 0.55, fabric=0.3, leather=1.0),
        'trouser': look((0.02, 0.015, 0.014), 0.88, fabric=1.0),
        'boot': look((0.065, 0.038, 0.025), 0.45, leather=1.0, veins=0.5),
        'sole': look((0.02, 0.016, 0.014), 0.75),
        'gem': look((0.95, 0.45, 0.08), 0.1, glow=1.0, gem=1.0),
        'cape': look((0.06, 0.03, 0.022), 0.85, fabric=1.0, veins=0.6),
    }


def blend(a, b, factor):
    result = {name: weight * (1.0 - factor) for name, weight in a.items()}
    for name, weight in b.items():
        result[name] = result.get(name, 0.0) + weight * factor
    return result


def chain(co, joints, names, blend_length=0.05):
    point = np.asarray(co, dtype=np.float64)
    best = None
    for index in range(len(names)):
        a, b = np.asarray(joints[index]), np.asarray(joints[index + 1])
        ab = b - a
        length = float(np.linalg.norm(ab))
        t = float(np.clip((point - a) @ ab / (length * length), 0.0, 1.0))
        distance = float(np.linalg.norm(point - (a + ab * t)))
        if best is None or distance < best[0] - 1e-9:
            best = (distance, index, t * length, length)
    _, index, along, length = best
    weights = {names[index]: 1.0}
    if along < blend_length and index > 0:
        share = 0.5 * (1.0 - along / blend_length)
        weights = {names[index]: 1.0 - share, names[index - 1]: share}
    elif length - along < blend_length and index + 1 < len(names):
        share = 0.5 * (1.0 - (length - along) / blend_length)
        weights = {names[index]: 1.0 - share, names[index + 1]: share}
    return weights, index, along


def torso_field(co, param=None):
    x, y, z = co
    hips = 1.0 - float(smoothstep(0.98, 1.22, z))
    chest = float(smoothstep(1.16, 1.44, z))
    entry = {'hips': hips, 'spine': max(0.0, 1.0 - hips - chest), 'chest': chest}
    neck = 0.5 * float(smoothstep(1.55, 1.64, z))
    if neck > 0.0:
        entry = blend(entry, {'neck': 1.0}, neck)
    for side, s in SIDES:
        shoulder = ARMS[side].shoulder
        reach = float(np.linalg.norm(np.asarray(co) - shoulder))
        amount = 0.55 * float(smoothstep(0.17, 0.06, reach)) * float(smoothstep(0.08, 0.14, s * x))
        if amount > 0.0:
            entry = blend(entry, {f'shoulder.{side}': 1.0}, amount)
    return entry


def arm_field(side):
    arm = ARMS[side]
    joints = (arm.shoulder, arm.elbow, arm.wrist, arm.palm, arm.tip)
    names = [f'{name}.{side}' for name in ('upperarm', 'lowerarm', 'wrist', 'hand')]

    def field(co, param=None):
        weights, index, along = chain(co, joints, names, blend_length=0.06)
        if index == 0:
            ahead = float(np.dot(np.asarray(co) - arm.shoulder, arm.d1))
            keep = float(smoothstep(-0.03, 0.07, ahead))
            weights = {name: weight * keep for name, weight in weights.items()}
            weights[f'shoulder.{side}'] = weights.get(f'shoulder.{side}', 0.0) + 0.65 * (1.0 - keep)
            weights['chest'] = weights.get('chest', 0.0) + 0.35 * (1.0 - keep)
        return weights
    return field


def hand_field(side):
    arm = ARMS[side]
    names = [f'{name}.{side}' for name in ('lowerarm', 'wrist', 'hand')]

    def field(co, param=None):
        return chain(co, (arm.elbow, arm.wrist, arm.palm, arm.tip), names, blend_length=0.025)[0]
    return field


def leg_field(side):
    hip, knee, ankle, ball, toe = LEGS[side]
    names = [f'{name}.{side}' for name in ('upperleg', 'lowerleg', 'foot', 'toes')]

    def field(co, param=None):
        weights, index, along = chain(co, (hip + np.array((0.0, 0.0, 0.12)), knee, ankle, ball, toe), names, blend_length=0.09)
        if index == 0:
            top = float(smoothstep(0.16, 0.04, along))
            weights = blend(weights, {'hips': 1.0}, 0.7 * top)
        return weights
    return field


def foot_field(side):
    hip, knee, ankle, ball, toe = LEGS[side]
    names = [f'{name}.{side}' for name in ('lowerleg', 'foot', 'toes')]

    def field(co, param=None):
        return chain(co, (knee, ankle, ball, toe), names, blend_length=0.03)[0]
    return field


def robe_field(co, param):
    s, z = param[0], param[1]
    top = float(smoothstep(0.97, 1.07, z))
    c = ((s % 2.0) * 4.0) % 8.0
    k0 = int(math.floor(c)) % 8
    f = c - math.floor(c)
    k1 = (k0 + 1) % 8
    lower = float(smoothstep(0.75, 0.65, z))
    weights = {'hips': top}
    for k, amount in ((k0, 1.0 - f), (k1, f)):
        weights[f'robe.{k}.1'] = weights.get(f'robe.{k}.1', 0.0) + amount * (1.0 - lower) * (1.0 - top)
        weights[f'robe.{k}.2'] = weights.get(f'robe.{k}.2', 0.0) + amount * lower * (1.0 - top)
    return weights


def cape_field(co, param):
    s, z = param[0], param[1]
    columns = CAPE_COLUMNS
    s = min(max(s, columns[0]), columns[-1])
    j = min(int(np.searchsorted(columns, s, side='right')) - 1, len(columns) - 2)
    f = (s - columns[j]) / (columns[j + 1] - columns[j])
    letters = 'abcde'
    top = float(smoothstep(CAPE_ROWS[0] - 0.05, CAPE_ROWS[0] + 0.03, z))
    weights = {'chest': top}
    rows = CAPE_ROWS
    link = len(rows) - 1
    for k in range(len(rows) - 1):
        if z > rows[k + 1]:
            link = k
            break
    shares = {link: 1.0}
    if link + 1 < len(rows):
        below = rows[link + 1]
        near = float(smoothstep(below + 0.05, below - 0.05, z))
        if near > 0.0 and link + 1 < len(rows):
            shares = {link: 1.0 - 0.5 * near, link + 1: 0.5 * near}
    if link > 0:
        above = rows[link]
        near = float(smoothstep(above - 0.05, above + 0.05, z))
        if near > 0.0:
            shares = {key: value * (1.0 - 0.5 * near) for key, value in shares.items()}
            shares[link - 1] = shares.get(link - 1, 0.0) + 0.5 * near
    for column, amount in ((j, 1.0 - f), (j + 1, f)):
        for k, share in shares.items():
            name = f'cape.{letters[column]}.{k + 1}'
            weights[name] = weights.get(name, 0.0) + amount * share * (1.0 - top)
    return weights


def head_field(co, param=None):
    z = co[2]
    head = float(smoothstep(1.58, 1.64, z))
    return {'head': head, 'neck': 1.0 - head}


def hood_field(co, param=None):
    z = co[2]
    head = float(smoothstep(1.52, 1.78, z))
    neck = 0.6 * float(smoothstep(1.44, 1.62, z)) * (1.0 - head)
    return {'head': head, 'neck': neck, 'chest': max(0.0, 1.0 - head - neck)}


def collar_field(co, param=None):
    z = co[2]
    neck = 0.6 * float(smoothstep(1.56, 1.66, z))
    return {'neck': neck, 'chest': 1.0 - neck}


def rigid(weights):
    def field(co, param=None):
        return dict(weights)
    return field


def at_metric(surface, s, z):
    return s / surface.half(z)


def binding(piece, surface, path, offset, thickness, width, value, closed=False, lip=0.003):
    points, normals, params = sample_path(surface, path, offset=offset)
    bar(piece, points, normals, params, width, thickness * 0.5 + lip, value, center=-thickness * 0.5, depth=thickness * 0.5 + lip, closed=closed)


def line(piece, surface, path, offset, width, value, height=0.003, closed=False):
    points, normals, params = sample_path(surface, path, offset=offset)
    bar(piece, points, normals, params, width, height, value, center=0.0, depth=0.0015, closed=closed)


def ring_path(z, count=96):
    return [(-1.0 + 2.0 * k / count, z) for k in range(count + 1)]


def build_torso(target):
    pieces = []
    coat = Piece('Torso', 'Coat', torso_field)
    patch(coat, TORSO, lambda tc, tr: (-1.0 + 2.0 * tc, 1.62 - (1.62 - 0.95) * tr), 44, 80, L['coat'], wrap=True)
    pieces.append(coat.build(target))

    def v_bottom(s):
        return min(1.455 + 0.9 * abs(s), 1.52)

    def mantle_region(tc, tr):
        s = -0.27 + 0.54 * tc
        top = 1.572 - 0.018 * smoothstep(0.08, 0.25, abs(s))
        z = top - (top - v_bottom(s)) * tr
        return at_metric(TORSO, s, z), z

    mantle = Piece('Mantle', 'Coat', torso_field)
    patch(mantle, TORSO, mantle_region, 12, 56, L['cream'], offset=0.007)
    pieces.append(mantle.build(target, thickness=0.004))
    trims = Piece('ChestTrims', 'Gear', torso_field)
    line(trims, TORSO, edge_path(mantle_region, 'bottom', 64), 0.007, 0.018, L['metal'], height=0.004)

    def width(z):
        return 0.04 + 0.11 * (z - 1.17)

    def panel_region(tc, tr):
        z = 1.47 - (1.47 - 1.165) * tr
        w = width(z)
        return at_metric(TORSO, -w + 2.0 * w * tc, z), z

    panel = Piece('ChestPanel', 'Coat', torso_field)
    patch(panel, TORSO, panel_region, 24, 14, L['cream'], offset=0.004)
    pieces.append(panel.build(target))
    for sign in (1.0, -1.0):
        def stripe_region(tc, tr, sign=sign):
            z = 1.5 - (1.5 - 1.165) * tr
            w = width(z)
            return at_metric(TORSO, sign * (w + 0.03 * tc), z), z
        stripe = Piece(f'ChestStripe{int(sign)}', 'Coat', torso_field)
        patch(stripe, TORSO, stripe_region, 24, 4, L['black'], offset=0.0035)
        pieces.append(stripe.build(target))
        for edge, wide in ((0.0, 0.011), (0.03, 0.014)):
            path = [(at_metric(TORSO, sign * (width(z) + edge), z), z) for z in np.linspace(1.165, 1.52 if edge else 1.47, 30)]
            line(trims, TORSO, path, 0.0045, wide, L['metal'], height=0.0035)
    p, n = TORSO.at(0.0, 1.455)
    star(trims, p + n * 0.012, n, (0.0, 0.0, 1.0), 0.06, L['metal'], thickness=0.012, inner=0.34, ratio=1.2)
    gem(trims, p + n * 0.026, n, (0.0, 0.0, 1.0), 0.026, 0.036, L['metal'], L['gem'], depth=0.01, lift=0.016)
    pieces.append(trims.build(target))

    belt = Piece('Belt', 'Gear', torso_field)
    patch(belt, TORSO, lambda tc, tr: (-1.0 + 2.0 * tc, BELT[1] - (BELT[1] - BELT[0]) * tr), 6, 120, L['leather'], offset=0.02, wrap=True)
    pieces.append(belt.build(target, thickness=0.006))
    studs = Piece('BeltMetal', 'Gear', torso_field)
    for z in (BELT[0] + 0.004, BELT[1] - 0.004):
        line(studs, TORSO, ring_path(z, 120), 0.02, 0.007, L['metal'], height=0.003, closed=True)
    for k in range(20):
        s = -1.0 + 2.0 * (k + 0.5) / 20
        if abs(s) < 0.12:
            continue
        p, n = TORSO.at(s, (BELT[0] + BELT[1]) * 0.5)
        star(studs, p + n * 0.02, n, (0.0, 0.0, 1.0), 0.011, L['metal'], thickness=0.004, inner=0.7)
    p, n = TORSO.at(0.0, 1.132)
    star(studs, p + n * 0.026, n, (0.0, 0.0, 1.0), 0.07, L['metal'], thickness=0.014, inner=0.38, ratio=1.2)
    gem(studs, p + n * 0.042, n, (0.0, 0.0, 1.0), 0.028, 0.038, L['metal'], L['gem'], depth=0.012, lift=0.016)
    pieces.append(studs.build(target))
    for sign in (1.0, -1.0):
        pieces.append(pouch(target, sign))
    return pieces


def pouch(target, sign):
    p, n = TORSO.at(sign * 0.36, 1.06)
    n = unit(np.array((n[0], n[1], 0.0)))
    up = np.array((0.0, 0.0, 1.0))
    side = np.cross(up, n)
    center = p + n * 0.05
    piece = Piece(f'Pouch{int(sign)}', 'Gear', rigid({'hips': 1.0}))
    w, d, h = 0.045, 0.026, 0.045

    def box(center, w, d, h, value):
        corners = []
        for dz in (-1.0, 1.0):
            for dn in (-1.0, 1.0):
                for ds in (-1.0, 1.0):
                    corners.append(piece.vert(center + side * (ds * w) + n * (dn * d) + up * (dz * h), value, (0.0, 0.0), side * ds + n * dn + up * dz))
        faces = ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3))
        for face in faces:
            piece.bm.faces.new(tuple(corners[i] for i in face))

    box(center, w, d, h, L['leather'])
    box(center + n * 0.004 + up * 0.026, w * 1.08, d * 1.06, h * 0.45, L['leather'])
    star(piece, center + n * (d + 0.006) + up * 0.012, n, up, 0.012, L['metal'], thickness=0.005, inner=0.5)
    return piece.build(target, bevel=0.006)


def skirt_panel(target, name, s0, s1, hem, offset, value, edges=('left', 'right', 'bottom'), band=None, rows=34):
    def region(tc, tr):
        s = s0 + (s1 - s0) * tc
        z = SKIRT_TOP - (SKIRT_TOP - hem(s)) * tr
        return s, z

    cols = max(6, int(abs(s1 - s0) * 70))
    piece = Piece(name, 'Coat', robe_field)
    patch(piece, SKIRT, region, rows, cols, value, offset=offset)
    obj = piece.build(target, thickness=0.004)
    trims = Piece(name + 'Trim', 'Gear', robe_field)
    path = []
    if 'left' in edges:
        path += edge_path(region, 'left', 40)
    if 'bottom' in edges:
        path += edge_path(region, 'bottom', 40)[1 if path else 0:]
    if 'right' in edges:
        right = list(reversed(edge_path(region, 'right', 40)))
        path += right[1 if path else 0:]
    binding(trims, SKIRT, path, offset, 0.004, 0.022, L['metal'])
    parts = [obj]
    if band is not None:
        inner, wide = band

        def band_region(tc, tr):
            z = SKIRT_TOP - 0.02 - (SKIRT_TOP - 0.02 - hem(inner) - 0.03) * tr
            s = inner + math.copysign((0.016 + wide * tc) / SKIRT.half(z), s1 - s0 if inner == s0 else s0 - s1)
            return s, z

        cream = Piece(name + 'Band', 'Coat', robe_field)
        patch(cream, SKIRT, band_region, rows, 4, L['cream'], offset=offset + 0.002)
        parts.append(cream.build(target))
        line(trims, SKIRT, edge_path(band_region, 'right', 40), offset + 0.002, 0.009, L['metal'], height=0.003)
    parts.append(trims.build(target))
    return parts


def build_skirt(target):
    parts = []
    parts += skirt_panel(target, 'SkirtBack', 0.6, 1.4, lambda s: 0.42, 0.0, L['coat'])
    for sign in (1.0, -1.0):
        tag = 'L' if sign > 0 else 'R'
        lo, hi = sorted((sign * 0.36, sign * 0.66))
        parts += skirt_panel(target, f'SkirtSide{tag}', lo, hi, lambda s, sign=sign: 0.41 - 0.03 * smoothstep(0.4, 0.66, abs(s)), 0.004, L['coat'])
        lo, hi = sorted((sign * 0.09, sign * 0.4))
        inner = sign * 0.09
        parts += skirt_panel(target, f'SkirtFront{tag}', lo, hi, lambda s: 0.385 + 0.06 * smoothstep(0.09, 0.4, abs(s)), 0.008, L['coat'], band=(inner, 0.045))
    parts += skirt_panel(target, 'SkirtCenter', -0.12, 0.12, lambda s: 0.245 + 0.1 * abs(s) / 0.12, 0.002, L['coat_plain'], edges=('bottom',))
    parts += tabard(target)
    return parts


def tabard(target):
    half = 0.068
    offset = 0.016

    def hem(s):
        return 0.39 + 0.08 * min(abs(s) / half, 1.0)

    def region(tc, tr):
        s = -half + 2.0 * half * tc
        z0 = SKIRT_TOP + 0.01
        z = z0 - (z0 - hem(s)) * tr
        return at_metric(SKIRT, s, z), z

    piece = Piece('Tabard', 'Coat', robe_field)
    patch(piece, SKIRT, region, 40, 12, L['cream'], offset=offset)
    obj = piece.build(target, thickness=0.004)
    trims = Piece('TabardTrim', 'Gear', robe_field)
    path = edge_path(region, 'left', 40) + edge_path(region, 'bottom', 24)[1:] + list(reversed(edge_path(region, 'right', 40)))[1:]
    binding(trims, SKIRT, path, offset, 0.004, 0.018, L['metal'])

    def diamond(cz, wide, tall):
        return [(at_metric(SKIRT, x, z), z) for x, z in ((0.0, cz + tall), (wide, cz), (0.0, cz - tall), (-wide, cz), (0.0, cz + tall))]

    line(trims, SKIRT, diamond(0.62, 0.042, 0.07), offset, 0.011, L['metal'], closed=True)
    line(trims, SKIRT, diamond(0.62, 0.02, 0.034), offset, 0.007, L['metal'], closed=True)
    p, n = SKIRT.at(0.0, 0.62)
    gem(trims, p + n * (offset + 0.002), n, (0.0, 0.0, 1.0), 0.012, 0.018, L['metal'], L['metal'], depth=0.004, lift=0.01)
    for cz in (0.98, 0.9):
        chevron = [(at_metric(SKIRT, x, z), z) for x, z in ((-0.05, cz + 0.05), (0.0, cz), (0.05, cz + 0.05))]
        line(trims, SKIRT, chevron, offset, 0.009, L['metal'])
    return [obj, trims.build(target)]


def hood_open(z):
    table = np.array(HOOD_OPEN)
    x = float(np.interp(z, table[:, 0], table[:, 1]))
    sigma, points, _, _ = HOOD.ring(z)
    front = (sigma >= 0.0) & (sigma <= 0.5)
    if x <= 0.0:
        return 0.0
    return float(np.interp(x, points[front, 0], sigma[front]))


def build_hood(target):
    bottom, top = 1.44, 1.865

    def region(tc, tr):
        z = top - (top - bottom) * tr
        o = hood_open(z)
        return o + (2.0 - 2.0 * o) * tc, z

    piece = Piece('Hood', 'Coat', hood_field)
    patch(piece, HOOD, region, 44, 72, L['coat'])
    obj = piece.build(target, thickness=0.006, merge=1e-5)
    trims = Piece('HoodTrim', 'Gear', hood_field)
    path = list(reversed(edge_path(region, 'left', 60))) + edge_path(region, 'right', 60)[1:]
    path = [(s, z) for s, z in path]
    binding(trims, HOOD, path, 0.0, 0.006, 0.03, L['metal'])
    return [obj, trims.build(target)]


def build_collar(target):
    def region(tc, tr):
        z = 1.665 - (1.665 - 1.54) * tr
        return 0.14 + 1.72 * tc, z

    piece = Piece('Collar', 'Coat', collar_field)
    patch(piece, COLLAR, region, 10, 60, L['coat_plain'])
    obj = piece.build(target, thickness=0.005)
    trims = Piece('CollarTrim', 'Gear', collar_field)
    path = list(reversed(edge_path(region, 'left', 12))) + edge_path(region, 'top', 60)[1:] + edge_path(region, 'right', 12)[1:]
    binding(trims, COLLAR, path, 0.0, 0.005, 0.014, L['metal'])
    return [obj, trims.build(target)]


def body_bvh(objects):
    verts, faces = [], []
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in objects:
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        base = len(verts)
        verts += [obj.matrix_world @ v.co for v in mesh.vertices]
        faces += [tuple(base + i for i in poly.vertices) for poly in mesh.polygons]
        evaluated.to_mesh_clear()
    return BVHTree.FromPolygons(verts, faces)


class CapeSurface(Grid):
    def __init__(self, rows, values, points, halves):
        super().__init__(rows, values, points, 0.0)
        self.halves = np.asarray(halves)

    def half(self, z):
        order = np.argsort(self.rows)
        return float(np.interp(z, self.rows[order], self.halves[order]))


def cape_surface(bvh):
    zs = np.linspace(1.53, 0.16, 70)
    phis = np.radians(np.linspace(-118.0, 118.0, 237))
    hits = np.zeros((len(zs), len(phis)))
    for i, z in enumerate(zs):
        for j, phi in enumerate(phis):
            direction = Vector((math.sin(phi), math.cos(phi), 0.0))
            origin = Vector((0.0, 0.0, z)) + direction * 1.2
            hit = bvh.ray_cast(origin, -direction, 1.25)
            hits[i, j] = 1.2 - hit[3] if hit[0] is not None else 0.0
    clearance = np.interp(zs, (0.2, 0.9, 1.53), (0.022, 0.018, 0.014))[:, None]
    need = np.where(hits > 0.0, hits + clearance, 0.0)
    radius = np.zeros_like(need)
    radius[0] = np.maximum(need[0], 0.12)
    for i in range(1, len(zs)):
        radius[i] = np.maximum(need[i], radius[i - 1] - 0.1 * (zs[i - 1] - zs[i]))
    kernel = np.array((1.0, 4.0, 6.0, 4.0, 1.0)) / 16.0
    for _ in range(6):
        padded = np.pad(radius, ((0, 0), (2, 2)), mode='edge')
        radius = sum(padded[:, k:k + len(phis)] * kernel[k] for k in range(5))
        padded = np.pad(radius, ((2, 2), (0, 0)), mode='edge')
        radius = sum(padded[k:k + len(zs)] * kernel[k] for k in range(5))
        radius = np.maximum(radius, need)
    width = np.array(CAPE_WIDTH)
    sigmas = np.linspace(-1.0, 1.0, 49)
    table, halves = [], []
    for i, z in enumerate(zs):
        points = np.stack([radius[i] * np.sin(phis), radius[i] * np.cos(phis), np.full(len(phis), z)], axis=1)
        target = float(np.interp(z, width[::-1, 0], width[::-1, 1]))
        wrap = math.radians(float(np.interp(z, (0.2, 1.0, 1.53), (88.0, 84.0, 78.0))))
        half_count = len(phis) // 2
        reached = (points[half_count:, 0] >= target) | (phis[half_count:] >= wrap)
        right = half_count + int(np.argmax(reached)) if reached.any() else len(phis) - 1
        left = len(phis) - 1 - right
        row = points[left:right + 1]
        arc = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(row, axis=0), axis=1))])
        u = arc / arc[-1] * 2.0 - 1.0
        table.append(np.stack([np.interp(sigmas, u, row[:, k]) for k in range(3)], axis=1))
        halves.append(arc[-1] * 0.5)
    return CapeSurface(zs, sigmas, table, halves)


def cape_top(s):
    return 1.5 - 0.03 * s * s


def cape_hem(s):
    return 0.22 + 0.09 * abs(s)


def build_cape(target, body):
    surface = cape_surface(body_bvh(body))

    def region(tc, tr):
        s = -1.0 + 2.0 * tc
        return s, cape_top(s) - (cape_top(s) - cape_hem(s)) * tr

    piece = Piece('Cape', 'Cape', cape_field)
    patch(piece, surface, region, 64, 48, L['cape'])
    obj = piece.build(target, thickness=0.005)
    trims = Piece('CapeTrim', 'Cape', cape_field)
    path = edge_path(region, 'left', 60) + edge_path(region, 'bottom', 40)[1:] + list(reversed(edge_path(region, 'right', 60)))[1:]
    binding(trims, surface, path, 0.0, 0.005, 0.034, L['metal'])

    def inset(z, sign):
        return sign * (1.0 - 0.075 / surface.half(z))

    inner = [(inset(z, -1.0), z) for z in np.linspace(1.36, cape_hem(-0.9) + 0.07, 40)]
    inner += [(s, cape_hem(s) + 0.075) for s in np.linspace(-0.86, 0.86, 30)]
    inner += [(inset(z, 1.0), z) for z in np.linspace(cape_hem(0.9) + 0.07, 1.36, 40)]
    line(trims, surface, inner, 0.0, 0.012, L['metal'])

    def metric(points):
        return [(x / surface.half(z), z) for x, z in points]

    line(trims, surface, metric(((0.0, 1.31), (0.0, 1.15))), 0.0, 0.014, L['metal'])
    line(trims, surface, metric(((0.0, 0.89), (0.0, 0.5))), 0.0, 0.014, L['metal'])
    line(trims, surface, metric(((0.0, 1.13), (0.078, 1.02), (0.0, 0.91), (-0.078, 1.02), (0.0, 1.13))), 0.0, 0.018, L['metal'], closed=True)
    line(trims, surface, metric(((0.0, 1.085), (0.043, 1.02), (0.0, 0.955), (-0.043, 1.02), (0.0, 1.085))), 0.0, 0.01, L['metal'], closed=True)
    line(trims, surface, metric(((0.0, 0.5), (0.034, 0.45), (0.0, 0.4), (-0.034, 0.45), (0.0, 0.5))), 0.0, 0.01, L['metal'], closed=True)
    line(trims, surface, metric(((-0.18, 1.48), (0.0, 1.33), (0.18, 1.48))), 0.0, 0.016, L['metal'])
    p, n = surface.at(0.0, 1.02)
    gem(trims, p + n * 0.003, n, (0.0, 0.0, 1.0), 0.026, 0.036, L['metal'], L['metal'], depth=0.006, lift=0.016)
    p, n = surface.at(0.0, 1.415)
    star(trims, p + n * 0.004, n, (0.0, 0.0, 1.0), 0.032, L['metal'], thickness=0.007, inner=0.4, ratio=1.2)
    for s in CAPE_COLUMNS:
        chain_points = []
        for z in CAPE_ROWS + (cape_hem(s) + 0.02,):
            q, m = surface.at(s, z)
            chain_points.append(q - m * 0.012)
        CAPE_SPRINGS.append((s, chain_points))
    return [obj, trims.build(target)]


def lame(side, k, target):
    s = 1.0 if side == 'l' else -1.0
    arm = ARMS[side]
    tau = math.radians(16.0 + 15.0 * k)
    lateral = unit((s * math.cos(tau), 0.0, -math.sin(tau)))
    up = orth((0.0, 0.0, 1.0), lateral)
    fwd = np.cross(lateral, up) * s
    origin = arm.shoulder + up * (0.0 - 0.042 * k) + lateral * (0.0 + 0.04 * k)
    spread = math.radians(104.0 - 6.0 * k)
    start = -0.085 if k == 0 else -0.01
    base, spike = ((0.095, 0.05), (0.085, 0.045), (0.075, 0.04))[k]
    rho0 = 0.098 + 0.013 * k

    def reach(v):
        return base + spike * max(1.0 - abs(v), 0.0) ** 2.2 + 0.02 * math.exp(-((abs(v) - 0.62) / 0.12) ** 2)

    class Shell(pkit.Surface):
        def point(self, v, u):
            ell = start + (reach(v) - start) * u
            alpha = spread * v
            out = max(ell, 0.0)
            rho = rho0 + 0.22 * out * out
            curl = (0.028 if k == 0 else -0.012 * k) * (max(ell - 0.06, 0.0) / 0.1) ** 2 * max(1.0 - abs(v), 0.0) ** 2
            return origin + lateral * ell + (up * math.cos(alpha) + fwd * math.sin(alpha)) * rho + up * curl

        def center(self, v, u):
            ell = start + (reach(v) - start) * u
            return origin + lateral * ell

    surface = Shell()

    def region(tc, tr):
        return -1.0 + 2.0 * tc, tr

    weights = {f'shoulder.{side}': 1.0} if k == 0 else {f'upperarm.{side}': 1.0}
    piece = Piece(f'Lame{k}_{side}', 'Gear', rigid(weights))
    patch(piece, surface, region, 18, 34, L['plate'])
    obj = piece.build(target, thickness=0.008)
    trims = Piece(f'LameTrim{k}_{side}', 'Gear', rigid(weights))
    path = list(reversed(edge_path(region, 'left', 16))) + edge_path(region, 'bottom', 60)[1:] + edge_path(region, 'right', 16)[1:]
    binding(trims, surface, path, 0.0, 0.008, 0.018, L['metal'])
    if k == 0:
        p, n = surface.at(-0.42, 0.42)
        star(trims, p + n * 0.006, n, (0.0, 0.0, 1.0), 0.034, L['metal'], thickness=0.008, inner=0.36, ratio=1.15)
        gem(trims, p + n * 0.014, n, (0.0, 0.0, 1.0), 0.014, 0.019, L['metal'], L['gem'], depth=0.006, lift=0.011)
    return [obj, trims.build(target)]


def build_arm(side, target):
    arm = ARMS[side]
    parts = []
    points, total = arm.centerline(30)
    elbow_l = float(np.linalg.norm(np.diff(points[:int(np.argmin(np.linalg.norm(points - arm.elbow, axis=1))) + 1], axis=0), axis=1).sum())
    profile = ((0.0, 0.08), (0.06, 0.077), (0.16, 0.071), (elbow_l, 0.062), (elbow_l + 0.1, 0.055), (total, 0.046))

    def radius(l, theta):
        r = float(np.interp(l, [p[0] for p in profile], [p[1] for p in profile]))
        return r * (1.0 + 0.04 * math.cos(2.0 * theta))

    sleeve_surface = Tube(points, arm.dorsal, radius)
    field = arm_field(side)
    sleeve = Piece(f'Sleeve_{side}', 'Coat', field)
    patch(sleeve, sleeve_surface, lambda tc, tr: (-1.0 + 2.0 * tc, total * tr), 40, 32, L['coat'], wrap=True)
    parts.append(sleeve.build(target))

    c0, c1 = elbow_l - 0.085, elbow_l + 0.03
    cuff = Piece(f'Cuff_{side}', 'Coat', field)
    patch(cuff, sleeve_surface, lambda tc, tr: (-1.0 + 2.0 * tc, c0 + (c1 - c0) * tr), 8, 36, L['cream'], offset=0.008, wrap=True)
    parts.append(cuff.build(target, thickness=0.006))
    trims = Piece(f'ArmTrim_{side}', 'Gear', field)
    for l in (c0 + 0.004, c1 - 0.004):
        line(trims, sleeve_surface, [(-1.0 + 2.0 * k / 48, l) for k in range(49)], 0.008, 0.012, L['metal'], height=0.004, closed=True)

    start = arm.elbow + arm.d2 * 0.04
    end = arm.wrist + arm.d2 * 0.012
    length = float(np.linalg.norm(end - start))
    forearm = Tube(np.array([start + (end - start) * t for t in np.linspace(0.0, 1.0, 8)]), arm.dorsal, lambda l, theta: 0.058 - 0.012 * l / length)
    bands = 3
    step = length / bands
    rigid_forearm = rigid({f"lowerarm.{side}": 1.0})
    brace = Piece(f"BracerTrim_{side}", "Gear", rigid_forearm)
    for k in range(bands):
        l0, l1 = k * step - (0.012 if k else 0.0), (k + 1) * step

        def band_radius(l, theta, l0=l0, l1=l1):
            return 0.059 - 0.012 * l / length + 0.007 * (1.0 - (l - l0) / (l1 - l0)) + 0.003 * k

        band_surface = Tube(np.array([start + (end - start) * t for t in np.linspace(0.0, 1.0, 8)]), arm.dorsal, band_radius)
        band = Piece(f"Bracer{k}_{side}", "Gear", rigid_forearm)
        patch(band, band_surface, lambda tc, tr, l0=l0, l1=l1: (-1.0 + 2.0 * tc, l0 + (l1 - l0) * tr), 6, 40, L['dark_plate'], wrap=True)
        parts.append(band.build(target, thickness=0.005))
        line(brace, band_surface, [(-1.0 + 2.0 * j / 48, l0 + 0.005) for j in range(49)], 0.0, 0.01, L['metal'], height=0.004, closed=True)
        if k < 2:
            p0, n0 = band_surface.at(0.0, l0 + 0.012)
            p1, _ = band_surface.at(0.0, l1 - 0.004)
            axis = unit(p1 - p0)
            across = np.cross(n0, axis)
            tip = p0 - axis * 0.022 + n0 * 0.022
            base = [p0 + across * 0.014, p1, p0 - across * 0.014]
            fin = [brace.vert(v, L["metal"], (0.0, l0), n0) for v in base]
            apex = brace.vert(tip, L['metal'], (0.0, l0), n0)
            lift = [brace.vert(v + n0 * 0.008, L['metal'], (0.0, l0), n0) for v in base]
            brace.bm.faces.new((fin[0], fin[1], lift[1], lift[0]))
            brace.bm.faces.new((fin[1], fin[2], lift[2], lift[1]))
            brace.bm.faces.new((lift[0], lift[1], apex))
            brace.bm.faces.new((lift[1], lift[2], apex))
            brace.bm.faces.new((lift[2], fin[2], apex))
            brace.bm.faces.new((fin[0], lift[0], apex))
    line(brace, forearm, [(-1.0 + 2.0 * j / 48, length - 0.004) for j in range(49)], 0.003, 0.008, L['metal'], height=0.003, closed=True)
    parts.append(trims.build(target))
    parts.append(brace.build(target))

    glove_points = np.array([arm.wrist - arm.d3 * 0.02 + arm.d3 * 0.06 * t for t in np.linspace(0.0, 1.0, 5)])
    glove = Piece(f'GloveCuff_{side}', 'Gear', hand_field(side))
    patch(glove, Tube(glove_points, arm.dorsal, lambda l, theta: 0.043 - 0.05 * l), lambda tc, tr: (-1.0 + 2.0 * tc, 0.06 * tr), 6, 28, L['glove'], wrap=True)
    parts.append(glove.build(target))
    fist = gear.fist(arm, target, 1.0 if side == 'r' else 0.0)
    pkit.ensure_layers(fist, L['glove'])
    fist['slot'] = 'Gear'
    pkit.WEIGHTS[fist.name] = hand_field(side)
    parts.append(fist)
    return parts


def build_leg(side, target):
    hip, knee, ankle, ball, toe = LEGS[side]
    parts = []
    points = np.array([hip + np.array((0.0, 0.0, 0.08)), hip, (hip + knee) * 0.5, knee, (knee + ankle) * 0.5, ankle])
    lengths = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(points, axis=0), axis=1))])

    def radius(l, theta):
        return float(np.interp(l, lengths, (0.09, 0.088, 0.08, 0.07, 0.064, 0.054)))

    trouser = Piece(f'Trouser_{side}', 'Coat', leg_field(side))
    patch(trouser, Tube(points, (0.0, -1.0, 0.0), radius), lambda tc, tr: (-1.0 + 2.0 * tc, lengths[-1] * tr), 30, 28, L['trouser'], wrap=True)
    parts.append(trouser.build(target))

    shin = unit(knee - ankle)
    shaft_points = np.array([ankle + shin * t for t in np.linspace(-0.05, 0.25, 8)])
    shaft_length = 0.3 * float(np.linalg.norm(knee - ankle))

    def shaft_radius(l, theta):
        r = float(np.interp(l, (0.0, 0.06, 0.18, shaft_length), (0.064, 0.066, 0.072, 0.08)))
        return r * (1.0 + 0.06 * math.cos(theta) ** 2)

    shaft_surface = Tube(shaft_points, (0.0, -1.0, 0.0), shaft_radius)
    field = foot_field(side)
    shaft = Piece(f'BootShaft_{side}', 'Gear', field)
    patch(shaft, shaft_surface, lambda tc, tr: (-1.0 + 2.0 * tc, shaft_length * tr), 14, 32, L['boot'], wrap=True)
    parts.append(shaft.build(target))
    trims = Piece(f'BootTrim_{side}', 'Gear', field)
    top0, top1 = shaft_length - 0.06, shaft_length
    cuff = Piece(f'BootCuff_{side}', 'Gear', field)
    patch(cuff, shaft_surface, lambda tc, tr: (-1.0 + 2.0 * tc, top0 + (top1 - top0) * tr), 4, 32, L['boot'], offset=0.007, wrap=True)
    parts.append(cuff.build(target, thickness=0.005))
    for l, wide in ((top1 - 0.004, 0.01), (top0 + 0.004, 0.008)):
        line(trims, shaft_surface, [(-1.0 + 2.0 * j / 40, l) for j in range(41)], 0.007, wide, L['metal'], height=0.004, closed=True)

    def plate_region(tc, tr):
        l = top0 - 0.005 - 0.11 * tr
        half = 0.2 * (1.0 - tr) ** 0.8 + 0.01
        return -half + 2.0 * half * tc, l

    plate = Piece(f'ShinPlate_{side}', 'Gear', field)
    patch(plate, shaft_surface, plate_region, 10, 10, L['dark_plate'], offset=0.004)
    parts.append(plate.build(target, thickness=0.004))
    binding(trims, shaft_surface, list(reversed(edge_path(plate_region, 'left', 16))) + edge_path(plate_region, 'top', 16)[1:] + edge_path(plate_region, 'right', 16)[1:], 0.004, 0.004, 0.01, L['metal'])
    strap0, strap1 = 0.045, 0.085
    strap = Piece(f'AnkleStrap_{side}', 'Gear', field)
    patch(strap, shaft_surface, lambda tc, tr: (-1.0 + 2.0 * tc, strap0 + (strap1 - strap0) * tr), 3, 32, L['leather'], offset=0.006, wrap=True)
    parts.append(strap.build(target, thickness=0.004))
    for l in (strap0 + 0.003, strap1 - 0.003):
        line(trims, shaft_surface, [(-1.0 + 2.0 * j / 40, l) for j in range(41)], 0.006, 0.007, L['metal'], height=0.003, closed=True)
    outward = 0.5 if side == 'l' else -0.5
    p, n = shaft_surface.at(outward, (strap0 + strap1) * 0.5)
    star(trims, p + n * 0.008, n, shin, 0.018, L['metal'], thickness=0.005, inner=0.5)

    heel = ankle + np.array((0.0, 0.085, -0.045))
    mid = ankle + np.array((0.0, -0.03, -0.035))
    tip = toe + np.array((0.0, -0.012, 0.012))
    foot_points = np.array([heel, (heel + mid) * 0.5, mid, ball + np.array((0.0, 0.0, 0.016)), tip])
    foot_lengths = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(foot_points, axis=0), axis=1))])
    total = foot_lengths[-1]

    def foot_radius(l, theta, lift=0.0, wider=0.0, flat=1.0):
        u = l / total
        h = float(np.interp(u, (0.0, 0.25, 0.5, 0.75, 1.0), (0.044, 0.05, 0.048, 0.034, 0.026))) * flat
        w = float(np.interp(u, (0.0, 0.25, 0.5, 0.75, 1.0), (0.046, 0.05, 0.055, 0.058, 0.046))) + wider
        cap = math.sqrt(max(0.0, 1.0 - ((max(0.018 - l, 0.0) + max(l - total + 0.03, 0.0)) / 0.03) ** 2))
        c, s = math.cos(theta), math.sin(theta)
        return cap * 1.0 / math.sqrt((c / max(h, 1e-4)) ** 2 + (s / max(w, 1e-4)) ** 2) + lift

    foot_surface = Tube(foot_points, (0.0, 0.0, 1.0), foot_radius)
    foot = Piece(f'Foot_{side}', 'Gear', field)
    patch(foot, foot_surface, lambda tc, tr: (-1.0 + 2.0 * tc, total * tr), 24, 28, L['boot'], wrap=True)
    parts.append(foot.build(target, merge=1e-5))
    sole_points = np.array([(p[0], p[1], 0.014) for p in foot_points])
    sole_surface = Tube(sole_points, (0.0, 0.0, 1.0), lambda l, theta: foot_radius(l, theta, wider=0.008, flat=0.3))
    sole = Piece(f'Sole_{side}', 'Gear', field)
    patch(sole, sole_surface, lambda tc, tr: (-1.0 + 2.0 * tc, total * tr), 16, 28, L['sole'], wrap=True)
    parts.append(sole.build(target, merge=1e-5))

    def toe_region(tc, tr):
        return -0.3 + 0.6 * tc, total * (0.6 + 0.38 * tr)

    cap = Piece(f'ToeCap_{side}', 'Gear', field)
    patch(cap, foot_surface, toe_region, 10, 12, L['dark_plate'], offset=0.003)
    parts.append(cap.build(target, thickness=0.003))
    binding(trims, foot_surface, list(reversed(edge_path(toe_region, 'left', 12))) + edge_path(toe_region, 'top', 12)[1:] + edge_path(toe_region, 'right', 12)[1:], 0.003, 0.003, 0.009, L['metal'])
    parts.append(trims.build(target))
    return parts


def build_head(target):
    hero.HEAD_FRAME = HEAD_FRAME
    m = {'skin': hero.skin_material('Skin'), 'hair': hero.hair_material('Hair', hero.HAIR_TONE), 'sclera': hero.sclera_material('Sclera'), 'iris': hero.iris_material('Iris'), 'eyes': hero.eye_glow_material('SpiritEyes')}
    parts, glows, hair = hero.head(m, target, random.Random(5))
    for obj in glows:
        bpy.data.objects.remove(obj, do_unlink=True)
    for obj in parts + hair:
        obj['slot'] = 'Head'
        pkit.WEIGHTS[obj.name] = head_field
    return parts + hair


def setup_skeleton():
    for side, s in SIDES:
        arm = gear.Arm(side)
        a = math.radians(gear.ARM_POSE[side][0])
        arm.dorsal = tuple(orth((s * math.cos(a), 0.0, math.sin(a)), arm.d1))
        ARMS[side] = arm


def build_pieces(target):
    pieces = []
    pieces += build_torso(target)
    pieces += build_collar(target)
    skirt = build_skirt(target)
    pieces += skirt
    body = [obj for obj in pieces if obj.name in ('Torso', 'Belt', 'Collar', 'Pouch1', 'Pouch-1') or obj.name.startswith('Skirt') or obj.name.startswith('Tabard')]
    pieces += build_cape(target, body)
    pieces += build_hood(target)
    for side, _ in SIDES:
        for k in range(3):
            pieces += lame(side, k, target)
        pieces += build_arm(side, target)
        pieces += build_leg(side, target)
    pieces += build_head(target)
    return pieces


def joint_layout():
    joints = [
        ('root', None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.12), (0.0, -1.0, 0.0)),
        ('hips', 'root', (0.0, 0.0, 0.98), (0.0, 0.0, 1.12), (0.0, -1.0, 0.0)),
        ('spine', 'hips', (0.0, 0.0, 1.12), (0.0, 0.005, 1.3), (0.0, -1.0, 0.0)),
        ('chest', 'spine', (0.0, 0.005, 1.3), (0.0, 0.01, 1.5), (0.0, -1.0, 0.0)),
        ('neck', 'chest', (0.0, 0.012, 1.53), (0.0, 0.01, 1.64), (0.0, -1.0, 0.0)),
        ('head', 'neck', (0.0, 0.01, 1.64), (0.0, 0.0, 1.84), (0.0, -1.0, 0.0)),
    ]
    letters = 'abcde'
    for column, (s, chain_points) in enumerate(CAPE_SPRINGS):
        parent = 'chest'
        for index in range(len(chain_points) - 1):
            name = f'cape.{letters[column]}.{index + 1}'
            joints.append((name, parent, tuple(chain_points[index]), tuple(chain_points[index + 1]), (0.0, 1.0, 0.0)))
            parent = name
    for chain_index, s in enumerate(ROBE_SIGMA):
        parent = 'hips'
        positions = []
        for z in ROBE_Z:
            p, n = SKIRT.at(s, z)
            positions.append(p - n * 0.015)
        outward = unit(np.array((positions[0][0], positions[0][1], 0.0)))
        for index in range(len(positions) - 1):
            name = f'robe.{chain_index}.{index + 1}'
            joints.append((name, parent, tuple(positions[index]), tuple(positions[index + 1]), tuple(outward)))
            parent = name
    for side, _ in SIDES:
        arm = ARMS[side]
        hip, knee, ankle, ball, toe = LEGS[side]
        dorsal = arm.dorsal
        joints += [
            (f'shoulder.{side}', 'chest', tuple(arm.clavicle), tuple(arm.shoulder), (0.0, 0.0, 1.0)),
            (f'upperarm.{side}', f'shoulder.{side}', tuple(arm.shoulder), tuple(arm.elbow), dorsal),
            (f'lowerarm.{side}', f'upperarm.{side}', tuple(arm.elbow), tuple(arm.wrist), dorsal),
            (f'wrist.{side}', f'lowerarm.{side}', tuple(arm.wrist), tuple(arm.palm), dorsal),
            (f'hand.{side}', f'wrist.{side}', tuple(arm.palm), tuple(arm.tip), dorsal),
            (f'upperleg.{side}', 'hips', tuple(hip), tuple(knee), (0.0, 1.0, 0.0)),
            (f'lowerleg.{side}', f'upperleg.{side}', tuple(knee), tuple(ankle), (0.0, 1.0, 0.0)),
            (f'foot.{side}', f'lowerleg.{side}', tuple(ankle), tuple(ball), (0.0, 0.0, 1.0)),
            (f'toes.{side}', f'foot.{side}', tuple(ball), tuple(toe), (0.0, 0.0, 1.0)),
        ]
    return joints


def build_rig(target):
    data = bpy.data.armatures.new(NAME)
    rig = bpy.data.objects.new(NAME, data)
    target.objects.link(rig)
    kit.select_only([rig])
    bpy.ops.object.mode_set(mode='EDIT')
    bones = {}
    for name, parent, head, tail, up in joint_layout():
        bone = data.edit_bones.new(name)
        bone.head = head
        bone.tail = tail
        bone.align_roll(Vector(up))
        bone.use_deform = name != 'root'
        if parent:
            bone.parent = bones[parent]
        bones[name] = bone
    for side, _ in SIDES:
        slot = data.edit_bones.new(f'handslot.{side}')
        slot.tail = (0.0, 0.06, 0.0)
        slot.matrix = ARMS[side].grip()
        slot.parent = bones[f'hand.{side}']
        slot.use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')
    for side, _ in SIDES:
        for name in ('upperarm', 'lowerarm', 'wrist', 'hand'):
            data.bones[f'{name}.{side}']['dorsal'] = ARMS[side].dorsal
    return rig


def skin(obj, rig):
    field = pkit.WEIGHTS[obj.name]
    mesh = obj.data
    count = len(mesh.vertices)
    co = np.empty(count * 3)
    mesh.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    params = np.zeros((count, 4), dtype=np.float32)
    if 'Param' in mesh.color_attributes:
        mesh.color_attributes['Param'].data.foreach_get('color', params.ravel())
    groups = {}
    for index in range(count):
        entry = field(co[index], params[index, :3])
        chosen = sorted(((w, n) for n, w in entry.items() if w > 0.0), reverse=True)[:4]
        chosen = [(w, n) for w, n in chosen if w > 0.015] or [(1.0, 'hips')]
        total = sum(w for w, _ in chosen)
        for w, name in chosen:
            groups.setdefault(name, []).append((index, w / total))
    obj.vertex_groups.clear()
    for name, members in groups.items():
        group = obj.vertex_groups.new(name=name)
        for index, weight in members:
            group.add([index], weight, 'REPLACE')
    obj.parent = rig
    obj.matrix_parent_inverse = Matrix.Identity(4)
    obj.modifiers.new('Armature', 'ARMATURE').object = rig


def master_material(name, size=2048):
    s = kit.Shader(name)
    p = s.coords('Object')

    def attribute(label):
        return s.node('ShaderNodeAttribute', attribute_name=label, attribute_type='GEOMETRY').outputs['Color']

    def split(color):
        node = s.node('ShaderNodeSeparateColor')
        s.feed(node.inputs['Color'], color)
        return node.outputs['Red'], node.outputs['Green'], node.outputs['Blue']

    def tint(color, scale):
        return s.mix(1.0, color, scale, blend='MULTIPLY')

    gold = METALS[METAL]
    albedo = attribute('Albedo')
    rough, metal, fabric = split(attribute('Surface'))
    veins, trim, stone = split(attribute('Mask'))
    plate, leather, _ = split(attribute('Kind'))
    glow, _, _ = split(attribute('Glow'))
    dirt = pk.cavity(s, 0.04)
    edge = pk.wear(s, 0.006)
    weave = s.mul(s.wave(p, 150.0, kind='BANDS', direction='X', distortion=0.6), s.wave(p, 150.0, kind='BANDS', direction='Z', distortion=0.6))
    mottle = s.noise(p, 6.0, detail=4.0)
    grain = s.noise(p, 85.0, detail=3.0)
    crackle = s.voronoi(p, 240.0, 'F1', 1.0, 'Distance')
    hammer = s.voronoi(p, 45.0, 'F1', 1.0, 'Distance')
    scratches = s.noise(s.stretch(p, (800.0, 800.0, 25.0)), 1.0, detail=2.0)
    shade = s.madd(mottle, 0.45, 0.78)
    base = tint(albedo, s.combine(shade, shade, shade))
    sheen = s.mul(s.smooth(0.55, 0.85, s.noise(s.stretch(p, (1.0, 1.0, 0.3)), 3.0, detail=3.0)), s.mul(fabric, 0.25))
    base = s.mix(sheen, base, tint(albedo, (1.6, 1.55, 1.5)))
    base = s.mix(s.mul(plate, s.smooth(0.35, 0.75, s.noise(p, 14.0, detail=3.0))), base, tint(albedo, (0.86, 0.83, 0.78)))
    base = s.mix(s.mul(plate, s.mul(edge, 0.7)), base, tint(albedo, (1.25, 1.22, 1.15)))
    base = s.mix(s.mul(leather, s.mul(edge, 0.6)), base, tint(albedo, (1.9, 1.75, 1.6)))
    lines = s.voronoi(s.stretch(p, (1.0, 1.0, 0.55)), 6.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    branch = s.voronoi(s.stretch(p, (1.0, 1.0, 0.6)), 17.0, 'DISTANCE_TO_EDGE', 1.0, 'Distance')
    sparse = s.smooth(0.52, 0.66, s.noise(p, 2.4, detail=2.0))
    vein = s.math('MAXIMUM', s.sub(1.0, s.smooth(0.004, 0.011, lines)), s.mul(s.sub(1.0, s.smooth(0.003, 0.008, branch)), 0.8))
    vein = s.mul(s.mul(vein, sparse), veins)
    base = s.mix(vein, base, gold)
    metal = s.math('MAXIMUM', metal, vein)
    rough = s.lerp(vein, rough, 0.32)
    base = s.mix(s.mul(dirt, 0.6), base, tint(albedo, (0.3, 0.28, 0.3)))
    base = s.mix(s.mul(metal, s.mul(edge, 0.55)), base, tint(albedo, (1.45, 1.4, 1.35)))
    rough = s.add(rough, s.mul(s.sub(grain, 0.5), 0.14), clamp=True)
    rough = s.lerp(s.mul(leather, s.mul(edge, 0.7)), rough, 0.3)
    rough = s.lerp(s.mul(metal, s.mul(scratches, 0.5)), rough, 0.55)
    rough = s.lerp(s.mul(stone, 1.0), rough, 0.08)
    rough = s.lerp(s.mul(dirt, 0.5), rough, 0.9)
    folds = s.noise(s.stretch(p, (9.0, 9.0, 1.6)), 1.0, detail=2.0)
    height = s.mul(fabric, s.add(s.madd(weave, 0.45, s.mul(mottle, 0.25)), s.mul(folds, 2.2)))
    height = s.add(height, s.mul(leather, s.mul(s.sub(1.0, crackle), 0.22)))
    height = s.add(height, s.mul(plate, s.mul(s.sub(1.0, hammer), 0.2)))
    height = s.add(height, s.mul(metal, s.mul(grain, 0.12)))
    height = s.add(height, s.mul(vein, 0.4))
    return s.finish(base, rough, metallic=metal, height=height, bump=0.3, bump_distance=0.0015, bake_size=size, emission=glow, emission_color=GLOW[0], emission_strength=GLOW[1])


def assign_materials(pieces):
    materials = {}
    for slot in ('Coat', 'Gear', 'Cape'):
        materials[slot] = master_material(slot, SIZES[slot])
    for obj in pieces:
        slot = obj.get('slot')
        if slot in materials:
            obj.data.materials.clear()
            obj.data.materials.append(materials[slot])
    return materials


def join_nodes(pieces, target):
    groups = {}
    for obj in pieces:
        groups.setdefault(NODES[obj['slot']], []).append(obj)
    nodes = []
    for name, members in groups.items():
        node = kit.join(members, name)
        nodes.append(node)
    return nodes


def unwrap(nodes):
    for node in nodes:
        kit.select_only([node])
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.context.tool_settings.mesh_select_mode = (False, False, True)
        for index, material in enumerate(node.data.materials):
            bm = bmesh.from_edit_mesh(node.data)
            bm.select_mode = {'FACE'}
            for face in bm.faces:
                face.select_set(face.material_index == index)
            bm.select_flush_mode()
            bmesh.update_edit_mesh(node.data)
            margin = 0.004 if node.name == 'Hero_Head' else 0.003
            bpy.ops.uv.smart_project(angle_limit=math.radians(62.0), island_margin=margin, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
            bpy.ops.uv.select_all(action='SELECT')
            bpy.ops.uv.average_islands_scale()
            bpy.ops.uv.pack_islands(rotate=True, margin=margin, shape_method='CONCAVE')
        bpy.ops.object.mode_set(mode='OBJECT')


def bake(nodes, folder):
    materials = []
    for node in nodes:
        for material in node.data.materials:
            if material not in materials:
                materials.append(material)
    baked = {}
    emissive = {}
    for material in materials:
        users = [node for node in nodes if material.name in node.data.materials]
        files = kit.bake_material(material, users, folder, NAME, ao_distance=0.2, ao_samples=96)
        if material.name != 'Gear' and 'emit' in files:
            os.remove(files.pop('emit'))
        baked[material.name] = kit.baked_material(material, files)
        if 'emit' in files:
            emissive[material.name] = (tuple(material['emit_color']), float(material['emit_strength']))
    for node in nodes:
        for slot in node.material_slots:
            slot.material = baked[slot.material.name]
        for name in [attribute.name for attribute in node.data.color_attributes]:
            node.data.color_attributes.remove(node.data.color_attributes[name])
    return emissive


def build(target, rigged=True):
    setup_skeleton()
    pieces = build_pieces(target)
    rig = build_rig(target) if rigged else None
    if rigged:
        for obj in pieces:
            skin(obj, rig)
    assign_materials(pieces)
    return pieces, rig


SHOTS = {
    'full': ((0.0, 0.0, 0.95), 20.0, 6.0, 5.4), 'fullback': ((0.0, 0.05, 0.95), 200.0, 6.0, 5.4), 'fullside': ((0.0, 0.0, 0.95), 95.0, 6.0, 5.4),
    'front': ((0.0, 0.0, 0.95), 0.0, 2.0, 5.4), 'back': ((0.0, 0.0, 0.95), 180.0, 2.0, 5.4), 'side': ((0.0, 0.0, 0.95), 90.0, 2.0, 5.4),
    'game': ((0.0, 0.0, 0.9), 0.0, 57.0, 5.6), 'gameback': ((0.0, 0.0, 0.9), 180.0, 57.0, 5.6),
    'bust': ((0.0, -0.02, 1.45), 15.0, 8.0, 1.9), 'bustback': ((0.0, 0.08, 1.35), 195.0, 14.0, 1.9),
    'face': ((0.0, -0.06, 1.7), 10.0, 4.0, 0.9), 'legs': ((0.0, -0.05, 0.4), 15.0, 8.0, 2.0), 'shoulder': ((0.25, 0.0, 1.42), 60.0, 20.0, 1.2),
    'arm': ((0.42, -0.04, 1.2), 40.0, 8.0, 1.2),
}


def shot(path, center, yaw, pitch, distance, samples, size=(700, 1000)):
    scene = bpy.context.scene
    data = bpy.data.cameras.new('ShotCamera')
    data.angle = math.radians(30.0)
    data.sensor_fit = 'VERTICAL'
    camera = bpy.data.objects.new('ShotCamera', data)
    scene.collection.objects.link(camera)
    yaw, pitch = math.radians(yaw), math.radians(pitch)
    direction = Vector((math.cos(pitch) * math.sin(yaw), -math.cos(pitch) * math.cos(yaw), math.sin(pitch)))
    camera.location = Vector(center) + direction * distance
    camera.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = camera
    scene.render.resolution_x, scene.render.resolution_y = size
    scene.render.resolution_percentage = 100
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera)


def preview(folder, prefix, views, samples=32):
    kit.environment('forest.exr', 1.0)
    pk.ground_preview(4.0)
    for label in views:
        center, yaw, pitch, distance = SHOTS[label]
        shot(os.path.join(folder, f'{prefix}_{label}.png'), center, yaw, pitch, distance, samples)


def main():
    global METAL, L
    args = pk.options()
    METAL = args.get('--metal', 'gold')
    L.update(make_looks(METAL))
    kit.reset()
    kit.enable_gpu()
    target = kit.collection('Export')
    folder = args.get('--out') or args.get('--preview') or args.get('--quick') or args.get('--rigtest')
    folder = os.path.abspath(folder)
    os.makedirs(folder, exist_ok=True)
    views = args['--views'].split(',') if args.get('--views') else ['full', 'fullback', 'fullside', 'bust', 'game']
    prefix = args.get('--prefix', 'paladin')
    samples = int(args.get('--samples', '32'))
    if args.get('--quick'):
        build(target, rigged=False)
        if args.get('--blend'):
            bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, prefix + '.blend'))
        preview(folder, prefix, views, samples)
        return
    pieces, rig = build(target)
    nodes = join_nodes(pieces, target)
    if args.get('--rigtest'):
        heroanim.build_actions(rig)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, NAME + '.blend'))
        return
    unwrap(nodes)
    if args.get('--out'):
        heroanim.build_actions(rig)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, NAME + '.blend'), compress=True)
    emissive = bake(nodes, folder)
    if args.get('--out'):
        kit.export_gltf(nodes, os.path.join(folder, NAME + '.gltf'), emissive, armature=rig, animations=True)
    if args.get('--preview'):
        preview(folder, prefix, views, samples)


if __name__ == '__main__':
    main()
