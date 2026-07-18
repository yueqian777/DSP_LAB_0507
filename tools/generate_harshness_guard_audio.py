from __future__ import annotations

import argparse
import hashlib
import json
import math
import wave
from pathlib import Path

import numpy as np


SAMPLE_RATE = 50_000
FFT_SIZE = 1024
SAMPLE_COUNT = 512_000
FREQUENCIES = {
    "bass": 97.65625,
    "mud": 390.625,
    "presence": 1953.125,
    "brightness": 5859.375,
}
TARGET_HARSHNESS_DB = (2.0, 7.0, 10.0, 14.0, 18.0)
POWER_EPSILON = 1.0e-20


def sine(time: np.ndarray, frequency_hz: float, phase: float = 0.0) -> np.ndarray:
    return np.sin(2.0 * np.pi * frequency_hz * time + phase)


def deterministic_pseudorandom(sample_count: int) -> np.ndarray:
    samples = np.empty(sample_count, dtype=np.float64)
    state = 0x13579BDF
    for index in range(sample_count):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        samples[index] = (((state >> 16) & 0x3FFF) - 8192) / 8192.0
    return 0.18 * samples


def analyzer_measure(samples: np.ndarray) -> dict[str, float]:
    frame = np.asarray(samples[:FFT_SIZE], dtype=np.float64)
    centered = frame - np.mean(frame)
    windowed = centered * np.hanning(FFT_SIZE)
    spectrum = np.fft.fft(windowed)
    bins = np.arange(1, FFT_SIZE // 2)
    frequencies = bins * SAMPLE_RATE / FFT_SIZE
    power = (
        np.abs(spectrum[bins]) ** 2
        * (2.0 / (FFT_SIZE * FFT_SIZE))
    )
    masks = {
        "bass": (frequencies >= 20.0) & (frequencies < 200.0),
        "mud": (frequencies >= 200.0) & (frequencies < 800.0),
        "presence": (frequencies >= 800.0) & (frequencies < 4000.0),
        "brightness": (frequencies >= 4000.0) & (frequencies <= 16000.0),
    }
    density = {name: float(np.mean(power[mask])) for name, mask in masks.items()}
    global_density = float(np.mean(power[np.logical_or.reduce(tuple(masks.values()))]))
    relative = {
        name: 10.0
        * math.log10((value + POWER_EPSILON) / (global_density + POWER_EPSILON))
        for name, value in density.items()
    }
    rms = float(np.sqrt(np.mean(centered * centered)))
    rms_dbfs = 20.0 * math.log10(max(rms, 1.0e-12))
    return {
        "bass_db": relative["bass"],
        "mud_db": relative["mud"],
        "presence_db": relative["presence"],
        "brightness_db": relative["brightness"],
        "harshness_db": relative["brightness"] - relative["presence"],
        "masking_db": relative["mud"] - relative["presence"],
        "rms_dbfs": rms_dbfs,
    }


def calibrate_band_difference(
    time: np.ndarray,
    target_db: float,
    numerator_frequency: float,
    denominator_frequency: float,
    denominator_amplitude: float,
) -> tuple[np.ndarray, float]:
    denominator = denominator_amplitude * sine(time, denominator_frequency)
    numerator_amplitude = denominator_amplitude * 2.0
    numerator_name = (
        "brightness_db" if numerator_frequency == FREQUENCIES["brightness"]
        else "mud_db"
    )
    for _ in range(12):
        candidate = denominator + numerator_amplitude * sine(
            time, numerator_frequency, 0.37
        )
        measured = analyzer_measure(candidate)
        difference = measured[numerator_name] - measured["presence_db"]
        numerator_amplitude *= 10.0 ** ((target_db - difference) / 20.0)
    samples = denominator + numerator_amplitude * sine(
        time, numerator_frequency, 0.37
    )
    return samples, numerator_amplitude


def write_wave(path: Path, samples: np.ndarray, metadata: dict[str, object]) -> dict[str, object]:
    peak = float(np.max(np.abs(samples)))
    if peak >= 0.98:
        raise ValueError(f"stimulus peak is not headroom-safe: {path.name}={peak}")
    pcm = np.clip(np.rint(samples * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(pcm.tobytes())
    measured = analyzer_measure(pcm.astype(np.float64) / 32768.0)
    entry: dict[str, object] = {
        "file_name": path.name,
        "size_bytes": path.stat().st_size,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest().upper(),
        "peak": peak,
        **measured,
        **metadata,
    }
    return entry


def sequence_analyzer_segments(
    phases: tuple[np.ndarray, ...],
    sequence: tuple[int, ...],
    segment_samples: int,
    crossfade_samples: int = 256,
) -> np.ndarray:
    output = np.empty_like(phases[0])
    sample_count = phases[0].size
    segment = 0
    start = 0
    while start < sample_count:
        end = min(start + segment_samples, sample_count)
        source = phases[sequence[segment % len(sequence)]]
        output[start:end] = source[start:end]
        if (start > 0):
            fade_count = min(crossfade_samples, end - start)
            previous = phases[sequence[(segment - 1) % len(sequence)]]
            phase = np.arange(fade_count, dtype=np.float64)
            weight = 0.5 - 0.5 * np.cos(
                np.pi * (phase + 1.0) / float(fade_count)
            )
            output[start : start + fade_count] = (
                previous[start : start + fade_count] * (1.0 - weight)
                + source[start : start + fade_count] * weight
            )
        segment += 1
        start = end
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    time = np.arange(SAMPLE_COUNT, dtype=np.float64) / SAMPLE_RATE
    tones = {name: sine(time, frequency) for name, frequency in FREQUENCIES.items()}
    files: list[dict[str, object]] = []

    files.append(
        write_wave(
            args.output_dir / "presence_tone.wav",
            0.60 * tones["presence"],
            {"stimulus": "presence_tone"},
        )
    )
    files.append(
        write_wave(
            args.output_dir / "silence.wav",
            np.zeros(SAMPLE_COUNT, dtype=np.float64),
            {"stimulus": "silence"},
        )
    )
    files.append(
        write_wave(
            args.output_dir / "brightness_tone.wav",
            0.60 * tones["brightness"],
            {"stimulus": "brightness_tone"},
        )
    )

    mixed, _ = calibrate_band_difference(
        time, 10.0, FREQUENCIES["brightness"],
        FREQUENCIES["presence"], 0.02
    )
    files.append(
        write_wave(
            args.output_dir / "brightness_presence_mix.wav",
            mixed,
            {"stimulus": "brightness_presence_mix", "target_harshness_db": 10.0},
        )
    )

    for target_db in TARGET_HARSHNESS_DB:
        samples, _ = calibrate_band_difference(
            time, target_db, FREQUENCIES["brightness"],
            FREQUENCIES["presence"], 0.02
        )
        files.append(
            write_wave(
                args.output_dir / f"harshness_sweep_{int(target_db)}db.wav",
                samples,
                {"stimulus": "harshness_sweep", "target_harshness_db": target_db},
            )
        )

    presence_amplitude = 0.02
    _, brightness_amplitude = calibrate_band_difference(
        time, 18.0, FREQUENCIES["brightness"],
        FREQUENCIES["presence"], presence_amplitude
    )
    _, mud_amplitude = calibrate_band_difference(
        time, 14.0, FREQUENCIES["mud"],
        FREQUENCIES["presence"], presence_amplitude
    )
    bass_mud_phase = (
        0.10 * tones["bass"]
        + mud_amplitude * sine(time, FREQUENCIES["mud"], 0.71)
        + presence_amplitude * sine(time, FREQUENCIES["presence"], 1.17)
    )
    brightness_phase = (
        presence_amplitude * sine(time, FREQUENCIES["presence"], 1.17)
        + brightness_amplitude
        * sine(time, FREQUENCIES["brightness"], 0.37)
    )
    overlap_phase = (
        0.04673652 * sine(time, FREQUENCIES["bass"], 0.13)
        + 0.03209594 * sine(time, FREQUENCIES["mud"], 0.71)
        + 0.02859356 * sine(time, FREQUENCIES["presence"], 1.17)
        + 0.14319700 * sine(time, FREQUENCIES["brightness"], 0.37)
    )
    analyzer_segment_samples = 8 * FFT_SIZE
    segment_count = math.ceil(SAMPLE_COUNT / analyzer_segment_samples)
    all_dynamic_sequence = (
        (2,) * 8
        + (0,) * 27
        + (1,) * max(1, segment_count - 35)
    )
    all_dynamic = sequence_analyzer_segments(
        (bass_mud_phase, brightness_phase, overlap_phase),
        all_dynamic_sequence,
        analyzer_segment_samples,
    )
    phase_measurements = {
        "bass_mud_phase": analyzer_measure(bass_mud_phase),
        "brightness_phase": analyzer_measure(brightness_phase),
        "overlap_phase": analyzer_measure(overlap_phase),
        "segment_samples": analyzer_segment_samples,
    }
    files.append(
        write_wave(
            args.output_dir / "all_dynamic_trigger.wav",
            all_dynamic,
            {"stimulus": "all_dynamic_trigger", **phase_measurements},
        )
    )

    modulation = 0.78 + 0.22 * sine(time, 0.1953125)
    music_like_sequence = (2,) * 4 + (0,) * 8 + (1,) * 8
    music_like = modulation * sequence_analyzer_segments(
        (bass_mud_phase, brightness_phase, overlap_phase),
        music_like_sequence,
        analyzer_segment_samples,
    )
    files.append(
        write_wave(
            args.output_dir / "music_like.wav",
            music_like,
            {"stimulus": "music_like", **phase_measurements},
        )
    )

    files.append(
        write_wave(
            args.output_dir / "transition_dual.wav",
            mixed,
            {"stimulus": "transition_dual"},
        )
    )
    files.append(
        write_wave(
            args.output_dir / "transition_music.wav",
            music_like,
            {"stimulus": "transition_music", **phase_measurements},
        )
    )
    transition_noise_period_samples = FFT_SIZE
    transition_noise = np.resize(
        deterministic_pseudorandom(transition_noise_period_samples),
        SAMPLE_COUNT,
    )
    files.append(
        write_wave(
            args.output_dir / "transition_noise.wav",
            transition_noise,
            {
                "stimulus": "transition_noise",
                "generator": "LCG32_PERIODIC",
                "period_samples": transition_noise_period_samples,
            },
        )
    )

    manifest = {
        "evidence_label": "HOST_GENERATED_FIXED_STIMULUS",
        "sample_rate_hz": SAMPLE_RATE,
        "sample_count": SAMPLE_COUNT,
        "duration_seconds": SAMPLE_COUNT / SAMPLE_RATE,
        "fft_size": FFT_SIZE,
        "transition_noise_period_samples": transition_noise_period_samples,
        "window": "Hann N-1",
        "power": "one-sided density with Analyzer band-bin compensation",
        "frequencies_hz": FREQUENCIES,
        "files": files,
    }
    (args.output_dir / "audio_manifest.json").write_text(
        json.dumps(manifest, indent=2), encoding="ascii"
    )
    print(f"harshness_audio_files={len(files)} label={manifest['evidence_label']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
