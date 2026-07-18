from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path


SCRIPT_VERSION = "1.1"
MODULES = ("dynamic_clarity", "smart_bass", "harshness_guard")
INPUTS = (
    "tone_400hz",
    "dual_400hz_1953_125hz",
    "music_like_multitone",
    "deterministic_pseudorandom",
)
CASES = (
    "level_0_identity",
    "level_1_stable",
    "level_medium_max_stable",
    "transition_0_to_1",
    "transition_1_to_2",
    "transition_medium_to_previous",
    "transition_1_to_0",
)
DIAGNOSTIC_INPUTS = (
    "dual_400hz_1953_125hz",
    "deterministic_pseudorandom",
)
DIAGNOSTIC_CASES = (
    "level_0_identity",
    "level_1_stable",
    "level_medium_max_stable",
    "transition_0_to_1",
    "transition_1_to_2",
    "transition_1_to_0",
)
MEDIUM_MAX_LEVEL = {
    "dynamic_clarity": 4,
    "smart_bass": 4,
    "harshness_guard": 3,
}
WARMUP_COUNT = 64
MEASURED_COUNT = 4096
JOB_COUNT = len(MODULES) * len(INPUTS) * len(CASES)
P99_FRACTION = 0.99
NORMAL_95 = 1.959963984540054


def percentile(sorted_values: list[int], fraction: float) -> float:
    position = (len(sorted_values) - 1) * fraction
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return float(sorted_values[lower])
    weight = position - lower
    return (
        float(sorted_values[lower]) * (1.0 - weight)
        + float(sorted_values[upper]) * weight
    )


def quantile_rank_interval(
    sorted_values: list[int], fraction: float
) -> tuple[float, float]:
    count = len(sorted_values)
    center = (count - 1) * fraction
    rank_sigma = math.sqrt(count * fraction * (1.0 - fraction))
    lower = max(0, int(math.floor(center - NORMAL_95 * rank_sigma)))
    upper = min(count - 1, int(math.ceil(center + NORMAL_95 * rank_sigma)))
    return float(sorted_values[lower]), float(sorted_values[upper])


def summarize(
    values: list[int], first_call: int, warm_max: int
) -> dict[str, object]:
    ordered = sorted(values)
    p99_low, p99_high = quantile_rank_interval(ordered, P99_FRACTION)
    return {
        "count": len(values),
        "min": ordered[0],
        "median": percentile(ordered, 0.5),
        "p95": percentile(ordered, 0.95),
        "p99": percentile(ordered, P99_FRACTION),
        "max": ordered[-1],
        "first": first_call,
        "warm_max": warm_max,
        "p99_rank_interval_approx_95": [p99_low, p99_high],
    }


def p99_significance(
    guard: dict[str, object], other: dict[str, object]
) -> str:
    guard_low, guard_high = guard["p99_rank_interval_approx_95"]
    other_low, other_high = other["p99_rank_interval_approx_95"]
    if float(guard_low) > float(other_high):
        return "GUARD_HIGHER_APPROX_95_PERCENT"
    if float(guard_high) < float(other_low):
        return "GUARD_LOWER_APPROX_95_PERCENT"
    return "NOT_SEPARATED_APPROX_95_PERCENT"


def load_cycles(path: Path, job_count: int) -> list[int]:
    payload = path.read_bytes()
    value_count = job_count * MEASURED_COUNT
    expected = value_count * 4
    if len(payload) != expected:
        raise ValueError(
            f"cycle RAW size mismatch: expected {expected}, got {len(payload)}"
        )
    return list(struct.unpack(f"<{value_count}I", payload))


def analyze(result_dir: Path) -> dict[str, object]:
    board_path = result_dir / "harshness_guard_benchmark_board.json"
    board = json.loads(board_path.read_text(encoding="utf-8"))
    if not board.get("pass"):
        raise ValueError(f"board benchmark failed: {board.get('error')}")
    kernel_diagnostics = bool(board.get("kernel_diagnostics", False))
    inputs = DIAGNOSTIC_INPUTS if kernel_diagnostics else INPUTS
    cases = DIAGNOSTIC_CASES if kernel_diagnostics else CASES
    job_count = len(MODULES) * len(inputs) * len(cases)
    if int(board["job_count"]) != job_count:
        raise ValueError("board benchmark job count does not match analyzer")
    if int(board["measured_count"]) != MEASURED_COUNT:
        raise ValueError("board measured count does not match analyzer")
    if int(board["warmup_count"]) != WARMUP_COUNT:
        raise ValueError("board warmup count does not match analyzer")
    if tuple(board["modules"]) != MODULES:
        raise ValueError("board module order does not match analyzer")
    if tuple(board["inputs"]) != inputs:
        raise ValueError("board input order does not match analyzer")
    if tuple(board["cases"]) != cases:
        raise ValueError("board case order does not match analyzer")
    if board["medium_max_level"] != MEDIUM_MAX_LEVEL:
        raise ValueError("board MEDIUM maximum levels do not match analyzer")

    cycles = load_cycles(
        result_dir / "harshness_guard_benchmark_cycles.raw", job_count
    )
    first_calls = [int(value) for value in board["first_call"]]
    warm_maxes = [int(value) for value in board["warm_max"]]
    if len(first_calls) != job_count or len(warm_maxes) != job_count:
        raise ValueError("board first-call or warm-max metadata is incomplete")

    jobs: list[dict[str, object]] = []
    by_key: dict[tuple[str, str, str], dict[str, object]] = {}
    job = 0
    for module in MODULES:
        for input_name in inputs:
            for case in cases:
                offset = job * MEASURED_COUNT
                metrics = summarize(
                    cycles[offset : offset + MEASURED_COUNT],
                    first_calls[job],
                    warm_maxes[job],
                )
                row = {
                    "job_index": job,
                    "module": module,
                    "input": input_name,
                    "case": case,
                    "medium_max_level": MEDIUM_MAX_LEVEL[module],
                    **metrics,
                }
                jobs.append(row)
                by_key[(module, input_name, case)] = row
                job += 1

    comparisons: list[dict[str, object]] = []
    for input_name in inputs:
        for case in cases:
            dynamic = by_key[("dynamic_clarity", input_name, case)]
            smart = by_key[("smart_bass", input_name, case)]
            guard = by_key[("harshness_guard", input_name, case)]
            comparisons.append(
                {
                    "input": input_name,
                    "case": case,
                    "guard_to_dynamic_p99_ratio": (
                        float(guard["p99"]) / max(float(dynamic["p99"]), 1.0)
                    ),
                    "guard_to_smart_p99_ratio": (
                        float(guard["p99"]) / max(float(smart["p99"]), 1.0)
                    ),
                    "guard_vs_dynamic_p99_significance": p99_significance(
                        guard, dynamic
                    ),
                    "guard_vs_smart_p99_significance": p99_significance(
                        guard, smart
                    ),
                }
            )

    safety = board["state"]
    safety_names = (
        "dynamic_saturation",
        "dynamic_nonfinite",
        "smart_saturation",
        "smart_nonfinite",
        "harshness_saturation",
        "harshness_nonfinite",
    )
    safety_counters = {name: int(safety[name]) for name in safety_names}
    for name, value in safety_counters.items():
        if value != 0:
            raise ValueError(f"non-zero benchmark safety counter: {name}")

    return {
        "evidence_label": "BOARD_ISOLATED_THREE_MODULE_MICROBENCHMARK",
        "pass": True,
        "kernel_diagnostics": kernel_diagnostics,
        "analyzer_version": SCRIPT_VERSION,
        "percentile_definition": "linear interpolation at (count-1)*p",
        "p99_significance_method": (
            "non-overlap of approximate 95 percent normal order-rank intervals"
        ),
        "performance_acceptance_threshold": None,
        "performance_interpretation": (
            "P99 ratios and significance labels are descriptive only"
        ),
        "guard_safety_counters": {
            "saturation": safety_counters["harshness_saturation"],
            "nonfinite": safety_counters["harshness_nonfinite"],
        },
        "all_safety_counters": safety_counters,
        "board": board,
        "jobs": jobs,
        "comparisons": comparisons,
        "subjective_observation": "NOT_PERFORMED",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_dir", type=Path)
    args = parser.parse_args()
    result_dir = args.result_dir.resolve()
    report = analyze(result_dir)
    output = result_dir / "harshness_guard_benchmark.json"
    output.write_text(
        json.dumps(report, indent=2, ensure_ascii=True) + "\n",
        encoding="ascii",
    )
    print(json.dumps({"pass": True, "report": str(output)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
