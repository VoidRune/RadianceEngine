import math
import os
import sys
import numpy as np

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import hooded as hd

hd.configure(
    NAME='FrostMage',
    SOURCE=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'FrostMage.glb'),
    ARM_LEFT=((0.06, 0.02, 1.5), (0.19, 0.025, 1.46), (0.32, 0.03, 1.28), (0.43, 0.0, 1.11), (0.47, -0.04, 1.01), (0.525, -0.045, 0.915)),
    LEG_LEFT=((0.09, 0.02, 0.95), (0.165, 0.02, 0.52), (0.205, 0.03, 0.1), (0.22, -0.1, 0.03), (0.23, -0.17, 0.02)),
    FACE_BOX=((-0.085, 0.085), (-0.2, -0.04), (1.55, 1.8)),
    LEG_REACH=((0.0, 0.15, 0.25, 0.36, 0.46, 0.55), (0.17, 0.15, 0.12, 0.12, 0.1, 0.075)),
    LEG_TOP=(0.46, 0.55),
    MASKS=('Hand', 'Armor', 'Fur', 'Hood'),
    CAPE_CONTRAST=0.7,
    EMISSIVE={'Coat': ((0.45, 0.78, 1.0), 3.0)},
)

STEEL_TONE = (0.48, 0.56, 0.72)
STEEL_LEVEL = (0.72, 0.3, 0.66)
STEEL_EDGE = (0.3, 0.6)
CRYSTAL_GLOW = 0.8
CRYSTAL_RULE = (0.45, 0.58, 0.6, 0.72)
CRYSTAL_CLEAN = (2, 6)
ROUGHNESS = {'steel': 0.35, 'crystal': 0.12, 'fur': 0.95, 'cloth': 0.85, 'leather': 0.6, 'skin': 0.55}
CAPE_AROUND = 105.0
CAPE_RADIUS = 0.42
CAPE_FACING = 0.1
CAPE_SMOOTH = 3
PAULDRON = (0.12, 0.16, 1.24, 1.32)
PAULDRON_WEIGHTS = (0.6, 0.4)
smoothstep = hd.smoothstep
base_field = hd.field


def field(point):
    entry = base_field(point)
    x, y, z = point
    for side, s in hd.SIDES:
        plate = float(smoothstep(PAULDRON[0], PAULDRON[1], s * x)) * float(smoothstep(PAULDRON[2], PAULDRON[3], z))
        if plate > 0.0:
            entry = hd.blend(entry, {f'shoulder.{side}': PAULDRON_WEIGHTS[0], f'upperarm.{side}': PAULDRON_WEIGHTS[1]}, plate)
    return entry


def face_kinds(obj):
    return hd.outer_cape_kinds(obj, CAPE_AROUND, CAPE_RADIUS, CAPE_FACING, CAPE_SMOOTH)


def mark_regions(obj):
    hd.mark_hands(obj)
    mesh = obj.data
    co = np.zeros(len(mesh.vertices) * 3)
    mesh.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    x, y, z = co[:, 0], co[:, 1], co[:, 2]
    (x0, x1), (y0, y1), (z0, z1) = hd.FACE_BOX
    face = (x > x0) & (x < x1) & (y > y0 - 0.03) & (y < y1) & (z > z0) & (z < z1)
    reach = np.hypot(x - hd.HEAD_AXIS[0], y - hd.HEAD_AXIS[1])
    pauldron = smoothstep(0.11, 0.15, np.abs(x)) * smoothstep(1.18, 1.24, z) * smoothstep(1.66, 1.6, z)
    torso = smoothstep(0.98, 1.02, z) * smoothstep(1.52, 1.46, z) * smoothstep(0.32, 0.28, np.hypot(x, y - hd.AXIS[1]))
    bracer = np.zeros(len(co))
    boots = np.zeros(len(co))
    for side, _ in hd.SIDES:
        _, _, elbow, wrist, _, _ = hd.ARMS[side]
        bracer = np.maximum(bracer, smoothstep(0.12, 0.09, hd.segment_distance(co, elbow, wrist)))
        _, knee, ankle, ball, toe = hd.LEGS[side]
        leg = np.minimum(hd.segment_distance(co, knee, ankle), hd.segment_distance(co, ankle, toe))
        boots = np.maximum(boots, smoothstep(0.16, 0.12, leg) * smoothstep(0.5, 0.44, z))
    hood = smoothstep(1.58, 1.64, z) * (~face)
    armor = np.maximum.reduce([pauldron, torso, bracer, boots]) * (1.0 - hood) * (~face)
    collar = smoothstep(1.34, 1.4, z) * smoothstep(1.68, 1.62, z) * smoothstep(0.28, 0.24, reach)
    hem = smoothstep(0.52, 0.46, z) * (1.0 - boots)
    fur = np.maximum(collar, hem) * (1.0 - hood) * (~face)
    hd.write_mask(mesh, 'Armor', armor)
    hd.write_mask(mesh, 'Fur', fur)
    hd.write_mask(mesh, 'Hood', hood)


def crystal_mask(mask):
    opening, closing = CRYSTAL_CLEAN
    mask = hd.morph(hd.morph(mask, opening, np.minimum), opening, np.maximum)
    return hd.morph(hd.morph(mask, closing, np.maximum), closing, np.minimum)


def finish_maps(maps, ao, face, even=False):
    base = maps['base'][..., :3].copy()
    luma = np.array((0.2126, 0.7152, 0.0722))
    if even:
        tone = base @ luma
        valid = tone > 0.002
        middle = np.median(tone[valid]) if valid.any() else 0.05
        target = middle + (tone - middle) * hd.CAPE_CONTRAST
        base = base * (np.clip(target, 0.0, 1.0) / np.maximum(tone, 1e-4))[..., None]
    srgb = kit.linear_to_srgb(base)
    top, bottom = srgb.max(axis=-1), srgb.min(axis=-1)
    value = top
    saturation = np.where(top > 1e-4, (top - bottom) / np.maximum(top, 1e-4), 0.0)
    r, g, b = srgb[..., 0], srgb[..., 1], srgb[..., 2]
    hue = np.degrees(np.arctan2(math.sqrt(3.0) * (g - b), 2.0 * r - g - b)) % 360.0
    warm = smoothstep(4.0, 12.0, hue) * (1.0 - smoothstep(52.0, 62.0, hue))
    cool = smoothstep(175.0, 195.0, hue) * (1.0 - smoothstep(250.0, 265.0, hue))
    region = np.ones_like(value) if face else np.clip(maps['hand'], 0.0, 1.0)
    hood = 0.0 if face else np.clip(maps['hood'], 0.0, 1.0)
    crystal = np.zeros_like(value) if even else hd.soften(crystal_mask(cool * smoothstep(CRYSTAL_RULE[0], CRYSTAL_RULE[1], saturation) * smoothstep(CRYSTAL_RULE[2], CRYSTAL_RULE[3], value)))
    skin = hd.soften(warm * smoothstep(0.18, 0.28, saturation) * smoothstep(0.3, 0.42, value) * region) * (1.0 - crystal)
    fur = hd.soften(np.clip(maps['fur'], 0.0, 1.0) * (1.0 - smoothstep(0.08, 0.14, saturation)) * smoothstep(0.5, 0.6, value)) * (1.0 - skin) * (1.0 - crystal) * (1.0 - hood)
    if even or face:
        steel = np.zeros_like(value)
    else:
        plate = np.clip(maps['armor'], 0.0, 1.0) * smoothstep(0.3, 0.4, value)
        trim = smoothstep(0.48, 0.6, value)
        steel = (1.0 - smoothstep(0.3, 0.42, saturation)) * np.maximum(plate, trim) * (1.0 - crystal) * (1.0 - skin) * (1.0 - fur) * (1.0 - hood)
        steel = smoothstep(STEEL_EDGE[0], STEEL_EDGE[1], hd.tidy(steel, *hd.METAL_TIDY)) * (1.0 - crystal)
    leather = warm * (1.0 - smoothstep(0.3, 0.45, value)) * (1.0 - skin) * (1.0 - steel) * (1.0 - fur)
    cloth = np.clip(1.0 - crystal - skin - fur - steel - leather, 0.0, 1.0)
    classes = {'steel': steel, 'crystal': crystal, 'fur': fur, 'cloth': cloth, 'leather': leather, 'skin': skin}

    luminance = base @ luma
    grey = luminance[..., None]
    base = np.clip(grey + (base - grey) * hd.SATURATION, 0.0, 1.0)
    if steel.max() > 0.0:
        bright = np.percentile(luminance[steel > 0.5], 95) if (steel > 0.5).any() else 1.0
        level = np.clip(STEEL_LEVEL[0] + STEEL_LEVEL[1] * luminance / max(bright, 1e-4), STEEL_LEVEL[2], 1.0)
        tone = np.array(STEEL_TONE)[None, None, :] * level[..., None]
        base = base * (1.0 - steel[..., None]) + tone * steel[..., None]
    rough = sum(classes[name] * ROUGHNESS[name] for name in classes)
    dark = 1.0 - smoothstep(0.0, 0.25, value)
    rough = rough + steel * dark * 0.15 + skin * (1.0 - smoothstep(0.15, 0.3, value)) * 0.15
    orm = np.ones_like(maps['base'])
    orm[..., 0] = 1.0 - hd.AO_STRENGTH * (1.0 - ao)
    orm[..., 1] = np.clip(rough, 0.05, 1.0)
    orm[..., 2] = np.clip(steel, 0.0, 1.0)
    emit = np.clip(crystal * CRYSTAL_GLOW, 0.0, 1.0) if crystal.max() > 0.05 and not (face or even) else None
    print('classes', {name: round(float(mask.mean()), 3) for name, mask in classes.items()})
    return base, orm, emit


hd.field = field
hd.face_kinds = face_kinds
hd.mark_regions = mark_regions
hd.finish_maps = finish_maps

if __name__ == '__main__':
    hd.main()
