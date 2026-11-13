import os
import struct
import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, 'data', 'CelestialWarden.glb')
TARGET = os.path.join(HERE, '..', '..', '..', 'bin', 'windows-x86_64', 'Game', 'res', 'Cosmos', 'Warden.bin')
POINTS = 320000
RIDGE_SHARE = 0.42
HAND_DENSITY = 1.0
SEED = 7

SHOULDER = (0.15, 0.0, 0.89)
ELBOW = (0.19, 0.06, 0.70)
WRIST = (0.228, -0.025, 0.615)
EYE = (0.016, -0.09, 1.045)
CHEST_HEIGHT = 0.71
ARM_GAP = ((0.5, 0.6, 0.65, 0.7, 0.75, 0.8, 0.84), (0.17, 0.16, 0.13, 0.11, 0.1, 0.1, 0.11))
KNUCKLE = 0.42


def to_game(p):
    p = np.asarray(p, np.float64)
    return np.stack([p[..., 0], p[..., 2], -p[..., 1]], axis=-1)


def load():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=SOURCE)
    obj = [o for o in bpy.data.objects if o.type == 'MESH'][0]
    mesh = obj.data
    mesh.calc_loop_triangles()
    verts = np.empty(len(mesh.vertices) * 3, np.float32)
    mesh.vertices.foreach_get('co', verts)
    tris = np.empty(len(mesh.loop_triangles) * 3, np.int32)
    mesh.loop_triangles.foreach_get('vertices', tris)
    verts = verts.reshape(-1, 3).astype(np.float64)
    world = np.array(obj.matrix_world)
    verts = verts @ world[:3, :3].T + world[:3, 3]
    return verts, tris.reshape(-1, 3)


def curvature(verts, tris):
    a, b, c = verts[tris[:, 0]], verts[tris[:, 1]], verts[tris[:, 2]]
    cross = np.cross(b - a, c - a)
    area = 0.5 * np.linalg.norm(cross, axis=1)
    normals = np.zeros_like(verts)
    for k in range(3):
        np.add.at(normals, tris[:, k], cross)
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-12)
    edges = np.concatenate([tris[:, [0, 1]], tris[:, [1, 2]], tris[:, [2, 0]]])
    edges = np.concatenate([edges, edges[:, ::-1]])
    total = np.zeros_like(verts)
    count = np.zeros(len(verts))
    np.add.at(total, edges[:, 0], verts[edges[:, 1]])
    np.add.at(count, edges[:, 0], 1.0)
    count = np.maximum(count, 1.0)
    offset = verts - total / count[:, None]
    length = np.zeros(len(verts))
    np.add.at(length, edges[:, 0], np.linalg.norm(verts[edges[:, 0]] - verts[edges[:, 1]], axis=1))
    length = np.maximum(length / count, 1e-6)
    convex = np.einsum('ij,ij->i', offset, normals) / length
    for _ in range(2):
        smooth = np.zeros(len(verts))
        np.add.at(smooth, edges[:, 0], convex[edges[:, 1]])
        convex = 0.5 * convex + 0.5 * smooth / count
    scale = np.percentile(convex[convex > 0], 92)
    return normals, np.clip(convex / scale, 0.0, 1.0), area


def segment_distance(p, a, b):
    ab = b - a
    t = np.clip(((p - a) @ ab) / (ab @ ab), 0.0, 1.0)
    return np.linalg.norm(p - (a + t[:, None] * ab), axis=1)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def rig(verts):
    weights = np.zeros((len(verts), 4))
    weights[:, 0] = 1.0
    curl = np.zeros(len(verts))
    frames = []
    for side, sign in enumerate((-1.0, 1.0)):
        s = np.array([sign * SHOULDER[0], SHOULDER[1], SHOULDER[2]])
        e = np.array([sign * ELBOW[0], ELBOW[1], ELBOW[2]])
        w = np.array([sign * WRIST[0], WRIST[1], WRIST[2]])
        fore = (w - e) / np.linalg.norm(w - e)
        lateral = verts[:, 0] * sign
        gap = np.interp(verts[:, 2], ARM_GAP[0], ARM_GAP[1])
        d_upper = segment_distance(verts, s, e)
        d_fore = segment_distance(verts, e, w)
        lower = smoothstep(gap - 0.01, gap + 0.012, lateral) * smoothstep(0.075, 0.05, np.minimum(d_upper, d_fore))
        upper = smoothstep(0.93, 0.83, verts[:, 2]) * smoothstep(0.09, 0.14, lateral)
        arm = np.where(verts[:, 2] < 0.84, lower, upper) * smoothstep(0.42, 0.5, verts[:, 2])
        in_hand = (np.linalg.norm(verts - w, axis=1) < 0.15) & (lateral > 0.14) & (verts[:, 2] < 0.66)
        hand = np.where(in_hand, smoothstep(-0.022, 0.004, (verts - w) @ fore), 0.0)
        arm = np.maximum(arm, hand)
        forearm = smoothstep(-0.012, 0.012, d_upper - d_fore)
        mine = lateral > 0.0
        weights[mine, 0] = 1.0 - arm[mine]
        weights[mine, 1] = arm[mine] * (1.0 - forearm[mine]) * (1.0 - hand[mine])
        weights[mine, 2] = arm[mine] * forearm[mine] * (1.0 - hand[mine])
        weights[mine, 3] = arm[mine] * hand[mine]

        core = in_hand & (hand > 0.99)
        points = verts[core]
        centroid = points.mean(axis=0)
        axis = (centroid - w) / np.linalg.norm(centroid - w)
        along = (points - w) @ axis
        far = points[along > np.percentile(along, 85)]
        tip_axis = w + axis * ((far - w) @ axis).mean()
        bend = far.mean(axis=0) - tip_axis
        bend -= axis * (bend @ axis)
        palm = bend / np.linalg.norm(bend)
        reach = along.max()
        knuckle = KNUCKLE * reach
        c = np.clip(((verts - w) @ axis - knuckle) / (reach - knuckle), 0.0, 1.0)
        curl = np.where(in_hand, c, curl)
        frames.append((w, axis, palm, knuckle, (centroid - w) @ axis))
        print('HAND', side, 'axis', axis.round(3), 'palm', palm.round(3), 'reach', round(reach, 4), 'count', core.sum())
    return weights, curl, frames


def sample(verts, tris, normals, ridge, area, weights, curl, rng):
    side = (verts[tris].mean(axis=1)[:, 0] > 0.0).astype(int)
    boost = 1.0 + (HAND_DENSITY - 1.0) * weights[tris][:, :, 3].mean(axis=1)
    tri_ridge = ridge[tris].mean(axis=1)
    fill = area * boost
    sharp = area * boost * tri_ridge ** 3
    ridged = int(POINTS * RIDGE_SHARE)
    picks = np.concatenate([rng.choice(len(tris), POINTS - ridged, p=fill / fill.sum()), rng.choice(len(tris), ridged, p=sharp / sharp.sum())])
    u = rng.random(POINTS)
    v = rng.random(POINTS)
    flip = u + v > 1.0
    u[flip] = 1.0 - u[flip]
    v[flip] = 1.0 - v[flip]
    bary = np.stack([1.0 - u - v, u, v], axis=1)
    t = tris[picks]
    position = np.einsum('ij,ijk->ik', bary, verts[t])
    normal = np.einsum('ij,ijk->ik', bary, normals[t])
    normal /= np.maximum(np.linalg.norm(normal, axis=1, keepdims=True), 1e-12)
    edge = np.einsum('ij,ij->i', bary, ridge[t])
    w = np.einsum('ij,ijk->ik', bary, weights[t])
    c = np.einsum('ij,ij->i', bary, curl[t])
    rows = np.arange(POINTS)
    order = np.argsort(-w, axis=1)
    a = w[rows, order[:, 0]]
    b = w[rows, order[:, 1]]
    blend = a / np.maximum(a + b, 1e-6)
    s = side[picks]
    bone = lambda k: np.where(k == 0, 0, 1 + s * 3 + k - 1)
    bone0 = bone(order[:, 0])
    bone1 = bone(order[:, 1])
    rigid = (order[:, 0] == 3) & (blend > 0.995)
    bone1 = np.where(rigid, bone0, bone1)
    blend = np.where(rigid, c, blend)
    return position, normal, edge, bone0, bone1, blend


def octahedral(n):
    n = n / np.abs(n).sum(axis=1, keepdims=True)
    x, y = n[:, 0].copy(), n[:, 1].copy()
    back = n[:, 2] < 0.0
    x[back], y[back] = (1.0 - np.abs(n[back, 1])) * np.sign(n[back, 0] + 1e-12), (1.0 - np.abs(n[back, 0])) * np.sign(n[back, 1] + 1e-12)
    return np.stack([x, y], axis=1)


def main():
    rng = np.random.default_rng(SEED)
    verts, tris = load()
    verts[:, 2] -= verts[:, 2].min()
    height = verts[:, 2].max()
    normals, ridge, area = curvature(verts, tris)
    weights, curl, frames = rig(verts)
    chest_band = (np.abs(verts[:, 0]) < 0.03 * height) & (np.abs(verts[:, 2] - CHEST_HEIGHT * height) < 0.015 * height)
    anchor = to_game(np.array([0.0, verts[chest_band][:, 1].min() + 0.03 * height, CHEST_HEIGHT * height]))
    position, normal, edge, bone0, bone1, blend = sample(verts, tris, normals, ridge, area, weights, curl, rng)
    position = (to_game(position) - anchor) / height
    normal = to_game(normal)

    low = position.min(axis=0)
    high = position.max(axis=0)
    q = np.round((position - low) / (high - low) * 65535.0).astype(np.uint32)
    o = np.round((octahedral(normal) * 0.5 + 0.5) * 255.0).astype(np.uint32)
    e = np.round(np.clip(edge, 0.0, 1.0) * 255.0).astype(np.uint32)
    b = np.round(np.clip(blend, 0.0, 1.0) * 255.0).astype(np.uint32)
    words = np.stack([q[:, 0] | (q[:, 1] << 16), q[:, 2] | (o[:, 0] << 16) | (o[:, 1] << 24), e | (bone0.astype(np.uint32) << 8) | (bone1.astype(np.uint32) << 16) | (b << 24)], axis=1).astype(np.uint32)

    eyes = []
    joints = []
    for sign in (-1.0, 1.0):
        eyes.extend(((to_game([sign * EYE[0], EYE[1], EYE[2]]) - anchor) / height).tolist())
        for joint in (SHOULDER, ELBOW, WRIST):
            joints.extend(((to_game([sign * joint[0], joint[1], joint[2]]) - anchor) / height).tolist())
    hands = []
    for w, axis, palm, knuckle, middle in frames:
        hands.extend(((to_game(w) - anchor) / height).tolist())
        hands.extend(to_game(axis).tolist())
        hands.extend(to_game(palm).tolist())
        hands.extend([knuckle / height, middle / height, 0.0])
    os.makedirs(os.path.dirname(TARGET), exist_ok=True)
    with open(TARGET, 'wb') as f:
        f.write(struct.pack('<4sI', b'WRD3', len(words)))
        f.write(struct.pack('<12f', *low, *(high - low), *eyes))
        f.write(struct.pack('<18f', *joints))
        f.write(struct.pack('<24f', *hands))
        f.write(words.tobytes())
    print('WROTE', os.path.abspath(TARGET), len(words), 'bones', np.bincount(bone0, minlength=7).tolist())


main()
