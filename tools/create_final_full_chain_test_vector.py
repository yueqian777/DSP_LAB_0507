"""Build a reproducible 50 kHz mode-8 hardware input WAV from local sources."""

from __future__ import annotations

import hashlib
import json
import wave
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
SPEECH = Path(r"D:\NDM\20241034145.wav")
NOISE = Path(r"D:\NDM\20241034145_white_noise_20pct.wav")
OUT = ROOT / "test_vectors" / "final_full_chain_2s_noise_speech_stationary_50k.wav"
MANIFEST = ROOT / "test_vectors" / "final_full_chain_2s_noise_speech_stationary_50k.json"
RATE = 50000
NOISE_ONLY_SECONDS = 2
SPEECH_NOISE_SECONDS = 12
TARGET_PEAK = 16384


def read_mono_pcm16(path: Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path), "rb") as source:
        if source.getnchannels() != 1 or source.getsampwidth() != 2:
            raise ValueError(f"{path} must be mono PCM16")
        samples = np.frombuffer(source.readframes(source.getnframes()), dtype="<i2")
        return samples.astype(np.float64), source.getframerate()


def resample_linear(samples: np.ndarray, old_rate: int) -> np.ndarray:
    target_count = round(len(samples) * RATE / old_rate)
    positions = np.linspace(0.0, len(samples) - 1.0, target_count)
    return np.interp(positions, np.arange(len(samples)), samples)


def repeat_to(samples: np.ndarray, count: int) -> np.ndarray:
    return np.resize(samples, count)


def main() -> None:
    speech, speech_rate = read_mono_pcm16(SPEECH)
    noise, noise_rate = read_mono_pcm16(NOISE)
    speech = resample_linear(speech, speech_rate)
    noise = resample_linear(noise, noise_rate)
    noise_count = NOISE_ONLY_SECONDS * RATE
    speech_count = SPEECH_NOISE_SECONDS * RATE
    noise_only = repeat_to(noise, noise_count)
    speech_plus_noise = repeat_to(speech, speech_count) + repeat_to(noise, speech_count)
    combined = np.concatenate((noise_only, speech_plus_noise))
    combined *= TARGET_PEAK / np.max(np.abs(combined))
    pcm = np.clip(np.rint(combined), -32768, 32767).astype("<i2")
    with wave.open(str(OUT), "wb") as result:
        result.setnchannels(1)
        result.setsampwidth(2)
        result.setframerate(RATE)
        result.writeframes(pcm.tobytes())
    digest = hashlib.sha256(OUT.read_bytes()).hexdigest()
    manifest = {
        "filename": OUT.name,
        "sha256": digest,
        "channels": 1,
        "sample_width_bits": 16,
        "sample_rate_hz": RATE,
        "duration_seconds": len(pcm) / RATE,
        "noise_only_seconds": NOISE_ONLY_SECONDS,
        "speech_plus_stationary_noise_seconds": SPEECH_NOISE_SECONDS,
        "target_peak_lsb": TARGET_PEAK,
        "source_speech": str(SPEECH),
        "source_noise": str(NOISE),
        "resampler": "linear",
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
