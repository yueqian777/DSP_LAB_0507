#!/usr/bin/env python3
"""Independent Project 3.3 equalizer response verifier.

This script intentionally does not call the C implementation.  It reads the
C-exported coefficient and response CSV files, evaluates H(z) in Python, and
fails when the independent magnitude result differs by 0.01 dB or more.
"""

from __future__ import annotations

import argparse
import cmath
import csv
import math
from collections import defaultdict
from pathlib import Path


MAGNITUDE_LIMIT_DB = 0.01


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def as_float(row: dict[str, str], field: str) -> float:
    value = float(row[field])
    if not math.isfinite(value):
        raise ValueError(f"non-finite {field} in {row}")
    return value


def response_of_section(row: dict[str, str], frequency_hz: float) -> complex:
    omega = 2.0 * math.pi * frequency_hz / 50000.0
    z1 = cmath.exp(-1j * omega)
    z2 = z1 * z1
    numerator = as_float(row, "b0") + as_float(row, "b1") * z1 + as_float(row, "b2") * z2
    denominator = 1.0 + as_float(row, "a1") * z1 + as_float(row, "a2") * z2
    if abs(denominator) < 1.0e-20:
        raise ValueError(f"singular denominator in {row}")
    return numerator / denominator


def pole_radii(row: dict[str, str]) -> tuple[float, float]:
    a1 = as_float(row, "a1")
    a2 = as_float(row, "a2")
    root = cmath.sqrt(a1 * a1 - 4.0 * a2)
    p1 = (-a1 + root) * 0.5
    p2 = (-a1 - root) * 0.5
    return abs(p1), abs(p2)


def wrap_degrees(value: float) -> float:
    while value > 180.0:
        value -= 360.0
    while value < -180.0:
        value += 360.0
    return value


def response_for_case(
    core: str,
    sections: list[dict[str, str]],
    frequency_hz: float,
    preamp_db: float,
) -> complex:
    if core == "legacy":
        total = 1.0 + 0.0j
        for row in sections:
            gain = 10.0 ** (as_float(row, "gain_db") / 20.0)
            total += (gain - 1.0) * response_of_section(row, frequency_hz)
        return total
    if core == "rbj_cascade":
        total = 10.0 ** (preamp_db / 20.0)
        for row in sections:
            total *= response_of_section(row, frequency_hz)
        return total
    raise ValueError(f"unsupported core {core}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coefficients", type=Path, default=Path("equalizer_coefficients.csv"))
    parser.add_argument("--system-response", type=Path, default=Path("equalizer_system_response.csv"))
    parser.add_argument("--output", type=Path, default=Path("equalizer_reference_comparison.csv"))
    args = parser.parse_args()

    coefficient_rows = read_csv(args.coefficients)
    response_rows = read_csv(args.system_response)
    sections_by_case: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    max_pole_radius_by_case: dict[tuple[str, str], float] = defaultdict(float)
    bad_coefficients = 0

    for row in coefficient_rows:
        key = (row["core"], row["preset"])
        try:
            for field in ("b0", "b1", "b2", "a1", "a2", "gain_db", "q_or_bw"):
                as_float(row, field)
            radius_1, radius_2 = pole_radii(row)
            max_radius = max(radius_1, radius_2)
            max_pole_radius_by_case[key] = max(max_pole_radius_by_case[key], max_radius)
            if max_radius >= 1.0:
                bad_coefficients += 1
        except (KeyError, ValueError, OverflowError):
            bad_coefficients += 1
            continue
        sections_by_case[key].append(row)

    comparisons: list[dict[str, object]] = []
    failures = bad_coefficients
    max_error = 0.0
    for row in response_rows:
        core = row["core"]
        preset = row["preset"]
        key = (core, preset)
        try:
            frequency_hz = as_float(row, "freq_hz")
            c_magnitude = as_float(row, "magnitude_db")
            c_phase = as_float(row, "phase_deg")
            preamp_db = as_float(row, "preamp_db")
            python_response = response_for_case(core, sections_by_case[key], frequency_hz, preamp_db)
            python_magnitude = 20.0 * math.log10(max(abs(python_response), 1.0e-20))
            python_phase = math.degrees(cmath.phase(python_response))
            magnitude_error = abs(c_magnitude - python_magnitude)
            phase_error = abs(wrap_degrees(c_phase - python_phase))
            finite = all(math.isfinite(value) for value in (
                c_magnitude,
                c_phase,
                python_magnitude,
                python_phase,
                magnitude_error,
                phase_error,
            ))
            max_radius = max_pole_radius_by_case[key]
            passed = finite and max_radius < 1.0 and magnitude_error < MAGNITUDE_LIMIT_DB
        except (KeyError, ValueError, OverflowError, ZeroDivisionError):
            python_magnitude = math.nan
            python_phase = math.nan
            magnitude_error = math.inf
            phase_error = math.inf
            preamp_db = math.nan
            max_radius = math.inf
            passed = False

        max_error = max(max_error, magnitude_error)
        if not passed:
            failures += 1
        comparisons.append({
            "core": core,
            "preset": preset,
            "freq_hz": row["freq_hz"],
            "c_magnitude_db": row["magnitude_db"],
            "python_magnitude_db": f"{python_magnitude:.12f}",
            "magnitude_error_db": f"{magnitude_error:.12f}",
            "c_phase_deg": row["phase_deg"],
            "python_phase_deg": f"{python_phase:.12f}",
            "phase_error_deg": f"{phase_error:.12f}",
            "preamp_db": f"{preamp_db:.12f}",
            "max_pole_radius": f"{max_radius:.12f}",
            "pass": "PASS" if passed else "FAIL",
        })

    with args.output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=[
            "core", "preset", "freq_hz", "c_magnitude_db", "python_magnitude_db",
            "magnitude_error_db", "c_phase_deg", "python_phase_deg", "phase_error_deg",
            "preamp_db", "max_pole_radius", "pass",
        ])
        writer.writeheader()
        writer.writerows(comparisons)

    print(
        f"equalizer_reference_comparison.csv rows={len(comparisons)} "
        f"max_magnitude_error_db={max_error:.12f} coefficient_failures={bad_coefficients} "
        f"failures={failures}"
    )
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
