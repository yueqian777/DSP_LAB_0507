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
        "guard_center_transfer_db": transfer_db(
            spectrum_in, spectrum_out, 960
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
    brightness_off = capture_metrics(root, "brightness_off")
    brightness_on = capture_metrics(root, "brightness_on")
    guard_center_delta = float(brightness_on["guard_center_transfer_db"]) - float(
        brightness_off["guard_center_transfer_db"]
    )

    require(bool(runtime_off["byte_exact"]), "Runtime OFF capture is not byte-exact")
    require(bool(presence["byte_exact"]), "Presence identity capture is not byte-exact")
    require(
        -1.7 <= guard_center_delta <= -1.3,
        "5859.375 Hz level-3 attenuation is outside [-1.7, -1.3] dB",
    )
    for capture in (runtime_off, presence, brightness_off, brightness_on):
        require(float(capture["output"]["peak"]) < 32767.0,
                f"{capture['label']} clipped")

    return {
        "evidence_label": "MEASURED_ON_CURRENT_BOARD_OBJECTIVE_ONLY",
        "runtime_off_identity": runtime_off,
        "presence_identity": presence,
        "brightness_off": brightness_off,
        "brightness_on": brightness_on,
        "guard_center_delta_db": guard_center_delta,
        "acceptance_basis": "Host level-3 response is approximately -1.493 dB",
        "subjective_observation": "NOT_PERFORMED",
        "external_analog_metrics": "UNMEASURED",
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
