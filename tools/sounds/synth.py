"""Original UI sounds for RecompStation, synthesized from scratch.

Two families share one key (E major) and one loudness scale so any sound
can sit next to any other:
  glass - soft sine/bell tones with a short airy tail (the layered blues and glow)
  pixel - rounded pulse waves, low-passed (a nod to the pixel d-pad mark)
"""
import json
import os
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt

SR = 48000
OUT = sys.argv[1] if len(sys.argv) > 1 else "out"
rng = np.random.default_rng(13)

# E major, equal temperament.
def note(name):
    names = {"C": -9, "C#": -8, "D": -7, "D#": -6, "E": -5, "F": -4, "F#": -3,
             "G": -2, "G#": -1, "A": 0, "A#": 1, "B": 2}
    pitch, octave = name[:-1], int(name[-1])
    return 440.0 * 2 ** ((names[pitch] + 12 * (octave - 4)) / 12)


def t_axis(seconds):
    return np.arange(int(SR * seconds)) / SR


def envelope(n, attack=0.002, decay=0.1):
    t = np.arange(n) / SR
    env = np.minimum(t / attack, 1.0) * np.exp(-np.maximum(t - attack, 0) / decay)
    return env


def fade_tail(x, ms=8):
    k = min(len(x), int(SR * ms / 1000))
    x[-k:] *= np.linspace(1, 0, k)
    return x


def lowpass(x, cutoff, order=2):
    return sosfilt(butter(order, cutoff, "low", fs=SR, output="sos"), x)


def bandpass(x, lo, hi):
    return sosfilt(butter(2, [lo, hi], "band", fs=SR, output="sos"), x)


def air(x, length=0.35, mix=0.18, bright=6000):
    """A small, soft room: decaying filtered noise used as an impulse response."""
    n = int(SR * length)
    ir = rng.standard_normal(n) * np.exp(-np.arange(n) / (SR * length / 5))
    ir = lowpass(ir, bright)
    ir /= np.sqrt(np.sum(ir ** 2))
    wet = np.convolve(x, ir)
    dry = np.concatenate([x, np.zeros(len(wet) - len(x))])
    return dry + mix * wet


# --- voices -----------------------------------------------------------------

def glass_tone(freq, dur, decay, level=1.0, partials=((1, 1.0), (2.0, 0.18), (3.01, 0.05))):
    t = t_axis(dur)
    x = sum(a * np.sin(2 * np.pi * freq * r * t) * np.exp(-t / (decay / r ** 0.6))
            for r, a in partials)
    return level * x * envelope(len(t), 0.0015, 10)  # partial decays shape it


def pixel_tone(freq, dur, decay, level=1.0, duty=0.25, cutoff=3200):
    t = t_axis(dur)
    ph = 2 * np.pi * freq * t
    x = band_limited_pulse(ph, freq, duty)
    # Round it further: a raw pulse is harsh; this keeps the shape but softens it.
    x = lowpass(x, cutoff, 2)
    return level * x * envelope(len(t), 0.002, decay)


def band_limited_pulse(ph, top_freq, duty=0.25, ceiling=7000):
    """Additive pulse wave with no harmonics above `ceiling`, so nothing aliases."""
    x = np.zeros_like(ph)
    n = 1
    while n * top_freq < ceiling:
        x += (2 / (n * np.pi)) * np.sin(n * np.pi * duty) * np.cos(n * ph)
        n += 1
    return x


def place(parts, total):
    out = np.zeros(int(SR * total))
    for start, sig in parts:
        i = int(SR * start)
        j = min(len(out), i + len(sig))
        out[i:j] += sig[: j - i]
    return out


def glide(f0, f1, dur, decay, voice):
    t = t_axis(dur)
    f = f0 * (f1 / f0) ** (t / dur)
    ph = 2 * np.pi * np.cumsum(f) / SR
    if voice == "glass":
        x = np.sin(ph) + 0.12 * np.sin(2 * ph)
    else:
        x = lowpass(band_limited_pulse(ph, max(f0, f1)), 2600)
    return x * envelope(len(t), 0.004, decay)


def swish(dur, f0, f1):
    """Filtered noise sweep: the shelf or system sliding over."""
    n = int(SR * dur)
    noise = rng.standard_normal(n)
    out = np.zeros(n)
    steps = 24
    edges = np.linspace(0, n, steps + 1).astype(int)
    for k in range(steps):
        fc = f0 * (f1 / f0) ** (k / (steps - 1))
        seg = bandpass(noise, fc * 0.7, min(fc * 1.4, SR / 2 - 100))
        out[edges[k]:edges[k + 1]] = seg[edges[k]:edges[k + 1]]
    t = np.arange(n) / n
    return out * np.sin(np.pi * t) ** 1.5


# --- the events ---------------------------------------------------------------

def build(voice):
    tone = glass_tone if voice == "glass" else pixel_tone
    room = (lambda x, **k: air(x, **k)) if voice == "glass" else (lambda x, **k: air(x, mix=k.get("mix", 0.18) * 0.6, length=k.get("length", 0.35)))
    s = {}

    # Moving focus between games: tiny, high, quiet. Heard dozens of times.
    if voice == "glass":
        s["move"] = room(tone(note("B6"), 0.09, 0.03, partials=((1, 1.0), (2.0, 0.08))), length=0.15, mix=0.10)
    else:
        s["move"] = room(tone(note("B5"), 0.06, 0.018, duty=0.5, cutoff=2600), length=0.12, mix=0.08)

    # Hitting the end of a row or list: softer, lower, falls slightly.
    s["edge"] = room(glide(note("E5"), note("B4"), 0.09, 0.03, voice) * 0.9, length=0.12, mix=0.08)

    # Selecting a game (opens Game Details): two notes rising a fifth.
    s["select"] = room(place([(0.0, tone(note("E6"), 0.35, 0.12)),
                              (0.055, tone(note("B6"), 0.45, 0.16))], 0.55), length=0.4, mix=0.2)

    # Back: the same interval, falling, a little shorter and softer.
    s["back"] = room(place([(0.0, 0.8 * tone(note("B5"), 0.25, 0.08)),
                            (0.05, 0.8 * tone(note("E5"), 0.3, 0.1))], 0.4), length=0.3, mix=0.15)

    # Switching shelf or system (LB/RB, Q/E): a short sweep ending on a tick.
    sweep = lowpass(swish(0.14, 1400, 3800), 6000) * (0.35 if voice == "glass" else 0.25)
    tick = tone(note("G#6"), 0.08, 0.025) * 0.6
    s["switch"] = room(place([(0.0, sweep), (0.11, tick)], 0.25), length=0.2, mix=0.12)

    # Opening a menu or picker: a quick rising glide.
    s["menu"] = room(glide(note("E5"), note("B5"), 0.12, 0.05, voice), length=0.25, mix=0.14)

    # Toggle on (rising pair) and off (falling pair).
    s["toggle-on"] = room(place([(0.0, 0.7 * tone(note("G#5"), 0.08, 0.025)),
                                 (0.045, 0.8 * tone(note("B5"), 0.1, 0.03))], 0.16), length=0.15, mix=0.1)
    s["toggle-off"] = room(place([(0.0, 0.7 * tone(note("B5"), 0.08, 0.025)),
                                  (0.045, 0.7 * tone(note("G#5"), 0.1, 0.03))], 0.16), length=0.15, mix=0.1)

    # Play: an E major arpeggio that blooms, the one "big" sound.
    arp = [(0.00, "E5"), (0.07, "G#5"), (0.14, "B5"), (0.21, "E6")]
    parts = [(st, tone(note(n), 0.9 - st, 0.35 + st)) for st, n in arp]
    if voice == "glass":
        parts.append((0.21, 0.35 * tone(note("B6"), 0.7, 0.4)))
    s["launch"] = room(place(parts, 1.0), length=0.7, mix=0.25)

    # Something can't be done: two low notes a semitone apart, muted.
    s["error"] = room(place([(0.0, tone(note("E4"), 0.14, 0.05)),
                             (0.11, tone(note("D#4"), 0.18, 0.06))], 0.32), length=0.2, mix=0.1)
    return s


# Relative loudness (dB below the loudest). Frequent sounds sit lower.
LEVELS = {"move": -12, "edge": -11, "switch": -9, "cycle": -11, "toggle-on": -9, "toggle-off": -9,
          "menu": -8, "back": -7, "select": -5, "error": -7, "launch": -3}


def finish(x, name):
    x = x - np.mean(x)
    # Match loudness on the loudest 30 ms (what the ear judges for short
    # sounds), then apply the level table. Nothing is limited, so the
    # table holds exactly; the loudest sound peaks near -3 dBFS.
    w = int(SR * 0.03)
    short_rms = np.sqrt(np.convolve(x ** 2, np.ones(w) / w, "valid")).max()
    x = x / short_rms * 0.4 * 10 ** (LEVELS[name] / 20)
    # Trim the tail once it is below -60 dB of the peak.
    peak = np.max(np.abs(x))
    above = np.nonzero(np.abs(x) > peak * 1e-3)[0]
    x = fade_tail(x[: above[-1] + int(SR * 0.01)].copy(), 10)
    return x


def scroll_demo(move, edge):
    """What fast scrolling sounds like: slight pitch variation, then the end of the row."""
    parts = []
    for k in range(9):
        cents = rng.uniform(-25, 25)
        factor = 2 ** (cents / 1200)
        idx = np.arange(0, len(move) - 1, factor)
        varied = np.interp(idx, np.arange(len(move)), move)
        parts.append((0.1 + k * 0.11, varied * rng.uniform(0.85, 1.0)))
    parts.append((0.1 + 9 * 0.11 + 0.05, edge))
    return place(parts, 1.6)


def main():
    os.makedirs(OUT, exist_ok=True)
    report = {}
    for voice in ("glass", "pixel"):
        sounds = build(voice)
        done = {}
        for name, x in sounds.items():
            y = finish(x, name)
            done[name] = y
        done["scroll-demo"] = scroll_demo(done["move"], done["edge"])
        for name, y in done.items():
            path = os.path.join(OUT, f"{voice}-{name}.wav")
            wavfile.write(path, SR, (np.clip(y, -1, 1) * 32767).astype(np.int16))
            report[f"{voice}-{name}"] = {
                "ms": round(1000 * len(y) / SR),
                "peak_dbfs": round(20 * np.log10(np.max(np.abs(y)) + 1e-12), 1),
                "start": round(float(abs(y[0])), 5), "end": round(float(abs(y[-1])), 5)}
    print(json.dumps(report, indent=1))


if __name__ == "__main__":
    main()
