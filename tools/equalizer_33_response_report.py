#!/usr/bin/env python3
"""Independent Host reference and report generator for Project 3.3."""

from __future__ import annotations

import argparse
import cmath
import csv
import json
import math
import struct
from pathlib import Path
from typing import Iterable, Sequence

from PIL import Image, ImageDraw
import numpy as np


SAMPLE_RATE = 50000.0
CENTERS = (31.25, 62.5, 125.0, 250.0, 500.0,
           1000.0, 2000.0, 4000.0, 8000.0, 16000.0)
PRESETS = {
    "flat": (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
    "bass": (3.0, 3.0, 2.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
    "vocal": (-2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 2.0, 1.0, 0.0, -1.0),
    "treble": (-1.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 3.0),
    "v_shape": (2.0, 2.0, 1.0, 0.0, -1.0, -1.0, 0.0, 1.0, 2.0, 2.0),
    "custom": (-3.0, -1.0, 1.0, 3.0, 4.0, 2.0, 0.0, -2.0, 1.0, 3.0),
}

Coeff = tuple[float, float, float, float, float]


def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]


def log_grid(count: int, low: float = 20.0,
             high: float = 20000.0) -> list[float]:
    return [math.exp(math.log(low) +
                     (math.log(high) - math.log(low)) * index / (count - 1))
            for index in range(count)]


def _normalize(values: Sequence[float], a0: float) -> Coeff:
    return tuple(f32(value / a0) for value in values)  # type: ignore[return-value]


def rbj_peaking(frequency_hz: float, gain_db: float) -> Coeff:
    if abs(gain_db) < 1.0e-7:
        return (1.0, 0.0, 0.0, 0.0, 0.0)
    f = np.float32
    amplitude = f(np.power(f(10.0), f(gain_db) / f(40.0)))
    omega = f(f(f(2.0) * f(math.pi)) * f(frequency_hz) / f(SAMPLE_RATE))
    sine = f(np.sin(omega))
    cosine = f(np.cos(omega))
    alpha = f(sine * f(np.sinh(
        f(f(f(math.log(2.0)) * f(0.5)) * omega / sine))))
    a0 = f(f(1.0) + f(alpha / amplitude))
    return _normalize((f(f(1.0) + f(alpha * amplitude)),
                       f(f(-2.0) * cosine),
                       f(f(1.0) - f(alpha * amplitude)),
                       f(f(-2.0) * cosine),
                       f(f(1.0) - f(alpha / amplitude))), a0)


def rbj_shelf(frequency_hz: float, gain_db: float,
              high_shelf: bool) -> Coeff:
    if abs(gain_db) < 1.0e-7:
        return (1.0, 0.0, 0.0, 0.0, 0.0)
    f = np.float32
    amplitude = f(np.power(f(10.0), f(gain_db) / f(40.0)))
    omega = f(f(f(2.0) * f(math.pi)) * f(frequency_hz) / f(SAMPLE_RATE))
    sine = f(np.sin(omega))
    cosine = f(np.cos(omega))
    alpha = f(f(sine * f(0.5)) * f(np.sqrt(f(2.0))))
    cross = f(f(f(2.0) * f(np.sqrt(amplitude))) * alpha)
    if not high_shelf:
        values = (
            f(amplitude * f(f(f(amplitude + f(1.0)) -
                              f(f(amplitude - f(1.0)) * cosine)) + cross)),
            f(f(f(2.0) * amplitude) *
              f(f(amplitude - f(1.0)) -
                f(f(amplitude + f(1.0)) * cosine))),
            f(amplitude * f(f(f(amplitude + f(1.0)) -
                              f(f(amplitude - f(1.0)) * cosine)) - cross)),
            f(f(-2.0) * f(f(amplitude - f(1.0)) +
                           f(f(amplitude + f(1.0)) * cosine))),
            f(f(f(amplitude + f(1.0)) +
                f(f(amplitude - f(1.0)) * cosine)) - cross),
        )
        a0 = f(f(f(amplitude + f(1.0)) +
                  f(f(amplitude - f(1.0)) * cosine)) + cross)
    else:
        values = (
            f(amplitude * f(f(f(amplitude + f(1.0)) +
                              f(f(amplitude - f(1.0)) * cosine)) + cross)),
            f(f(f(-2.0) * amplitude) *
              f(f(amplitude - f(1.0)) +
                f(f(amplitude + f(1.0)) * cosine))),
            f(amplitude * f(f(f(amplitude + f(1.0)) +
                              f(f(amplitude - f(1.0)) * cosine)) - cross)),
            f(f(2.0) * f(f(amplitude - f(1.0)) -
                          f(f(amplitude + f(1.0)) * cosine))),
            f(f(f(amplitude + f(1.0)) -
                f(f(amplitude - f(1.0)) * cosine)) - cross),
        )
        a0 = f(f(f(amplitude + f(1.0)) -
                  f(f(amplitude - f(1.0)) * cosine)) + cross)
    return _normalize(values, a0)


def design_bank(gains_db: Sequence[float]) -> list[Coeff]:
    sections: list[Coeff] = []
    for index, (center, gain) in enumerate(zip(CENTERS, gains_db)):
        if index == 0:
            sections.append(rbj_shelf(center, gain, False))
        elif index == len(CENTERS) - 1:
            sections.append(rbj_shelf(center, gain, True))
        else:
            sections.append(rbj_peaking(center, gain))
    return sections


def section_response(coeff: Coeff, frequency_hz: float) -> complex:
    b0, b1, b2, a1, a2 = coeff
    omega = 2.0 * math.pi * frequency_hz / SAMPLE_RATE
    z1 = cmath.exp(-1j * omega)
    z2 = cmath.exp(-2j * omega)
    return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2)


def cascade_response(bank: Sequence[Coeff], frequency_hz: float) -> complex:
    result = 1.0 + 0.0j
    for coeff in bank:
        result *= section_response(coeff, frequency_hz)
    return result


def magnitude_db(value: complex) -> float:
    return 20.0 * math.log10(max(abs(value), 1.0e-20))


def headroom(bank: Sequence[Coeff]) -> tuple[float, float, float]:
    log_min = f32(math.log(20.0))
    log_max = f32(math.log(20000.0))
    step = f32(f32(log_max - log_min) / 511.0)
    peak_db = -120.0
    for index in range(512):
        frequency = f32(math.exp(f32(log_min + f32(step * index))))
        magnitude = f32(max(abs(cascade_response(bank, frequency)), 1.0e-20))
        value_db = f32(f32(20.0) * f32(math.log10(magnitude)))
        peak_db = max(peak_db, value_db)
    preamp_db = f32(f32(-peak_db) - 0.5) if peak_db > 0.0 else -0.5
    return peak_db, preamp_db, f32(10.0 ** f32(preamp_db / 20.0))


def desired_visual(gains_db: Sequence[float], frequency_hz: float) -> float:
    if frequency_hz <= CENTERS[0]:
        return gains_db[0]
    if frequency_hz >= CENTERS[-1]:
        return gains_db[-1]
    log_frequency = math.log2(frequency_hz)
    for band in range(len(CENTERS) - 1):
        if frequency_hz <= CENTERS[band + 1]:
            left = math.log2(CENTERS[band])
            right = math.log2(CENTERS[band + 1])
            mix = (log_frequency - left) / (right - left)
            return gains_db[band] + mix * (gains_db[band + 1] - gains_db[band])
    raise ValueError("frequency interpolation failed")


def unwrap_phase(values: Sequence[complex]) -> list[float]:
    phases = [cmath.phase(value) for value in values]
    result = [phases[0]]
    for phase in phases[1:]:
        while phase - result[-1] > math.pi:
            phase -= 2.0 * math.pi
        while phase - result[-1] < -math.pi:
            phase += 2.0 * math.pi
        result.append(phase)
    return result


def group_delay(frequencies: Sequence[float],
                responses: Sequence[complex]) -> list[float]:
    phases = unwrap_phase(responses)
    omega = [2.0 * math.pi * frequency / SAMPLE_RATE
             for frequency in frequencies]
    delay = [0.0] * len(frequencies)
    for index in range(1, len(frequencies) - 1):
        delay[index] = -(phases[index + 1] - phases[index - 1]) / (
            omega[index + 1] - omega[index - 1])
    delay[0] = delay[1]
    delay[-1] = delay[-2]
    return delay


def interaction_matrix(gain_db: float = 6.0) -> list[list[float]]:
    matrix: list[list[float]] = []
    for source in range(len(CENTERS)):
        gains = [0.0] * len(CENTERS)
        gains[source] = gain_db
        bank = design_bank(gains)
        matrix.append([magnitude_db(cascade_response(bank, center))
                       for center in CENTERS])
    return matrix


def _write_csv(path: Path, header: Sequence[str],
               rows: Iterable[Sequence[object]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(header)
        writer.writerows(rows)


def _line_plot(path: Path, title: str, frequencies: Sequence[float],
               series: Sequence[tuple[str, Sequence[float], tuple[int, int, int]]],
               y_min: float, y_max: float) -> None:
    image = Image.new("RGB", (1000, 620), "white")
    draw = ImageDraw.Draw(image)
    left, top, right, bottom = 75, 45, 970, 555
    draw.rectangle((left, top, right, bottom), outline=(30, 30, 30), width=2)
    for grid in range(6):
        y = top + (bottom - top) * grid / 5.0
        draw.line((left, y, right, y), fill=(220, 220, 220))
    draw.text((left, 15), title, fill=(0, 0, 0))
    log_low, log_high = math.log(20.0), math.log(20000.0)

    def point(frequency: float, value: float) -> tuple[float, float]:
        x = left + (math.log(frequency) - log_low) / (log_high - log_low) * (right - left)
        clipped = min(max(value, y_min), y_max)
        y = bottom - (clipped - y_min) / (y_max - y_min) * (bottom - top)
        return x, y

    for label, values, color in series:
        points = [point(frequency, value)
                  for frequency, value in zip(frequencies, values)]
        draw.line(points, fill=color, width=2)
        legend_y = 565 + 14 * list(series).index((label, values, color))
        draw.text((left, legend_y), label, fill=color)
    image.save(path)


def _heatmap(path: Path, matrix: Sequence[Sequence[float]]) -> None:
    image = Image.new("RGB", (720, 720), "white")
    draw = ImageDraw.Draw(image)
    margin, cell = 75, 58
    maximum = max(abs(value) for row in matrix for value in row) or 1.0
    draw.text((margin, 20), "10 x 10 RBJ interaction matrix (dB)", fill="black")
    for row_index, row in enumerate(matrix):
        for column_index, value in enumerate(row):
            normalized = min(abs(value) / maximum, 1.0)
            color = ((int(245 * (1.0 - normalized))),
                     int(245 * (1.0 - normalized)),
                     255 if value >= 0.0 else 80)
            x0 = margin + column_index * cell
            y0 = margin + row_index * cell
            draw.rectangle((x0, y0, x0 + cell, y0 + cell),
                           fill=color, outline=(80, 80, 80))
            draw.text((x0 + 5, y0 + 20), f"{value:.1f}", fill="black")
    image.save(path)


def compare_c_reference(directory: Path) -> dict[str, float]:
    response_rows = list(csv.DictReader(
        (directory / "c_response_512.csv").open(encoding="utf-8")))
    section_rows = list(csv.DictReader(
        (directory / "c_sections.csv").open(encoding="utf-8")))
    bank = design_bank(PRESETS["custom"])
    peak_db, preamp_db, _ = headroom(bank)
    max_shape = 0.0
    max_total = 0.0
    for row in response_rows:
        frequency = float(row["frequency_hz"])
        shape = magnitude_db(cascade_response(bank, frequency))
        total = shape + preamp_db
        max_shape = max(max_shape, abs(shape - float(row["shape_db"])))
        max_total = max(max_total, abs(total - float(row["total_db"])))
    max_builder_sync = 0.0
    max_python_c_coeff = 0.0
    for row, coeff in zip(section_rows, bank):
        for name, python_value in zip(("b0", "b1", "b2", "a1", "a2"), coeff):
            builder = float(row[f"builder_{name}"])
            synchronous = float(row[f"sync_{name}"])
            max_builder_sync = max(max_builder_sync, abs(builder - synchronous))
            max_python_c_coeff = max(max_python_c_coeff,
                                     abs(python_value - builder))
    first = section_rows[0]
    return {
        "c_python_shape_max_error_db": max_shape,
        "c_python_total_max_error_db": max_total,
        "builder_sync_coefficient_max_diff": max_builder_sync,
        "python_c_coefficient_max_diff": max_python_c_coeff,
        "builder_sync_peak_diff_db": abs(float(first["builder_peak_db"]) -
                                          float(first["sync_peak_db"])),
        "builder_sync_preamp_diff_db": abs(float(first["builder_preamp_db"]) -
                                            float(first["sync_preamp_db"])),
        "python_c_peak_diff_db": abs(peak_db - float(first["builder_peak_db"])),
        "python_c_preamp_diff_db": abs(preamp_db -
                                        float(first["builder_preamp_db"])),
    }


def generate_report(output_dir: Path,
                    c_reference_dir: Path | None = None) -> dict[str, float]:
    output_dir.mkdir(parents=True, exist_ok=True)
    frequencies = log_grid(129)
    response_rows: list[tuple[object, ...]] = []
    section_rows: list[tuple[object, ...]] = []
    group_rows: list[tuple[object, ...]] = []
    preset_results: dict[str, tuple[list[float], list[float], list[float]]] = {}
    for name, gains in PRESETS.items():
        bank = design_bank(gains)
        peak_db, preamp_db, preamp_gain = headroom(bank)
        responses = [cascade_response(bank, frequency)
                     for frequency in frequencies]
        shape = [magnitude_db(value) for value in responses]
        total = [value + preamp_db for value in shape]
        desired = [desired_visual(gains, frequency)
                   for frequency in frequencies]
        delays = group_delay(frequencies, responses)
        preset_results[name] = (desired, shape, total)
        for index, frequency in enumerate(frequencies):
            response_rows.append((name, index, frequency, desired[index],
                                  shape[index], total[index], peak_db,
                                  preamp_db, preamp_gain))
            group_rows.append((name, index, frequency,
                               unwrap_phase(responses)[index], delays[index]))
        for section_index, coeff in enumerate(bank):
            for frequency in frequencies:
                section_rows.append((name, section_index, frequency,
                                     magnitude_db(section_response(
                                         coeff, frequency))))
        _line_plot(output_dir / f"target_vs_actual_{name}.png",
                   f"Project 3.3 {name}: desired / shape / total",
                   frequencies,
                   (("desired", desired, (30, 100, 210)),
                    ("shape", shape, (20, 150, 80)),
                    ("total", total, (200, 60, 50))), -20.0, 15.0)

    matrix = interaction_matrix()
    self_gains = [matrix[index][index] for index in range(10)]
    _write_csv(output_dir / "response_curves.csv",
               ("preset", "point", "frequency_hz", "desired_db",
                "shape_db", "total_db", "predicted_peak_db",
                "preamp_db", "preamp_gain"), response_rows)
    _write_csv(output_dir / "section_response.csv",
               ("preset", "section", "frequency_hz", "magnitude_db"),
               section_rows)
    _write_csv(output_dir / "group_delay.csv",
               ("preset", "point", "frequency_hz", "phase_rad",
                "group_delay_samples"), group_rows)
    _write_csv(output_dir / "interaction_matrix.csv",
               ("source_band",) + tuple(f"response_band_{index}"
                                         for index in range(10)),
               ((index,) + tuple(row) for index, row in enumerate(matrix)))

    custom_bank = design_bank(PRESETS["custom"])
    section_series = []
    for index, coeff in enumerate(custom_bank):
        values = [magnitude_db(section_response(coeff, frequency))
                  for frequency in frequencies]
        color = ((37 * index) % 220, (83 * index) % 220,
                 (151 * index) % 220)
        section_series.append((f"section {index}", values, color))
    _line_plot(output_dir / "rbj_individual_sections.png",
               "Custom RBJ individual section response", frequencies,
               tuple(section_series), -20.0, 15.0)
    _heatmap(output_dir / "interaction_matrix_heatmap.png", matrix)

    delay_series = []
    for index, name in enumerate(("flat", "bass", "vocal", "treble",
                                  "v_shape", "custom")):
        responses = [cascade_response(design_bank(PRESETS[name]), frequency)
                     for frequency in frequencies]
        delays = group_delay(frequencies, responses)
        delay_series.append((name, delays,
                             ((43 * index) % 220, (91 * index) % 220,
                              (157 * index) % 220)))
    finite_delays = [value for _, values, _ in delay_series for value in values
                     if math.isfinite(value)]
    y_limit = max(5.0, min(200.0, max(abs(value) for value in finite_delays)))
    _line_plot(output_dir / "group_delay_comparison.png",
               "RBJ cascade group delay", frequencies,
               tuple(delay_series), -y_limit, y_limit)

    metrics: dict[str, float] = {
        "interaction_rows": float(len(matrix)),
        "interaction_columns": float(len(matrix[0])),
        "interaction_finite_count": float(sum(
            math.isfinite(value) for row in matrix for value in row)),
        "interaction_self_gain_min_db": min(self_gains),
        "interaction_self_gain_max_db": max(self_gains),
    }
    if c_reference_dir is not None:
        metrics.update(compare_c_reference(c_reference_dir))
    _write_csv(output_dir / "metrics.csv", ("metric", "value"),
               sorted(metrics.items()))
    return metrics


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--c-reference-dir", type=Path)
    arguments = parser.parse_args()
    metrics = generate_report(arguments.output_dir,
                              arguments.c_reference_dir)
    print(json.dumps(metrics, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
