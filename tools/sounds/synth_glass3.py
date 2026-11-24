"""Two options each for "open a game" and "Play", built to sit with the
refined Glass sounds the user kept.

Open a game
  A: the Back sound turned around - the same two notes rising instead of
     falling, same length and level, so Back and Open read as a pair.
  B: one soft tone with the felt tap from Move, a fifth above it: Move's
     bigger sibling, barely longer than a step.
Play
  A: three quick rising notes, short tail, about half a second.
  B: a soft two-note chord with a gentle rise into it, about 0.6 s.
"""
import os, sys
import numpy as np
from scipy.io import wavfile
import synth as base
import synth_glass2 as g2
from synth_glass2 import bell, tap, stereo_air
from synth import SR, note, place

OUT = sys.argv[1] if len(sys.argv) > 1 else "out3"
LEVELS = dict(base.LEVELS, select=-7, launch=-5)


def sounds():
    s = {}
    s["glass2a-select"] = ("select", stereo_air(place([
        (0.0, 0.8 * bell(note("E5"), 0.28, 0.09)),
        (0.055, 0.8 * bell(note("B5"), 0.34, 0.11))], 0.45), length=0.35, mix=0.17))
    s["glass2b-select"] = ("select", stereo_air(place([
        (0.0, tap(0.16)),
        (0.0, bell(note("B5"), 0.3, 0.09, attack=0.004)),
        (0.0, 0.35 * bell(note("E6"), 0.3, 0.07, attack=0.004, partials=((1, 1.0),)))], 0.32),
        length=0.3, mix=0.16))
    s["glass2a-launch"] = ("launch", stereo_air(place([
        (0.0, 0.85 * bell(note("E5"), 0.3, 0.1)),
        (0.06, 0.9 * bell(note("G#5"), 0.3, 0.11)),
        (0.12, bell(note("B5"), 0.4, 0.16))], 0.55), length=0.35, mix=0.18))
    t = np.arange(int(SR * 0.5)) / SR
    rise = np.minimum(t / 0.05, 1.0)
    chord = (bell(note("E5"), 0.5, 0.2) + 0.8 * bell(note("B5"), 0.5, 0.2)) * rise
    s["glass2b-launch"] = ("launch", stereo_air(place([
        (0.0, 0.5 * bell(note("B4"), 0.18, 0.06)), (0.07, chord)], 0.6), length=0.35, mix=0.18))
    return s


def main():
    os.makedirs(OUT, exist_ok=True)
    base_levels = base.LEVELS.copy()
    base.LEVELS.update(LEVELS)
    for name, (event, x) in sounds().items():
        y = g2.finish(x, event)
        wavfile.write(os.path.join(OUT, name + ".wav"), SR, (np.clip(y, -1, 1) * 32767).astype(np.int16))
        X = np.abs(np.fft.rfft(y.mean(axis=1))) ** 2
        f = np.fft.rfftfreq(len(y), 1 / SR)
        print(f"{name:16s} {1000*len(y)/SR:5.0f} ms  peak {20*np.log10(np.abs(y).max()):6.1f} dBFS"
              f"  centroid {(f*X).sum()/X.sum():5.0f} Hz  ends {abs(y[0]).max():.4f}/{abs(y[-1]).max():.4f}")
    base.LEVELS.clear(); base.LEVELS.update(base_levels)


if __name__ == "__main__":
    main()
