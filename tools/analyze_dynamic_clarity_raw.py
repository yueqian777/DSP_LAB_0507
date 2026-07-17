from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


SAMPLE_COUNT = 8192


def read_pcm16(path: Path) -> tuple[bytes, np.ndarray]:
    payload = path.read_bytes()
    if len(payload) != SAMPLE_COUNT * 2:
        raise ValueError(f"invalid 8192-sample PCM16 capture: {path}")
    return payload, np.frombuffer(payload, dtype="<i2").astype(np.float64)


def transfer_db(spectrum_in: np.ndarray, spectrum_out: np.ndarray, bin_: int) -> float | None:
    magnitude_in = float(abs(spectrum_in[bin_]))
    if magnitude_in <= 1.0:
        return None
    return float(20.0 * np.log10(abs(spectrum_out[bin_]) / magnitude_in))


def capture_metrics(root: Path, label: str) -> dict[str, object]:
    input_bytes, input_samples = read_pcm16(root / f"{label}_input.raw")
    output_bytes, output_samples = read_pcm16(root / f"{label}_output.raw")
    input_rms = float(np.sqrt(np.mean(input_samples * input_samples)))
    output_rms = float(np.sqrt(np.mean(output_samples * output_samples)))
    if input_rms <= 0.0 or output_rms <= 0.0:
        raise ValueError(f"{label} capture contains zero RMS")
    spectrum_in = np.fft.rfft(input_samples)
    spectrum_out = np.fft.rfft(output_samples)
    return {
        "label": label,
        "input": {
            "samples": SAMPLE_COUNT,
            "rms": input_rms,
            "peak": float(np.max(np.abs(input_samples))),
        },
        "output": {
            "samples": SAMPLE_COUNT,
            "rms": output_rms,
            "peak": float(np.max(np.abs(output_samples))),
        },
        "input_sha256": hashlib.sha256(input_bytes).hexdigest().upper(),
        "output_sha256": hashlib.sha256(output_bytes).hexdigest().upper(),
        "byte_exact": input_bytes == output_bytes,
        "rms_transfer_db": float(20.0 * np.log10(output_rms / input_rms)),
        "mud_transfer_db": transfer_db(spectrum_in, spectrum_out, 64),
        "presence_transfer_db": transfer_db(spectrum_in, spectrum_out, 320),
        "bass_transfer_db": transfer_db(spectrum_in, spectrum_out, 16),
        "brightness_transfer_db": transfer_db(
            spectrum_in, spectrum_out, 1312
        ),
    }


def transition_metrics(root: Path, label: str) -> dict[str, object]:
    _, input_samples = read_pcm16(root / f"{label}_input.raw")
    _, output_samples = read_pcm16(root / f"{label}_output.raw")
    input_step = float(np.max(np.abs(np.diff(input_samples))))
    output_step = float(np.max(np.abs(np.diff(output_samples))))
    frames = output_samples.astype("<i2", copy=False).reshape(8, 1024)
    repeated = sum(np.array_equal(frames[index - 1], frames[index]) for index in range(1, 8))
    clip_count = int(np.count_nonzero((output_samples == 32767) | (output_samples == -32768)))
    return {
        "label": label,
        "samples": SAMPLE_COUNT,
        "input_max_adjacent_step": input_step,
        "output_max_adjacent_step": output_step,
        "output_max_adjacent_step_full_scale": output_step / 32768.0,
        "repeated_adjacent_output_frames": repeated,
        "clip_count": clip_count,
    }


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def analyze(root: Path) -> dict[str, object]:
    runtime_off = capture_metrics(root, "runtime_off_identity")
    presence = capture_metrics(root, "presence_identity")
    mud_off = capture_metrics(root, "mud_dominant_off")
    mud_on = capture_metrics(root, "mud_dominant_on")
    probe100_off = capture_metrics(root, "probe100_off")
    probe100_on = capture_metrics(root, "probe100_on")
    probe8k_off = capture_metrics(root, "probe8k_off")
    probe8k_on = capture_metrics(root, "probe8k_on")
    triple_bass = transition_metrics(root, "triple_flat_to_bass")
    triple_vocal = transition_metrics(root, "triple_bass_to_vocal")

    clarity_mud_delta = float(mud_on["mud_transfer_db"]) - float(
        mud_off["mud_transfer_db"]
    )
    clarity_presence_delta = float(mud_on["presence_transfer_db"]) - float(
        mud_off["presence_transfer_db"]
    )
    clarity_100hz_delta = float(probe100_on["bass_transfer_db"]) - float(
        probe100_off["bass_transfer_db"]
    )
    clarity_8khz_delta = float(
        probe8k_on["brightness_transfer_db"]
    ) - float(probe8k_off["brightness_transfer_db"])

    require(bool(runtime_off["byte_exact"]), "Runtime OFF capture is not byte-exact")
    require(bool(presence["byte_exact"]), "Presence identity capture is not byte-exact")
    require(-2.2 <= clarity_mud_delta <= -1.7, "400 Hz attenuation is outside [-2.2, -1.7] dB")
    require(abs(clarity_presence_delta) <= 0.2, "Presence-band deviation exceeds 0.2 dB")
    require(abs(clarity_100hz_delta) <= 0.2, "100 Hz deviation exceeds 0.2 dB")
    require(abs(clarity_8khz_delta) <= 0.2, "8 kHz deviation exceeds 0.2 dB")
    for transition in (triple_bass, triple_vocal):
        require(int(transition["clip_count"]) == 0, f"{transition['label']} clipped")
        require(
            int(transition["repeated_adjacent_output_frames"]) == 0,
            f"{transition['label']} repeated a frame",
        )
        require(
            float(transition["output_max_adjacent_step_full_scale"]) < 0.25,
            f"{transition['label']} has an excessive adjacent-sample jump",
        )

    return {
        "evidence_label": "MEASURED_ON_CURRENT_BOARD_OBJECTIVE_ONLY",
        "runtime_off_identity": runtime_off,
        "presence_identity": presence,
        "mud_dominant_off": mud_off,
        "mud_dominant_on": mud_on,
        "probe100_off": probe100_off,
        "probe100_on": probe100_on,
        "probe8k_off": probe8k_off,
        "probe8k_on": probe8k_on,
        "triple_flat_to_bass": triple_bass,
        "triple_bass_to_vocal": triple_vocal,
        "clarity_mud_delta_db": clarity_mud_delta,
        "clarity_presence_delta_db": clarity_presence_delta,
        "clarity_100hz_delta_db": clarity_100hz_delta,
        "clarity_8khz_delta_db": clarity_8khz_delta,
        "pass": True,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_dir", type=Path)
    args = parser.parse_args()
    result = analyze(args.result_dir)
    output = args.result_dir / "capture_metrics.json"
    output.write_text(json.dumps(result, indent=2), encoding="ascii")
    print(json.dumps({"pass": True, "capture_metrics": str(output)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
