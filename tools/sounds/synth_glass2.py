"""Glass, refined. Same key, same loudness scale, same events as synth.py.

What changes from the first Glass set:
- Stereo: each tone gets a slightly different airy tail on the left and right,
  so it feels wider and softer without getting louder.
- A gentle chorus (two copies a few cents apart) gives the bell a soft
  shimmer instead of a single pure line.
- Move is lower (E6, not B6) with a slower attack and a tiny felt tap, so
  hundreds of repeats stay comfortable.
- Select and Play get a quiet low octave underneath for warmth.
- Edge and "can't do that" are rounder: no semitone clash, fewer highs.
- Switch replaces the noise sweep with two quick glass taps and a breath of air.
"""
import os
import sys

import numpy as np
from scipy.io import wavfile

import synth as base
from synth import SR, note, t_axis, envelope, lowpass, bandpass, place

OUT = sys.argv[1] if len(sys.argv) > 1 else "out2"
rng = np.random.default_rng(29)


def bell(freq, dur, decay, level=1.0, attack=0.003, chorus_cents=4.0,
         partials=((1, 1.0), (2.0, 0.14), (3.01, 0.035))):
    t = t_axis(dur)
    x = np.zeros_like(t)
    for detune in (-chorus_cents / 2, chorus_cents / 2):
        f = freq * 2 ** (detune / 1200)
        for r, a in partials:
            x += 0.5 * a * np.sin(2 * np.pi * f * r * t + rng.uniform(0, 2 * np.pi)) \
                * np.exp(-t / (decay / r ** 0.7))
    return level * x * np.minimum(t / attack, 1.0)


def tap(level=0.25, dur=0.012, centre=2500):
    """A tiny felt tap: band-passed noise, a couple of milliseconds."""
    n = int(SR * dur)
    x = bandpass(rng.standard_normal(n), centre * 0.6, centre * 1.6)
    return level * x * np.exp(-np.arange(n) / (SR * 0.002))


def glide(f0, f1, dur, decay):
    t = t_axis(dur)
    f = f0 * (f1 / f0) ** (t / dur)
    ph = 2 * np.pi * np.cumsum(f) / SR
    x = np.sin(ph) + 0.08 * np.sin(2 * ph)
    return x * envelope(len(t), 0.006, decay)


def stereo_air(x, length=0.35, mix=0.18, bright=5500, width=0.6):
    """Different small-room tails left and right around a centred dry tone."""
    n = int(SR * length)
    tails = []
    for _ in range(2):
        ir = rng.standard_normal(n) * np.exp(-np.arange(n) / (SR * length / 5))
        ir = lowpass(ir, bright)
        ir /= np.sqrt(np.sum(ir ** 2))
        tails.append(np.convolve(x, ir))
    m = len(tails[0])
    dry = np.concatenate([x, np.zeros(m - len(x))])
    mid = (tails[0] + tails[1]) / 2
    side = (tails[0] - tails[1]) / 2 * width
    left = dry + mix * (mid + side)
    right = dry + mix * (mid - side)
    return np.stack([left, right], axis=1)


def build():
    s = {}
    s["move"] = stereo_air(place([(0.0, tap(0.18)),
                                  (0.0, bell(note("E6"), 0.1, 0.035, attack=0.004,
                                             partials=((1, 1.0), (2.0, 0.06))))], 0.1),
                           length=0.16, mix=0.12)
    s["edge"] = stereo_air(lowpass(glide(note("B4"), note("E4"), 0.11, 0.035), 2500) * 0.9,
                           length=0.14, mix=0.1)
    low = 0.22
    s["select"] = stereo_air(place([(0.0, bell(note("E6"), 0.4, 0.13)),
                                    (0.0, low * bell(note("E5"), 0.5, 0.18, partials=((1, 1.0),))),
                                    (0.06, bell(note("B6"), 0.55, 0.18))], 0.65),
                             length=0.5, mix=0.22)
    s["back"] = stereo_air(place([(0.0, 0.8 * bell(note("B5"), 0.28, 0.09)),
                                  (0.055, 0.8 * bell(note("E5"), 0.34, 0.11))], 0.45),
                           length=0.35, mix=0.17)
    air_breath = lowpass(bandpass(rng.standard_normal(int(SR * 0.12)), 2000, 6000), 6000) \
        * np.sin(np.pi * np.linspace(0, 1, int(SR * 0.12))) ** 2 * 0.06
    s["switch"] = stereo_air(place([(0.0, air_breath),
                                    (0.02, 0.55 * bell(note("B5"), 0.09, 0.025)),
                                    (0.075, 0.7 * bell(note("E6"), 0.12, 0.035))], 0.25),
                             length=0.22, mix=0.16, width=0.9)
    s["menu"] = stereo_air(glide(note("E5"), note("B5"), 0.13, 0.055), length=0.28, mix=0.15)
    s["toggle-on"] = stereo_air(place([(0.0, 0.7 * bell(note("G#5"), 0.09, 0.028)),
                                       (0.045, 0.8 * bell(note("B5"), 0.11, 0.034))], 0.17),
                                length=0.16, mix=0.11)
    s["toggle-off"] = stereo_air(place([(0.0, 0.7 * bell(note("B5"), 0.09, 0.028)),
                                        (0.045, 0.7 * bell(note("G#5"), 0.11, 0.034))], 0.17),
                                 length=0.16, mix=0.11)
    arp = [(0.00, "E5"), (0.075, "G#5"), (0.15, "B5"), (0.225, "E6")]
    parts = [(st, bell(note(n), 1.0 - st, 0.38 + st)) for st, n in arp]
    parts.append((0.225, 0.3 * bell(note("B6"), 0.8, 0.45)))
    # A soft swell of the whole chord underneath: the "bloom".
    t = t_axis(1.1)
    swell = sum(np.sin(2 * np.pi * note(n) * t) for n in ("E4", "B4", "G#5")) / 3
    swell *= np.sin(np.pi * np.minimum(t / 1.1, 1)) ** 2 * 0.18
    parts.append((0.0, swell))
    s["launch"] = stereo_air(place(parts, 1.15), length=0.8, mix=0.26, width=0.8)
    s["error"] = stereo_air(lowpass(place([(0.0, bell(note("B4"), 0.14, 0.05, partials=((1, 1.0), (2.0, 0.08)))),
                                           (0.12, bell(note("G#4"), 0.2, 0.065, partials=((1, 1.0), (2.0, 0.08))))],
                                          0.34), 3000), length=0.2, mix=0.1)
    return s


def finish(x, name):
    x = x - x.mean(axis=0)
    mono = x.mean(axis=1)
    w = int(SR * 0.03)
    short_rms = np.sqrt(np.convolve(mono ** 2, np.ones(w) / w, "valid")).max()
    x = x / short_rms * 0.4 * 10 ** (base.LEVELS[name] / 20)
    peak = np.abs(x).max()
    above = np.nonzero(np.abs(x).max(axis=1) > peak * 1e-3)[0]
    x = x[: above[-1] + int(SR * 0.01)].copy()
    k = int(SR * 0.01)
    x[-k:] *= np.linspace(1, 0, k)[:, None]
    k = int(SR * 0.001)
    x[:k] *= np.linspace(0, 1, k)[:, None]
    return x


def scroll_demo(move, edge):
    parts = []
    out = np.zeros((int(SR * 1.6), 2))
    def add(start, sig):
        i = int(SR * start)
        j = min(len(out), i + len(sig))
        out[i:j] += sig[: j - i]
    for k in range(9):
        factor = 2 ** (rng.uniform(-25, 25) / 1200)
        idx = np.arange(0, len(move) - 1, factor)
        varied = np.stack([np.interp(idx, np.arange(len(move)), move[:, c]) for c in (0, 1)], axis=1)
        add(0.1 + k * 0.11, varied * rng.uniform(0.85, 1.0))
    add(0.1 + 9 * 0.11 + 0.05, edge)
    return out


def main():
    os.makedirs(OUT, exist_ok=True)
    done = {name: finish(x, name) for name, x in build().items()}
    done["scroll-demo"] = scroll_demo(done["move"], done["edge"])
    for name, y in done.items():
        wavfile.write(os.path.join(OUT, f"glass2-{name}.wav"), SR,
                      (np.clip(y, -1, 1) * 32767).astype(np.int16))
        X = np.abs(np.fft.rfft(y.mean(axis=1))) ** 2
        f = np.fft.rfftfreq(len(y), 1 / SR)
        print(f"{name:12s} {1000*len(y)/SR:5.0f} ms  peak {20*np.log10(np.abs(y).max()):6.1f} dBFS"
              f"  centroid {(f*X).sum()/X.sum():5.0f} Hz  >8k {10*np.log10(X[f>8000].sum()/X.sum()):6.1f} dB"
              f"  ends {abs(y[0]).max():.4f}/{abs(y[-1]).max():.4f}")


if __name__ == "__main__":
    main()
