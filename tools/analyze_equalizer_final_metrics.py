#!/usr/bin/env python3
"""Analyze Project 3.3 C6748 internal-digital metric captures."""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path
import sys
from typing import Iterable, Sequence

import numpy as np

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))
import equalizer_33_response_report as model


SAMPLE_RATE = 50000.0
FRAME_LEN = 1024
CAPTURE_SAMPLES = 8192
PREROLL_SAMPLES = 4 * FRAME_LEN
IMPULSE_PEAK = 16384.0
THD_LIMIT_DB = -60.0
MAGNITUDE_ERROR_LIMIT_DB = 0.15
CASE_NAMES = ("FLAT", "BASS", "VOCAL", "TREBLE", "V_SHAPE",
              "CUSTOM_STRESS")
MODEL_NAMES = ("flat", "bass", "vocal", "treble", "v_shape", "custom")
THD_FREQUENCIES_HZ = (97.65625, 390.625, 976.5625,
                      4003.90625, 8007.8125, 15966.796875)
SIGNAL_NAMES = ("deterministic_broadband_noise", "music_like", "multitone")


def _write_csv(path: Path, header: Sequence[str],
               rows: Iterable[Sequence[object]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(header)
        writer.writerows(rows)


def _write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=True,
                               allow_nan=False) + "\n", encoding="utf-8")


def unpack_pcm16(path: Path, expected_words: int) -> np.ndarray:
    words = np.fromfile(path, dtype="<u4")
    if words.size != expected_words:
        raise ValueError(f"{path.name}: expected {expected_words} words, "
                         f"got {words.size}")
    return np.frombuffer(words.tobytes(), dtype="<i2").copy()


def _db(values: np.ndarray) -> np.ndarray:
    return 20.0 * np.log10(np.maximum(np.abs(values), 1.0e-20))


def _percentile(values: np.ndarray, percentile: float) -> float:
    finite = values[np.isfinite(values)]
    if finite.size == 0:
        raise ValueError("metric has no finite points")
    return float(np.percentile(finite, percentile))


def _wrapped_phase_error(left: np.ndarray, right: np.ndarray) -> np.ndarray:
    return np.angle(np.exp(1j * (left - right)))


def analyze_response(response: np.ndarray,
                     clip_counts: Sequence[int], output_dir: Path) -> dict:
    frequencies = np.fft.rfftfreq(CAPTURE_SAMPLES, 1.0 / SAMPLE_RATE)
    omega = 2.0 * math.pi * frequencies / SAMPLE_RATE
    report_mask = (frequencies >= 20.0) & (frequencies <= 20000.0)
    magnitude_mask = (frequencies >= 20.0) & (frequencies <= 16000.0)
    response_rows: list[tuple[object, ...]] = []
    group_rows: list[tuple[object, ...]] = []
    case_summaries: list[dict] = []
    all_magnitude_errors: list[float] = []
    all_group_errors: list[float] = []

    for case_index, (case_name, model_name) in enumerate(
            zip(CASE_NAMES, MODEL_NAMES)):
        impulse = response[case_index].astype(np.float64)
        board = np.fft.rfft(impulse) / IMPULSE_PEAK
        gains = model.PRESETS[model_name]
        bank = model.design_bank(gains)
        _, preamp_db, preamp_gain = model.headroom(bank, gains)
        host = np.asarray([
            model.cascade_response(bank, float(frequency)) * preamp_gain
            for frequency in frequencies
        ], dtype=np.complex128)

        board_magnitude = _db(board)
        host_magnitude = _db(host)
        magnitude_error = board_magnitude - host_magnitude
        board_phase = np.unwrap(np.angle(board))
        host_phase = np.unwrap(np.angle(host))
        phase_error = _wrapped_phase_error(np.angle(board), np.angle(host))
        board_delay = -np.gradient(board_phase, omega, edge_order=2)
        host_delay = -np.gradient(host_phase, omega, edge_order=2)
        group_error = board_delay - host_delay
        valid = ((frequencies >= 30.0) & (frequencies <= 18000.0) &
                 (board_magnitude > -60.0) & (host_magnitude > -60.0) &
                 np.isfinite(board_delay) & np.isfinite(host_delay))

        for point in np.flatnonzero(report_mask):
            response_rows.append((
                case_name, point, frequencies[point],
                board_magnitude[point], host_magnitude[point],
                magnitude_error[point], board_phase[point], host_phase[point],
                phase_error[point], "BOARD_INTERNAL_DIGITAL", "HOST_MODEL"))
            group_rows.append((
                case_name, point, frequencies[point], board_delay[point],
                host_delay[point], group_error[point], int(valid[point])))

        case_magnitude_errors = np.abs(magnitude_error[magnitude_mask])
        case_group_errors = np.abs(group_error[valid])
        if case_group_errors.size == 0:
            raise ValueError(f"{case_name}: no valid group-delay points")
        all_magnitude_errors.extend(case_magnitude_errors.tolist())
        all_group_errors.extend(case_group_errors.tolist())
        flat_exact = (case_name != "FLAT" or
                      (impulse[0] == IMPULSE_PEAK and
                       np.count_nonzero(impulse[1:]) == 0))
        case_summaries.append({
            "case": case_name,
            "preamp_db": float(preamp_db),
            "clip_count": int(clip_counts[case_index]),
            "flat_impulse_byte_exact": bool(flat_exact),
            "magnitude_error_p95_db": _percentile(case_magnitude_errors, 95.0),
            "magnitude_error_max_db": float(np.max(case_magnitude_errors)),
            "phase_error_max_rad": float(np.max(np.abs(phase_error[valid]))),
            "group_delay_error_p95_samples": _percentile(case_group_errors, 95.0),
            "group_delay_error_max_samples": float(np.max(case_group_errors)),
            "group_delay_valid_point_count": int(np.count_nonzero(valid)),
        })

    _write_csv(output_dir / "static_eq_frequency_response.csv",
               ("case", "point", "frequency_hz", "board_magnitude_db",
                "host_model_magnitude_db", "magnitude_error_db",
                "board_phase_rad", "host_model_phase_rad", "phase_error_rad",
                "board_evidence_class", "reference_evidence_class"),
               response_rows)
    _write_csv(output_dir / "static_eq_group_delay.csv",
               ("case", "point", "frequency_hz", "board_group_delay_samples",
                "host_model_group_delay_samples", "group_delay_error_samples",
                "valid"), group_rows)

    maximum_magnitude_error = max(all_magnitude_errors)
    summary = {
        "pass": bool(maximum_magnitude_error <= MAGNITUDE_ERROR_LIMIT_DB and
                     sum(int(value) for value in clip_counts) == 0 and
                     all(item["flat_impulse_byte_exact"]
                         for item in case_summaries)),
        "evidence_class": "BOARD_INTERNAL_DIGITAL",
        "reference_class": "HOST_MODEL",
        "magnitude_error_limit_db": MAGNITUDE_ERROR_LIMIT_DB,
        "magnitude_error_p95_db": float(np.percentile(all_magnitude_errors, 95.0)),
        "magnitude_error_max_db": float(maximum_magnitude_error),
        "group_delay_error_p95_samples": float(
            np.percentile(all_group_errors, 95.0)),
        "group_delay_error_max_samples": float(max(all_group_errors)),
        "group_delay_has_no_fabricated_limit": True,
        "cases": case_summaries,
    }
    _write_json(output_dir / "static_eq_response_summary.json", summary)
    return summary


def _thd_db(samples: np.ndarray, expected_frequency: float) -> tuple:
    spectrum = np.fft.rfft(samples.astype(np.float64))
    expected_bin = int(round(expected_frequency * samples.size / SAMPLE_RATE))
    low = max(1, expected_bin - 1)
    high = min(spectrum.size - 1, expected_bin + 1)
    fundamental_bin = low + int(np.argmax(np.abs(spectrum[low:high + 1])))
    fundamental = abs(spectrum[fundamental_bin])
    harmonic_power = 0.0
    harmonic_count = 0
    for harmonic in range(2, 21):
        harmonic_bin = fundamental_bin * harmonic
        if harmonic_bin >= spectrum.size:
            break
        harmonic_power += abs(spectrum[harmonic_bin]) ** 2
        harmonic_count += 1
    if fundamental <= 0.0:
        raise ValueError("THD spectrum has no usable fundamental")
    actual_frequency = fundamental_bin * SAMPLE_RATE / samples.size
    if harmonic_count == 0:
        return (None, None, fundamental_bin, actual_frequency, 0)
    ratio = math.sqrt(harmonic_power) / fundamental
    if ratio <= 0.0 or not math.isfinite(ratio):
        raise ValueError("THD ratio is not finite and positive")
    return (20.0 * math.log10(ratio), ratio, fundamental_bin,
            actual_frequency, harmonic_count)


def analyze_thd(inputs: np.ndarray, outputs: np.ndarray,
                clip_counts: Sequence[int], output_dir: Path) -> dict:
    rows: list[tuple[object, ...]] = []
    case_results: list[dict] = []
    failures = 0
    not_measurable = 0
    output_values: list[float] = []
    increase_values: list[float] = []
    for frequency_index, expected_frequency in enumerate(THD_FREQUENCIES_HZ):
        input_result = _thd_db(inputs[frequency_index], expected_frequency)
        for case_index, case_name in enumerate(CASE_NAMES):
            output_result = _thd_db(outputs[case_index, frequency_index],
                                    expected_frequency)
            clip_count = int(clip_counts[
                case_index * len(THD_FREQUENCIES_HZ) + frequency_index])
            coherent = (input_result[2] == output_result[2] and
                        abs(input_result[3] - expected_frequency) < 1.0e-9)
            measurable = (input_result[4] > 0 and output_result[4] > 0)
            if measurable:
                increase_db = output_result[0] - input_result[0]
                passed = (coherent and clip_count == 0 and
                          math.isfinite(output_result[0]) and
                          output_result[0] <= THD_LIMIT_DB)
                failures += int(not passed)
                output_values.append(output_result[0])
                increase_values.append(increase_db)
                measurement_status = "MEASURED"
                pass_text = "PASS" if passed else "FAIL"
            else:
                increase_db = "NOT_MEASURABLE"
                passed = None
                not_measurable += 1
                measurement_status = "NOT_MEASURABLE_NO_IN_BAND_HARMONICS"
                pass_text = "NOT_APPLICABLE"
            rows.append((
                case_name, expected_frequency, output_result[3],
                output_result[2], int(coherent), "rectangular_coherent",
                input_result[0], output_result[0], increase_db, THD_LIMIT_DB,
                output_result[4], clip_count, measurement_status, pass_text))
            case_results.append({
                "case": case_name,
                "frequency_hz": expected_frequency,
                "input_thd_db": input_result[0],
                "output_thd_db": output_result[0],
                "thd_increase_db": increase_db,
                "clip_count": clip_count,
                "measurement_status": measurement_status,
                "pass": passed,
            })

    _write_csv(output_dir / "static_eq_thd_cases.csv",
               ("case", "requested_frequency_hz", "actual_frequency_hz",
                "fundamental_bin", "coherent", "window", "input_thd_db",
                "output_thd_db", "thd_increase_db", "thd_limit_db",
                "harmonic_count", "clip_count", "measurement_status", "pass"),
               rows)
    if not output_values:
        raise ValueError("THD suite produced no measurable cases")
    total_clip_count = sum(int(value) for value in clip_counts)
    summary = {
        "pass": failures == 0 and total_clip_count == 0,
        "evidence_label": "BOARD_INTERNAL_DIGITAL_NO_DAC_ANALOG_THD",
        "case_count": len(rows),
        "measured_case_count": len(rows) - not_measurable,
        "not_measurable_case_count": not_measurable,
        "not_measurable_reason": (
            "At 15.966796875 kHz, 2*f exceeds the 25 kHz Nyquist frequency; "
            "no 2nd-20th in-band harmonic exists, so THD is not fabricated."),
        "failure_count": failures,
        "thd_limit_db": THD_LIMIT_DB,
        "output_thd_worst_db": max(output_values),
        "output_thd_median_db": float(np.median(output_values)),
        "thd_increase_max_db": max(increase_values),
        "normal_mode_clip_count": total_clip_count,
        "external_analog_thd": "NOT_MEASURED",
        "cases": case_results,
    }
    _write_json(output_dir / "static_eq_thd_summary.json", summary)
    return summary


def _round_pcm16(values: np.ndarray) -> np.ndarray:
    clipped = np.clip(values, -32768.0, 32767.0)
    rounded = np.where(clipped >= 0.0, np.floor(clipped + 0.5),
                       np.ceil(clipped - 0.5))
    return rounded.astype(np.int16)


def high_precision_reference(samples: np.ndarray, model_name: str) -> np.ndarray:
    if model_name == "flat":
        return samples.copy()
    gains = model.PRESETS[model_name]
    bank = model.design_bank(gains)
    _, _, preamp_gain = model.headroom(bank, gains)
    state1 = np.zeros(len(bank), dtype=np.float64)
    state2 = np.zeros(len(bank), dtype=np.float64)
    output = np.empty(samples.size, dtype=np.float64)
    for sample_index, sample in enumerate(samples):
        value = float(sample) * float(preamp_gain)
        for section, coeff in enumerate(bank):
            b0, b1, b2, a1, a2 = (float(item) for item in coeff)
            section_output = b0 * value + state1[section]
            state1[section] = b1 * value - a1 * section_output + state2[section]
            state2[section] = b2 * value - a2 * section_output
            value = section_output
        output[sample_index] = value
    return _round_pcm16(output)


def align_reference(reference: np.ndarray, board: np.ndarray,
                    maximum_delay: int = 32) -> tuple[int, np.ndarray, np.ndarray, float]:
    candidates: list[tuple[float, int, np.ndarray, np.ndarray]] = []
    reference_float = reference.astype(np.float64)
    board_float = board.astype(np.float64)
    for delay in range(-maximum_delay, maximum_delay + 1):
        if delay > 0:
            left, right = reference_float[:-delay], board_float[delay:]
        elif delay < 0:
            left, right = reference_float[-delay:], board_float[:delay]
        else:
            left, right = reference_float, board_float
        denominator = math.sqrt(float(np.dot(left, left) * np.dot(right, right)))
        correlation = float(np.dot(left, right) / denominator) if denominator else 0.0
        candidates.append((correlation, delay, left, right))
    correlation, delay, left, right = max(
        candidates, key=lambda item: (item[0], -abs(item[1])))
    return delay, left, right, correlation


def analyze_reference_snr(inputs: np.ndarray, outputs: np.ndarray,
                          clip_counts: Sequence[int], output_dir: Path) -> dict:
    rows: list[tuple[object, ...]] = []
    records: list[dict] = []
    failures = 0
    for case_index, (case_name, model_name) in enumerate(
            zip(CASE_NAMES, MODEL_NAMES)):
        for signal_index, signal_name in enumerate(SIGNAL_NAMES):
            full_reference = high_precision_reference(inputs[signal_index], model_name)
            reference = full_reference[PREROLL_SAMPLES:]
            board = outputs[case_index, signal_index]
            delay, aligned_reference, aligned_board, correlation = align_reference(
                reference, board)
            error = aligned_board - aligned_reference
            signal_energy = float(np.dot(aligned_reference, aligned_reference))
            error_energy = float(np.dot(error, error))
            byte_exact = error_energy == 0.0
            snr_db = None if byte_exact else 10.0 * math.log10(
                signal_energy / error_energy)
            board_energy = float(np.dot(aligned_board, aligned_board))
            energy_ratio = board_energy / signal_energy if signal_energy else 0.0
            max_abs_error = int(np.max(np.abs(error)))
            clip_count = int(clip_counts[
                case_index * len(SIGNAL_NAMES) + signal_index])
            flat_contract = (case_name != "FLAT" or
                             (byte_exact and delay == 0))
            passed = (flat_contract and clip_count == 0 and
                      math.isfinite(correlation) and correlation > 0.99 and
                      math.isfinite(energy_ratio))
            failures += int(not passed)
            snr_text: object = "BYTE_EXACT" if byte_exact else snr_db
            rows.append((
                case_name, signal_name, delay, snr_text, correlation,
                energy_ratio, max_abs_error, clip_count,
                "BYTE_EXACT" if byte_exact else "FINITE_ERROR",
                "PASS" if passed else "FAIL"))
            records.append({
                "case": case_name,
                "signal": signal_name,
                "aligned_delay_samples": delay,
                "reference_snr_db": "BYTE_EXACT" if byte_exact else float(snr_db),
                "correlation": correlation,
                "energy_ratio": energy_ratio,
                "max_abs_error": max_abs_error,
                "clipped_samples": clip_count,
                "pass": passed,
            })

    _write_csv(output_dir / "static_eq_reference_snr.csv",
               ("case", "signal", "aligned_delay_samples", "reference_snr_db",
                "correlation", "energy_ratio", "max_abs_error",
                "clipped_samples", "comparison_status", "pass"), rows)
    finite_snr = [record["reference_snr_db"] for record in records
                  if isinstance(record["reference_snr_db"], float)]
    summary = {
        "pass": failures == 0,
        "evidence_class": "BOARD_INTERNAL_DIGITAL",
        "reference_class": "HOST_MODEL_HIGH_PRECISION_PCM16",
        "case_count": len(records),
        "failure_count": failures,
        "minimum_finite_reference_snr_db": (
            min(finite_snr) if finite_snr else "ALL_BYTE_EXACT"),
        "flat_all_byte_exact": all(
            record["reference_snr_db"] == "BYTE_EXACT"
            for record in records if record["case"] == "FLAT"),
        "external_analog_snr": "NOT_MEASURED",
        "cases": records,
    }
    _write_json(output_dir / "static_eq_reference_snr_summary.json", summary)
    return summary


def analyze(raw_dir: Path, output_dir: Path) -> dict:
    output_dir.mkdir(parents=True, exist_ok=True)
    dss = json.loads((raw_dir / "dss_summary.json").read_text(encoding="utf-8"))
    if not dss.get("pass"):
        raise ValueError(f"DSS capture failed: {dss}")
    response = unpack_pcm16(
        raw_dir / "response_packed.raw", len(CASE_NAMES) * CAPTURE_SAMPLES // 2
    ).reshape(len(CASE_NAMES), CAPTURE_SAMPLES)
    thd_inputs = unpack_pcm16(
        raw_dir / "thd_input_packed.raw",
        len(THD_FREQUENCIES_HZ) * CAPTURE_SAMPLES // 2
    ).reshape(len(THD_FREQUENCIES_HZ), CAPTURE_SAMPLES)
    thd_outputs = unpack_pcm16(
        raw_dir / "thd_output_packed.raw",
        len(CASE_NAMES) * len(THD_FREQUENCIES_HZ) * CAPTURE_SAMPLES // 2
    ).reshape(len(CASE_NAMES), len(THD_FREQUENCIES_HZ), CAPTURE_SAMPLES)
    snr_inputs = unpack_pcm16(
        raw_dir / "snr_input_packed.raw",
        len(SIGNAL_NAMES) * (PREROLL_SAMPLES + CAPTURE_SAMPLES) // 2
    ).reshape(len(SIGNAL_NAMES), PREROLL_SAMPLES + CAPTURE_SAMPLES)
    snr_outputs = unpack_pcm16(
        raw_dir / "snr_output_packed.raw",
        len(CASE_NAMES) * len(SIGNAL_NAMES) * CAPTURE_SAMPLES // 2
    ).reshape(len(CASE_NAMES), len(SIGNAL_NAMES), CAPTURE_SAMPLES)

    response_summary = analyze_response(
        response, dss["response_clip_count"], output_dir)
    thd_summary = analyze_thd(
        thd_inputs, thd_outputs, dss["thd_clip_count"], output_dir)
    snr_summary = analyze_reference_snr(
        snr_inputs, snr_outputs, dss["snr_clip_count"], output_dir)
    summary = {
        "pass": bool(response_summary["pass"] and thd_summary["pass"] and
                     snr_summary["pass"]),
        "commit_sha": dss["commit_sha"],
        "output_sha256": dss["output_sha256"],
        "evidence_classes": ["HOST_MODEL", "BOARD_INTERNAL_DIGITAL"],
        "response": response_summary,
        "thd": thd_summary,
        "reference_snr": snr_summary,
        "not_measured": {
            "EXTERNAL_ANALOG_THD": "NOT_MEASURED",
            "EXTERNAL_ANALOG_SNR": "NOT_MEASURED",
            "EXTERNAL_ANALOG_FREQUENCY_RESPONSE": "NOT_MEASURED",
            "SPL": "NOT_MEASURED",
            "ADC_ENOB": "NOT_MEASURED",
        },
    }
    _write_json(output_dir / "final_metrics_analysis_summary.json", summary)
    return summary


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    arguments = parser.parse_args()
    summary = analyze(arguments.raw_dir.resolve(), arguments.output_dir.resolve())
    print(json.dumps({
        "pass": summary["pass"],
        "magnitude_error_max_db": summary["response"]["magnitude_error_max_db"],
        "group_delay_error_max_samples":
            summary["response"]["group_delay_error_max_samples"],
        "output_thd_worst_db": summary["thd"]["output_thd_worst_db"],
        "minimum_finite_reference_snr_db":
            summary["reference_snr"]["minimum_finite_reference_snr_db"],
    }, allow_nan=False))
    return 0 if summary["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
