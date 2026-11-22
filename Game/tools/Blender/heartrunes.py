import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SHADERS = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'bin', 'windows-x86_64', 'Game', 'res', 'Shaders', 'Game'))


def arc(a, m, b, width):
    return ('arc', a, m, b, width)


def dot(c, r):
    return ('dot', c, r)


def mirror(stroke):
    if stroke[0] == 'dot':
        return ('dot', (-stroke[1][0], stroke[1][1]), stroke[2])
    _, a, m, b, w = stroke
    return ('arc', (-a[0], a[1]), (-m[0], m[1]), (-b[0], b[1]), w)


GLYPHS = [
    [
        arc((0.0, -0.88), (0.05, -0.25), (0.0, 0.4), 0.075),
        arc((-0.34, 0.8), (0.0, 0.48), (0.34, 0.8), 0.065),
        dot((0.0, 0.84), 0.07),
        arc((-0.02, 0.12), (-0.52, 0.2), (-0.6, 0.66), 0.06),
        arc((0.02, 0.12), (0.52, 0.2), (0.6, 0.66), 0.06),
        arc((-0.52, -0.78), (-0.08, -0.42), (0.48, -0.12), 0.055),
        arc((0.52, -0.78), (0.08, -0.42), (-0.48, -0.12), 0.055),
        dot((-0.68, -0.12), 0.05),
        dot((0.68, -0.12), 0.05),
    ],
    [
        arc((-0.72, 0.25), (0.0, -0.62), (0.72, 0.25), 0.08),
        arc((-0.46, 0.22), (0.0, -0.28), (0.46, 0.22), 0.05),
        dot((0.0, 0.06), 0.1),
        arc((0.0, 0.3), (0.07, 0.58), (0.0, 0.88), 0.055),
        arc((-0.72, 0.25), (-0.86, 0.5), (-0.66, 0.66), 0.045),
        arc((0.72, 0.25), (0.86, 0.5), (0.66, 0.66), 0.045),
        dot((0.0, -0.82), 0.055),
    ],
    [
        arc((-0.48, 0.72), (-0.06, 0.46), (0.0, 0.0), 0.07),
        arc((0.0, 0.0), (0.06, -0.46), (0.48, -0.72), 0.07),
        arc((-0.48, 0.72), (-0.7, 0.52), (-0.52, 0.34), 0.05),
        arc((0.48, -0.72), (0.7, -0.52), (0.52, -0.34), 0.05),
        arc((-0.58, -0.06), (0.0, 0.1), (0.58, -0.06), 0.045),
        dot((-0.38, -0.42), 0.05),
        dot((0.38, 0.42), 0.05),
    ],
    [
        arc((0.0, -0.86), (0.04, -0.3), (0.0, 0.18), 0.07),
        arc((0.0, 0.18), (-0.05, 0.52), (0.0, 0.86), 0.055),
        arc((0.0, 0.02), (-0.46, 0.16), (-0.52, 0.72), 0.055),
        arc((0.0, 0.02), (0.46, 0.16), (0.52, 0.72), 0.055),
        arc((-0.42, -0.5), (0.0, -0.72), (0.42, -0.5), 0.05),
        dot((-0.56, 0.86), 0.045),
        dot((0.56, 0.86), 0.045),
    ],
    [
        arc((-0.12, 0.72), (-0.66, 0.0), (-0.12, -0.72), 0.075),
        arc((0.12, 0.72), (0.66, 0.0), (0.12, -0.72), 0.075),
        dot((0.0, 0.0), 0.1),
        arc((0.0, 0.24), (-0.05, 0.55), (0.0, 0.88), 0.045),
        arc((0.0, -0.24), (0.05, -0.55), (0.0, -0.88), 0.045),
        dot((-0.8, 0.0), 0.045),
        dot((0.8, 0.0), 0.045),
    ],
    [
        arc((0.0, 0.86), (-0.5, -0.1), (0.0, -0.62), 0.07),
        arc((0.0, 0.86), (0.5, -0.1), (0.0, -0.62), 0.07),
        arc((0.04, -0.36), (-0.2, -0.02), (0.06, 0.3), 0.045),
        dot((0.0, 0.5), 0.045),
        arc((-0.16, -0.6), (-0.44, -0.72), (-0.62, -0.56), 0.045),
        arc((0.16, -0.6), (0.44, -0.72), (0.62, -0.56), 0.045),
    ],
    [
        arc((0.0, -0.7), (0.05, 0.0), (0.0, 0.66), 0.07),
        arc((0.0, 0.4), (-0.46, 0.56), (-0.78, 0.22), 0.055),
        arc((0.0, 0.06), (-0.4, 0.12), (-0.62, -0.22), 0.05),
        arc((0.0, 0.4), (0.46, 0.56), (0.78, 0.22), 0.055),
        arc((0.0, 0.06), (0.4, 0.12), (0.62, -0.22), 0.05),
        arc((-0.28, -0.86), (0.0, -0.7), (0.28, -0.86), 0.05),
        dot((0.0, 0.86), 0.06),
    ],
    [
        arc((-0.56, -0.28), (0.0, 0.62), (0.56, -0.28), 0.075),
        dot((0.0, 0.06), 0.12),
        arc((-0.56, -0.28), (-0.36, -0.66), (-0.06, -0.86), 0.05),
        arc((0.56, -0.28), (0.36, -0.66), (0.06, -0.86), 0.05),
        arc((0.0, 0.62), (-0.06, 0.76), (0.0, 0.92), 0.045),
        arc((0.0, -0.14), (0.04, -0.46), (0.0, -0.78), 0.045),
    ],
]


def circle(a, m, b):
    ux, uy = m[0] - a[0], m[1] - a[1]
    vx, vy = b[0] - a[0], b[1] - a[1]
    d = 2.0 * (ux * vy - uy * vx)
    if abs(d) < 1e-6:
        raise ValueError(f'collinear arc {a} {m} {b}')
    u2, v2 = ux * ux + uy * uy, vx * vx + vy * vy
    cx = a[0] + (vy * u2 - uy * v2) / d
    cy = a[1] + (ux * v2 - vx * u2) / d
    r = math.hypot(a[0] - cx, a[1] - cy)
    ta = math.atan2(a[1] - cy, a[0] - cx)
    tm = math.atan2(m[1] - cy, m[0] - cx)
    tb = math.atan2(b[1] - cy, b[0] - cx)
    ccw = (tb - ta) % (2.0 * math.pi)
    mid = (tm - ta) % (2.0 * math.pi)
    sweep = ccw if mid < ccw else ccw - 2.0 * math.pi
    return cx, cy, r, ta, sweep


def strokes_of(glyph):
    out = []
    for s in glyph:
        if s[0] == 'dot':
            _, c, r = s
            out.append({'arc': (c[0], c[1], 0.0, 0.0), 'sweep': 0.0, 'width': r, 'ends': (c[0], c[1], c[0], c[1]), 'length': 0.3})
        else:
            _, a, m, b, w = s
            cx, cy, r, ta, sweep = circle(a, m, b)
            out.append({'arc': (cx, cy, r, ta), 'sweep': sweep, 'width': w, 'ends': (a[0], a[1], b[0], b[1]), 'length': abs(sweep) * r})
    total = sum(s['length'] for s in out)
    cursor = 0.0
    for s in out:
        s['start'] = cursor / total * 0.94
        cursor += s['length']
        s['end'] = min(cursor / total, 1.0)
    return out


def fmt(v):
    return f'{v:.4f}'


def write_glsl(path):
    arcs, looks, ends, ranges = [], [], [], []
    for glyph in GLYPHS:
        strokes = strokes_of(glyph)
        ranges.append((len(arcs), len(strokes)))
        for s in strokes:
            arcs.append('vec4(' + ', '.join(fmt(v) for v in s['arc']) + ')')
            looks.append('vec4(' + ', '.join(fmt(v) for v in (s['sweep'], s['width'], s['start'], s['end'])) + ')')
            ends.append('vec4(' + ', '.join(fmt(v) for v in s['ends']) + ')')
    lines = [
        f'const int HeartGlyphCount = {len(GLYPHS)};',
        f'const ivec2 HeartGlyphRange[{len(ranges)}] = ivec2[](' + ', '.join(f'ivec2({a}, {b})' for a, b in ranges) + ');',
        f'const vec4 HeartStrokeArc[{len(arcs)}] = vec4[](\n\t' + ',\n\t'.join(arcs) + ');',
        f'const vec4 HeartStrokeLook[{len(looks)}] = vec4[](\n\t' + ',\n\t'.join(looks) + ');',
        f'const vec4 HeartStrokeEnds[{len(ends)}] = vec4[](\n\t' + ',\n\t'.join(ends) + ');',
    ]
    with open(path, 'w', newline='\r\n') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'wrote {path}: {len(GLYPHS)} glyphs, {len(arcs)} strokes')


def field(strokes, x, y, progress):
    import numpy as np
    best = np.full(x.shape, 1e3)
    fill = np.full(x.shape, -1.0)
    for s in strokes:
        cx, cy, r, a0 = s['arc']
        qx, qy = x - cx, y - cy
        if r <= 0.0:
            d = np.hypot(qx, qy)
            width = s['width']
            param = np.full(x.shape, 0.5)
        else:
            phi = np.arctan2(qy, qx)
            t = np.mod(phi - a0, 2.0 * math.pi) if s['sweep'] >= 0.0 else np.mod(a0 - phi, 2.0 * math.pi)
            span = abs(s['sweep'])
            ax, ay, bx, by = s['ends']
            da, db = np.hypot(x - ax, y - ay), np.hypot(x - bx, y - by)
            inside = t <= span
            d = np.where(inside, np.abs(np.hypot(qx, qy) - r), np.minimum(da, db))
            param = np.where(inside, t / span, np.where(da < db, 0.0, 1.0))
            width = s['width'] * (0.3 + 0.7 * np.sqrt(np.sin(math.pi * np.clip(param, 0.0, 1.0))))
        edge = d - width
        at = s['start'] + (s['end'] - s['start']) * param
        better = edge < best
        best = np.where(better, edge, best)
        fill = np.where(better, progress - at, fill)
    return best, fill


def preview(path, size=256):
    import bpy
    import numpy as np
    cols = 4
    rows = (len(GLYPHS) + cols - 1) // cols
    image = np.zeros((rows * size, cols * size, 4), dtype=np.float32)
    image[..., 3] = 1.0
    for g, glyph in enumerate(GLYPHS):
        strokes = strokes_of(glyph)
        ys, xs = np.mgrid[0:size, 0:size]
        x = (xs + 0.5) / size * 2.2 - 1.1
        y = (ys + 0.5) / size * 2.2 - 1.1
        progress = 0.35 + 0.65 * (g % 2)
        edge, fill = field(strokes, x, y, progress)
        groove = np.clip(0.5 - edge * size / 2.2, 0.0, 1.0)
        filled = (fill >= 0.0).astype(np.float32)
        stone = np.array([0.09, 0.085, 0.08])
        hot = np.array([1.0, 0.55, 0.15])
        cold = np.array([0.02, 0.018, 0.016])
        halo = np.exp(-np.maximum(edge, 0.0) / 0.05)[..., None] * filled[..., None] * 0.35
        color = stone * (1.0 - groove[..., None]) + (cold * (1.0 - filled[..., None]) + hot * filled[..., None]) * groove[..., None] + hot * halo * (1.0 - groove[..., None])
        r0, c0 = (rows - 1 - g // cols) * size, (g % cols) * size
        image[r0:r0 + size, c0:c0 + size, :3] = np.clip(color, 0.0, 1.0)
    img = bpy.data.images.new('runes', cols * size, rows * size, alpha=True)
    img.pixels.foreach_set(image.ravel())
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()
    print(f'preview {path}')


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    write_glsl(os.path.join(SHADERS, 'HeartGlyphs.glsl'))
    if '--preview' in argv:
        preview(os.path.abspath(argv[argv.index('--preview') + 1]))


if __name__ == '__main__':
    main()
