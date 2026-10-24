import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import propkit as pk
import hubprops
import stations

NAME = 'Market'
ARCANE = (0.62, 0.38, 1.0)
UPRIGHT = Matrix.Rotation(0.5 * math.pi, 4, 'Y')


def plank(name, size, center, m, target, rng, yaw=0.0, chips=1):
    bm = pk.chipped(size, rng, chips=chips, depth=0.008)
    return pk.part(name, bm, target, m, matrix=Matrix.Translation(center) @ Matrix.Rotation(yaw, 4, 'Z'), rng=rng, bevel=(0.008, 1, 30.0))


def post(name, start, end, radius, m, target, rng, end_radius=None, steps=8):
    bm = pk.rod((0.0, 0.0, 0.0), (end - start).length * Vector((0.0, 0.0, 1.0)), radius, steps, end_radius=end_radius)
    obj = pk.part(name, bm, target, m, frame=UPRIGHT, rng=rng, smooth=40.0)
    obj.matrix_world = Matrix.Translation(start) @ (end - start).normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    return obj


def stave(inner, outer, start, end, height, profile, steps=8):
    bm = bmesh.new()
    rows = []
    for i in range(steps + 1):
        z = height * i / steps
        bulge = profile(z)
        row = []
        for radius, angle in ((bulge - (outer - inner), start), (bulge, start), (bulge, end), (bulge - (outer - inner), end)):
            row.append(bm.verts.new((math.cos(angle) * radius, math.sin(angle) * radius, z)))
        rows.append(row)
    for i in range(steps):
        a, b = rows[i], rows[i + 1]
        for j in range(4):
            k = (j + 1) % 4
            bm.faces.new((a[j], a[k], b[k], b[j]))
    bm.faces.new(rows[0])
    bm.faces.new(list(reversed(rows[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def barrel(key, m, target, rng, height=0.9, radius=0.3, bulge=0.045, staves=16):
    profile = lambda z: radius + bulge * math.sin(math.pi * z / height)
    parts = []
    step = 2.0 * math.pi / staves
    for k in range(staves):
        a = k * step + rng.uniform(-0.01, 0.01)
        parts.append(pk.part(f'Barrel{key}_Stave{k}', stave(0.0, 0.026, a + 0.006, a + step - 0.006, height + rng.uniform(-0.01, 0.006), profile), target, m['wood'], frame=UPRIGHT, rng=rng, bevel=(0.006, 1, 35.0)))
    for index, z in enumerate((0.09, 0.24, height - 0.24, height - 0.09)):
        r = profile(z)
        parts.append(pk.part(f'Barrel{key}_Hoop{index}', pk.lathe([(r - 0.004, z - 0.03), (r + 0.008, z - 0.026), (r + 0.01, z), (r + 0.008, z + 0.026), (r - 0.004, z + 0.03)], 32), target, m['iron'], rng=rng, smooth=50.0))
    parts.append(pk.part(f'Barrel{key}_Lid', pk.cylinder(radius - 0.012, 0.03, 24, z=height - 0.07), target, m['wood'], rng=rng, smooth=30.0))
    parts.append(pk.part(f'Barrel{key}_Bung', pk.cylinder(0.025, 0.012, 10, z=height - 0.045), target, m['iron'], matrix=Matrix.Translation((radius * 0.45, 0.06, 0.0)), rng=rng, smooth=30.0))
    return kit.join(parts, f'Barrel_{key}')


def crate(key, size, m, target, rng, prefix='Crate'):
    parts = []
    t = 0.022
    rows = 4
    h = (size - 0.02) / rows
    for face, (axis, sign) in enumerate((('y', -1.0), ('y', 1.0), ('x', -1.0), ('x', 1.0))):
        for r in range(rows):
            z = 0.01 + h * (r + 0.5)
            offset = sign * (size * 0.5 - t * 0.5)
            center = (0.0, offset, z) if axis == 'y' else (offset, 0.0, z)
            length = size - (0.0 if axis == 'y' else 2.0 * t)
            parts.append(plank(f'{prefix}{key}_Board{face}_{r}', (length - 0.004, t, h - 0.008), center, m['wood'], target, rng, yaw=0.0 if axis == 'y' else 0.5 * math.pi))
        for edge in (-1.0, 1.0):
            along = edge * (size * 0.5 - 0.035)
            out = sign * (size * 0.5 + t * 0.5 - 0.002)
            center = (along, out, size * 0.5) if axis == 'y' else (out, along, size * 0.5)
            batten = pk.part(f'{prefix}{key}_Batten{face}_{int(edge)}', pk.chipped((size, t, 0.07), rng, chips=1, depth=0.006), target, m['wood'], matrix=Matrix.Translation(center) @ Matrix.Rotation(0.0 if axis == 'y' else 0.5 * math.pi, 4, 'Z') @ Matrix.Rotation(0.5 * math.pi, 4, 'Y'), rng=rng, bevel=(0.008, 1, 30.0))
            parts.append(batten)
        if axis == 'y':
            brace = pk.part(f'{prefix}{key}_Brace{face}', pk.chipped((size * 1.18, t, 0.07), rng, chips=1, depth=0.006), target, m['wood'], matrix=Matrix.Translation((0.0, sign * (size * 0.5 + t * 0.5 - 0.002), size * 0.5)) @ Matrix.Rotation(sign * math.radians(42.0), 4, 'Y'), rng=rng, bevel=(0.008, 1, 30.0))
            parts.append(brace)
    for r in range(4):
        y = -size * 0.5 + size * (r + 0.5) / 4.0
        parts.append(plank(f'{prefix}{key}_Lid{r}', (size - 0.004, size / 4.0 - 0.008, t), (0.0, y, size - t * 0.5), m['wood'], target, rng))
    for corner in range(4):
        sx = 1.0 if corner & 1 else -1.0
        sy = 1.0 if corner & 2 else -1.0
        parts.append(pk.part(f'{prefix}{key}_Corner{corner}', pk.box((0.05, 0.05, 0.12), (sx * (size * 0.5 + 0.012), sy * (size * 0.5 + 0.012), size - 0.06)), target, m['iron'], rng=rng, bevel=(0.006, 1, 30.0)))
    return kit.join(parts, f'{prefix}_{key}')


def crate_stack(m, target, rng):
    low = crate('Low', 0.84, m, target, rng, prefix='StackA')
    high = crate('High', 0.62, m, target, rng, prefix='StackB')
    high.matrix_world = Matrix.Translation((0.05, -0.05, 0.84)) @ Matrix.Rotation(0.35, 4, 'Z')
    return kit.join([low, high], 'CrateStack')


def stash_chest(m, target, rng):
    parts = []
    width, depth, height = 1.18, 0.74, 0.5
    for face, (axis, sign) in enumerate((('y', -1.0), ('y', 1.0), ('x', -1.0), ('x', 1.0))):
        for r in range(3):
            z = 0.04 + (height - 0.04) * (r + 0.5) / 3.0
            span = width if axis == 'y' else depth - 0.05
            center = (0.0, sign * (depth * 0.5 - 0.0125), z) if axis == 'y' else (sign * (width * 0.5 - 0.0125), 0.0, z)
            parts.append(plank(f'Chest_Board{face}_{r}', (span - 0.004, 0.025, (height - 0.04) / 3.0 - 0.008), center, m['wood'], target, rng, yaw=0.0 if axis == 'y' else 0.5 * math.pi))
    parts.append(pk.part('Chest_Floor', pk.box((width - 0.04, depth - 0.04, 0.04), (0.0, 0.0, 0.02)), target, m['wood'], rng=rng))
    radius = depth * 0.5
    slats = 6
    dome = lambda a: Vector((0.0, -math.cos(a) * radius, height + math.sin(a) * radius * 0.56))
    for k in range(slats):
        a0 = math.pi * k / slats
        a1 = math.pi * (k + 1) / slats
        mid = 0.5 * (a0 + a1)
        chord = (dome(a1) - dome(a0)).length
        tilt = math.atan2(0.56 * math.cos(mid), math.sin(mid))
        slat = pk.chipped((width + 0.02, chord * 1.04, 0.03), rng, chips=1, depth=0.006)
        center = (dome(a0) + dome(a1)) * 0.5 + Vector((0.0, -math.cos(mid) * 0.012, math.sin(mid) * 0.012))
        parts.append(pk.part(f'Chest_Slat{k}', slat, target, m['wood'], matrix=Matrix.Translation(center) @ Matrix.Rotation(tilt, 4, 'X'), rng=rng, bevel=(0.008, 1, 30.0)))
    for side in (-1.0, 1.0):
        cap = pk.lathe([(0.0, -0.02), (radius + 0.005, -0.02), (radius + 0.005, 0.02), (0.0, 0.02)], 24)
        bmesh.ops.transform(cap, matrix=Matrix.Diagonal((1.0, 1.0, 0.56, 1.0)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Y'), verts=cap.verts)
        kit.cut(cap, Vector((0.0, 0.0, 0.0)), Vector((0.0, 0.0, -1.0)))
        parts.append(pk.part(f'Chest_LidEnd{int(side)}', cap, target, m['wood'], matrix=Matrix.Translation((side * (width * 0.5 - 0.01), 0.0, height)), rng=rng, smooth=30.0))
    for index, x in enumerate((-0.38, 0.38)):
        band = pk.lathe([(radius + 0.012, -0.03), (radius + 0.022, -0.026), (radius + 0.022, 0.026), (radius + 0.012, 0.03)], 32)
        bmesh.ops.transform(band, matrix=Matrix.Rotation(0.5 * math.pi, 4, 'Y'), verts=band.verts)
        kit.cut(band, Vector((0.0, 0.0, 0.0)), Vector((0.0, 0.0, -1.0)))
        parts.append(pk.part(f'Chest_Band{index}', band, target, m['iron'], matrix=Matrix.Translation((x, 0.0, height)) @ Matrix.Diagonal((1.0, 1.0, 0.56, 1.0)), rng=rng, smooth=40.0))
        for sign in (-1.0, 1.0):
            parts.append(pk.part(f'Chest_Drop{index}_{int(sign)}', pk.box((0.06, 0.012, height - 0.02), (x, sign * (depth * 0.5 + 0.006), height * 0.5)), target, m['iron'], rng=rng, bevel=(0.004, 1, 30.0)))
    for corner in range(4):
        sx = 1.0 if corner & 1 else -1.0
        sy = 1.0 if corner & 2 else -1.0
        parts.append(pk.part(f'Chest_Bracket{corner}', pk.box((0.07, 0.07, height + 0.02), (sx * (width * 0.5 - 0.02), sy * (depth * 0.5 - 0.02), height * 0.5)), target, m['iron'], rng=rng, bevel=(0.006, 1, 30.0)))
    parts.append(pk.part('Chest_Lock', pk.box((0.15, 0.03, 0.17), (0.0, -depth * 0.5 - 0.02, height - 0.02)), target, m['brass'], rng=rng, bevel=(0.006, 1, 30.0)))
    parts.append(pk.part('Chest_Keyhole', pk.box((0.03, 0.012, 0.06), (0.0, -depth * 0.5 - 0.036, height - 0.04)), target, m['arcane'], rng=rng))
    for side in (-1.0, 1.0):
        handle = pk.torus(0.07, 0.012, 16, 6)
        kit.cut(handle, Vector((0.0, 0.0, 0.0)), Vector((0.0, -1.0, 0.0)))
        parts.append(pk.part(f'Chest_Handle{int(side)}', handle, target, m['iron'], matrix=Matrix.Translation((side * (width * 0.5 + 0.02), 0.0, height * 0.62)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Z') @ Matrix.Rotation(-0.5 * math.pi, 4, 'X'), rng=rng, smooth=40.0))
    return kit.join(parts, 'StashChest')


def banner_cloth(width, height, rng, columns=10, rows=16, tail=0.22):
    bm = bmesh.new()
    phase = rng.uniform(0.0, 6.28)
    grid = []
    for j in range(rows + 1):
        t = j / rows
        line = []
        for i in range(columns + 1):
            u = i / columns - 0.5
            x = u * width
            z = -t * height
            if j == rows:
                z += max(0.0, tail - abs(x) * 1.6) * 1.0
            fold = 0.03 * math.sin(u * math.pi * 4.0 + phase) * (0.35 + 0.65 * t)
            line.append(bm.verts.new((x, fold, z)))
        grid.append(line)
    for j in range(rows):
        for i in range(columns):
            bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
    return bm


def banner(m, target, rng):
    parts = [pk.part('Banner_Foot', pk.chipped((0.34, 0.34, 0.14), rng, chips=2, depth=0.025, center=(0.0, 0.0, 0.07)), target, m['stone'], rng=rng, bevel=(0.025, 2, 30.0))]
    parts.append(post('Banner_Pole', Vector((0.0, 0.0, 0.1)), Vector((0.0, 0.0, 2.72)), 0.032, m['wood'], target, rng, end_radius=0.026))
    parts.append(pk.part('Banner_Tip', pk.cone_bm((0.0, 0.0, 2.7), (0.0, 0.0, 2.92), 0.045, sides=6), target, m['iron'], rng=rng, smooth=30.0))
    parts.append(pk.part('Banner_Collar', pk.cylinder(0.04, 0.05, 10, z=2.66), target, m['iron'], rng=rng, smooth=30.0))
    bar = pk.rod((-0.56, 0.0, 0.0), (0.56, 0.0, 0.0), 0.022, 8)
    parts.append(pk.part('Banner_Bar', bar, target, m['wood'], matrix=Matrix.Translation((0.0, -0.05, 2.46)), rng=rng, smooth=40.0))
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'Banner_Knob{int(side)}', pk.ellipsoid_bm((side * 0.58, -0.05, 2.46), (0.035, 0.035, 0.035), segments=8, rings=6), target, m['brass'], rng=rng, smooth=50.0))
    cloth = pk.part('Banner_Cloth', banner_cloth(0.92, 1.5, rng), target, m['cloth'], matrix=Matrix.Translation((0.0, -0.06, 2.43)))
    solid = cloth.modifiers.new('Solidify', 'SOLIDIFY')
    solid.thickness = 0.012
    solid.offset = 0.0
    kit.apply_modifiers(cloth)
    cloth.data.shade_smooth()
    parts.append(cloth)
    for k in range(5):
        x = -0.4 + k * 0.2
        parts.append(pk.part(f'Banner_Loop{k}', pk.torus(0.03, 0.009, 12, 6), target, m['iron'], matrix=Matrix.Translation((x, -0.05, 2.45)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Y'), rng=rng, smooth=40.0))
    return kit.join(parts, 'Banner')


def books(prefix, x0, x1, z, depth, m, target, rng, top=0.42):
    parts = []
    x = x0 + rng.uniform(0.0, 0.03)
    index = 0
    while x < x1 - 0.04:
        if rng.random() < 0.08 and x < x1 - 0.3:
            stack_x = x + 0.13
            lift = 0.0
            for s in range(rng.randint(2, 4)):
                w, h, d = rng.uniform(0.2, 0.26), rng.uniform(0.03, 0.05), depth * rng.uniform(0.8, 0.98)
                frame = Matrix.Translation((stack_x + rng.uniform(-0.015, 0.015), -0.02, z + lift + h * 0.5)) @ Matrix.Rotation(rng.uniform(-0.15, 0.15), 4, 'Z')
                parts.append(pk.part(f'{prefix}_Book{index}', pk.box((w, d, h)), target, m['books'], matrix=frame, bevel=(0.004, 1, 30.0)))
                lift += h
                index += 1
            x += 0.3
            continue
        w = rng.uniform(0.028, 0.06)
        h = min(top - 0.03, rng.uniform(0.2, 0.36))
        d = depth * rng.uniform(0.78, 0.98)
        lean = rng.uniform(-0.12, 0.12) if rng.random() < 0.15 else 0.0
        frame = Matrix.Translation((x + w * 0.5, -0.02 + (depth - d) * 0.5, z)) @ Matrix.Rotation(lean, 4, 'Y') @ Matrix.Translation((0.0, 0.0, h * 0.5))
        parts.append(pk.part(f'{prefix}_Book{index}', pk.box((w, d, h)), target, m['books'], matrix=frame, bevel=(0.004, 1, 30.0)))
        x += w + rng.uniform(0.002, 0.012) + (0.04 if rng.random() < 0.06 else 0.0)
        index += 1
    return parts


def bookshelf(m, target, rng):
    width, depth, height = 2.0, 0.48, 2.2
    parts = []
    for side in (-1.0, 1.0):
        parts.append(plank(f'Shelf_Side{int(side)}', (depth, 0.05, height), (side * (width * 0.5 - 0.025), 0.0, height * 0.5), m['dark_wood'], target, rng, yaw=0.5 * math.pi))
    parts.append(plank('Shelf_Top', (width + 0.06, depth + 0.05, 0.05), (0.0, -0.01, height - 0.025), m['dark_wood'], target, rng))
    parts.append(plank('Shelf_Crown', (width + 0.12, depth + 0.1, 0.04), (0.0, -0.02, height + 0.02), m['dark_wood'], target, rng))
    parts.append(plank('Shelf_Plinth', (width, 0.05, 0.1), (0.0, -depth * 0.5 + 0.03, 0.05), m['dark_wood'], target, rng))
    parts.append(pk.part('Shelf_Back', pk.box((width - 0.06, 0.02, height - 0.06), (0.0, depth * 0.5 - 0.02, height * 0.5)), target, m['dark_wood'], rng=rng))
    levels = (0.1, 0.62, 1.12, 1.62)
    for index, z in enumerate(levels):
        parts.append(plank(f'Shelf_Board{index}', (width - 0.1, depth - 0.04, 0.03), (0.0, 0.0, z), m['dark_wood'], target, rng))
        ceiling = (levels[index + 1] if index + 1 < len(levels) else height - 0.05) - z
        parts += books(f'Shelf{index}', -width * 0.5 + 0.06, width * 0.5 - 0.06, z + 0.015, depth - 0.12, m, target, rng, top=ceiling)
    orb = pk.part('Shelf_Orb', pk.ellipsoid_bm((0.62, -0.06, 1.635 + 0.08), (0.07, 0.07, 0.07), segments=14, rings=8), target, m['crystal'], rng=rng, smooth=60.0)
    parts.append(orb)
    return kit.join(parts, 'Bookshelf')


def table_leg(name, x, y, height, m, target, rng):
    profile = [(0.04, 0.0), (0.045, 0.06), (0.03, 0.12), (0.042, 0.2), (0.03, 0.28), (0.028, 0.52), (0.04, 0.6), (0.036, height)]
    return pk.part(name, pk.lathe(profile, 10), target, m['dark_wood'], matrix=Matrix.Translation((x, y, 0.0)), frame=UPRIGHT, rng=rng, smooth=40.0)


def open_book(name, center, m, target, rng):
    parts = [pk.part(name + '_Cover', pk.box((0.46, 0.32, 0.012), center), target, m['books'], bevel=(0.003, 1, 30.0))]
    for side in (-1.0, 1.0):
        parts.append(pk.part(f'{name}_Pages{int(side)}', pk.box((0.21, 0.3, 0.025), (center[0] + side * 0.112, center[1], center[2] + 0.018)), target, m['paper'], rng=rng, bevel=(0.004, 1, 30.0)))
    return parts


def reading_table(m, target, rng):
    parts = []
    for k in range(5):
        y = -0.4 + k * 0.2
        parts.append(plank(f'Table_Plank{k}', (2.4, 0.196, 0.06), (0.0, y, 0.75), m['dark_wood'], target, rng))
    for corner in range(4):
        sx = 1.0 if corner & 1 else -1.0
        sy = 1.0 if corner & 2 else -1.0
        parts.append(table_leg(f'Table_Leg{corner}', sx * 1.06, sy * 0.38, 0.72, m, target, rng))
    for sy in (-1.0, 1.0):
        parts.append(plank(f'Table_Rail{int(sy)}', (2.1, 0.04, 0.09), (0.0, sy * 0.38, 0.66), m['dark_wood'], target, rng))
    parts.append(plank('Table_Stretcher', (2.1, 0.05, 0.05), (0.0, 0.0, 0.16), m['dark_wood'], target, rng))
    for sx in (-1.0, 1.0):
        parts.append(plank(f'Table_Foot{int(sx)}', (0.05, 0.76, 0.05), (sx * 1.06, 0.0, 0.16), m['dark_wood'], target, rng))
    parts += open_book('Table_Book', (-0.45, 0.02, 0.786), m, target, rng)
    lift = 0.78
    for k, (w, d, h, yaw) in enumerate(((0.3, 0.22, 0.05, 0.1), (0.26, 0.2, 0.045, -0.15), (0.24, 0.17, 0.04, 0.3))):
        parts.append(pk.part(f'Table_Stack{k}', pk.box((w, d, h)), target, m['books'], matrix=Matrix.Translation((0.3, 0.22, lift + h * 0.5)) @ Matrix.Rotation(yaw, 4, 'Z'), bevel=(0.004, 1, 30.0)))
        lift += h
    parts.append(pk.part('Table_Dish', pk.lathe([(0.0, 0.78), (0.07, 0.78), (0.075, 0.795), (0.065, 0.8), (0.02, 0.805), (0.0, 0.805)], 14), target, m['brass'], matrix=Matrix.Translation((0.75, 0.1, 0.0)), rng=rng, smooth=40.0))
    parts.append(pk.part('Table_Candle', pk.lathe([(0.0, 0.8), (0.022, 0.8), (0.021, 0.92), (0.017, 0.945), (0.006, 0.95), (0.0, 0.95)], 10), target, m['wax'], matrix=Matrix.Translation((0.75, 0.1, 0.0)), rng=rng, smooth=40.0))
    parts.append(pk.part('Table_Scroll', pk.cylinder(0.025, 0.32, 10), target, m['paper'], matrix=Matrix.Translation((-0.95, -0.2, 0.805)) @ Matrix.Rotation(0.5 * math.pi, 4, 'Y') @ Matrix.Rotation(0.4, 4, 'X'), rng=rng, smooth=40.0))
    parts.append(pk.part('Table_Ink', pk.lathe([(0.0, 0.78), (0.035, 0.78), (0.035, 0.82), (0.015, 0.84), (0.015, 0.855), (0.0, 0.855)], 10), target, m['glass'], matrix=Matrix.Translation((-0.05, 0.3, 0.0)), rng=rng, smooth=40.0))
    parts.append(post('Table_Quill', Vector((-0.04, 0.3, 0.83)), Vector((0.08, 0.38, 1.0)), 0.004, m['paper'], target, rng, end_radius=0.012, steps=5))
    return kit.join(parts, 'ReadingTable')


def fence_post(m, target, rng):
    shaft = pk.chipped((0.11, 0.11, 0.98), rng, chips=2, depth=0.012, center=(0.0, 0.0, 0.39))
    kit.cut(shaft, Vector((0.0, 0.035, 0.88)), Vector((0.0, 0.6, 0.8)).normalized())
    kit.cut(shaft, Vector((0.0, -0.035, 0.88)), Vector((0.0, -0.6, 0.8)).normalized())
    parts = [pk.part('Fence_Post', shaft, target, m['wood'], frame=UPRIGHT, rng=rng, bevel=(0.012, 1, 30.0))]
    for z in (0.4425, 0.8225):
        parts.append(pk.part(f'Fence_Nail{int(z * 100)}', pk.cylinder(0.012, 0.01, 6, z=0.0), target, m['iron'], matrix=Matrix.Translation((0.0, -0.075, z)) @ Matrix.Rotation(0.5 * math.pi, 4, 'X'), rng=rng, smooth=30.0))
    return kit.join(parts, 'FencePost')


def fence_rail(m, target, rng):
    parts = []
    for index, z in enumerate((0.0, 0.38)):
        rail = pk.chipped((1.02, 0.035, 0.085), rng, chips=2, depth=0.008, center=(0.0, -0.06, z + 0.0425))
        for vertex in rail.verts:
            vertex.co.z += 0.012 * math.sin(vertex.co.x * 2.4 + index * 1.7)
        parts.append(pk.part(f'Fence_Rail{index}', rail, target, m['wood'], rng=rng, bevel=(0.01, 1, 30.0)))
    return kit.join(parts, 'FenceRail')


def cloth_material(name, tone=(0.16, 0.06, 0.26), trim=(0.78, 0.55, 0.2), bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    x, y, z = s.separate(p)
    warp = s.wave(p, 260.0, kind='BANDS', direction='X', distortion=1.0, detail=1.0)
    weft = s.wave(p, 260.0, kind='BANDS', direction='Z', distortion=1.0, detail=1.0)
    weave = s.mul(warp, weft)
    blotch = s.noise(p, 5.0, detail=3.0)
    base = s.mix(s.add(s.mul(weave, 0.25), s.mul(blotch, 0.4)), (tone[0] * 0.7, tone[1] * 0.7, tone[2] * 0.7), (tone[0] * 1.25, tone[1] * 1.2, tone[2] * 1.2))
    side = s.math('ABSOLUTE', x)
    border = s.mul(s.sub(1.0, s.smooth(0.006, 0.012, s.math('ABSOLUTE', s.sub(side, 0.39)))), s.smooth(-1.42, -1.38, z))
    head = s.sub(1.0, s.smooth(0.006, 0.012, s.math('ABSOLUTE', s.add(z, 0.09))))
    dz = s.add(z, 0.68)
    ring = s.sub(1.0, s.smooth(0.008, 0.016, s.math('ABSOLUTE', s.sub(s.vector('LENGTH', s.combine(x, dz, 0.0)), 0.24))))
    rise = s.sub(dz, -0.12)
    flame_width = s.mul(s.mul(s.clamp01(s.math('DIVIDE', rise, 0.34)), s.sub(1.0, s.clamp01(s.math('DIVIDE', rise, 0.34)))), 0.55)
    flame = s.mul(s.sub(1.0, s.smooth(s.sub(flame_width, 0.012), flame_width, side)), s.mul(s.smooth(-0.01, 0.01, rise), s.sub(1.0, s.smooth(0.33, 0.35, rise))))
    core = s.mul(flame, s.sub(1.0, s.smooth(s.mul(flame_width, 0.35), s.mul(flame_width, 0.55), side)))
    thread = s.clamp01(s.add(s.add(border, head), s.add(ring, flame)))
    base = s.mix(thread, base, trim)
    base = s.mix(core, base, (1.0, 0.5, 0.15))
    dirt = pk.cavity(s, 0.04)
    base = s.mix(s.mul(dirt, 0.5), base, (tone[0] * 0.3, tone[1] * 0.3, tone[2] * 0.3))
    rough = s.lerp(thread, 0.9, 0.45)
    metal = s.mul(thread, 0.65)
    return s.finish(base, rough, height=s.add(s.mul(weave, 0.5), s.mul(thread, 0.5)), bump=0.3, bump_distance=0.002, metallic=metal, emission=s.mul(core, 0.8), emission_color=(1.0, 0.45, 0.12), emission_strength=1.6, bake_size=bake_size)


def book_material(name, bake_size=1024):
    s = kit.Shader(name)
    p = pk.local(s)
    x, y, z = s.separate(p)
    pick = s.geometry('Random Per Island')
    palette = [(0.3, 0.045, 0.035), (0.05, 0.1, 0.26), (0.06, 0.17, 0.08), (0.36, 0.24, 0.06), (0.18, 0.06, 0.26), (0.2, 0.11, 0.055), (0.12, 0.1, 0.09)]
    color = palette[0]
    for i in range(1, len(palette)):
        color = s.mix(s.smooth(i / len(palette) - 0.005, i / len(palette) + 0.005, pick), color, palette[i])
    grain = s.noise(s.offset(p, s.combine(s.mul(pick, 17.0), s.mul(pick, 29.0), 0.0)), 40.0, detail=3.0)
    base = s.mix(s.mul(grain, 0.35), color, (0.02, 0.015, 0.012))
    bands = s.mul(s.sub(1.0, s.smooth(0.004, 0.008, s.math('ABSOLUTE', s.sub(s.math('ABSOLUTE', z), 0.09)))), s.smooth(0.3, 0.35, pick))
    front = s.smooth(-0.3, -0.6, s.separate(s.geometry('Normal'))[1])
    base = s.mix(s.mul(bands, front), base, (0.7, 0.5, 0.18))
    up = s.smooth(0.6, 0.9, s.math('ABSOLUTE', s.separate(s.geometry('Normal'))[2]))
    lines = s.wave(p, 400.0, kind='BANDS', direction='Z', distortion=0.5, detail=1.0)
    paper = s.mix(s.mul(lines, 0.2), (0.72, 0.64, 0.48), (0.5, 0.43, 0.3))
    base = s.mix(s.mul(up, s.smooth(0.15, 0.2, s.math('ABSOLUTE', y))), base, paper)
    wear = pk.wear(s, 0.006)
    base = s.mix(s.mul(wear, 0.4), base, (0.45, 0.38, 0.3))
    return s.finish(base, s.remap(grain, 0.3, 0.7, 0.5, 0.72), height=grain, bump=0.1, bump_distance=0.002, metallic=s.mul(s.mul(bands, front), 0.7), bake_size=bake_size)


def build(target):
    rng = random.Random(61)
    m = {
        'wood': pk.wood_material('Wood', light=(0.25, 0.17, 0.1), dark=(0.1, 0.065, 0.04), weathered=0.35, bake_size=2048),
        'dark_wood': pk.wood_material('DarkWood', light=(0.17, 0.105, 0.06), dark=(0.065, 0.038, 0.022), weathered=0.12, bake_size=2048),
        'iron': pk.iron_material('Iron', bake_size=1024),
        'brass': pk.brass_material('Brass', bake_size=256),
        'stone': pk.stone_material('Stone', bake_size=512),
        'cloth': cloth_material('Cloth', bake_size=1024),
        'books': book_material('Books', bake_size=1024),
        'paper': pk.paper_material('Paper', bake_size=512),
        'wax': pk.wax_material('Wax', bake_size=128),
        'glass': pk.glow_material('InkGlass', color=(0.2, 0.25, 0.5), strength=0.6, base=(0.06, 0.07, 0.12), bake_size=128),
        'arcane': pk.glow_material('ArcaneGlass', color=ARCANE, strength=2.6, base=(0.4, 0.26, 0.7), bake_size=128),
        'crystal': pk.crystal_material('Orb', core=ARCANE, shell=(0.08, 0.04, 0.16), strength=2.4, bake_size=256),
    }
    return [
        barrel('A', m, target, rng),
        barrel('B', m, target, rng, height=0.66, radius=0.3, bulge=0.035, staves=14),
        crate('A', 0.8, m, target, rng),
        crate_stack(m, target, rng),
        stash_chest(m, target, rng),
        banner(m, target, rng),
        bookshelf(m, target, rng),
        reading_table(m, target, rng),
        fence_post(m, target, rng),
        fence_rail(m, target, rng),
    ]


def main():
    args = pk.options()
    kit.reset()
    target = kit.collection('Export')
    objects = build(target)
    for obj in objects:
        kit.select_only([obj])
        with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
            bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    pk.unwrap_all(objects, {'Brass': 0.01, 'Wax': 0.02, 'InkGlass': 0.02, 'ArcaneGlass': 0.02, 'Orb': 0.02, 'Paper': 0.008, 'Books': 0.004})
    if args.get('--out'):
        pk.export(objects, args['--out'], NAME, ao_distance=0.35)
    if args.get('--preview'):
        pk.ground_preview(14.0)
        for index, obj in enumerate(objects):
            obj.matrix_world = Matrix.Translation(((index % 4) * 2.6 - 3.9, (index // 4) * 2.6 - 1.3, 0.0))
        pk.preview(objects[:4], args['--preview'], 'market_a', fill=0.95)
        pk.preview(objects[4:], args['--preview'], 'market_b', fill=0.95)


if __name__ == '__main__':
    main()
