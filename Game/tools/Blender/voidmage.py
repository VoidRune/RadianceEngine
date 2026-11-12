import math
import os
import sys
import numpy as np

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit
import hooded as hd

hd.configure(
    NAME='VoidMage',
    SOURCE=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'VoidMage.glb'),
    ARM_LEFT=((0.06, 0.02, 1.5), (0.19, 0.025, 1.46), (0.32, 0.03, 1.27), (0.435, -0.005, 1.075), (0.465, -0.035, 0.99), (0.505, -0.05, 0.92)),
    LEG_LEFT=((0.09, 0.02, 0.95), (0.155, 0.02, 0.52), (0.215, 0.03, 0.1), (0.25, -0.08, 0.03), (0.28, -0.19, 0.02)),
    FACE_BOX=((-0.085, 0.085), (-0.2, -0.04), (1.55, 1.8)),
    LEG_REACH=((0.0, 0.12, 0.2, 0.3, 0.4, 0.5), (0.17, 0.12, 0.11, 0.1, 0.1, 0.08)),
    LEG_TOP=(0.44, 0.52),
    MASKS=('Hand', 'Boots', 'Hood'),
    CAPE_CONTRAST=0.6,
    EMISSIVE={'Coat': ((0.62, 0.36, 1.0), 3.0)},
)

STEEL_TONE = (0.46, 0.41, 0.54)
STEEL_LEVEL = (0.72, 0.3, 0.66)
STEEL_EDGE = (0.3, 0.6)
STEEL_RULE = (0.4, 0.52, 0.26, 0.36)
CRYSTAL_GLOW = 0.85
CRYSTAL_RULE = (0.55, 0.68, 0.62, 0.75)
CRYSTAL_CLEAN = (1, 4)
ROUGHNESS = {'steel': 0.35, 'crystal': 0.12, 'cloth': 0.85, 'leather': 0.6, 'skin': 0.55}
CAPE_AROUND = 105.0
CAPE_RADIUS = 0.42
CAPE_FACING = 0.1
CAPE_SMOOTH = 3
PAULDRON = (0.12, 0.16, 1.24, 1.32)
PAULDRON_WEIGHTS = (0.6, 0.4)
RIM_VALUE = (0.46, 0.56)
RIM_CLEAN = (4, 3)
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
    boots = np.zeros(len(co))
    for side, _ in hd.SIDES:
        _, knee, ankle, ball, toe = hd.LEGS[side]
        leg = np.minimum(hd.segment_distance(co, knee, ankle), hd.segment_distance(co, ankle, toe))
        boots = np.maximum(boots, smoothstep(0.13, 0.1, leg) * smoothstep(0.48, 0.42, z))
    hood = smoothstep(1.52, 1.58, z) * smoothstep(0.18, 0.14, reach) * (~face)
    hd.write_mask(mesh, 'Boots', boots)
    hd.write_mask(mesh, 'Hood', hood)


def clean(mask, opening, closing):
    mask = hd.morph(hd.morph(mask, opening, np.minimum), opening, np.maximum)
    return hd.morph(hd.morph(mask, closing, np.maximum), closing, np.minimum)


def finish_maps(maps, ao, face, even=False):
    base = maps['base'][..., :3].copy()
    luma = np.array((0.2126, 0.7152, 0.0722))
    rim = np.zeros(base.shape[:2])
    if even:
        rim = hd.soften(clean(smoothstep(RIM_VALUE[0], RIM_VALUE[1], kit.linear_to_srgb(base).max(axis=-1)), *RIM_CLEAN))
        tone = base @ luma
        inside = (rim > 0.5) & (tone > 0.002)
        middle = np.median(tone[inside]) if inside.any() else 0.05
        target = middle + (tone - middle) * hd.CAPE_CONTRAST
        evened = base * (np.clip(target, 0.0, 1.0) / np.maximum(tone, 1e-4))[..., None]
        base = base + (evened - base) * rim[..., None]
    srgb = kit.linear_to_srgb(base)
    top, bottom = srgb.max(axis=-1), srgb.min(axis=-1)
    value = top
    saturation = np.where(top > 1e-4, (top - bottom) / np.maximum(top, 1e-4), 0.0)
    r, g, b = srgb[..., 0], srgb[..., 1], srgb[..., 2]
    hue = np.degrees(np.arctan2(math.sqrt(3.0) * (g - b), 2.0 * r - g - b)) % 360.0
    violet = smoothstep(228.0, 242.0, hue) * (1.0 - smoothstep(298.0, 312.0, hue))
    flesh = np.maximum(smoothstep(325.0, 340.0, hue), 1.0 - smoothstep(40.0, 55.0, hue))
    region = np.ones_like(value) if face else np.clip(maps['hand'], 0.0, 1.0)
    hood = 0.0 if face else np.clip(maps['hood'], 0.0, 1.0)
    boots = 0.0 if face else np.clip(maps['boots'], 0.0, 1.0)
    crystal = np.zeros_like(value) if (even or face) else hd.soften(clean(violet * smoothstep(CRYSTAL_RULE[0], CRYSTAL_RULE[1], saturation) * smoothstep(CRYSTAL_RULE[2], CRYSTAL_RULE[3], value), *CRYSTAL_CLEAN))
    skin = hd.soften(flesh * smoothstep(0.08, 0.14, saturation) * smoothstep(0.28, 0.38, value) * region) * (1.0 - crystal)
    if face:
        steel = np.zeros_like(value)
    else:
        steel = (1.0 - smoothstep(STEEL_RULE[0], STEEL_RULE[1], saturation)) * smoothstep(STEEL_RULE[2], STEEL_RULE[3], value)
        steel = steel * (1.0 - crystal) * (1.0 - skin) * (1.0 - hood) * (1.0 - smoothstep(0.3, 0.6, region))
        steel = smoothstep(STEEL_EDGE[0], STEEL_EDGE[1], hd.tidy(steel, *hd.METAL_TIDY)) * (1.0 - crystal) * (1.0 - rim)
    dark = (1.0 - smoothstep(0.3, 0.42, saturation)) * (1.0 - smoothstep(0.22, 0.32, value))
    leather = np.clip(np.maximum(boots, dark * (1.0 - hood)) * (1.0 - steel) * (1.0 - crystal) * (1.0 - skin), 0.0, 1.0)
    cloth = np.clip(1.0 - crystal - skin - steel - leather, 0.0, 1.0)
    classes = {'steel': steel, 'crystal': crystal, 'cloth': cloth, 'leather': leather, 'skin': skin}

    luminance = base @ luma
    grey = luminance[..., None]
    base = np.clip(grey + (base - grey) * hd.SATURATION, 0.0, 1.0)
    if steel.max() > 0.0:
        bright = np.percentile(luminance[steel > 0.5], 95) if (steel > 0.5).any() else 1.0
        level = np.clip(STEEL_LEVEL[0] + STEEL_LEVEL[1] * luminance / max(bright, 1e-4), STEEL_LEVEL[2], 1.0)
        tone = np.array(STEEL_TONE)[None, None, :] * level[..., None]
        base = base * (1.0 - steel[..., None]) + tone * steel[..., None]
    rough = sum(classes[name] * ROUGHNESS[name] for name in classes)
    shade = 1.0 - smoothstep(0.0, 0.25, value)
    rough = rough + steel * shade * 0.15 + skin * (1.0 - smoothstep(0.15, 0.3, value)) * 0.15
    orm = np.ones_like(maps['base'])
    orm[..., 0] = 1.0 - hd.AO_STRENGTH * (1.0 - ao)
    orm[..., 1] = np.clip(rough, 0.05, 1.0)
    orm[..., 2] = np.clip(steel, 0.0, 1.0)
    emit = np.clip(crystal * CRYSTAL_GLOW, 0.0, 1.0) if crystal.max() > 0.05 else None
    print('classes', {name: round(float(mask.mean()), 3) for name, mask in classes.items()})
    if even:
        print('cape rim coverage', round(float((rim > 0.5).mean()), 3))
        base = np.concatenate([base, rim[..., None]], axis=-1)
    return base, orm, emit


hd.field = field
hd.face_kinds = face_kinds
hd.mark_regions = mark_regions
hd.finish_maps = finish_maps

if __name__ == '__main__':
    hd.main()
