import math
import os
import numpy as np
import bpy

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'mocap')
FPS = 30.0
UNIT = 0.0254 / 0.45
TO_BLENDER = np.array([[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]])
MIRROR = np.diag([-1.0, 1.0, 1.0])
FORWARD = np.array([0.0, -1.0, 0.0])
UP = np.array([0.0, 0.0, 1.0])
AXES = {'x': 0, 'y': 1, 'z': 2}
SOURCE = {
    'hips': ('root', False),
    'spine': ('upperback', False),
    'chest': ('thorax', False),
    'neck': ('lowerneck', False),
    'head': ('head', False),
}
for _side in 'lr':
    SOURCE.update({
        f'shoulder.{_side}': (f'{_side}clavicle', False),
        f'upperarm.{_side}': (f'{_side}humerus', True),
        f'lowerarm.{_side}': (f'{_side}radius', True),
        f'wrist.{_side}': (f'{_side}wrist', True),
        f'hand.{_side}': (f'{_side}hand', True),
        f'upperleg.{_side}': (f'{_side}femur', True),
        f'lowerleg.{_side}': (f'{_side}tibia', True),
        f'foot.{_side}': (f'{_side}foot', True),
        f'toes.{_side}': (f'{_side}toes', True),
    })


def axis_rotation(axis, angles):
    angles = np.asarray(angles, dtype=np.float64)
    c, s = np.cos(angles), np.sin(angles)
    m = np.zeros(angles.shape + (3, 3))
    i, j = [(1, 2), (2, 0), (0, 1)][axis]
    m[..., axis, axis] = 1.0
    m[..., i, i] = c
    m[..., j, j] = c
    m[..., i, j] = -s
    m[..., j, i] = s
    return m


def euler(rx, ry, rz):
    return axis_rotation(2, rz) @ axis_rotation(1, ry) @ axis_rotation(0, rx)


def rotate_about(axis, angle):
    axis = np.asarray(axis, dtype=np.float64)
    axis = axis / np.linalg.norm(axis)
    k = np.array([[0.0, -axis[2], axis[1]], [axis[2], 0.0, -axis[0]], [-axis[1], axis[0], 0.0]])
    return np.eye(3) + math.sin(angle) * k + (1.0 - math.cos(angle)) * (k @ k)


def yaw(angle):
    return rotate_about(UP, angle)


def to_quat(m):
    m = np.asarray(m)
    q = np.zeros(m.shape[:-2] + (4,))
    q[..., 0] = np.sqrt(np.maximum(0.0, 1.0 + m[..., 0, 0] + m[..., 1, 1] + m[..., 2, 2])) * 0.5
    q[..., 1] = np.copysign(np.sqrt(np.maximum(0.0, 1.0 + m[..., 0, 0] - m[..., 1, 1] - m[..., 2, 2])) * 0.5, m[..., 2, 1] - m[..., 1, 2])
    q[..., 2] = np.copysign(np.sqrt(np.maximum(0.0, 1.0 - m[..., 0, 0] + m[..., 1, 1] - m[..., 2, 2])) * 0.5, m[..., 0, 2] - m[..., 2, 0])
    q[..., 3] = np.copysign(np.sqrt(np.maximum(0.0, 1.0 - m[..., 0, 0] - m[..., 1, 1] + m[..., 2, 2])) * 0.5, m[..., 1, 0] - m[..., 0, 1])
    return q / np.linalg.norm(q, axis=-1, keepdims=True)


def to_matrix(q):
    q = q / np.linalg.norm(q, axis=-1, keepdims=True)
    w, x, y, z = q[..., 0], q[..., 1], q[..., 2], q[..., 3]
    m = np.empty(q.shape[:-1] + (3, 3))
    m[..., 0, 0] = 1 - 2 * (y * y + z * z)
    m[..., 0, 1] = 2 * (x * y - z * w)
    m[..., 0, 2] = 2 * (x * z + y * w)
    m[..., 1, 0] = 2 * (x * y + z * w)
    m[..., 1, 1] = 1 - 2 * (x * x + z * z)
    m[..., 1, 2] = 2 * (y * z - x * w)
    m[..., 2, 0] = 2 * (x * z - y * w)
    m[..., 2, 1] = 2 * (y * z + x * w)
    m[..., 2, 2] = 1 - 2 * (x * x + y * y)
    return m


def continuous(q):
    q = q.copy()
    for i in range(1, len(q)):
        if np.dot(q[i], q[i - 1]) < 0.0:
            q[i] = -q[i]
    return q


def arc(a, b):
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    if a.ndim > 1 or b.ndim > 1:
        a, b = np.broadcast_arrays(a, b)
        return np.stack([arc(x, y) for x, y in zip(a, b)])
    a = a / np.linalg.norm(a)
    b = b / np.linalg.norm(b)
    axis = np.cross(a, b)
    s = np.linalg.norm(axis)
    if s < 1e-9:
        return np.eye(3)
    return rotate_about(axis, math.atan2(s, float(np.dot(a, b))))


def slerp(a, b, t):
    qa, qb = to_quat(a), to_quat(b)
    t = np.asarray(t, dtype=np.float64)
    dot = np.sum(qa * qb, axis=-1, keepdims=True)
    qb = np.where(dot < 0.0, -qb, qb)
    dot = np.abs(dot)
    theta = np.arccos(np.clip(dot, -1.0, 1.0))
    sin = np.sin(theta)
    tt = t[..., None] if t.ndim else t
    wa = np.where(sin > 1e-6, np.sin((1.0 - tt) * theta) / np.maximum(sin, 1e-12), 1.0 - tt)
    wb = np.where(sin > 1e-6, np.sin(tt * theta) / np.maximum(sin, 1e-12), tt)
    return to_matrix(qa * wa + qb * wb)


def smoothstep(low, high, value):
    t = np.clip((np.asarray(value, dtype=np.float64) - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def gaussian(values, sigma):
    if sigma <= 0.0 or len(values) < 3:
        return values
    radius = int(math.ceil(sigma * 3.0))
    kernel = np.exp(-0.5 * (np.arange(-radius, radius + 1) / sigma) ** 2)
    kernel /= kernel.sum()
    padded = np.concatenate([np.repeat(values[:1], radius, 0), values, np.repeat(values[-1:], radius, 0)])
    flat = padded.reshape(len(padded), -1)
    out = np.stack([np.convolve(flat[:, k], kernel, mode='valid') for k in range(flat.shape[1])], axis=1)
    return out.reshape((len(values),) + values.shape[1:])


def smooth_rotations(r, sigma):
    if sigma <= 0.0:
        return r
    return to_matrix(gaussian(continuous(to_quat(r)), sigma))


class Skeleton:
    def __init__(self, path):
        self.bones = {}
        self.children = {}
        self.parents = {}
        section = None
        current = None
        for raw in open(path, encoding='latin-1'):
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith(':'):
                section = line.split()[0][1:]
                continue
            words = line.split()
            if section == 'bonedata':
                if words[0] == 'begin':
                    current = {'dof': [], 'axis': np.zeros(3), 'direction': np.zeros(3), 'length': 0.0}
                elif words[0] == 'end':
                    self.bones[current['name']] = current
                elif words[0] in ('name', 'direction', 'length', 'axis', 'dof'):
                    if words[0] == 'name':
                        current['name'] = words[1]
                    elif words[0] == 'direction':
                        current['direction'] = np.array([float(v) for v in words[1:4]])
                    elif words[0] == 'length':
                        current['length'] = float(words[1]) * UNIT
                    elif words[0] == 'axis':
                        current['axis'] = np.radians([float(v) for v in words[1:4]])
                    else:
                        current['dof'] = words[1:]
            elif section == 'hierarchy' and words[0] not in ('begin', 'end'):
                self.children.setdefault(words[0], []).extend(words[1:])
                for child in words[1:]:
                    self.parents[child] = words[0]
        self.order = []
        stack = list(reversed(self.children['root']))
        while stack:
            name = stack.pop()
            self.order.append(name)
            stack.extend(reversed(self.children.get(name, [])))
        for bone in self.bones.values():
            bone['C'] = euler(*bone['axis'])

    def rest_direction(self, name):
        return TO_BLENDER @ self.bones[name]['direction']

    def leg(self):
        return self.bones['lfemur']['length'] + self.bones['ltibia']['length']


def read_motion(path):
    frames = []
    offset = 0.0
    rate = 120.0
    current = None
    for raw in open(path, encoding='latin-1'):
        line = raw.strip()
        if line.startswith('# offset'):
            offset = float(line.split()[2])
            continue
        if line.startswith('# rate'):
            rate = float(line.split()[2])
            continue
        if not line or line.startswith('#') or line.startswith(':'):
            continue
        words = line.split()
        if len(words) == 1 and words[0].isdigit():
            current = {}
            frames.append(current)
        elif current is not None:
            current[words[0]] = [float(v) for v in words[1:]]
    return frames, offset, rate


def forward_kinematics(skeleton, frames):
    n = len(frames)
    root = np.array([frame['root'] for frame in frames])
    rot = {'root': euler(*np.radians(root[:, 3:6]).T)}
    end = {'root': root[:, 0:3] * UNIT}
    for name in skeleton.order:
        bone = skeleton.bones[name]
        parent = skeleton.parents[name]
        angles = [np.zeros(n), np.zeros(n), np.zeros(n)]
        if bone['dof']:
            values = np.array([frame.get(name, [0.0] * len(bone['dof'])) for frame in frames])
            for k, dof in enumerate(bone['dof']):
                angles[AXES[dof[1]]] = np.radians(values[:, k])
        rot[name] = rot[parent] @ (bone['C'] @ euler(*angles) @ bone['C'].T)
        end[name] = end[parent] + bone['length'] * (rot[name] @ bone['direction'])
    return rot, end


class Source:
    def __init__(self, trial, start=None, end=None, mirror=False, smooth=1.5, folder=None):
        folder = folder or DATA
        subject = trial.split('_')[0]
        self.skeleton = Skeleton(os.path.join(folder, f'{subject}.asf'))
        frames, offset, rate = read_motion(os.path.join(folder, f'{trial}.amc'))
        rot, end_points = forward_kinematics(self.skeleton, frames)
        rot = {name: TO_BLENDER @ r @ TO_BLENDER.T for name, r in rot.items()}
        points = {name: p @ TO_BLENDER.T for name, p in end_points.items()}
        if mirror:
            swap = lambda name: ('r' + name[1:]) if name.startswith('l') and name not in ('lowerback', 'lowerneck') else ('l' + name[1:]) if name.startswith('r') and name != 'root' else name
            rot = {swap(name): MIRROR @ r @ MIRROR for name, r in rot.items()}
            points = {swap(name): p @ MIRROR for name, p in points.items()}
        sigma = smooth * rate / 120.0
        rot = {name: smooth_rotations(r, sigma) for name, r in rot.items()}
        points = {name: gaussian(p, sigma) for name, p in points.items()}
        first = 0.0 if start is None else (start - offset) * rate
        last = len(frames) - 1 if end is None else (end - offset) * rate
        times = np.arange(max(first, 0.0), min(last, len(frames) - 1) + 1e-6, rate / FPS)
        i0 = np.clip(np.floor(times).astype(int), 0, len(frames) - 1)
        i1 = np.clip(i0 + 1, 0, len(frames) - 1)
        f = times - i0
        self.rotations = {}
        for name, r in rot.items():
            q = continuous(to_quat(r))
            self.rotations[name] = to_matrix(q[i0] * (1.0 - f)[:, None] + q[i1] * f[:, None])
        self.points = {name: p[i0] * (1.0 - f)[:, None] + p[i1] * f[:, None] for name, p in points.items()}
        self.count = len(times)


class Rig:
    def __init__(self, armature):
        self.object = armature
        self.bones = {}
        for bone in armature.data.bones:
            rest = np.array(bone.matrix_local)
            self.bones[bone.name] = {'rest': rest, 'rot': rest[:3, :3].copy(), 'head': np.array(bone.head_local), 'tail': np.array(bone.tail_local), 'parent': bone.parent.name if bone.parent else None}
        depth = {}
        names = [bone.name for bone in armature.data.bones]
        for name in names:
            parent = self.bones[name]['parent']
            chain = 0
            while parent:
                chain += 1
                parent = self.bones[parent]['parent']
            depth[name] = chain
        self.order = sorted(names, key=lambda name: depth[name])

    def direction(self, name):
        b = self.bones[name]
        d = b['tail'] - b['head']
        return d / np.linalg.norm(d)

    def leg(self):
        hip, knee, ankle = (self.bones[name]['head'] for name in ('upperleg.l', 'lowerleg.l', 'foot.l'))
        return np.linalg.norm(knee - hip) + np.linalg.norm(ankle - knee)

    def local(self, name):
        parent = self.bones[name]['parent']
        return self.bones[name]['rot'] if parent is None else self.bones[parent]['rot'].T @ self.bones[name]['rot']

    def offset(self, name):
        parent = self.bones[name]['parent']
        return self.bones[parent]['rot'].T @ (self.bones[name]['head'] - self.bones[parent]['head'])


class Clip:
    def __init__(self, rotations, hips, ankles, drift=None):
        self.rotations = rotations
        self.hips = hips
        self.ankles = ankles
        self.drift = drift

    @property
    def count(self):
        return len(self.hips)

    def copy(self):
        return Clip({k: v.copy() for k, v in self.rotations.items()}, self.hips.copy(), {k: v.copy() for k, v in self.ankles.items()}, None if self.drift is None else self.drift.copy())

    def frames(self, index):
        index = np.asarray(index)
        return Clip({k: v[index] for k, v in self.rotations.items()}, self.hips[index], {k: v[index] for k, v in self.ankles.items()})

    def concat(self, other):
        return Clip({k: np.concatenate([v, other.rotations[k]]) for k, v in self.rotations.items()}, np.concatenate([self.hips, other.hips]), {k: np.concatenate([v, other.ankles[k]]) for k, v in self.ankles.items()})


def transfer(rig, source):
    scale = rig.leg() / source.skeleton.leg()
    rotations = {}
    for target, (name, correct) in SOURCE.items():
        rest = rig.bones[target]['rot']
        if correct:
            fix = arc(rig.direction(target), source.skeleton.rest_direction(name))
            rotations[target] = source.rotations[name] @ (fix @ rest)
        else:
            rotations[target] = source.rotations[name] @ rest
    joints = (source.points['lhipjoint'] + source.points['rhipjoint']) * 0.5 * scale
    rest_joints = (rig.bones['upperleg.l']['head'] + rig.bones['upperleg.r']['head']) * 0.5
    above = rig.bones['hips']['rot'].T @ (rig.bones['hips']['head'] - rest_joints)
    hips = joints + rotations['hips'] @ above
    ankles = {side: source.points[f'{side}tibia'] * scale for side in 'lr'}
    return Clip(rotations, hips, ankles)


def turn(clip, angle):
    r = yaw(angle)
    clip.rotations = {k: r @ v for k, v in clip.rotations.items()}
    clip.hips = clip.hips @ r.T
    clip.ankles = {k: v @ r.T for k, v in clip.ankles.items()}
    return clip


def heading(rotation):
    f = rotation @ FORWARD
    return np.arctan2(f[..., 1], f[..., 0])


def face_hips(rig, clip, offset=0.0):
    angles = heading(clip.rotations['hips'] @ rig.bones['hips']['rot'].T)
    mean = math.atan2(np.mean(np.sin(angles)), np.mean(np.cos(angles)))
    return turn(clip, math.atan2(FORWARD[1], FORWARD[0]) - mean + math.radians(offset))


def face_direction(clip, direction, offset=0.0):
    angle = math.atan2(direction[1], direction[0])
    return turn(clip, math.atan2(FORWARD[1], FORWARD[0]) - angle + math.radians(offset))


def shift(clip, delta):
    clip.hips = clip.hips - delta
    clip.ankles = {k: v - delta for k, v in clip.ankles.items()}
    return clip


def in_place(clip, mode):
    n = clip.count
    drift = np.zeros((n, 3))
    if mode == 'linear':
        t = np.arange(n)
        for k in (0, 1):
            slope, intercept = np.polyfit(t, clip.hips[:, k], 1)
            drift[:, k] = slope * t + intercept
    elif mode == 'center':
        drift[:, 0:2] = clip.hips[:, 0:2].mean(axis=0)
    return shift(clip, drift)


def ground(rig, clip, height=None):
    low = min(clip.ankles['l'][:, 2].min(), clip.ankles['r'][:, 2].min()) if height is None else height
    return shift(clip, np.array([0.0, 0.0, low - rig.bones['foot.l']['head'][2]]))


def loop(clip, cycle, search=None, blend=6, spread=6, joints=('upperleg.l', 'upperleg.r', 'lowerleg.l', 'lowerleg.r', 'upperarm.l', 'upperarm.r', 'hips')):
    n = clip.count
    search = search or (n - cycle - blend)
    best, choice = 1e9, (0, cycle)
    for length in range(max(cycle - spread, 8), min(cycle + spread + 1, n - blend)):
        for start in range(0, min(search, n - length - blend)):
            cost = 0.0
            for name in joints:
                q0, q1 = to_quat(clip.rotations[name][start]), to_quat(clip.rotations[name][start + length])
                cost += 1.0 - abs(float(np.dot(q0, q1)))
            cost += 4.0 * abs(clip.hips[start, 2] - clip.hips[start + length, 2])
            if cost < best:
                best, choice = cost, (start, length)
    start, length = choice
    step = clip.hips[start + length] - clip.hips[start]
    step[2] = 0.0
    window = clip.frames(np.arange(start, start + length + blend))
    drift = np.outer(np.arange(length + blend) / length, step)
    shift(window, drift)
    out = window.frames(np.arange(length))
    tail = window.frames(np.arange(length, length + blend))
    weight = smoothstep(0.0, 1.0, (np.arange(blend) + 0.5) / blend)
    for name in out.rotations:
        out.rotations[name][:blend] = slerp(tail.rotations[name], out.rotations[name][:blend], weight)
    out.hips[:blend] = tail.hips + (out.hips[:blend] - tail.hips) * weight[:, None]
    for side in 'lr':
        out.ankles[side][:blend] = tail.ankles[side] + (out.ankles[side][:blend] - tail.ankles[side]) * weight[:, None]
    out.drift = np.outer(np.arange(length) / length, step)
    return out, best, float(np.linalg.norm(step)) * FPS / length


def closed(clip):
    out = clip.concat(clip.frames([0]))
    if clip.drift is not None:
        out.drift = np.vstack([clip.drift, clip.drift[-1:] + (clip.drift[-1:] - clip.drift[-2:-1])])
    return out


def start_at(clip, index):
    drift = clip.drift
    shifted = clip.frames(np.r_[index:clip.count, 0:index])
    if drift is not None:
        shifted.drift = drift
    return shifted


def contact(clip, side):
    height = clip.ankles[side][:, 2]
    ahead = -clip.ankles[side][:, 1]
    score = height - 0.05 * ahead
    return int(np.argmin(score))


def reverse(clip):
    drift = clip.drift
    reversed_clip = clip.frames(np.arange(clip.count)[::-1])
    if drift is not None:
        reversed_clip.drift = -drift
    return reversed_clip


def retime(clip, keys):
    keys = np.asarray(keys, dtype=np.float64)
    duration = keys[-1, 1]
    count = int(round(duration * FPS)) + 1
    out_times = np.linspace(0.0, duration, count)
    source_times = np.interp(out_times, keys[:, 1], keys[:, 0]) * FPS
    i0 = np.clip(np.floor(source_times).astype(int), 0, clip.count - 1)
    i1 = np.clip(i0 + 1, 0, clip.count - 1)
    f = source_times - i0
    rotations = {k: slerp(v[i0], v[i1], f) for k, v in clip.rotations.items()}
    hips = clip.hips[i0] * (1.0 - f)[:, None] + clip.hips[i1] * f[:, None]
    ankles = {k: v[i0] * (1.0 - f)[:, None] + v[i1] * f[:, None] for k, v in clip.ankles.items()}
    return Clip(rotations, hips, ankles)


def blend_pose(clip, pose, weight):
    weight = np.asarray(weight, dtype=np.float64)
    for name in clip.rotations:
        clip.rotations[name] = slerp(np.repeat(pose.rotations[name][:1], clip.count, 0), clip.rotations[name], weight)
    clip.hips = pose.hips[:1] + (clip.hips - pose.hips[:1]) * weight[:, None]
    for side in 'lr':
        clip.ankles[side] = pose.ankles[side][:1] + (clip.ankles[side] - pose.ankles[side][:1]) * weight[:, None]
    return clip


def ends(count, fade_in, fade_out):
    t = np.arange(count) / FPS
    total = (count - 1) / FPS
    w = np.ones(count)
    if fade_in > 0.0:
        w *= smoothstep(0.0, fade_in, t)
    if fade_out > 0.0:
        w *= smoothstep(total, total - fade_out, t)
    return w


def add_rotation(rig, clip, bone, rotation, weight=1.0, space='world'):
    weight = np.broadcast_to(np.asarray(weight, dtype=np.float64), (clip.count,))
    target = rotation @ clip.rotations[bone] if space == 'world' else clip.rotations[bone] @ rotation
    clip.rotations[bone] = slerp(clip.rotations[bone], target, weight)
    return clip


def globals_of(rig, clip):
    n = clip.count
    world = {}
    for name in rig.order:
        b = rig.bones[name]
        parent = b['parent']
        if name in clip.rotations:
            rotation = clip.rotations[name]
        elif parent is None:
            rotation = np.repeat(b['rot'][None], n, 0)
        else:
            rotation = world[parent][0] @ rig.local(name)
        if parent is None:
            position = np.repeat(b['head'][None], n, 0)
        elif name == 'hips':
            position = clip.hips
        else:
            position = world[parent][1] + world[parent][0] @ rig.offset(name)
        world[name] = (rotation, position)
    return world


def aim_wand(rig, clip, direction, weight, side='r'):
    world = globals_of(rig, clip)
    slot = rig.bones[f'hand.{side}']['rot'].T @ rig.bones[f'handslot.{side}']['rot']
    hand = clip.rotations[f'hand.{side}']
    current = (hand @ slot)[:, :, 1]
    if direction is None:
        target = world[f'lowerarm.{side}'][0][:, :, 1]
    else:
        target = np.broadcast_to(np.asarray(direction, dtype=np.float64), current.shape)
    fix = arc(current, target)
    clip.rotations[f'hand.{side}'] = slerp(hand, fix @ hand, np.broadcast_to(np.asarray(weight, dtype=np.float64), (clip.count,)))
    return clip


def plant(rig, clip):
    world = globals_of(rig, clip)
    for side in 'lr':
        thigh = clip.rotations[f'upperleg.{side}'].copy()
        shin = clip.rotations[f'lowerleg.{side}'].copy()
        hip = world[f'upperleg.{side}'][1]
        knee = world[f'lowerleg.{side}'][1]
        ankle = world[f'foot.{side}'][1]
        upper = np.linalg.norm(knee - hip, axis=1)
        lower = np.linalg.norm(ankle - knee, axis=1)
        for i in range(clip.count):
            reach = clip.ankles[side][i] - hip[i]
            distance = float(np.clip(np.linalg.norm(reach), 1e-5, (upper[i] + lower[i]) * 0.9995))
            axis = reach / max(np.linalg.norm(reach), 1e-9)
            line = (ankle[i] - hip[i]) / max(np.linalg.norm(ankle[i] - hip[i]), 1e-9)
            bend = knee[i] - hip[i]
            bend = bend - line * np.dot(bend, line)
            ahead = thigh[i] @ (rig.bones[f'upperleg.{side}']['rot'].T @ FORWARD)
            hint = ahead if np.linalg.norm(bend) < 0.03 else bend / np.linalg.norm(bend) + 0.25 * ahead
            pole = hint - axis * np.dot(hint, axis)
            if np.linalg.norm(pole) < 1e-6:
                pole = ahead - axis * np.dot(ahead, axis)
            pole /= max(np.linalg.norm(pole), 1e-9)
            along = (upper[i] ** 2 - lower[i] ** 2 + distance ** 2) / (2.0 * distance)
            height = math.sqrt(max(upper[i] ** 2 - along ** 2, 0.0))
            new_knee = hip[i] + axis * along + pole * height
            new_ankle = hip[i] + axis * distance
            fix_thigh = arc(knee[i] - hip[i], new_knee - hip[i])
            fix_shin = arc(fix_thigh @ (ankle[i] - knee[i]), new_ankle - new_knee)
            thigh[i] = fix_thigh @ thigh[i]
            shin[i] = fix_shin @ fix_thigh @ shin[i]
        clip.rotations[f'upperleg.{side}'] = thigh
        clip.rotations[f'lowerleg.{side}'] = shin
    return clip


def write_action(rig, name, clip):
    action = bpy.data.actions.get(name)
    if action:
        bpy.data.actions.remove(action)
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    n = clip.count
    world = {}
    for bone in rig.order:
        parent = rig.bones[bone]['parent']
        if bone in clip.rotations:
            world[bone] = clip.rotations[bone]
        elif parent is None:
            world[bone] = np.repeat(rig.bones[bone]['rot'][None], n, 0)
        else:
            world[bone] = world[parent] @ rig.local(bone)
    frames = np.arange(n, dtype=np.float64)

    def curve(path, index, values, group):
        fc = action.fcurves.new(path, index=index, action_group=group)
        fc.keyframe_points.add(n)
        fc.keyframe_points.foreach_set('co', np.stack([frames, values], 1).ravel())
        fc.keyframe_points.foreach_set('interpolation', np.ones(n, dtype=np.int32))
        fc.update()

    for bone in rig.order:
        parent = rig.bones[bone]['parent']
        pose_local = world[bone] if parent is None else np.transpose(world[parent], (0, 2, 1)) @ world[bone]
        q = continuous(to_quat(rig.local(bone).T @ pose_local))
        for k in range(4):
            curve(f'pose.bones["{bone}"].rotation_quaternion', k, q[:, k], bone)
        if parent is None and clip.drift is not None:
            local = rig.bones[bone]['rot'].T @ clip.drift.T
            for k in range(3):
                curve(f'pose.bones["{bone}"].location', k, local[k], bone)
        if bone == 'hips':
            parent_rest = rig.bones[parent]['rest']
            local_rest = np.linalg.inv(parent_rest) @ rig.bones[bone]['rest']
            target = np.repeat(np.eye(4)[None], n, 0)
            target[:, :3, :3] = world[bone]
            target[:, :3, 3] = clip.hips
            basis = np.linalg.inv(local_rest) @ np.linalg.inv(parent_rest) @ target
            for k in range(3):
                curve(f'pose.bones["{bone}"].location', k, basis[:, k, 3], bone)
    return action


def descendants(rig, bone):
    found = [bone]
    for name in rig.order:
        if rig.bones[name]['parent'] in found and name not in found:
            found.append(name)
    return [name for name in found if name in SOURCE]


def rotate_chain(rig, clip, bone, rotation, weight=1.0):
    weight = np.broadcast_to(np.asarray(weight, dtype=np.float64), (clip.count,))
    turn_by = slerp(np.repeat(np.eye(3)[None], clip.count, 0), np.repeat(np.asarray(rotation)[None], clip.count, 0), weight)
    for name in descendants(rig, bone):
        clip.rotations[name] = turn_by @ clip.rotations[name]
    return clip


def source(rig, trial, start, end, mirror=False, smooth=1.5):
    return transfer(rig, Source(trial, start, end, mirror, smooth))


def strike_direction(rig, clip, index, side='r'):
    world = globals_of(rig, clip)
    direction = world[f'hand.{side}'][1][index] - world['chest'][1][index]
    direction[2] = 0.0
    return direction


def face_start(rig, clip, frames, offset=0.0):
    angles = heading(clip.rotations['hips'][:frames] @ rig.bones['hips']['rot'].T)
    mean = math.atan2(np.mean(np.sin(angles)), np.mean(np.cos(angles)))
    return turn(clip, math.atan2(FORWARD[1], FORWARD[0]) - mean + math.radians(offset))


def cycle(rig, trial, frames, start=None, end=None, mirror=False):
    clip = source(rig, trial, start, end, mirror)
    face_hips(rig, clip)
    clip, cost, speed = loop(clip, frames)
    clip = start_at(clip, contact(clip, 'l'))
    in_place(clip, 'center')
    ground(rig, clip)
    aim_wand(rig, clip, None, 0.7)
    return closed(plant(rig, clip))


def still(rig, trial, start, end, frames, spread, blend):
    clip = source(rig, trial, start, end)
    face_hips(rig, clip)
    joints = ('hips', 'spine', 'chest', 'neck', 'head', 'upperarm.l', 'upperarm.r', 'lowerarm.l', 'lowerarm.r', 'upperleg.l', 'upperleg.r')
    clip, cost, speed = loop(clip, frames, blend=blend, spread=spread, joints=joints)
    clip.drift = None
    in_place(clip, 'center')
    pin_feet(clip, clip)
    ground(rig, clip)
    level_gaze(rig, clip)
    aim_wand(rig, clip, REST_WAND, 1.0)
    return closed(plant(rig, settle(rig, clip)))


def align_feet(clip, base):
    offset = (clip.ankles['l'][0] + clip.ankles['r'][0] - base.ankles['l'][0] - base.ankles['r'][0]) * 0.5
    offset[2] = 0.0
    return shift(clip, offset)


def settle(rig, clip):
    world = globals_of(rig, clip)
    drop = np.zeros(clip.count)
    for side in 'lr':
        hip = world[f'upperleg.{side}'][1]
        length = np.linalg.norm(world[f'lowerleg.{side}'][1][0] - hip[0]) + np.linalg.norm(world[f'foot.{side}'][1][0] - world[f'lowerleg.{side}'][1][0])
        delta = hip - clip.ankles[side]
        flat = np.sqrt(np.maximum((0.998 * length) ** 2 - delta[:, 0] ** 2 - delta[:, 1] ** 2, 0.0))
        drop = np.maximum(drop, delta[:, 2] - flat)
    padded = np.concatenate([np.repeat(drop[:1], 3), drop, np.repeat(drop[-1:], 3)])
    drop = np.max(np.stack([padded[k:k + clip.count] for k in range(7)]), axis=0)
    clip.hips[:, 2] -= gaussian(np.maximum(drop, 0.0)[:, None], 2.0)[:, 0]
    return clip


def pin_feet(clip, pose):
    for side in 'lr':
        clip.ankles[side] = np.repeat(pose.ankles[side].mean(axis=0)[None], clip.count, 0)
        for bone in (f'foot.{side}', f'toes.{side}'):
            clip.rotations[bone] = np.repeat(to_matrix(continuous(to_quat(pose.rotations[bone])).mean(axis=0))[None], clip.count, 0)
    return clip


def one_shot(rig, base, trial, start, end, fade=0.5, mirror=False, wand=0.0):
    clip = source(rig, trial, start, end, mirror)
    face_hips(rig, clip)
    align_feet(clip, base)
    ground(rig, clip)
    blend_pose(clip, base.frames([0]), ends(clip.count, fade, fade))
    if wand > 0.0:
        aim_wand(rig, clip, REST_WAND, wand)
    return plant(rig, settle(rig, pin_feet(clip, base.frames([0]))))


def cast(rig, base, trial, at, before, after, mirror=False, face='strike', offset=0.0, speed=1.0, fade=(0.15, 0.3), aim=(0.15, 0.2), side='r', calm=0.0):
    clip = source(rig, trial, at - before, at + after, mirror)
    index = int(round(before * FPS))
    if face == 'strike':
        face_direction(clip, strike_direction(rig, clip, index, side), offset)
    elif face == 'chest':
        ahead = clip.rotations['chest'][index] @ (rig.bones['chest']['rot'].T @ FORWARD)
        face_direction(clip, np.array([ahead[0], ahead[1], 0.0]), offset)
    else:
        face_start(rig, clip, index, offset)
    align_feet(clip, base)
    ground(rig, clip)
    if speed != 1.0:
        duration = (clip.count - 1) / FPS
        clip = retime(clip, [(0.0, 0.0), (duration, duration / speed)])
        index = int(round(index / speed))
    t = np.arange(clip.count) / FPS
    strike = index / FPS
    if calm > 0.0:
        other = 'l' if side == 'r' else 'r'
        for name in (f'upperarm.{other}', f'lowerarm.{other}', f'wrist.{other}', f'hand.{other}'):
            clip.rotations[name] = slerp(clip.rotations[name], np.repeat(base.rotations[name][:1], clip.count, 0), calm)
    blend_pose(clip, base.frames([0]), ends(clip.count, *fade))
    if aim:
        weight = smoothstep(strike - aim[0], strike, t) * smoothstep(strike + aim[1] + 0.25, strike + aim[1], t)
        aim_wand(rig, clip, None, weight, side)
    return plant(rig, clip)


def backpedal(rig, forward_clip, lean=10.0):
    clip = reverse(forward_clip.copy())
    rotate_chain(rig, clip, 'spine', rotate_about([1.0, 0.0, 0.0], math.radians(-lean)))
    return plant(rig, clip)


def collapse(rig, base, trial, start, end, fade=0.15):
    clip = source(rig, trial, start, end)
    face_start(rig, clip, int(0.3 * FPS))
    align_feet(clip, base)
    ground(rig, clip)
    blend_pose(clip, base.frames([0]), ends(clip.count, fade, 0.0))
    return plant(rig, clip)


def track(keys, times, size):
    keys = sorted(keys, key=lambda key: key[0])
    stamps = np.array([key[0] for key in keys])
    values = np.array([np.atleast_1d(np.asarray(key[1], dtype=np.float64)) for key in keys])
    slopes = np.zeros_like(values)
    for k in range(1, len(keys) - 1):
        slopes[k] = (values[k + 1] - values[k - 1]) / (stamps[k + 1] - stamps[k - 1])
    out = np.zeros((len(times), size))
    for i, t in enumerate(times):
        if t <= stamps[0] or len(keys) == 1:
            out[i] = values[0]
        elif t >= stamps[-1]:
            out[i] = values[-1]
        else:
            k = int(np.searchsorted(stamps, t, side='right') - 1)
            span = stamps[k + 1] - stamps[k]
            u = (t - stamps[k]) / span
            out[i] = (2 * u ** 3 - 3 * u ** 2 + 1) * values[k] + (u ** 3 - 2 * u ** 2 + u) * span * slopes[k] + (-2 * u ** 3 + 3 * u ** 2) * values[k + 1] + (u ** 3 - u ** 2) * span * slopes[k + 1]
    return out


def body_rotation(angles):
    pitch, turn_, roll = (np.radians(angles[:, k]) for k in range(3))
    return axis_rotation(2, turn_) @ axis_rotation(0, pitch) @ axis_rotation(1, roll)


class Author:
    def __init__(self, rig, base, duration, live=False, relative=False):
        self.rig = rig
        self.base = base
        self.live = live
        self.relative = relative
        self.times = np.arange(int(round(duration * FPS)) + 1) / FPS
        self.keys = {}

    def key(self, channel, time, value):
        self.keys.setdefault(channel, []).append((time, value))
        return self

    def hips(self, time, move=(0.0, 0.0, 0.0), angles=(0.0, 0.0, 0.0)):
        return self.key('hips.move', time, move).key('hips.turn', time, angles)

    def bend(self, bone, time, angles):
        return self.key(f'{bone}.turn', time, angles)

    def reach(self, side, time, target, pole=(0.0, 0.3, -1.0), weight=1.0):
        return self.key(f'reach.{side}', time, target).key(f'pole.{side}', time, pole).key(f'weight.{side}', time, weight)

    def step(self, side, time, move):
        return self.key(f'step.{side}', time, move)

    def aim(self, time, weight):
        return self.key('aim', time, weight)

    def flick(self, time, degrees):
        return self.key('flick', time, degrees)

    def palm(self, side, time, direction, weight=1.0):
        return self.key(f'palm.{side}', time, direction).key(f'palmweight.{side}', time, weight)

    def curve(self, channel, size, default=0.0):
        if channel not in self.keys:
            return np.full((len(self.times), size), default)
        return track(self.keys[channel], self.times, size)

    def build(self):
        n = len(self.times)
        clip = self.base.frames(np.arange(n) % max(self.base.count - 1, 1) if self.live else np.zeros(n, dtype=int))
        rig = self.rig
        move = self.curve('hips.move', 3)
        clip.hips = clip.hips + move
        turned = body_rotation(self.curve('hips.turn', 3))
        pivot = clip.hips.copy()
        for name in descendants(rig, 'hips'):
            clip.rotations[name] = turned @ clip.rotations[name]
        for bone in ('spine', 'chest', 'neck', 'head'):
            if f'{bone}.turn' in self.keys:
                rotation = body_rotation(self.curve(f'{bone}.turn', 3))
                for name in descendants(rig, bone):
                    clip.rotations[name] = rotation @ clip.rotations[name]
        for side in 'lr':
            clip.ankles[side] = clip.ankles[side] + self.curve(f'step.{side}', 3)
            if f'reach.{side}' in self.keys:
                targets = self.curve(f'reach.{side}', 3)
                if self.relative:
                    targets = targets + globals_of(rig, clip)[f'upperarm.{side}'][1]
                arm_ik(rig, clip, side, targets, self.curve(f'pole.{side}', 3), self.curve(f'weight.{side}', 1)[:, 0])
        if 'aim' in self.keys:
            aim_wand(rig, clip, None, np.clip(self.curve('aim', 1)[:, 0], 0.0, 1.0))
        if 'flick' in self.keys:
            flick_wand(rig, clip, self.curve('flick', 1)[:, 0])
        for side in 'lr':
            if f'palm.{side}' in self.keys:
                face_palm(rig, clip, side, self.curve(f'palm.{side}', 3), self.curve(f'palmweight.{side}', 1)[:, 0])
        return plant(rig, clip)


def arm_ik(rig, clip, side, targets, poles, weights):
    world = globals_of(rig, clip)
    chain = [f'upperarm.{side}', f'lowerarm.{side}', f'wrist.{side}', f'hand.{side}']
    shoulder = world[chain[0]][1]
    elbow = world[chain[1]][1]
    wrist = world[chain[2]][1]
    for i in range(clip.count):
        weight = float(np.clip(weights[i], 0.0, 1.0))
        if weight <= 0.0:
            continue
        upper = np.linalg.norm(elbow[i] - shoulder[i])
        lower = np.linalg.norm(wrist[i] - elbow[i])
        goal = wrist[i] + (targets[i] - wrist[i]) * weight
        reach = goal - shoulder[i]
        distance = float(np.clip(np.linalg.norm(reach), 1e-4, (upper + lower) * 0.999))
        axis = reach / max(np.linalg.norm(reach), 1e-9)
        hint = poles[i] / max(np.linalg.norm(poles[i]), 1e-9)
        pole = hint - axis * np.dot(hint, axis)
        if np.linalg.norm(pole) < 1e-6:
            pole = np.array([0.0, 0.0, -1.0]) - axis * axis[2]
        pole /= np.linalg.norm(pole)
        along = (upper ** 2 - lower ** 2 + distance ** 2) / (2.0 * distance)
        height = math.sqrt(max(upper ** 2 - along ** 2, 0.0))
        new_elbow = shoulder[i] + axis * along + pole * height
        new_wrist = shoulder[i] + axis * distance
        fix_upper = arc(elbow[i] - shoulder[i], new_elbow - shoulder[i])
        fix_lower = arc(fix_upper @ (wrist[i] - elbow[i]), new_wrist - new_elbow)
        clip.rotations[chain[0]][i] = fix_upper @ clip.rotations[chain[0]][i]
        for name in chain[1:]:
            clip.rotations[name][i] = fix_lower @ fix_upper @ clip.rotations[name][i]
    return clip


def flick_wand(rig, clip, degrees, side='r'):
    slot = rig.bones[f'hand.{side}']['rot'].T @ rig.bones[f'handslot.{side}']['rot']
    hand = clip.rotations[f'hand.{side}']
    frame = hand @ slot
    axis = np.cross(frame[:, :, 1], UP)
    length = np.linalg.norm(axis, axis=1)
    axis = np.where(length[:, None] > 0.2, axis / np.maximum(length, 1e-9)[:, None], frame[:, :, 0])
    turn = np.stack([rotate_about(a, math.radians(float(d))) for a, d in zip(axis, degrees)])
    clip.rotations[f'hand.{side}'] = turn @ hand
    return clip


def face_palm(rig, clip, side, directions, weights):
    normal = rig.bones[f'hand.{side}']['rot'].T @ np.array([0.0, 0.0, -1.0])
    hand = clip.rotations[f'hand.{side}']
    fix = arc(hand @ normal, directions / np.linalg.norm(directions, axis=1, keepdims=True))
    clip.rotations[f'hand.{side}'] = slerp(hand, fix @ hand, np.clip(weights, 0.0, 1.0))
    return clip


def level_gaze(rig, clip, chest=6.0):
    ahead = (clip.rotations['head'] @ rig.bones['head']['rot'].T) @ FORWARD
    pitch = np.degrees(np.arctan2(-ahead[:, 2], np.linalg.norm(ahead[:, :2], axis=1)))
    rotate_chain(rig, clip, 'chest', rotate_about([1.0, 0.0, 0.0], math.radians(-chest)))
    return rotate_chain(rig, clip, 'neck', rotate_about([1.0, 0.0, 0.0], math.radians(chest - float(np.mean(pitch)))))


def amplify_arms(rig, clip, gain, abduct):
    for side, sign in (('l', -1.0), ('r', 1.0)):
        upper = clip.rotations[f'upperarm.{side}']
        mean = continuous(to_quat(upper)).mean(axis=0)
        mean = to_matrix(mean / np.linalg.norm(mean))
        swung = slerp(np.repeat(mean[None], clip.count, 0), upper, np.full(clip.count, gain))
        turn = axis_rotation(1, np.full(clip.count, sign * math.radians(abduct))) @ swung @ np.transpose(upper, (0, 2, 1))
        for name in descendants(rig, f'upperarm.{side}'):
            clip.rotations[name] = turn @ clip.rotations[name]
    return clip


def lengthen(clip, scale):
    for side in 'lr':
        relative = clip.ankles[side] - clip.hips
        relative[:, 1] *= scale
        clip.ankles[side] = clip.hips + relative
    if clip.drift is not None:
        clip.drift = clip.drift * scale
    return clip


def stride_cycle(rig, trial, frames, gain, abduct, scale, lean):
    clip = source(rig, trial, None, None)
    face_hips(rig, clip)
    clip, cost, speed = loop(clip, frames)
    clip = start_at(clip, contact(clip, 'l'))
    in_place(clip, 'center')
    ground(rig, clip)
    rotate_chain(rig, clip, 'spine', rotate_about([1.0, 0.0, 0.0], math.radians(lean)))
    amplify_arms(rig, clip, gain, abduct)
    lengthen(clip, scale)
    settle(rig, clip)
    aim_wand(rig, clip, None, 0.7)
    return closed(plant(rig, clip))


REST_WAND = np.array([0.15, -0.75, -0.64]) / np.linalg.norm([0.15, -0.75, -0.64])


def joint(rig, clip, bone, frame=0):
    return globals_of(rig, clip.frames([frame]))[bone][1][0]


def nova(rig, base):
    a = Author(rig, base, 0.95)
    sr, sl = joint(rig, base, 'upperarm.r'), joint(rig, base, 'upperarm.l')
    a.hips(0.0).hips(0.13, (0.0, 0.02, -0.07), (9.0, 0.0, 0.0)).hips(0.23, (0.0, -0.01, 0.0), (-5.0, 0.0, 0.0)).hips(0.5, (0.0, 0.0, -0.015), (-2.0, 0.0, 0.0)).hips(0.95)
    a.bend('chest', 0.0, (0.0, 0.0, 0.0)).bend('chest', 0.13, (12.0, 0.0, 0.0)).bend('chest', 0.23, (-10.0, 0.0, 0.0)).bend('chest', 0.5, (-4.0, 0.0, 0.0)).bend('chest', 0.95, (0.0, 0.0, 0.0))
    a.bend('head', 0.0, (0.0, 0.0, 0.0)).bend('head', 0.13, (12.0, 0.0, 0.0)).bend('head', 0.23, (-6.0, 0.0, 0.0)).bend('head', 0.95, (0.0, 0.0, 0.0))
    for side, s, shoulder in (('r', -1.0, sr), ('l', 1.0, sl)):
        rise = 0.0 if side == 'r' else 0.06
        a.reach(side, 0.0, shoulder + (0.0, -0.1, -0.45), (s, 0.0, -1.0), 0.0)
        a.reach(side, 0.13, shoulder + (-s * 0.26, -0.28, -0.12 + rise), (s, 0.0, -1.0), 1.0)
        a.reach(side, 0.23, shoulder + (s * 0.52, -0.22, -0.28), (0.0, 1.0, -0.6), 1.0)
        a.reach(side, 0.5, shoulder + (s * 0.42, -0.2, -0.36), (0.0, 1.0, -0.6), 0.9)
        a.reach(side, 0.95, shoulder + (s * 0.2, -0.1, -0.45), (0.0, 1.0, -0.6), 0.0)
    a.aim(0.0, 0.0).aim(0.2, 1.0).aim(0.55, 1.0).aim(0.95, 0.0)
    return a.build()


def blink(rig, base):
    a = Author(rig, base, 0.6)
    sr = joint(rig, base, 'upperarm.r')
    a.hips(0.0).hips(0.08, (0.0, 0.03, -0.05), (10.0, 18.0, 0.0)).hips(0.16, (0.0, -0.02, -0.02), (4.0, -8.0, 0.0)).hips(0.6)
    a.bend('chest', 0.0, (0.0, 0.0, 0.0)).bend('chest', 0.08, (8.0, 14.0, 0.0)).bend('chest', 0.16, (2.0, -10.0, 0.0)).bend('chest', 0.6, (0.0, 0.0, 0.0))
    a.reach('r', 0.0, sr + (0.0, -0.1, -0.45), (-1.0, 0.0, -1.0), 0.0)
    a.reach('r', 0.08, sr + (0.32, -0.25, 0.05), (-1.0, 0.0, -1.0), 1.0)
    a.reach('r', 0.17, sr + (-0.25, -0.42, -0.05), (-0.5, 1.0, -0.5), 1.0)
    a.reach('r', 0.6, sr + (-0.1, -0.1, -0.45), (-0.5, 1.0, -0.5), 0.0)
    a.aim(0.0, 0.0).aim(0.12, 1.0).aim(0.3, 1.0).aim(0.6, 0.0)
    return a.build()


def dodge(rig, base, direction):
    a = Author(rig, base, 0.55)
    sr, sl = joint(rig, base, 'upperarm.r'), joint(rig, base, 'upperarm.l')
    rest = (0.0, -0.1, -0.45)
    if direction == 'forward':
        a.hips(0.0).hips(0.05, (0.0, 0.03, -0.06), (16.0, 0.0, 0.0)).hips(0.13, (0.0, -0.08, -0.06), (32.0, -6.0, 0.0)).hips(0.22, (0.0, -0.12, -0.12), (22.0, -4.0, 0.0)).hips(0.36, (0.0, -0.05, -0.05), (8.0, 0.0, 0.0)).hips(0.55)
        a.bend('chest', 0.0, (0.0, 0.0, 0.0)).bend('chest', 0.13, (10.0, 0.0, 0.0)).bend('chest', 0.3, (4.0, 0.0, 0.0)).bend('chest', 0.55, (0.0, 0.0, 0.0))
        a.step('l', 0.0, (0.0, 0.0, 0.0)).step('l', 0.06, (0.0, -0.04, 0.06)).step('l', 0.14, (0.0, -0.34, 0.14)).step('l', 0.22, (0.0, -0.3, 0.0)).step('l', 0.4, (0.0, -0.12, 0.0)).step('l', 0.55, (0.0, 0.0, 0.0))
        a.step('r', 0.0, (0.0, 0.0, 0.0)).step('r', 0.08, (0.0, 0.12, 0.02)).step('r', 0.16, (0.0, 0.42, 0.12)).step('r', 0.26, (0.0, 0.3, 0.06)).step('r', 0.4, (0.0, 0.06, 0.0)).step('r', 0.55, (0.0, 0.0, 0.0))
        arms = {'r': ((-0.05, -0.32, -0.25), (0.0, 0.1, -0.45)), 'l': ((0.12, 0.25, -0.4), (0.05, -0.15, -0.42))}
    elif direction == 'back':
        a.hips(0.0).hips(0.05, (0.0, -0.02, -0.07), (6.0, 0.0, 0.0)).hips(0.14, (0.0, 0.05, 0.03), (-10.0, 0.0, 0.0)).hips(0.24, (0.0, 0.06, -0.11), (10.0, 0.0, 0.0)).hips(0.38, (0.0, 0.02, -0.04), (4.0, 0.0, 0.0)).hips(0.55)
        for side, lead in (('l', 0.0), ('r', 0.03)):
            a.step(side, 0.0, (0.0, 0.0, 0.0)).step(side, 0.07 + lead, (0.0, -0.02, 0.05)).step(side, 0.15 + lead, (0.0, -0.12, 0.15)).step(side, 0.24 + lead, (0.0, -0.04, 0.0)).step(side, 0.55, (0.0, 0.0, 0.0))
        arms = {'r': ((-0.1, -0.35, -0.2), (-0.05, -0.2, -0.38)), 'l': ((0.1, -0.35, -0.15), (0.08, -0.2, -0.38))}
    else:
        s = 1.0 if direction == 'left' else -1.0
        lead, trail = ('l', 'r') if s > 0.0 else ('r', 'l')
        a.hips(0.0).hips(0.05, (s * -0.02, 0.0, -0.06), (4.0, 0.0, s * -6.0)).hips(0.14, (s * 0.06, 0.0, -0.02), (6.0, s * 8.0, s * 16.0)).hips(0.24, (s * 0.1, 0.0, -0.1), (8.0, s * 4.0, s * 8.0)).hips(0.38, (s * 0.04, 0.0, -0.04), (3.0, 0.0, s * 3.0)).hips(0.55)
        a.step(lead, 0.0, (0.0, 0.0, 0.0)).step(lead, 0.06, (s * 0.05, 0.0, 0.07)).step(lead, 0.14, (s * 0.36, -0.04, 0.12)).step(lead, 0.22, (s * 0.32, -0.02, 0.0)).step(lead, 0.4, (s * 0.1, 0.0, 0.0)).step(lead, 0.55, (0.0, 0.0, 0.0))
        a.step(trail, 0.0, (0.0, 0.0, 0.0)).step(trail, 0.09, (s * 0.06, 0.02, 0.04)).step(trail, 0.17, (s * 0.12, 0.04, 0.18)).step(trail, 0.28, (s * 0.18, 0.02, 0.0)).step(trail, 0.55, (0.0, 0.0, 0.0))
        arms = {lead: ((s * 0.35, -0.05, -0.15), (s * 0.12, -0.1, -0.42)), trail: ((s * 0.15, -0.3, -0.05), (s * 0.08, -0.15, -0.42))}
    for side, shoulder, sign in (('r', sr, -1.0), ('l', sl, 1.0)):
        fly, land = arms[side]
        a.reach(side, 0.0, shoulder + rest, (sign, 0.4, -1.0), 0.0)
        a.reach(side, 0.15, shoulder + fly, (sign, 0.4, -1.0), 0.85)
        a.reach(side, 0.26, shoulder + land, (sign, 0.4, -1.0), 0.6)
        a.reach(side, 0.55, shoulder + rest, (sign, 0.4, -1.0), 0.0)
    a.bend('head', 0.0, (0.0, 0.0, 0.0)).bend('head', 0.14, (-8.0 if direction == 'forward' else 0.0, 0.0, 0.0)).bend('head', 0.55, (0.0, 0.0, 0.0))
    return a.build()


def flinch(rig, base):
    a = Author(rig, base, 0.5)
    sr, sl = joint(rig, base, 'upperarm.r'), joint(rig, base, 'upperarm.l')
    a.hips(0.0).hips(0.06, (0.0, 0.03, -0.02), (-5.0, 0.0, 0.0)).hips(0.2, (0.0, 0.01, -0.03), (4.0, 0.0, 0.0)).hips(0.5)
    a.bend('chest', 0.0, (0.0, 0.0, 0.0)).bend('chest', 0.06, (-14.0, 6.0, 3.0)).bend('chest', 0.2, (6.0, 2.0, 0.0)).bend('chest', 0.5, (0.0, 0.0, 0.0))
    a.bend('head', 0.0, (0.0, 0.0, 0.0)).bend('head', 0.05, (-14.0, -10.0, 0.0)).bend('head', 0.2, (8.0, -4.0, 0.0)).bend('head', 0.5, (0.0, 0.0, 0.0))
    for name, shoulder, s in (('r', sr, -1.0), ('l', sl, 1.0)):
        a.reach(name, 0.0, shoulder + (0.0, -0.1, -0.45), (s, 0.0, -1.0), 0.0)
        a.reach(name, 0.07, shoulder + (-s * 0.06, -0.24, -0.28), (s, 0.0, -1.0), 0.65)
        a.reach(name, 0.5, shoulder + (0.0, -0.1, -0.45), (s, 0.0, -1.0), 0.0)
    return a.build()


def stagger(rig, base):
    a = Author(rig, base, 0.9)
    sr, sl = joint(rig, base, 'upperarm.r'), joint(rig, base, 'upperarm.l')
    a.hips(0.0).hips(0.08, (0.0, 0.12, -0.04), (-16.0, 0.0, -4.0)).hips(0.3, (0.0, 0.2, -0.12), (8.0, 4.0, 0.0)).hips(0.9, (0.0, 0.18, -0.1), (12.0, 3.0, 0.0))
    a.bend('chest', 0.0, (0.0, 0.0, 0.0)).bend('chest', 0.08, (-18.0, 8.0, 0.0)).bend('chest', 0.3, (14.0, 2.0, 0.0)).bend('chest', 0.9, (16.0, 0.0, 0.0))
    a.bend('head', 0.0, (0.0, 0.0, 0.0)).bend('head', 0.06, (-22.0, 6.0, 0.0)).bend('head', 0.3, (14.0, 0.0, 0.0)).bend('head', 0.9, (18.0, 0.0, 0.0))
    a.step('l', 0.0, (0.0, 0.0, 0.0)).step('l', 0.12, (0.0, 0.18, 0.12)).step('l', 0.24, (0.0, 0.3, 0.0)).step('l', 0.9, (0.0, 0.3, 0.0))
    a.step('r', 0.0, (0.0, 0.0, 0.0)).step('r', 0.3, (0.0, 0.05, 0.0)).step('r', 0.9, (0.0, 0.08, 0.0))
    for name, shoulder, s in (('r', sr, -1.0), ('l', sl, 1.0)):
        a.reach(name, 0.0, shoulder + (0.0, -0.1, -0.45), (s, 0.0, -1.0), 0.0)
        a.reach(name, 0.1, shoulder + (s * 0.25, -0.38, 0.02), (s, 0.5, -1.0), 0.9)
        a.reach(name, 0.32, shoulder + (s * 0.12, -0.3, -0.38), (s, 0.5, -1.0), 0.8)
        a.reach(name, 0.9, shoulder + (s * 0.08, -0.25, -0.44), (s, 0.5, -1.0), 0.6)
    return a.build()


SEGMENTS = {
    '77_02': (0.0, 8.0),
    '40_10': (4.0, 22.2),
    '16_46': (None, None),
    '09_06': (None, None),
    '07_01': (None, None),
    '143_20': (3.5, 4.9),
    '17_10': (17.4, 18.6),
    '90_18': (0.8, 3.3),
}


def clips(rig):
    base = still(rig, '77_02', 0.5, 7.5, 90, 12, 18)
    jog = cycle(rig, '09_06', 20)
    return {
        'Idle_A': base,
        'Hero_Idle_Look': glance(rig, base, 'left'),
        'Hero_Idle_LookRight': glance(rig, base, 'right'),
        'Hero_Idle_Scan': scan(rig, base),
        'Hero_Idle_Wand': flourish(rig, base),
        'Hero_Idle_Bracer': one_shot(rig, base, '40_10', 4.2, 6.8, wand=0.6),
        'Hero_Idle_Scratch': one_shot(rig, base, '40_10', 18.9, 21.9, mirror=True, wand=0.8),
        'Walking_A': cycle(rig, '07_01', 31),
        'Hero_Jog': jog,
        'Running_A': stride_cycle(rig, '16_46', 21, 2.0, 12.0, 1.3, 7.0),
        'Walking_Backwards': backpedal(rig, jog),
        'Hero_Cast_Flick': flick_cast(rig, base),
        'Hero_Cast_Slash': slash_cast(rig, base),
        'Hero_Cast_Backhand': backhand_cast(rig, base),
        'Hero_Cast_Lunge': lunge_cast(rig, base),
        'Hero_Cast_Hurl': cast(rig, base, '143_20', 4.16, 0.45, 0.6, mirror=True),
        'Hero_Cast_Surge': surge_cast(rig, base),
        'Hero_Cast_Lash': lash_cast(rig, base),
        'Hero_Cast_Grasp': grasp_cast(rig, base),
        'Hero_Cast_Lob': lob_cast(rig, base),
        'Hero_Cast_Smite': smite_cast(rig, base),
        'Hero_Cast_Summon': summon_cast(rig, base),
        'Hero_Cast_Call': cast(rig, base, '17_10', 17.99, 0.28, 0.45, face='chest', aim=None),
        'Hero_Cast_Push': shove_cast(rig, base),
        'Hero_Cast_Ward': guard_cast(rig, base),
        'Hero_Cast_Nova': nova(rig, base),
        'Hero_Cast_Blink': blink(rig, base),
        'Dodge_Forward': dodge(rig, base, 'forward'),
        'Dodge_Backward': dodge(rig, base, 'back'),
        'Dodge_Left': dodge(rig, base, 'left'),
        'Dodge_Right': dodge(rig, base, 'right'),
        'Hit_A': flinch(rig, base),
        'Hit_B': stagger(rig, base),
        'Death_A': collapse(rig, base, '90_18', 0.9, 3.3),
    }


def build_actions(armature):
    bpy.context.scene.render.fps = int(FPS)
    bpy.context.scene.render.fps_base = 1.0
    rig = Rig(armature)
    for bone in armature.pose.bones:
        bone.rotation_mode = 'QUATERNION'
    data = armature.animation_data_create()
    actions = []
    for name, clip in clips(rig).items():
        action = write_action(rig, name, clip)
        data.action = action
        data.action = None
        track = data.nla_tracks.new()
        track.name = name
        track.strips.new(name, 0, action)
        track.mute = True
        actions.append(action)
    for bone in armature.pose.bones:
        bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        bone.location = (0.0, 0.0, 0.0)
    return actions


REST_ARM = (0.0, -0.1, -0.45)


def keys(author, channel, frames):
    for time, value in frames:
        getattr(author, channel)(time, value)
    return author


def limb(author, side, frames):
    for time, offset, pole, weight in frames:
        author.reach(side, time, offset, pole, weight)
    return author


def bends(author, bone, frames):
    for time, angles in frames:
        author.bend(bone, time, angles)
    return author


def body(author, frames):
    for time, move, angles in frames:
        author.hips(time, move, angles)
    return author


def steps(author, side, frames):
    for time, move in frames:
        author.step(side, time, move)
    return author


def glance(rig, base, side):
    s = 1.0 if side == 'left' else -1.0
    a = Author(rig, base, 3.0, live=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.7, (s * 0.015, 0.0, -0.005), (0.0, s * 6.0, -s * 1.5)), (2.1, (s * 0.02, 0.0, -0.005), (0.0, s * 7.0, -s * 1.5)), (2.7, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (3.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.8, (-2.0, s * 16.0, 0.0)), (2.0, (-1.0, s * 18.0, 0.0)), (2.6, (0.0, 0.0, 0.0)), (3.0, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.35, (1.0, -s * 4.0, 0.0)), (0.75, (-3.0, s * 38.0, s * 4.0)), (1.3, (-2.0, s * 34.0, s * 3.0)), (1.6, (3.0, s * 41.0, s * 4.0)), (2.05, (0.0, s * 36.0, s * 3.0)), (2.5, (0.0, s * 2.0, 0.0)), (3.0, (0.0, 0.0, 0.0))])
    return a.build()


def scan(rig, base):
    a = Author(rig, base, 4.4, live=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.8, (0.015, 0.0, 0.0), (0.0, 5.0, -1.0)), (1.9, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (2.6, (-0.015, 0.0, 0.0), (0.0, -6.0, 1.0)), (3.5, (-0.012, 0.0, 0.0), (0.0, -5.0, 1.0)), (4.1, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (4.4, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.8, (-2.0, 14.0, 0.0)), (1.9, (0.0, 0.0, 0.0)), (2.6, (-2.0, -16.0, 0.0)), (3.5, (-1.0, -14.0, 0.0)), (4.1, (0.0, 0.0, 0.0)), (4.4, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.3, (0.0, -3.0, 0.0)), (0.8, (-4.0, 36.0, 3.0)), (1.5, (-2.0, 32.0, 2.0)), (1.9, (2.0, 0.0, 0.0)), (2.6, (-4.0, -40.0, -3.0)), (3.3, (0.0, -36.0, -2.0)), (4.0, (0.0, -2.0, 0.0)), (4.4, (0.0, 0.0, 0.0))])
    return a.build()


def flourish(rig, base):
    a = Author(rig, base, 3.2, live=True, relative=True)
    pole = (-1.0, 0.3, -1.0)
    limb(a, 'r', [(0.0, REST_ARM, pole, 0.0), (0.6, (0.12, -0.32, -0.22), pole, 1.0), (0.9, (0.16, -0.36, -0.18), pole, 1.0), (1.15, (0.12, -0.38, -0.12), pole, 1.0), (1.4, (0.08, -0.36, -0.18), pole, 1.0), (1.65, (0.12, -0.34, -0.24), pole, 1.0), (1.9, (0.16, -0.36, -0.18), pole, 1.0), (2.4, (0.1, -0.3, -0.26), pole, 1.0), (3.2, REST_ARM, pole, 0.0)])
    keys(a, 'aim', [(0.0, 0.0), (0.5, 1.0), (2.5, 1.0), (3.2, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.6, 40.0), (1.15, 30.0), (1.65, 40.0), (2.4, 20.0), (3.2, 0.0)])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.6, (4.0, 4.0, 0.0)), (2.4, (3.0, 3.0, 0.0)), (3.2, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.6, (16.0, -10.0, 0.0)), (1.4, (12.0, -6.0, 2.0)), (2.4, (12.0, -8.0, 0.0)), (3.0, (0.0, 0.0, 0.0)), (3.2, (0.0, 0.0, 0.0))])
    return a.build()


def flick_cast(rig, base):
    a = Author(rig, base, 0.95, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.19, (0.0, 0.03, -0.02), (-3.0, -10.0, 0.0)), (0.24, (0.0, 0.035, -0.025), (-3.5, -12.0, 0.0)), (0.31, (0.0, -0.05, -0.03), (6.0, 8.0, 0.0)), (0.4, (0.0, -0.04, -0.03), (5.0, 6.0, 0.0)), (0.95, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.19, (-4.0, -12.0, 0.0)), (0.24, (-5.0, -14.0, 0.0)), (0.31, (6.0, 12.0, 0.0)), (0.4, (4.0, 8.0, 0.0)), (0.95, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.19, (2.0, 20.0, 0.0)), (0.24, (2.0, 24.0, 0.0)), (0.31, (-4.0, -18.0, 0.0)), (0.4, (-3.0, -12.0, 0.0)), (0.95, (0.0, 0.0, 0.0))])
    limb(a, 'r', [(0.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0), (0.19, (-0.12, 0.06, 0.12), (-1.0, 0.2, -0.8), 1.0), (0.24, (-0.14, 0.08, 0.14), (-1.0, 0.2, -0.8), 1.0), (0.31, (0.1, -0.56, 0.02), (-0.6, 0.3, -1.0), 1.0), (0.39, (0.08, -0.54, -0.04), (-0.6, 0.3, -1.0), 1.0), (0.66, (0.02, -0.32, -0.26), (-0.8, 0.4, -1.0), 0.6), (0.95, REST_ARM, (-1.0, 0.3, -1.0), 0.0)])
    limb(a, 'l', [(0.0, REST_ARM, (1.0, 0.3, -1.0), 0.0), (0.2, (-0.02, -0.22, -0.2), (1.0, 0.3, -1.0), 0.8), (0.31, (0.06, 0.1, -0.38), (1.0, 0.3, -1.0), 0.8), (0.95, REST_ARM, (1.0, 0.3, -1.0), 0.0)])
    keys(a, 'aim', [(0.0, 0.0), (0.16, 1.0), (0.64, 1.0), (0.95, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.19, 45.0), (0.24, 50.0), (0.29, 10.0), (0.32, -14.0), (0.39, -4.0), (0.64, 0.0), (0.95, 0.0)])
    steps(a, 'l', [(0.0, (0.0, 0.0, 0.0)), (0.24, (0.0, -0.03, 0.05)), (0.31, (0.0, -0.1, 0.0)), (0.74, (0.0, -0.1, 0.0)), (0.84, (0.0, -0.05, 0.04)), (0.95, (0.0, 0.0, 0.0))])
    return a.build()


def lunge_cast(rig, base):
    a = Author(rig, base, 1.0, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.18, (0.0, 0.04, -0.05), (-4.0, -16.0, 0.0)), (0.3, (0.0, -0.16, -0.08), (12.0, 10.0, 0.0)), (0.42, (0.0, -0.15, -0.08), (10.0, 8.0, 0.0)), (0.75, (0.0, -0.06, -0.03), (3.0, 2.0, 0.0)), (1.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.18, (-2.0, -10.0, 0.0)), (0.3, (6.0, 10.0, 0.0)), (0.42, (5.0, 8.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.18, (4.0, 24.0, 0.0)), (0.3, (-12.0, -18.0, 0.0)), (0.42, (-10.0, -14.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
    limb(a, 'r', [(0.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0), (0.18, (-0.06, 0.18, -0.32), (-1.0, 0.6, -0.4), 1.0), (0.3, (0.08, -0.58, -0.02), (-0.6, 0.3, -1.0), 1.0), (0.42, (0.08, -0.56, -0.04), (-0.6, 0.3, -1.0), 1.0), (0.75, (0.02, -0.3, -0.28), (-0.8, 0.4, -1.0), 0.6), (1.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0)])
    limb(a, 'l', [(0.0, REST_ARM, (1.0, 0.3, -1.0), 0.0), (0.18, (-0.04, -0.3, -0.15), (1.0, 0.3, -1.0), 0.9), (0.3, (0.08, 0.22, -0.35), (1.0, 0.3, -1.0), 0.9), (0.42, (0.08, 0.2, -0.36), (1.0, 0.3, -1.0), 0.9), (1.0, REST_ARM, (1.0, 0.3, -1.0), 0.0)])
    keys(a, 'aim', [(0.0, 0.0), (0.15, 1.0), (0.7, 1.0), (1.0, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.18, 30.0), (0.28, -8.0), (0.36, -3.0), (0.7, 0.0), (1.0, 0.0)])
    steps(a, 'l', [(0.0, (0.0, 0.0, 0.0)), (0.2, (0.0, -0.1, 0.07)), (0.3, (0.0, -0.32, 0.0)), (0.75, (0.0, -0.32, 0.0)), (0.86, (0.0, -0.16, 0.05)), (1.0, (0.0, 0.0, 0.0))])
    return a.build()


def slash_cast(rig, base):
    a = Author(rig, base, 0.95, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.18, (0.0, 0.02, -0.02), (-2.0, -14.0, 0.0)), (0.29, (0.0, -0.05, -0.04), (8.0, 12.0, 0.0)), (0.38, (0.0, -0.05, -0.04), (7.0, 14.0, 0.0)), (0.95, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.18, (-4.0, -16.0, 6.0)), (0.29, (8.0, 16.0, -4.0)), (0.38, (7.0, 18.0, -4.0)), (0.95, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.18, (2.0, 26.0, -4.0)), (0.29, (-6.0, -22.0, 2.0)), (0.38, (-5.0, -24.0, 2.0)), (0.95, (0.0, 0.0, 0.0))])
    limb(a, 'r', [(0.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0), (0.18, (-0.22, -0.06, 0.3), (-1.0, 0.5, 0.2), 1.0), (0.29, (0.38, -0.4, -0.32), (-0.5, 0.5, -1.0), 1.0), (0.38, (0.44, -0.28, -0.38), (-0.5, 0.5, -1.0), 1.0), (0.65, (0.2, -0.25, -0.38), (-0.8, 0.4, -1.0), 0.6), (0.95, REST_ARM, (-1.0, 0.3, -1.0), 0.0)])
    limb(a, 'l', [(0.0, REST_ARM, (1.0, 0.3, -1.0), 0.0), (0.18, (0.02, -0.2, -0.25), (1.0, 0.3, -1.0), 0.7), (0.29, (0.1, 0.08, -0.36), (1.0, 0.3, -1.0), 0.7), (0.95, REST_ARM, (1.0, 0.3, -1.0), 0.0)])
    keys(a, 'aim', [(0.0, 0.0), (0.15, 1.0), (0.6, 1.0), (0.95, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.18, 40.0), (0.27, 0.0), (0.3, -15.0), (0.4, -8.0), (0.7, 0.0), (0.95, 0.0)])
    steps(a, 'l', [(0.0, (0.0, 0.0, 0.0)), (0.2, (0.0, -0.03, 0.04)), (0.29, (0.0, -0.08, 0.0)), (0.7, (0.0, -0.08, 0.0)), (0.82, (0.0, -0.04, 0.03)), (0.95, (0.0, 0.0, 0.0))])
    return a.build()


def backhand_cast(rig, base):
    a = Author(rig, base, 0.95, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.19, (0.0, 0.02, -0.02), (-2.0, 14.0, 0.0)), (0.24, (0.0, 0.025, -0.025), (-2.5, 16.0, 0.0)), (0.31, (0.0, -0.04, -0.03), (6.0, -10.0, 0.0)), (0.4, (0.0, -0.04, -0.03), (5.0, -12.0, 0.0)), (0.95, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.19, (-2.0, 14.0, 0.0)), (0.24, (-2.5, 16.0, 0.0)), (0.31, (5.0, -14.0, 0.0)), (0.4, (4.0, -16.0, 0.0)), (0.95, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.19, (2.0, -26.0, 0.0)), (0.24, (2.0, -30.0, 0.0)), (0.31, (-4.0, 22.0, 0.0)), (0.4, (-3.0, 26.0, 0.0)), (0.95, (0.0, 0.0, 0.0))])
    limb(a, 'r', [(0.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0), (0.19, (0.32, -0.16, 0.04), (-0.2, 1.0, -0.6), 1.0), (0.24, (0.34, -0.14, 0.06), (-0.2, 1.0, -0.6), 1.0), (0.31, (-0.12, -0.54, 0.02), (-1.0, 0.4, -0.6), 1.0), (0.4, (-0.22, -0.46, -0.04), (-1.0, 0.4, -0.6), 1.0), (0.66, (-0.1, -0.3, -0.28), (-1.0, 0.4, -1.0), 0.6), (0.95, REST_ARM, (-1.0, 0.3, -1.0), 0.0)])
    limb(a, 'l', [(0.0, REST_ARM, (1.0, 0.3, -1.0), 0.0), (0.2, (0.04, 0.02, -0.38), (1.0, 0.3, -1.0), 0.6), (0.31, (0.0, -0.18, -0.3), (1.0, 0.3, -1.0), 0.6), (0.95, REST_ARM, (1.0, 0.3, -1.0), 0.0)])
    keys(a, 'aim', [(0.0, 0.0), (0.16, 1.0), (0.64, 1.0), (0.95, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.19, 25.0), (0.24, 30.0), (0.29, 0.0), (0.32, -14.0), (0.4, -6.0), (0.64, 0.0), (0.95, 0.0)])
    return a.build()


def surge_cast(rig, base):
    sr, sl = joint(rig, base, 'upperarm.r'), joint(rig, base, 'upperarm.l')
    a = Author(rig, base, 1.1, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.22, (0.0, 0.04, -0.06), (4.0, -22.0, 0.0)), (0.36, (0.0, -0.14, -0.07), (10.0, 14.0, 0.0)), (0.5, (0.0, -0.11, -0.06), (8.0, 10.0, 0.0)), (0.8, (0.0, -0.05, -0.03), (3.0, 4.0, 0.0)), (1.1, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.22, (2.0, -10.0, 0.0)), (0.36, (6.0, 8.0, 0.0)), (0.5, (5.0, 6.0, 0.0)), (1.1, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.22, (0.0, 30.0, 0.0)), (0.36, (-8.0, -20.0, 0.0)), (0.5, (-6.0, -16.0, 0.0)), (1.1, (0.0, 0.0, 0.0))])
    limb(a, 'r', [(0.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0), (0.22, (0.04, -0.14, -0.34), (-1.0, 0.2, -0.8), 1.0), (0.36, (0.14, -0.56, -0.02), (-0.7, 0.3, -1.0), 1.0), (0.5, (0.12, -0.5, -0.06), (-0.7, 0.3, -1.0), 1.0), (0.8, (0.04, -0.3, -0.28), (-0.8, 0.4, -1.0), 0.5), (1.1, REST_ARM, (-1.0, 0.3, -1.0), 0.0)])
    limb(a, 'l', [(0.0, REST_ARM, (1.0, 0.3, -1.0), 0.0), (0.22, sr - sl + (0.18, -0.2, -0.3), (0.6, -0.3, -1.0), 1.0), (0.36, (-0.14, -0.56, 0.0), (0.7, 0.3, -1.0), 1.0), (0.5, (-0.12, -0.5, -0.04), (0.7, 0.3, -1.0), 1.0), (0.8, (-0.04, -0.3, -0.28), (0.8, 0.4, -1.0), 0.5), (1.1, REST_ARM, (1.0, 0.3, -1.0), 0.0)])
    keys(a, 'aim', [(0.0, 0.0), (0.2, 1.0), (0.8, 1.0), (1.1, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.22, 20.0), (0.36, -5.0), (0.8, 0.0), (1.1, 0.0)])
    a.palm('l', 0.0, (0.0, 0.0, -1.0), 0.0).palm('l', 0.22, (-0.8, -0.2, 0.2), 0.8).palm('l', 0.36, (0.0, -1.0, 0.0), 1.0).palm('l', 0.6, (0.0, -1.0, -0.2), 1.0).palm('l', 1.1, (0.0, 0.0, -1.0), 0.0)
    steps(a, 'l', [(0.0, (0.0, 0.0, 0.0)), (0.26, (0.0, -0.12, 0.07)), (0.36, (0.0, -0.28, 0.0)), (0.8, (0.0, -0.28, 0.0)), (0.92, (0.0, -0.14, 0.05)), (1.1, (0.0, 0.0, 0.0))])
    return a.build()


def lash_cast(rig, base):
    a = Author(rig, base, 1.0, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.2, (0.0, 0.04, 0.0), (-6.0, -10.0, 0.0)), (0.3, (0.0, -0.06, -0.03), (10.0, 8.0, 0.0)), (0.4, (0.0, 0.02, -0.02), (-4.0, -4.0, 0.0)), (0.55, (0.0, 0.02, -0.02), (-3.0, -3.0, 0.0)), (1.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.2, (-8.0, -8.0, 0.0)), (0.3, (10.0, 8.0, 0.0)), (0.4, (-6.0, -6.0, 0.0)), (0.55, (-4.0, -4.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.2, (4.0, 18.0, 0.0)), (0.3, (-12.0, -16.0, 0.0)), (0.4, (2.0, 10.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
    limb(a, 'r', [(0.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0), (0.2, (-0.08, 0.14, 0.34), (-1.0, 0.2, 0.4), 1.0), (0.3, (0.04, -0.56, -0.04), (-1.0, 0.2, -0.6), 1.0), (0.4, (-0.1, -0.18, -0.12), (-1.0, 0.6, -0.4), 1.0), (0.55, (-0.12, -0.1, -0.16), (-1.0, 0.6, -0.4), 1.0), (0.8, (-0.06, -0.08, -0.3), (-1.0, 0.5, -0.8), 0.6), (1.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0)])
    limb(a, 'l', [(0.0, REST_ARM, (1.0, 0.3, -1.0), 0.0), (0.2, (0.0, -0.24, -0.18), (1.0, 0.3, -1.0), 0.8), (0.3, (0.04, -0.1, -0.34), (1.0, 0.3, -1.0), 0.8), (0.4, (0.06, 0.05, -0.38), (1.0, 0.3, -1.0), 0.6), (1.0, REST_ARM, (1.0, 0.3, -1.0), 0.0)])
    keys(a, 'aim', [(0.0, 0.0), (0.16, 1.0), (0.7, 1.0), (1.0, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.2, 50.0), (0.28, 10.0), (0.31, -20.0), (0.4, 10.0), (0.55, 8.0), (0.8, 0.0), (1.0, 0.0)])
    steps(a, 'l', [(0.0, (0.0, 0.0, 0.0)), (0.24, (0.0, -0.05, 0.05)), (0.3, (0.0, -0.12, 0.0)), (0.8, (0.0, -0.12, 0.0)), (0.9, (0.0, -0.06, 0.04)), (1.0, (0.0, 0.0, 0.0))])
    return a.build()


def lob_cast(rig, base):
    a = Author(rig, base, 1.0, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.18, (0.0, 0.02, -0.05), (8.0, -8.0, 0.0)), (0.32, (0.0, -0.06, 0.0), (-2.0, 6.0, 0.0)), (0.44, (0.0, -0.06, 0.0), (-3.0, 8.0, 0.0)), (1.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.18, (6.0, -8.0, 0.0)), (0.32, (-4.0, 6.0, 0.0)), (0.44, (-6.0, 6.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.18, (-4.0, 16.0, 0.0)), (0.32, (-8.0, -12.0, 0.0)), (0.44, (-12.0, -14.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
    limb(a, 'r', [(0.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0), (0.18, (-0.04, 0.3, -0.42), (-1.0, 0.2, -0.6), 1.0), (0.32, (0.04, -0.44, 0.04), (-1.0, 0.0, -1.0), 1.0), (0.44, (0.02, -0.32, 0.24), (-1.0, 0.0, -1.0), 1.0), (0.7, (0.0, -0.25, -0.15), (-1.0, 0.3, -1.0), 0.6), (1.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0)])
    limb(a, 'l', [(0.0, REST_ARM, (1.0, 0.3, -1.0), 0.0), (0.18, (0.04, -0.2, -0.25), (1.0, 0.3, -1.0), 0.7), (0.32, (0.06, 0.1, -0.36), (1.0, 0.3, -1.0), 0.7), (1.0, REST_ARM, (1.0, 0.3, -1.0), 0.0)])
    keys(a, 'aim', [(0.0, 0.0), (0.15, 1.0), (0.7, 1.0), (1.0, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.18, -20.0), (0.32, 20.0), (0.44, 25.0), (0.7, 0.0), (1.0, 0.0)])
    steps(a, 'l', [(0.0, (0.0, 0.0, 0.0)), (0.22, (0.0, -0.06, 0.06)), (0.32, (0.0, -0.2, 0.0)), (0.75, (0.0, -0.2, 0.0)), (0.86, (0.0, -0.1, 0.05)), (1.0, (0.0, 0.0, 0.0))])
    return a.build()


def smite_cast(rig, base):
    a = Author(rig, base, 1.0, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.22, (0.0, 0.03, 0.01), (-6.0, -4.0, 0.0)), (0.26, (0.0, 0.035, 0.015), (-7.0, -4.0, 0.0)), (0.33, (0.0, -0.06, -0.05), (10.0, 6.0, 0.0)), (0.45, (0.0, -0.06, -0.05), (9.0, 5.0, 0.0)), (1.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.22, (-8.0, 0.0, 0.0)), (0.26, (-9.0, 0.0, 0.0)), (0.33, (8.0, 4.0, 0.0)), (0.45, (7.0, 3.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.22, (-18.0, 4.0, 0.0)), (0.26, (-20.0, 4.0, 0.0)), (0.33, (6.0, -8.0, 0.0)), (0.45, (6.0, -6.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
    limb(a, 'r', [(0.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0), (0.22, (0.04, -0.04, 0.54), (-1.0, 0.2, 0.0), 1.0), (0.26, (0.04, -0.02, 0.56), (-1.0, 0.2, 0.0), 1.0), (0.33, (0.06, -0.5, -0.22), (-1.0, 0.3, -0.6), 1.0), (0.45, (0.06, -0.48, -0.26), (-1.0, 0.3, -0.6), 1.0), (0.7, (0.02, -0.3, -0.32), (-1.0, 0.3, -1.0), 0.6), (1.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0)])
    limb(a, 'l', [(0.0, REST_ARM, (1.0, 0.3, -1.0), 0.0), (0.22, (0.02, -0.25, -0.1), (1.0, 0.3, -1.0), 0.8), (0.33, (0.06, 0.06, -0.36), (1.0, 0.3, -1.0), 0.8), (1.0, REST_ARM, (1.0, 0.3, -1.0), 0.0)])
    keys(a, 'aim', [(0.0, 0.0), (0.14, 1.0), (0.7, 1.0), (1.0, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.22, 0.0), (0.33, -15.0), (0.45, -10.0), (0.7, 0.0), (1.0, 0.0)])
    a.palm('l', 0.0, (0.0, 0.0, -1.0), 0.0).palm('l', 0.22, (0.0, -1.0, -0.3), 0.8).palm('l', 0.45, (0.0, -1.0, -0.3), 0.3).palm('l', 1.0, (0.0, 0.0, -1.0), 0.0)
    return a.build()


def summon_cast(rig, base):
    a = Author(rig, base, 1.15, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.26, (0.0, 0.04, 0.01), (-8.0, 0.0, 0.0)), (0.4, (0.0, -0.08, -0.08), (14.0, 0.0, 0.0)), (0.55, (0.0, -0.08, -0.08), (13.0, 0.0, 0.0)), (0.85, (0.0, -0.03, -0.03), (4.0, 0.0, 0.0)), (1.15, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.26, (-10.0, 0.0, 0.0)), (0.4, (10.0, 0.0, 0.0)), (0.55, (9.0, 0.0, 0.0)), (1.15, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.26, (-16.0, 0.0, 0.0)), (0.4, (6.0, 0.0, 0.0)), (1.15, (0.0, 0.0, 0.0))])
    for side, s in (('r', -1.0), ('l', 1.0)):
        limb(a, side, [(0.0, REST_ARM, (s, 0.3, -1.0), 0.0), (0.26, (-s * 0.06, -0.06, 0.52), (s, 0.2, 0.3), 1.0), (0.4, (-s * 0.1, -0.46, -0.3), (s, 0.3, -0.6), 1.0), (0.55, (-s * 0.1, -0.44, -0.32), (s, 0.3, -0.6), 1.0), (0.85, (-s * 0.04, -0.25, -0.38), (s, 0.3, -1.0), 0.5), (1.15, REST_ARM, (s, 0.3, -1.0), 0.0)])
    keys(a, 'aim', [(0.0, 0.0), (0.2, 1.0), (0.85, 1.0), (1.15, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.26, 0.0), (0.4, -10.0), (0.55, -6.0), (0.85, 0.0), (1.15, 0.0)])
    a.palm('l', 0.0, (0.0, 0.0, -1.0), 0.0).palm('l', 0.26, (0.0, -0.6, 0.6), 0.8).palm('l', 0.4, (0.0, -0.3, -1.0), 1.0).palm('l', 0.85, (0.0, -0.3, -1.0), 0.5).palm('l', 1.15, (0.0, 0.0, -1.0), 0.0)
    steps(a, 'l', [(0.0, (0.0, 0.0, 0.0)), (0.3, (0.0, -0.06, 0.06)), (0.4, (0.0, -0.18, 0.0)), (0.85, (0.0, -0.18, 0.0)), (0.97, (0.0, -0.09, 0.05)), (1.15, (0.0, 0.0, 0.0))])
    return a.build()


def grasp_cast(rig, base):
    a = Author(rig, base, 1.05, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.2, (0.0, -0.02, -0.02), (4.0, -10.0, 0.0)), (0.34, (0.0, -0.04, -0.03), (6.0, 12.0, 0.0)), (0.46, (0.0, -0.04, -0.03), (5.0, 10.0, 0.0)), (1.05, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.2, (2.0, -8.0, 0.0)), (0.34, (4.0, 10.0, 0.0)), (0.46, (3.0, 8.0, 0.0)), (1.05, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.2, (0.0, 18.0, 0.0)), (0.34, (-6.0, -20.0, 0.0)), (1.05, (0.0, 0.0, 0.0))])
    limb(a, 'l', [(0.0, REST_ARM, (1.0, 0.3, -1.0), 0.0), (0.2, (-0.12, -0.54, 0.02), (1.0, 0.3, -1.0), 1.0), (0.34, (-0.06, -0.16, -0.14), (1.0, 0.5, -0.6), 1.0), (0.46, (-0.06, -0.16, -0.16), (1.0, 0.5, -0.6), 1.0), (0.8, (-0.02, -0.12, -0.32), (1.0, 0.4, -1.0), 0.5), (1.05, REST_ARM, (1.0, 0.3, -1.0), 0.0)])
    limb(a, 'r', [(0.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0), (0.2, (-0.04, 0.1, -0.32), (-1.0, 0.6, -0.4), 1.0), (0.34, (0.1, -0.54, -0.04), (-0.6, 0.3, -1.0), 1.0), (0.46, (0.08, -0.52, -0.08), (-0.6, 0.3, -1.0), 1.0), (0.8, (0.02, -0.3, -0.28), (-0.8, 0.4, -1.0), 0.5), (1.05, REST_ARM, (-1.0, 0.3, -1.0), 0.0)])
    a.palm('l', 0.0, (0.0, 0.0, -1.0), 0.0).palm('l', 0.2, (0.0, -1.0, 0.0), 1.0).palm('l', 0.34, (0.3, 0.3, -1.0), 1.0).palm('l', 0.8, (0.0, 0.0, -1.0), 0.5).palm('l', 1.05, (0.0, 0.0, -1.0), 0.0)
    keys(a, 'aim', [(0.0, 0.0), (0.18, 1.0), (0.8, 1.0), (1.05, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.2, 20.0), (0.34, -10.0), (0.46, -5.0), (0.8, 0.0), (1.05, 0.0)])
    return a.build()


def shove_cast(rig, base):
    a = Author(rig, base, 1.0, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.18, (0.0, 0.05, -0.04), (-6.0, 0.0, 0.0)), (0.3, (0.0, -0.15, -0.08), (14.0, 0.0, 0.0)), (0.44, (0.0, -0.14, -0.08), (12.0, 0.0, 0.0)), (0.75, (0.0, -0.06, -0.03), (4.0, 0.0, 0.0)), (1.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.18, (-6.0, 0.0, 0.0)), (0.3, (8.0, 0.0, 0.0)), (0.44, (7.0, 0.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.18, (4.0, 0.0, 0.0)), (0.3, (-14.0, 0.0, 0.0)), (0.44, (-12.0, 0.0, 0.0)), (1.0, (0.0, 0.0, 0.0))])
    for side, s in (('r', -1.0), ('l', 1.0)):
        limb(a, side, [(0.0, REST_ARM, (s, 0.3, -1.0), 0.0), (0.18, (-s * 0.02, 0.04, -0.22), (s, 0.8, -0.6), 1.0), (0.3, (-s * 0.1, -0.56, -0.06), (s * 0.7, 0.3, -1.0), 1.0), (0.44, (-s * 0.09, -0.54, -0.08), (s * 0.7, 0.3, -1.0), 1.0), (0.75, (-s * 0.03, -0.3, -0.28), (s * 0.8, 0.4, -1.0), 0.5), (1.0, REST_ARM, (s, 0.3, -1.0), 0.0)])
    a.palm('l', 0.0, (0.0, 0.0, -1.0), 0.0).palm('l', 0.18, (0.0, -1.0, 0.0), 0.8).palm('l', 0.3, (0.0, -1.0, 0.0), 1.0).palm('l', 0.75, (0.0, -1.0, 0.0), 0.6).palm('l', 1.0, (0.0, 0.0, -1.0), 0.0)
    keys(a, 'aim', [(0.0, 0.0), (0.16, 1.0), (0.75, 1.0), (1.0, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.18, 30.0), (0.3, 0.0), (0.75, 0.0), (1.0, 0.0)])
    steps(a, 'l', [(0.0, (0.0, 0.0, 0.0)), (0.2, (0.0, -0.1, 0.07)), (0.3, (0.0, -0.3, 0.0)), (0.75, (0.0, -0.3, 0.0)), (0.88, (0.0, -0.15, 0.05)), (1.0, (0.0, 0.0, 0.0))])
    return a.build()


def guard_cast(rig, base):
    a = Author(rig, base, 1.15, relative=True)
    body(a, [(0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0.12, (0.0, 0.02, -0.03), (2.0, 10.0, 0.0)), (0.22, (0.0, -0.03, -0.07), (6.0, -4.0, 0.0)), (0.7, (0.0, -0.03, -0.07), (5.0, -3.0, 0.0)), (0.9, (0.0, -0.01, -0.03), (2.0, 0.0, 0.0)), (1.15, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))])
    bends(a, 'chest', [(0.0, (0.0, 0.0, 0.0)), (0.12, (2.0, 10.0, 0.0)), (0.22, (4.0, -6.0, 0.0)), (0.7, (3.0, -5.0, 0.0)), (1.15, (0.0, 0.0, 0.0))])
    bends(a, 'head', [(0.0, (0.0, 0.0, 0.0)), (0.12, (6.0, -8.0, 0.0)), (0.22, (-6.0, 6.0, 0.0)), (0.7, (-5.0, 5.0, 0.0)), (1.15, (0.0, 0.0, 0.0))])
    limb(a, 'r', [(0.0, REST_ARM, (-1.0, 0.3, -1.0), 0.0), (0.12, (0.26, -0.12, -0.38), (-1.0, 0.5, -0.6), 1.0), (0.22, (0.16, -0.36, 0.14), (-1.0, 0.2, -1.0), 1.0), (0.7, (0.15, -0.34, 0.12), (-1.0, 0.2, -1.0), 1.0), (0.9, (0.06, -0.24, -0.2), (-1.0, 0.3, -1.0), 0.6), (1.15, REST_ARM, (-1.0, 0.3, -1.0), 0.0)])
    limb(a, 'l', [(0.0, REST_ARM, (1.0, 0.3, -1.0), 0.0), (0.12, (0.04, -0.12, -0.36), (1.0, 0.3, -1.0), 0.6), (0.22, (-0.08, -0.46, 0.02), (1.0, 0.3, -1.0), 1.0), (0.7, (-0.07, -0.44, 0.0), (1.0, 0.3, -1.0), 1.0), (0.9, (-0.03, -0.26, -0.26), (1.0, 0.3, -1.0), 0.5), (1.15, REST_ARM, (1.0, 0.3, -1.0), 0.0)])
    a.palm('l', 0.0, (0.0, 0.0, -1.0), 0.0).palm('l', 0.12, (0.0, -0.5, -1.0), 0.4).palm('l', 0.22, (0.0, -1.0, 0.1), 1.0).palm('l', 0.7, (0.0, -1.0, 0.1), 1.0).palm('l', 0.95, (0.0, -0.3, -1.0), 0.4).palm('l', 1.15, (0.0, 0.0, -1.0), 0.0)
    keys(a, 'aim', [(0.0, 0.0), (0.1, 1.0), (0.8, 1.0), (1.15, 0.0)])
    keys(a, 'flick', [(0.0, 0.0), (0.12, -30.0), (0.22, 10.0), (0.7, 8.0), (0.9, 0.0), (1.15, 0.0)])
    steps(a, 'l', [(0.0, (0.0, 0.0, 0.0)), (0.14, (0.03, -0.02, 0.05)), (0.22, (0.08, -0.08, 0.0)), (0.8, (0.08, -0.08, 0.0)), (0.95, (0.04, -0.04, 0.04)), (1.15, (0.0, 0.0, 0.0))])
    steps(a, 'r', [(0.0, (0.0, 0.0, 0.0)), (0.16, (-0.02, 0.01, 0.04)), (0.24, (-0.05, 0.04, 0.0)), (0.82, (-0.05, 0.04, 0.0)), (0.98, (-0.02, 0.02, 0.03)), (1.15, (0.0, 0.0, 0.0))])
    return a.build()
