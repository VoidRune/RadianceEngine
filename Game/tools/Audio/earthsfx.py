import os
import sys

import aud
import numpy as np

Rate = 48000
Here = os.path.dirname(os.path.abspath(__file__))
Root = os.path.normpath(os.path.join(Here, "..", "..", "..", "bin", "windows-x86_64", "Game", "res", "Audio", "Sfx"))


def load(name):
    sound = aud.Sound(os.path.join(Root, name + ".ogg"))
    rate = sound.specs[0]
    data = sound.data()[:, 0].astype(np.float64)
    if rate != Rate:
        t = np.arange(0.0, len(data) - 1, rate / Rate)
        data = np.interp(t, np.arange(len(data)), data)
    return data / max(np.abs(data).max(), 1e-6)


def save(name, x):
    peak = np.abs(x).max()
    x = np.tanh(1.2 * x / peak) / np.tanh(1.2) * 0.92
    fade = min(len(x), int(0.02 * Rate))
    x[-fade:] *= np.linspace(1.0, 0.0, fade)
    path = os.path.join(Root, name + ".ogg")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    aud.Sound.buffer(x.astype(np.float32).reshape(-1, 1), Rate).write(path, rate=Rate, channels=aud.CHANNELS_MONO, format=aud.FORMAT_FLOAT32, container=aud.CONTAINER_OGG, codec=aud.CODEC_VORBIS, bitrate=160000)
    if Spectra:
        spectrogram(name, x)
    print(f"{name}: {len(x) / Rate:.2f} s")


def filt(x, lo=None, hi=None, order=2):
    n = len(x)
    size = 1 << int(np.ceil(np.log2(n + Rate // 10)))
    spectrum = np.fft.rfft(x, size)
    f = np.maximum(np.fft.rfftfreq(size, 1.0 / Rate), 1.0)
    h = np.ones_like(f)
    if lo:
        h /= np.sqrt(1.0 + (lo / f) ** (2 * order))
    if hi:
        h /= np.sqrt(1.0 + (f / hi) ** (2 * order))
    return np.fft.irfft(spectrum * h, size)[:n]


def pitch(x, factor):
    t = np.arange(0.0, len(x) - 1, factor)
    return np.interp(t, np.arange(len(x)), x)


def env(n, attack, decay):
    t = np.arange(n) / Rate
    return np.minimum(t / max(attack, 1e-4), 1.0) * np.exp(-t / decay)


def add(out, x, at, gain=1.0):
    start = int(at * Rate)
    if start >= len(out):
        return
    end = min(len(out), start + len(x))
    out[start:end] += gain * x[: end - start]


def noise(rng, n, pink=False):
    x = rng.standard_normal(n)
    if pink:
        spectrum = np.fft.rfft(x)
        spectrum /= np.sqrt(np.maximum(np.fft.rfftfreq(n, 1.0 / Rate), 20.0))
        x = np.fft.irfft(spectrum, n)
    return x / np.abs(x).max()


def grain(rng, source, factor, length, attack=0.001, decay=None):
    body = pitch(source, factor)
    n = min(len(body), int(length * Rate))
    start = rng.integers(0, max(1, min(len(body) - n, int(0.02 * Rate))) + 1)
    piece = body[start : start + n]
    return piece * env(len(piece), attack, decay or length * 0.35)


class Library:
    def __init__(self):
        self.rock = [load(f"Impact/Rock_{i}") for i in range(1, 6)]
        self.dirt = [load(f"Player/Step_Dirt_{i}") for i in range(1, 6)]
        self.stone = [load(f"Player/Step_Stone_{i}") for i in range(1, 6)]
        self.pebble = [load(f"Impact/Bone_{i}") for i in range(1, 8)]
        self.rumble = load("Impact/Rumble")
        self.boom = [load(f"Impact/Explosion_{i}") for i in range(1, 3)]

    def pick(self, rng, items):
        return items[rng.integers(len(items))]


def fracture(rng, n):
    out = np.zeros(n)
    for i in range(rng.integers(7, 11)):
        at = 0.0015 + rng.random() ** 1.6 * 0.07
        size = int(rng.uniform(0.004, 0.014) * Rate)
        snap = filt(noise(rng, size), lo=rng.uniform(1500, 3000), hi=rng.uniform(6000, 11000)) * env(size, 0.0002, rng.uniform(0.0015, 0.004))
        add(out, snap, at, rng.uniform(0.35, 1.0) * (1.0 if i == 0 else 0.6))
    return out


def grind(rng, lib, n, start, length, density, low, high, gain):
    out = np.zeros(n)
    t = start
    while t < start + length:
        u = (t - start) / length
        rate = density * (1.0 - 0.75 * u)
        t += rng.exponential(1.0 / rate)
        source = lib.pick(rng, lib.rock if rng.random() < 0.6 else lib.stone)
        g = grain(rng, source, rng.uniform(low, high), rng.uniform(0.012, 0.035), 0.0015)
        add(out, g, t, rng.uniform(0.3, 1.0) * (1.0 - 0.6 * u))
    size = int(length * Rate)
    scrape = filt(noise(rng, size), lo=180, hi=2400)
    jitter = filt(np.abs(noise(rng, size)), hi=45)
    jitter = np.clip(jitter / np.abs(jitter).max(), 0.0, 1.0)
    shape = np.minimum(np.linspace(0.0, 1.0, size) / 0.08, 1.0) * np.exp(-np.linspace(0.0, length, size) / (length * 0.45))
    add(out, scrape * jitter * shape, start, 0.35)
    return filt(out, lo=120, hi=5500) * gain


def debris(rng, lib, n, start, length, density, gain, lowpass=4500):
    out = np.zeros(n)
    t = start
    while t < start + length:
        u = (t - start) / length
        t += rng.exponential(1.0 / (density * (1.0 - u) ** 1.5 + 2.0))
        if rng.random() < 0.55:
            g = grain(rng, lib.pick(rng, lib.pebble), rng.uniform(0.75, 1.3), rng.uniform(0.02, 0.06), 0.0005)
        else:
            g = grain(rng, lib.pick(rng, lib.rock), rng.uniform(1.3, 2.1), rng.uniform(0.02, 0.05), 0.0005)
        add(out, g, t, rng.uniform(0.15, 1.0) * (1.0 - 0.7 * u))
    size = int(length * Rate)
    hiss = filt(noise(rng, size, pink=True), lo=500, hi=3500) * env(size, 0.05, length * 0.35)
    add(out, hiss, start, 0.18)
    return filt(out, lo=300, hi=lowpass) * gain


def thump(rng, lib, n, at, gain, length=0.6):
    size = int(length * Rate)
    t = np.arange(size) / Rate
    sweep = np.sin(2.0 * np.pi * np.cumsum(np.interp(t, [0.0, 0.08, length], [95.0, 55.0, 38.0])) / Rate)
    body = sweep * env(size, 0.002, 0.16)
    boom = np.zeros(size)
    source = lib.pick(rng, lib.boom)[:size]
    boom[: len(source)] = source
    boom = filt(boom, hi=260, order=3) * env(size, 0.003, 0.22)
    out = np.zeros(n)
    add(out, body * 0.8 + boom * 0.7, at, gain)
    return out


def crack(rng, lib, length=1.05):
    n = int(length * Rate)
    out = np.zeros(n)
    out += fracture(rng, n) * 2.6
    add(out, grain(rng, lib.pick(rng, lib.rock), rng.uniform(0.85, 1.0), 0.22, 0.0005, 0.07), 0.003, 0.95)
    add(out, grain(rng, lib.pick(rng, lib.rock), rng.uniform(1.15, 1.35), 0.15, 0.0005, 0.045), 0.012, 0.7)
    add(out, grain(rng, lib.pick(rng, lib.stone), rng.uniform(0.7, 0.85), 0.2, 0.001, 0.06), 0.02, 0.55)
    out += thump(rng, lib, n, 0.0, 0.42)
    out += grind(rng, lib, n, 0.012, 0.3, 240.0, 0.55, 0.95, 1.9)
    out += debris(rng, lib, n, 0.14, length - 0.16, 70.0, 1.1)
    return out


def rise(rng, lib, length=0.42):
    n = int(length * Rate)
    out = np.zeros(n)
    t = 0.0
    while t < length - 0.02:
        u = t / length
        t += rng.exponential(1.0 / (25.0 + 230.0 * u * u))
        source = lib.pick(rng, lib.rock if rng.random() < 0.5 else lib.dirt)
        g = grain(rng, source, rng.uniform(0.6, 0.85) + 0.7 * u, rng.uniform(0.015, 0.04), 0.002)
        add(out, g, t, (0.25 + 0.75 * u) * rng.uniform(0.4, 1.0))
    out = filt(out, lo=150, hi=4000 + 3000 * 1.0)
    rumble = filt(lib.rumble[int(0.3 * Rate) : int(0.3 * Rate) + n], hi=300, order=3)
    swell = np.linspace(0.0, 1.0, n) ** 1.5
    out += rumble / np.abs(rumble).max() * swell * 0.9
    grit = filt(noise(rng, n, pink=True), lo=400, hi=2500) * swell ** 2
    out += grit * 0.25
    return out


def crumble(rng, lib, length=1.15):
    n = int(length * Rate)
    out = np.zeros(n)
    add(out, grain(rng, lib.pick(rng, lib.rock), rng.uniform(0.7, 0.8), 0.25, 0.001, 0.08), 0.0, 0.8)
    out += fracture(rng, n) * 1.2
    out += grind(rng, lib, n, 0.0, 0.42, 170.0, 0.45, 0.75, 1.6)
    for i in range(rng.integers(4, 7)):
        at = 0.08 + rng.random() * 0.5
        add(out, grain(rng, lib.pick(rng, lib.rock), rng.uniform(0.75, 1.15), 0.12, 0.0008, 0.04), at, rng.uniform(0.35, 0.7))
    out += debris(rng, lib, n, 0.05, length - 0.07, 55.0, 1.2, 3800)
    out += thump(rng, lib, n, 0.0, 0.2, 0.4)
    return out


def skewer(rng, lib, length=0.5):
    n = int(length * Rate)
    out = np.zeros(n)
    out += fracture(rng, n) * 1.8
    add(out, grain(rng, lib.pick(rng, lib.rock), rng.uniform(1.05, 1.25), 0.14, 0.0004, 0.04), 0.0, 1.0)
    add(out, grain(rng, lib.pick(rng, lib.stone), rng.uniform(0.85, 1.0), 0.14, 0.0005, 0.05), 0.004, 0.6)
    out += thump(rng, lib, n, 0.0, 0.3, 0.3)
    out += debris(rng, lib, n, 0.05, 0.4, 60.0, 0.6)
    return out


def spectrogram(name, x):
    import bpy

    window = 1024
    hop = 256
    frames = max(1, (len(x) - window) // hop)
    rows = np.stack([np.abs(np.fft.rfft(x[i * hop : i * hop + window] * np.hanning(window))) for i in range(frames)], axis=1)
    db = 20.0 * np.log10(rows + 1e-6)
    db = np.clip((db - db.max() + 70.0) / 70.0, 0.0, 1.0)
    bins = db.shape[0]
    edges = np.geomspace(1, bins - 1, 256).astype(int)
    image = db[edges, :]
    h, w = image.shape
    pixels = np.ones((h, w, 4), dtype=np.float32)
    pixels[..., 0] = image
    pixels[..., 1] = image ** 2
    pixels[..., 2] = image ** 4
    img = bpy.data.images.new(name.replace("/", "_"), w, h)
    img.pixels = pixels.ravel()
    img.filepath_raw = os.path.join(Spectra, name.replace("/", "_") + ".png")
    img.file_format = "PNG"
    img.save()


Spectra = None


def main():
    global Spectra
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if "--spectra" in args:
        Spectra = args[args.index("--spectra") + 1]
        os.makedirs(Spectra, exist_ok=True)
    lib = Library()
    for i in range(3):
        save(f"Spell/EarthCrack_{i + 1}", crack(np.random.default_rng(101 + i), lib))
    save("Spell/EarthRise", rise(np.random.default_rng(211), lib))
    for i in range(2):
        save(f"Spell/EarthCrumble_{i + 1}", crumble(np.random.default_rng(307 + i), lib))
    for i in range(3):
        save(f"Spell/EarthSkewer_{i + 1}", skewer(np.random.default_rng(401 + i), lib))


main()
