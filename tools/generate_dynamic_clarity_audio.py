from __future__ import annotations

import argparse
import json
import math
import wave
from pathlib import Path

import numpy as np


SAMPLE_RATE = 50_000
SAMPLE_COUNT = 512_000
FREQUENCIES = {
    "bass": 97.65625,
    "mud": 390.625,
    "presence": 1953.125,
    "brightness": 8007.8125,
}
# The Analyzer averages power per FFT bin. Mud has 12 positive bins while
# Presence has 65, so amplitude ratios must compensate before targeting a
# density-domain masking difference.
MUD_PRESENCE_DENSITY_COMPENSATION = math.sqrt(12.0 / 65.0)


def sine(time: np.ndarray, frequency_hz: float) -> np.ndarray:
    return np.sin(2.0 * np.pi * frequency_hz * time)


def write_wave(path: Path, samples: np.ndarray) -> dict[str, float | int | str]:
    peak = float(np.max(np.abs(samples)))
    if peak >= 0.98:
        raise ValueError(f"stimulus peak is not headroom-safe: {path.name}={peak}")
    pcm = np.clip(np.rint(samples * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(pcm.tobytes())
    return {
        "name": path.stem,
        "peak": peak,
        "rms": float(np.sqrt(np.mean(samples * samples))),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    time = np.arange(SAMPLE_COUNT, dtype=np.float64) / SAMPLE_RATE
    tones = {name: sine(time, frequency) for name, frequency in FREQUENCIES.items()}
    generated: list[dict[str, float | int | str]] = []

    generated.append(write_wave(args.output_dir / "presence.wav", 0.60 * tones["presence"]))
    generated.append(write_wave(args.output_dir / "mud.wav", 0.60 * tones["mud"]))

    masking_targets = [2.0, 5.0, 8.0, 12.0, 16.0]
    for masking_db in masking_targets:
        presence_scale = 0.10
        mud_scale = (
            presence_scale
            * 10.0 ** (masking_db / 20.0)
            * MUD_PRESENCE_DENSITY_COMPENSATION
        )
        name = f"mask{int(masking_db):02d}"
        samples = mud_scale * tones["mud"] + presence_scale * tones["presence"]
        entry = write_wave(args.output_dir / f"{name}.wav", samples)
        entry["target_masking_db"] = masking_db
        generated.append(entry)

    weak_mud = (
        0.025 * tones["mud"]
        + 0.008 * tones["presence"]
        + 0.40 * tones["brightness"]
    )
    generated.append(write_wave(args.output_dir / "weak_mud.wav", weak_mud))

    weak_presence = 0.35 * tones["mud"] + 0.006 * tones["presence"]
    generated.append(
        write_wave(args.output_dir / "weak_presence.wav", weak_presence)
    )

    combined = (
        0.38 * tones["bass"]
        + 0.153 * tones["mud"]
        + 0.080 * tones["presence"]
    )
    generated.append(write_wave(args.output_dir / "combined.wav", combined))

    modulation = 0.72 + 0.28 * sine(time, 0.1953125)
    generated.append(
        write_wave(
            args.output_dir / "combined_transition.wav",
            modulation * combined,
        )
    )

    mask16_mud = (
        0.10
        * 10.0 ** (16.0 / 20.0)
        * MUD_PRESENCE_DENSITY_COMPENSATION
    )
    mask16_base = mask16_mud * tones["mud"] + 0.10 * tones["presence"]
    generated.append(
        write_wave(
            args.output_dir / "probe100.wav",
            mask16_base + 0.08 * tones["bass"],
        )
    )
    generated.append(
        write_wave(
            args.output_dir / "probe8k.wav",
            mask16_base + 0.08 * tones["brightness"],
        )
    )

    music_like = (
        0.30 * modulation * tones["bass"]
        + 0.14 * tones["mud"]
        + 0.075 * tones["presence"]
        + 0.055 * tones["brightness"]
    )
    generated.append(write_wave(args.output_dir / "music_like.wav", music_like))

    transition_dual = (
        0.24 * sine(time, 400.0)
        + 0.20 * sine(time, 1953.125)
    )
    generated.append(
        write_wave(args.output_dir / "transition_dual.wav", transition_dual)
    )

    transition_music = (
        0.20 * sine(time, 97.65625)
        + 0.16 * sine(time, 400.0)
        + 0.11 * sine(time, 976.5625)
        + 0.09 * sine(time, 1953.125)
        + 0.06 * sine(time, 5859.375)
    )
    generated.append(
        write_wave(args.output_dir / "transition_music.wav", transition_music)
    )

    random_generator = np.random.default_rng(0x3317)
    transition_noise_period_samples = 1024
    transition_noise_period = random_generator.uniform(
        -1.0, 1.0, size=transition_noise_period_samples
    )
    transition_noise = 0.24 * np.resize(
        transition_noise_period, SAMPLE_COUNT
    )
    generated.append(
        write_wave(args.output_dir / "transition_noise.wav", transition_noise)
    )

    manifest = {
        "sample_rate": SAMPLE_RATE,
        "sample_count": SAMPLE_COUNT,
        "duration_seconds": SAMPLE_COUNT / SAMPLE_RATE,
        "frequencies_hz": FREQUENCIES,
        "masking_targets_db": masking_targets,
        "mud_presence_density_compensation": (
            MUD_PRESENCE_DENSITY_COMPENSATION
        ),
        "transition_noise_period_samples": transition_noise_period_samples,
        "files": generated,
        "label": "HOST_GENERATED_FIXED_STIMULUS",
    }
    (args.output_dir / "audio_manifest.json").write_text(
        json.dumps(manifest, indent=2), encoding="ascii"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
