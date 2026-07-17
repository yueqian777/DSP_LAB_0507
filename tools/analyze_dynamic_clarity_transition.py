from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path

import numpy as np


SCRIPT_VERSION = "1.0"
FRAME_LEN = 1024
PRE_FRAMES = 4
SAMPLE_RATE = 50_000
TRANSITION_SAMPLES = 4000
T0 = PRE_FRAMES * FRAME_LEN
T1 = T0 + TRANSITION_SAMPLES
WINDOW_LEN = 512


def db20(value: float) -> float:
    return 20.0 * math.log10(max(float(value), 1.0e-15))


def read_pcm16(path: Path, pre_write_index: int) -> np.ndarray:
    samples = np.fromfile(path, dtype="<i2")
    if samples.size != 12 * FRAME_LEN:
        raise ValueError(f"invalid trigger capture size: {path}={samples.size}")
    frames = samples.reshape(12, FRAME_LEN)
    pre = np.concatenate(
        (frames[pre_write_index:PRE_FRAMES], frames[:pre_write_index]),
        axis=0,
    )
    ordered = np.concatenate((pre, frames[PRE_FRAMES:]), axis=0).reshape(-1)
    return ordered.astype(np.float64) / 32768.0


def lag_score(reference: np.ndarray, candidate: np.ndarray, lag: int) -> float:
    if lag >= 0:
        left = reference[lag:]
        right = candidate[: candidate.size - lag]
    else:
        left = reference[: reference.size + lag]
        right = candidate[-lag:]
    left = left - np.mean(left)
    right = right - np.mean(right)
    denominator = float(np.linalg.norm(left) * np.linalg.norm(right))
    return float(np.dot(left, right) / denominator) if denominator > 0.0 else -1.0


def estimate_lag(reference: np.ndarray, candidate: np.ndarray) -> tuple[float, float]:
    search = range(-FRAME_LEN, FRAME_LEN + 1)
    scores = np.asarray([lag_score(reference, candidate, lag) for lag in search])
    best_index = int(np.argmax(scores))
    integer_lag = best_index - FRAME_LEN
    fractional = 0.0
    if 0 < best_index < scores.size - 1:
        left = scores[best_index - 1]
        center = scores[best_index]
        right = scores[best_index + 1]
        denominator = left - 2.0 * center + right
        if abs(denominator) > 1.0e-12:
            fractional = float(0.5 * (left - right) / denominator)
            fractional = max(-0.5, min(0.5, fractional))
    return integer_lag + fractional, float(scores[best_index])


def shift_signal(signal: np.ndarray, lag: float) -> np.ndarray:
    indices = np.arange(signal.size, dtype=np.float64) - lag
    return np.interp(indices, np.arange(signal.size), signal, left=np.nan, right=np.nan)


def fit_affine(source: np.ndarray, target: np.ndarray) -> tuple[float, float]:
    matrix = np.column_stack((source, np.ones(source.size)))
    coefficients, _, _, _ = np.linalg.lstsq(matrix, target, rcond=None)
    return float(coefficients[0]), float(coefficients[1])


def align_baseline(
    stable_input: np.ndarray,
    stable_output: np.ndarray,
    transition_input: np.ndarray,
    transition_output: np.ndarray,
) -> tuple[np.ndarray, dict[str, float | bool]]:
    lag, raw_correlation = estimate_lag(transition_input, stable_input)
    shifted_input = shift_signal(stable_input, lag)
    shifted_output = shift_signal(stable_output, lag)
    pre_indices = np.arange(T0 - 2048, T0 - 512)
    valid = np.isfinite(shifted_input[pre_indices])
    pre_indices = pre_indices[valid]
    input_gain, input_offset = fit_affine(
        shifted_input[pre_indices], transition_input[pre_indices]
    )
    fitted_input = shifted_input * input_gain + input_offset
    output_gain, output_offset = fit_affine(
        shifted_output[pre_indices], transition_output[pre_indices]
    )
    fitted_output = shifted_output * output_gain + output_offset
    input_residual = transition_input[pre_indices] - fitted_input[pre_indices]
    correlation = float(
        np.corrcoef(
            transition_input[pre_indices], fitted_input[pre_indices]
        )[0, 1]
    )
    metrics: dict[str, float | bool] = {
        "lag_samples": lag,
        "raw_correlation": raw_correlation,
        "input_gain": input_gain,
        "input_offset_full_scale": input_offset,
        "output_gain": output_gain,
        "output_offset_full_scale": output_offset,
        "input_correlation": correlation,
        "input_residual_rms_dbfs": db20(
            float(np.sqrt(np.mean(input_residual * input_residual)))
        ),
        "matched_input_valid": bool(math.isfinite(correlation) and correlation >= 0.95),
    }
    return fitted_output, metrics


def hf_energy(signal: np.ndarray) -> float:
    if signal.size != WINDOW_LEN:
        raise ValueError("HF energy requires a 512-sample window")
    window = np.hanning(WINDOW_LEN)
    spectrum = np.fft.rfft(signal * window)
    frequencies = np.fft.rfftfreq(WINDOW_LEN, 1.0 / SAMPLE_RATE)
    selected = (frequencies >= 4000.0) & (frequencies <= 20000.0)
    return float(
        2.0 * np.sum(np.abs(spectrum[selected]) ** 2)
        / (WINDOW_LEN * np.sum(window * window))
    )


def window_metrics(signal: np.ndarray, start: int, end: int) -> dict[str, float]:
    segment = signal[start:end]
    first = np.diff(segment)
    second = np.diff(first)
    return {
        "start_sample": start,
        "end_sample": end,
        "residual_peak_dbfs": db20(float(np.max(np.abs(segment)))),
        "residual_rms_dbfs": db20(float(np.sqrt(np.mean(segment * segment)))),
        "first_difference_peak_dbfs": db20(float(np.max(np.abs(first)))),
        "second_difference_peak_dbfs": db20(float(np.max(np.abs(second)))),
        "hf_4k_20k_rms_dbfs": db20(math.sqrt(hf_energy(segment))),
    }


def repeated_frame_count(samples: np.ndarray) -> int:
    frames = samples.reshape(12, FRAME_LEN)
    return sum(np.array_equal(frames[index - 1], frames[index]) for index in range(1, 12))


def transition_metrics(
    label: str,
    stable_input: np.ndarray,
    stable_output: np.ndarray,
    transition_input: np.ndarray,
    transition_output: np.ndarray,
) -> dict[str, object]:
    aligned_output, alignment = align_baseline(
        stable_input, stable_output, transition_input, transition_output
    )
    residual = transition_output - aligned_output
    finite = np.isfinite(residual)
    valid_region = finite & (np.arange(residual.size) >= T0 - 1024) & (
        np.arange(residual.size) < T1 + 1024
    )
    selected = residual[valid_region]
    first = np.diff(selected)
    second = np.diff(first)

    windows = {
        "pre": window_metrics(residual, T0 - 1024, T0 - 512),
        "start": window_metrics(residual, T0 - 256, T0 + 256),
        "internal": window_metrics(residual, T0 + 1744, T0 + 2256),
        "end": window_metrics(residual, T1 - 256, T1 + 256),
        "post": window_metrics(residual, T1 + 512, T1 + 1024),
    }
    internal_energies = []
    start = T0 + 512
    while start + WINDOW_LEN <= T1 - 512:
        internal_energies.append(hf_energy(residual[start : start + WINDOW_LEN]))
        start += 256
    internal_hf = float(np.median(internal_energies))
    boundary_hf = max(
        hf_energy(residual[T0 - 256 : T0 + 256]),
        hf_energy(residual[T1 - 256 : T1 + 256]),
    )
    boundary_ratio = boundary_hf / max(internal_hf, 1.0e-30)

    frame_steps = [
        float(abs(transition_output[index] - transition_output[index - 1]))
        for index in range(FRAME_LEN, transition_output.size, FRAME_LEN)
    ]
    clip_count = int(
        np.count_nonzero(
            (transition_output >= 32767.0 / 32768.0)
            | (transition_output <= -1.0)
        )
    )
    repeated = repeated_frame_count(transition_output)
    dc_start = float(
        np.mean(residual[T0 : T0 + 256])
        - np.mean(residual[T0 - 256 : T0])
    )
    dc_end = float(
        np.mean(residual[T1 : T1 + 256])
        - np.mean(residual[T1 - 256 : T1])
    )
    metrics: dict[str, object] = {
        "label": label,
        "evidence_label": "BOARD_INTERNAL_PCM_OBJECTIVE_TRANSIENT",
        "alignment": alignment,
        "residual_peak_dbfs": db20(float(np.max(np.abs(selected)))),
        "residual_rms_dbfs": db20(float(np.sqrt(np.mean(selected * selected)))),
        "first_difference_peak_dbfs": db20(float(np.max(np.abs(first)))),
        "second_difference_peak_dbfs": db20(float(np.max(np.abs(second)))),
        "windows": windows,
        "hf_4k_20k_internal_median_rms_dbfs": db20(math.sqrt(internal_hf)),
        "hf_4k_20k_boundary_max_rms_dbfs": db20(math.sqrt(boundary_hf)),
        "hf_boundary_to_internal_ratio": boundary_ratio,
        "hf_boundary_to_internal_ratio_db": 10.0 * math.log10(max(boundary_ratio, 1.0e-30)),
        "repeated_adjacent_frame_count": repeated,
        "clipped_sample_count": clip_count,
        "dc_step_start_full_scale": dc_start,
        "dc_step_end_full_scale": dc_end,
        "frame_boundary_discontinuity_max_full_scale": max(frame_steps),
        "frame_boundary_discontinuities_full_scale": frame_steps,
        "transition_start_discontinuity_full_scale": float(
            abs(transition_output[T0] - transition_output[T0 - 1])
        ),
        "transition_end_discontinuity_full_scale": float(
            abs(transition_output[T1] - transition_output[T1 - 1])
        ),
        "finite": bool(np.all(finite[T0 - 1024 : T1 + 1024])),
    }
    metrics["integrity_pass"] = bool(
        metrics["finite"]
        and alignment["matched_input_valid"]
        and clip_count == 0
        and repeated == 0
    )
    return metrics


def analyze(result_dir: Path) -> dict[str, object]:
    board_path = result_dir / "dynamic_clarity_transition_board.json"
    board = json.loads(board_path.read_text(encoding="utf-8"))
    if not board.get("pass"):
        raise ValueError(f"board transition capture failed: {board.get('error')}")
    captures = {entry["label"]: entry for entry in board["captures"]}
    results = []
    for prefix in ("dual", "music", "noise"):
        loaded: dict[str, tuple[np.ndarray, np.ndarray]] = {}
        for suffix in (
            "stable_0",
            "transition_0_to_1",
            "stable_1",
            "transition_1_to_2",
            "transition_1_to_0",
        ):
            label = f"{prefix}_{suffix}"
            entry = captures[label]
            pre_write = int(entry["pre_write_index"])
            loaded[suffix] = (
                read_pcm16(Path(entry["input"]), pre_write),
                read_pcm16(Path(entry["output"]), pre_write),
            )
        for transition, baseline in (
            ("transition_0_to_1", "stable_0"),
            ("transition_1_to_2", "stable_1"),
            ("transition_1_to_0", "stable_1"),
        ):
            stable_input, stable_output = loaded[baseline]
            transition_input, transition_output = loaded[transition]
            results.append(
                transition_metrics(
                    f"{prefix}_{transition}",
                    stable_input,
                    stable_output,
                    transition_input,
                    transition_output,
                )
            )

    report = {
        "evidence_label": "BOARD_INTERNAL_PCM_OBJECTIVE_TRANSIENT",
        "pass": all(bool(result["integrity_pass"]) for result in results),
        "analyzer_version": SCRIPT_VERSION,
        "sample_rate_hz": SAMPLE_RATE,
        "transition_start_sample": T0,
        "transition_end_sample": T1,
        "window_samples": WINDOW_LEN,
        "board": board,
        "transitions": results,
        "acceptance_scope": (
            "digital integrity and relative boundary metrics only; "
            "no perceptual threshold is applied"
        ),
        "subjective_observation": "NOT_PERFORMED",
        "external_analog_metrics": "UNMEASURED",
    }
    return report


def write_csv(path: Path, transitions: list[dict[str, object]]) -> None:
    columns = (
        "label",
        "residual_peak_dbfs",
        "residual_rms_dbfs",
        "first_difference_peak_dbfs",
        "second_difference_peak_dbfs",
        "hf_4k_20k_internal_median_rms_dbfs",
        "hf_4k_20k_boundary_max_rms_dbfs",
        "hf_boundary_to_internal_ratio",
        "hf_boundary_to_internal_ratio_db",
        "repeated_adjacent_frame_count",
        "clipped_sample_count",
        "dc_step_start_full_scale",
        "dc_step_end_full_scale",
        "frame_boundary_discontinuity_max_full_scale",
        "transition_start_discontinuity_full_scale",
        "transition_end_discontinuity_full_scale",
        "integrity_pass",
    )
    with path.open("w", newline="", encoding="ascii") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        for transition in transitions:
            writer.writerow({name: transition[name] for name in columns})


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_dir", type=Path)
    args = parser.parse_args()
    result_dir = args.result_dir.resolve()
    report = analyze(result_dir)
    json_path = result_dir / "dynamic_clarity_transition_artifact.json"
    csv_path = result_dir / "dynamic_clarity_transition_artifact.csv"
    json_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=True) + "\n",
        encoding="ascii",
    )
    write_csv(csv_path, report["transitions"])
    if not report["pass"]:
        raise SystemExit("transition capture integrity checks failed")
    print(json.dumps({"pass": True, "json": str(json_path), "csv": str(csv_path)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
