"""Two options for opening the system (pause) menu, replacing the rising
"menu" glide there (user, 2026-11-25). Built from the Glass parts.

  A, settle: a soft E5 + B5 chord that swells in over 40 ms and fades, with
     a breath of air under it.
  B, chime: E5 and E6 struck together, muted (fewer overtones), with a
     slightly longer tail.

    python3 tools/sounds/synth_pause.py [output folder]
"""
import os
import sys

import numpy as np
from scipy.io import wavfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import synth as base  # noqa: E402
import synth_glass2 as g2  # noqa: E402
from synth import SR, note, place, lowpass, bandpass, t_axis  # noqa: E402

LEVELS = {"pause": -7}


def sounds():
    rng = np.random.default_rng(41)
    g2.rng = rng
    s = {}
    t = t_axis(0.5)
    swell = np.minimum(t / 0.04, 1.0)
    chord = (g2.bell(note("E5"), 0.5, 0.16, partials=((1, 1.0), (2.0, 0.08)))
             + 0.75 * g2.bell(note("B5"), 0.5, 0.15, partials=((1, 1.0), (2.0, 0.06)))) * swell
    n = int(SR * 0.16)
    breath = lowpass(bandpass(rng.standard_normal(n), 1500, 5000), 5000) \
        * np.sin(np.pi * np.linspace(0, 1, n)) ** 2 * 0.05
    s["pause-a-settle"] = g2.stereo_air(place([(0.0, breath), (0.01, chord)], 0.55),
                                        length=0.35, mix=0.18, width=0.8)
    muted = ((1, 1.0), (2.0, 0.05))
    s["pause-b-chime"] = g2.stereo_air(place([
        (0.0, g2.bell(note("E5"), 0.6, 0.2, partials=muted)),
        (0.0, 0.45 * g2.bell(note("E6"), 0.5, 0.14, partials=muted))], 0.65),
        length=0.4, mix=0.2, width=0.7)
    return s


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "out-pause"
    os.makedirs(out, exist_ok=True)
    base.LEVELS.update(LEVELS)
    for name, x in sounds().items():
        y = g2.finish(x, "pause")
        wavfile.write(os.path.join(out, name + ".wav"), SR, (np.clip(y, -1, 1) * 32767).astype(np.int16))
        print(f"{name:16s} {1000 * len(y) / SR:5.0f} ms  peak {20 * np.log10(np.abs(y).max()):6.1f} dBFS")


if __name__ == "__main__":
    main()
