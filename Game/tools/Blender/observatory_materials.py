import os
import struct
import sys
import zlib

import numpy as np


def save_png(path, data):
    data = np.clip(data, 0.0, 1.0)
    image = (data * 255.0 + 0.5).astype(np.uint8)
    height, width = image.shape[:2]
    channels = 1 if image.ndim == 2 else image.shape[2]
    color = {1: 0, 3: 2, 4: 6}[channels]
    rows = np.zeros((height, width * channels + 1), dtype=np.uint8)
    rows[:, 1:] = image.reshape(height, width * channels)
    chunks = [b'\x89PNG\r\n\x1a\n']

    def chunk(tag, body):
        chunks.append(struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xFFFFFFFF))
    chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, color, 0, 0, 0))
    chunk(b'IDAT', zlib.compress(rows.tobytes(), 6))
    chunk(b'IEND', b'')
    with open(path, 'wb') as output:
        output.write(b''.join(chunks))


def to_srgb(linear):
    linear = np.clip(linear, 0.0, 1.0)
    return np.where(linear <= 0.0031308, linear * 12.92, 1.055 * linear ** (1.0 / 2.4) - 0.055)


def smooth(low, high, x):
    t = np.clip((x - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def fbm(size, rng, cells=4, octaves=5, gain=0.55):
    total = np.zeros((size, size), dtype=np.float32)
    amplitude = 1.0
    weight = 0.0
    for _ in range(octaves):
        lattice = rng.random((cells, cells), dtype=np.float32)
        axis = np.arange(size, dtype=np.float32) * cells / size
        i = axis.astype(np.int32)
        f = axis - i
        f = f * f * (3.0 - 2.0 * f)
        i1 = (i + 1) % cells
        top = lattice[np.ix_(i, i)] * (1.0 - f[None, :]) + lattice[np.ix_(i, i1)] * f[None, :]
        bottom = lattice[np.ix_(i1, i)] * (1.0 - f[None, :]) + lattice[np.ix_(i1, i1)] * f[None, :]
        total += amplitude * (top * (1.0 - f[:, None]) + bottom * f[:, None])
        weight += amplitude
        amplitude *= gain
        cells *= 2
    return total / weight


def normals(height, strength):
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    nz = np.ones_like(height)
    length = np.sqrt(dx * dx + dy * dy + nz * nz)
    return np.stack([(-dx / length) * 0.5 + 0.5, (dy / length) * 0.5 + 0.5, nz / length * 0.5 + 0.5], axis=-1)


def export(folder, name, albedo, height, rough, ao, strength, metal=None):
    target = os.path.join(folder, name)
    os.makedirs(target, exist_ok=True)
    save_png(os.path.join(target, name + '_diff.png'), to_srgb(albedo))
    save_png(os.path.join(target, name + '_disp.png'), height)
    save_png(os.path.join(target, name + '_nor_gl.png'), normals(height, strength))
    save_png(os.path.join(target, name + '_arm.png'), np.stack([ao, rough, np.zeros_like(ao) if metal is None else metal], axis=-1))
    print('MATERIAL', name, 'mean albedo', albedo.reshape(-1, 3).mean(axis=0))


def lerp(a, b, t):
    return a + (b - a) * t[..., None] if isinstance(t, np.ndarray) else a + (b - a) * t


def crack_field(size, rng, count, steps, step_px, wander=0.55):
    field = np.full((size, size), 1e6, dtype=np.float32)

    def stamp(ax, ay, bx, by, radius=6):
        low_x, high_x = int(min(ax, bx)) - radius, int(max(ax, bx)) + radius + 1
        low_y, high_y = int(min(ay, by)) - radius, int(max(ay, by)) + radius + 1
        xs = np.arange(low_x, high_x)
        ys = np.arange(low_y, high_y)
        gx, gy = np.meshgrid(xs, ys)
        dx, dy = bx - ax, by - ay
        t = np.clip(((gx - ax) * dx + (gy - ay) * dy) / max(dx * dx + dy * dy, 1e-6), 0.0, 1.0)
        distance = np.hypot(gx - ax - dx * t, gy - ay - dy * t)
        wrapped = np.ix_(ys % size, xs % size)
        field[wrapped] = np.minimum(field[wrapped], distance)

    for _ in range(count):
        x, y = rng.uniform(0.0, size, 2)
        heading = rng.uniform(0.0, 2.0 * np.pi)
        stack = [(x, y, heading, steps)]
        while stack:
            x, y, heading, left = stack.pop()
            while left > 0:
                heading += rng.uniform(-wander, wander)
                nx, ny = x + np.cos(heading) * step_px, y + np.sin(heading) * step_px
                stamp(x, y, nx, ny)
                x, y = nx, ny
                left -= 1
                if rng.random() < 0.09 and left > 3:
                    stack.append((x, y, heading + rng.choice((-1.0, 1.0)) * rng.uniform(0.5, 1.1), left // 2))
    return field


def flagstones(folder, name, seed, light, dark, accent, size=2048, rows=8, per_row=(4, 7), lichen_amount=0.3):
    rng = np.random.default_rng(seed)
    wobble_x = fbm(size, rng, 6, 3) - 0.5
    wobble_y = fbm(size, rng, 6, 3) - 0.5
    grid_y, grid_x = np.mgrid[0:size, 0:size].astype(np.float32) / size
    v = (grid_y + wobble_y * 0.012) % 1.0
    u = (grid_x + wobble_x * 0.012) % 1.0
    row = np.clip((v * rows).astype(np.int32), 0, rows - 1)
    row_v = v * rows - row
    edges = []
    tones = []
    for r in range(rows):
        count = int(rng.integers(per_row[0], per_row[1] + 1))
        widths = rng.uniform(0.7, 1.35, count)
        widths /= widths.sum()
        start = rng.random()
        positions = (np.concatenate([[0.0], np.cumsum(widths)[:-1]]) + start) % 1.0
        positions.sort()
        edges.append(positions)
        tones.append(rng.random((count, 4), dtype=np.float32))
    stone = np.zeros((size, size), dtype=np.int32)
    across = np.zeros((size, size), dtype=np.float32)
    stone_tone = np.zeros((size, size, 4), dtype=np.float32)
    for r in range(rows):
        mask = row == r
        positions = edges[r]
        values = u[mask]
        index = np.searchsorted(positions, values, side='right') - 1
        index = np.where(index < 0, len(positions) - 1, index)
        following = (index + 1) % len(positions)
        left = (values - positions[index]) % 1.0
        right = (positions[following] - values) % 1.0
        across[mask] = np.minimum(left, right) * (size / 1.0)
        stone[mask] = index + r * 16
        stone_tone[mask] = tones[r][index]
    down = np.minimum(row_v, 1.0 - row_v) * size / rows
    chip = fbm(size, rng, 20, 3)
    edge = np.minimum(across, down) + (chip - 0.5) * 16.0
    face = smooth(1.5, 7.0, edge)
    rise = smooth(1.5, 14.0, edge)
    large = fbm(size, rng, 3, 5)
    mid = fbm(size, rng, 10, 4)
    fine = fbm(size, rng, 64, 3)
    tilt_x = (stone_tone[..., 2] - 0.5) * (u % (1.0 / 4.0)) * 0.5
    height = (0.72 + 0.14 * stone_tone[..., 0] + 0.12 * (mid - 0.5) + 0.06 * (fine - 0.5) + tilt_x) * rise
    crack = 1.0 - smooth(0.6, 2.4, crack_field(size, rng, 26, 14, 14.0))
    pits = (1.0 - smooth(0.0, 0.05, np.abs(fbm(size, rng, 48, 2) - 0.5))) * smooth(0.5, 0.72, mid)
    height = height * (1.0 - 0.25 * pits)
    height = height * (1.0 - 0.55 * crack)
    tone = np.clip(stone_tone[..., 0] * 0.65 + (large - 0.5) * 0.9 + (mid - 0.5) * 0.5 + 0.2, 0.0, 1.0)
    base = lerp(np.array(dark, dtype=np.float32), np.array(light, dtype=np.float32), tone)
    odd = smooth(0.78, 0.9, stone_tone[..., 3])
    base = lerp(base, np.array(accent, dtype=np.float32) * (0.8 + 0.4 * tone[..., None]), odd * 0.75)
    speckle = fbm(size, rng, 160, 2)
    base = base * (0.74 + 0.34 * fine[..., None] + 0.26 * speckle[..., None])
    base = base * (1.0 - 0.28 * pits[..., None])
    base = base * (0.82 + 0.36 * mid[..., None])
    lichen = smooth(0.66, 0.74, fbm(size, rng, 24, 3)) * smooth(0.5, 0.7, large)
    base = lerp(base, np.array((0.5, 0.46, 0.3), dtype=np.float32), lichen * lichen_amount)
    wear = 1.0 - smooth(4.0, 12.0, edge)
    base = base * (1.0 - 0.3 * wear[..., None] * face[..., None])
    joint = np.array((0.045, 0.036, 0.026), dtype=np.float32) * (0.7 + 0.6 * mid[..., None])
    base = lerp(joint, base, face)
    base = base * (1.0 - 0.6 * crack[..., None])
    rough = np.clip(0.48 + 0.2 * mid + 0.3 * (1.0 - face), 0.0, 1.0)
    ao = np.clip(0.42 + 0.58 * smooth(0.0, 12.0, edge), 0.0, 1.0)
    export(folder, name, base, height, rough, ao, 7.0)


def ashlar(folder, name, seed, size=1024):
    rng = np.random.default_rng(seed)
    large = fbm(size, rng, 3, 5)
    mid = fbm(size, rng, 10, 4)
    fine = fbm(size, rng, 64, 3)
    speckle = fbm(size, rng, 160, 2)
    tone = np.clip((large - 0.5) * 1.4 + (mid - 0.5) * 0.7 + 0.5, 0.0, 1.0)
    base = lerp(np.array((0.34, 0.315, 0.27), dtype=np.float32), np.array((0.62, 0.585, 0.51), dtype=np.float32), tone)
    grey = smooth(0.58, 0.72, fbm(size, rng, 4, 3))
    base = lerp(base, np.array((0.4, 0.385, 0.35), dtype=np.float32), grey * 0.55)
    base = base * (0.76 + 0.3 * fine[..., None] + 0.22 * speckle[..., None])
    crack = 1.0 - smooth(0.5, 2.0, crack_field(size, rng, 16, 12, 12.0))
    pits = (1.0 - smooth(0.0, 0.05, np.abs(fbm(size, rng, 40, 2) - 0.5))) * smooth(0.48, 0.7, mid)
    base = base * (1.0 - 0.6 * crack[..., None]) * (1.0 - 0.3 * pits[..., None])
    height = np.clip(0.76 + 0.18 * (mid - 0.5) + 0.1 * (fine - 0.5) - 0.3 * crack - 0.12 * pits, 0.0, 1.0)
    rough = np.clip(0.56 + 0.2 * mid, 0.0, 1.0)
    ao = np.clip(1.0 - 0.45 * crack - 0.2 * pits, 0.0, 1.0)
    export(folder, name, base, height, rough, ao, 4.0)


def moss(folder, name, seed, size=1024):
    rng = np.random.default_rng(seed)
    clumps = fbm(size, rng, 10, 4, 0.6)
    tufts = fbm(size, rng, 48, 3, 0.6)
    fine = fbm(size, rng, 128, 2)
    patches = fbm(size, rng, 3, 3)
    height = np.clip(0.62 + 0.55 * (clumps - 0.5) + 0.4 * (tufts - 0.5) + 0.15 * (fine - 0.5), 0.0, 1.0)
    light = np.array((0.2, 0.3, 0.055), dtype=np.float32)
    dark = np.array((0.045, 0.085, 0.022), dtype=np.float32)
    yellow = np.array((0.32, 0.34, 0.07), dtype=np.float32)
    base = lerp(dark, light, smooth(0.35, 0.72, height))
    base = lerp(base, yellow, smooth(0.55, 0.72, patches) * smooth(0.5, 0.8, tufts) * 0.8)
    base = base * (0.8 + 0.4 * fine[..., None])
    rough = np.clip(0.8 + 0.18 * fine, 0.0, 1.0)
    ao = np.clip(0.45 + 0.75 * height, 0.0, 1.0)
    export(folder, name, base, height, rough, ao, 3.0)


def plates(folder, name, seed, seeds=19, size=1024):
    rng = np.random.default_rng(seed)
    points = rng.random((seeds, 2), dtype=np.float32) * size
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    nearest = np.full((size, size), 1e9, dtype=np.float32)
    second = np.full((size, size), 1e9, dtype=np.float32)
    owner = np.zeros((size, size), dtype=np.int32)
    for index, (px, py) in enumerate(points):
        dx = np.abs(x - px)
        dy = np.abs(y - py)
        dx = np.minimum(dx, size - dx)
        dy = np.minimum(dy, size - dy)
        distance = np.sqrt(dx * dx + dy * dy)
        closer = distance < nearest
        second = np.where(closer, nearest, np.minimum(second, distance))
        owner = np.where(closer, index, owner)
        nearest = np.minimum(nearest, distance)
    chip = fbm(size, rng, 24, 3)
    edge = (second - nearest) * 0.5 + (chip - 0.5) * 3.5
    face = smooth(0.2, 1.7, edge)
    rise = smooth(0.2, 6.0, edge)
    tones = rng.random((seeds, 3), dtype=np.float32)
    tone = tones[owner]
    large = fbm(size, rng, 3, 5)
    mid = fbm(size, rng, 10, 4)
    fine = fbm(size, rng, 64, 3)
    speckle = fbm(size, rng, 160, 2)
    shade = np.clip(tone[..., 0] * 0.55 + (large - 0.5) * 0.9 + (mid - 0.5) * 0.45 + 0.28, 0.0, 1.0)
    base = lerp(np.array((0.28, 0.255, 0.21), dtype=np.float32), np.array((0.48, 0.44, 0.37), dtype=np.float32), shade)
    warm = smooth(0.62, 0.86, tone[..., 1])
    base = lerp(base, np.array((0.42, 0.35, 0.26), dtype=np.float32), warm * 0.45)
    base = base * (0.78 + 0.26 * fine[..., None] + 0.2 * speckle[..., None])
    crack = 1.0 - smooth(0.5, 1.9, crack_field(size, rng, 18, 12, 11.0))
    pits = (1.0 - smooth(0.0, 0.05, np.abs(fbm(size, rng, 48, 2) - 0.5))) * smooth(0.5, 0.72, mid)
    wear = 1.0 - smooth(1.5, 7.0, edge)
    base = base * (1.0 - 0.22 * wear[..., None]) * (1.0 - 0.6 * crack[..., None]) * (1.0 - 0.22 * pits[..., None])
    joint = np.array((0.05, 0.045, 0.036), dtype=np.float32) * (0.7 + 0.6 * mid[..., None])
    base = lerp(joint, base, face)
    height = (0.74 + 0.12 * tone[..., 2] + 0.1 * (mid - 0.5) + 0.05 * (fine - 0.5)) * rise * (1.0 - 0.5 * crack) * (1.0 - 0.2 * pits)
    polish = smooth(0.34, 0.66, large) * (0.45 + 0.55 * tone[..., 2])
    rough = np.clip(0.54 - 0.34 * polish + 0.12 * (mid - 0.5) + 0.3 * (1.0 - face) + 0.35 * crack + 0.3 * wear, 0.0, 1.0)
    ao = np.clip(0.45 + 0.55 * smooth(0.0, 7.0, edge), 0.0, 1.0)
    export(folder, name, base, height, rough, ao, 5.0)


def inlay_blue(folder, name, seed, size=512):
    rng = np.random.default_rng(seed)
    cells = 12
    y, x = np.mgrid[0:size, 0:size].astype(np.float32) / size * cells
    ix = x.astype(np.int32)
    iy = (y + 0.5 * (ix % 2)).astype(np.int32) % cells
    fx = x - ix
    fy = (y + 0.5 * (ix % 2)) % 1.0
    edge = np.minimum(np.minimum(fx, 1.0 - fx), np.minimum(fy, 1.0 - fy)) * size / cells
    face = smooth(0.4, 1.8, edge)
    tiles = rng.random((cells, cells), dtype=np.float32)
    tone = tiles[iy, ix]
    mid = fbm(size, rng, 6, 4)
    base = lerp(np.array((0.055, 0.09, 0.14), dtype=np.float32), np.array((0.12, 0.185, 0.25), dtype=np.float32), np.clip(tone * 0.8 + (mid - 0.5) * 0.7, 0.0, 1.0))
    base = lerp(np.array((0.05, 0.05, 0.05), dtype=np.float32), base, face)
    height = (0.6 + 0.16 * tone) * face
    rough = np.clip(0.3 + 0.2 * mid + 0.35 * (1.0 - face), 0.0, 1.0)
    export(folder, name, base, height, rough, np.clip(0.55 + 0.45 * face, 0.0, 1.0), 1.1)


def metal(folder, name, seed, light, dark, patina, cover, size=512, roughness=(0.32, 0.6)):
    rng = np.random.default_rng(seed)
    large = fbm(size, rng, 4, 5)
    fine = fbm(size, rng, 64, 3)
    streak = fbm(size, rng, 18, 3)
    base = lerp(np.array(dark, dtype=np.float32), np.array(light, dtype=np.float32), smooth(0.3, 0.72, large))
    base = base * (0.84 + 0.3 * fine[..., None])
    green = smooth(cover, cover + 0.16, large * 0.65 + streak * 0.35)
    base = lerp(base, np.array(patina, dtype=np.float32) * (0.75 + 0.5 * fine[..., None]), green)
    metallic = 1.0 - green
    rough = np.clip(roughness[0] + (roughness[1] - roughness[0]) * large + 0.4 * green, 0.0, 1.0)
    height = np.clip(0.6 + 0.2 * (fine - 0.5) + 0.2 * green, 0.0, 1.0)
    export(folder, name, base, height, rough, np.clip(0.8 + 0.2 * large, 0.0, 1.0), 1.5, metal=metallic)


def mural(folder, name, seed, size=1024):
    rng = np.random.default_rng(seed)
    y, x = (np.mgrid[0:size, 0:size].astype(np.float32) + 0.5) / size
    night = lerp(np.array((0.035, 0.06, 0.11), dtype=np.float32), np.array((0.07, 0.12, 0.17), dtype=np.float32), fbm(size, rng, 4, 5))
    base = night.copy()
    cx, cy = 0.5, 0.42
    r = np.hypot(x - cx, y - cy)
    gold = np.array((0.72, 0.54, 0.24), dtype=np.float32)
    lines = np.zeros((size, size), dtype=np.float32)
    for radius, width in ((0.16, 0.003), (0.27, 0.0022), (0.37, 0.0022), (0.46, 0.003)):
        lines = np.maximum(lines, 1.0 - smooth(width * 0.6, width * 1.3, np.abs(r - radius)))
    sun = 1.0 - smooth(0.085, 0.095, r)
    angle = np.arctan2(y - cy, x - cx)
    rays = (1.0 - smooth(0.0, 0.05, np.abs(((angle * 12.0 / (2.0 * np.pi)) % 1.0) - 0.5) * 2.0 + (r - 0.1) * 6.0)) * (r > 0.09) * (r < 0.16)
    stars = np.zeros((size, size), dtype=np.float32)
    points = []
    for _ in range(90):
        px, py = rng.random(2)
        points.append((px, py))
        grow = rng.uniform(0.0016, 0.0042)
        stars = np.maximum(stars, 1.0 - smooth(grow * 0.6, grow * 1.2, np.hypot(x - px, y - py)))
    for index in range(0, 54, 3):
        for a, b in ((points[index], points[index + 1]), (points[index + 1], points[index + 2])):
            dx, dy = b[0] - a[0], b[1] - a[1]
            t = np.clip(((x - a[0]) * dx + (y - a[1]) * dy) / max(dx * dx + dy * dy, 1e-6), 0.0, 1.0)
            if dx * dx + dy * dy < 0.06:
                stars = np.maximum(stars, 0.6 * (1.0 - smooth(0.0007, 0.0016, np.hypot(x - a[0] - dx * t, y - a[1] - dy * t))))
    art = np.maximum.reduce([lines * 0.8, sun, rays, stars])
    base = lerp(base, gold * (0.8 + 0.5 * fbm(size, rng, 30, 3)[..., None]), art)
    plaster = np.array((0.3, 0.27, 0.21), dtype=np.float32) * (0.75 + 0.5 * fbm(size, rng, 20, 4)[..., None])
    flake = smooth(0.62, 0.72, fbm(size, rng, 8, 5))
    flake = np.maximum(flake, smooth(0.58, 0.68, fbm(size, rng, 40, 3)) * smooth(0.55, 0.72, fbm(size, rng, 5, 3)))
    base = lerp(base, plaster, flake)
    grime = fbm(size, rng, 6, 5)
    base = base * (0.62 + 0.5 * grime[..., None])
    crack = 1.0 - smooth(0.5, 1.7, crack_field(size, rng, 10, 16, 14.0))
    base = base * (1.0 - 0.7 * crack[..., None])
    height = np.clip(0.55 + 0.25 * flake + 0.1 * (grime - 0.5) - 0.3 * crack, 0.0, 1.0)
    export(folder, name, base, height, np.clip(0.82 + 0.15 * grime, 0.0, 1.0), np.clip(1.0 - 0.4 * crack, 0.0, 1.0), 2.0)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    options = dict(zip(argv[::2], argv[1::2]))
    output = options['--out']
    moss(output, 'CitadelMoss', 31)
    plates(output, 'ObservatoryFloor', 61)
    flagstones(output, 'ObservatoryBlock', 67, (0.58, 0.545, 0.47), (0.36, 0.335, 0.285), (0.47, 0.43, 0.36), rows=7, per_row=(3, 5))
    ashlar(output, 'ObservatoryAshlar', 71)
    inlay_blue(output, 'ObservatoryBlue', 73)
    metal(output, 'ObservatoryGold', 79, (0.72, 0.51, 0.22), (0.44, 0.29, 0.1), (0.26, 0.38, 0.3), 0.78, roughness=(0.3, 0.52))
    metal(output, 'ObservatoryBronze', 83, (0.46, 0.34, 0.2), (0.24, 0.17, 0.1), (0.2, 0.42, 0.36), 0.62, size=1024, roughness=(0.22, 0.5))
    metal(output, 'ForgeJointSilver', 101, (0.93, 0.93, 0.95), (0.7, 0.71, 0.74), (0.2, 0.42, 0.36), 2.0, roughness=(0.05, 0.16))
    mural(output, 'ObservatoryMural', 89)


if __name__ == '__main__':
    main()
