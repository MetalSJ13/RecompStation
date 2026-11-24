"""Writes RecompStation's interface sounds to assets/sounds/.

Every sound is synthesized here from sine tones and filtered noise; nothing is
sampled or taken from elsewhere. Run from the repository root:

    python3 tools/sounds/generate.py

Needs Python 3 with numpy and scipy (only to regenerate; the WAV files are
committed). The chosen set is "Glass, refined", with option A for opening a
game and for Play (approved 2026-11-24).
"""
import os
import sys

import numpy as np
from scipy.io import wavfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import synth as base  # noqa: E402
import synth_glass2 as g2  # noqa: E402
import synth_glass3 as g3  # noqa: E402

OUT = os.path.join(HERE, "..", "..", "assets", "sounds")
SR = base.SR


def write(name, y):
    path = os.path.normpath(os.path.join(OUT, name + ".wav"))
    wavfile.write(path, SR, (np.clip(y, -1, 1) * 32767).astype(np.int16))
    print(f"{name:12s} {1000 * len(y) / SR:5.0f} ms  peak {20 * np.log10(np.abs(y).max()):6.1f} dBFS")


def repitch(y, cents):
    """Resample to shift pitch slightly; used for the three Move variants."""
    factor = 2 ** (cents / 1200)
    idx = np.arange(0, len(y) - 1, factor)
    return np.stack([np.interp(idx, np.arange(len(y)), y[:, c]) for c in (0, 1)], axis=1)


def main():
    os.makedirs(OUT, exist_ok=True)
    kept = {name: g2.finish(x, name) for name, x in g2.build().items()
            if name not in ("select", "launch")}
    base.LEVELS.update(g3.LEVELS)
    # Same random state as when the options were reviewed, so the files
    # match the approved sounds exactly.
    g2.rng = np.random.default_rng(29)
    chosen = g3.sounds()
    kept["select"] = g2.finish(chosen["glass2a-select"][1], "select")
    kept["launch"] = g2.finish(chosen["glass2a-launch"][1], "launch")

    move = kept.pop("move")
    # Three slightly different pitches, picked at random while scrolling, so
    # holding a direction never sounds mechanical.
    for i, cents in enumerate((-18, 0, 18), start=1):
        write(f"move-{i}", move if cents == 0 else repitch(move, cents))
    for name in sorted(kept):
        write(name, kept[name])


if __name__ == "__main__":
    main()
