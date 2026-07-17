from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path


SCRIPT_VERSION = "1.0"
MODULES = ("dynamic_clarity", "smart_bass")
INPUTS = (
    "tone_400hz",
    "dual_400hz_1953_125hz",
    "music_like_multitone",
    "deterministic_pseudorandom",
)
CASES = (
    "level_0_identity",
    "level_1_stable",
    "level_4_stable",
    "transition_0_to_1",
    "transition_1_to_2",
    "transition_4_to_3",
    "transition_1_to_0",
)
MEASURED_COUNT = 4096
JOB_COUNT = len(MODULES) * len(INPUTS) * len(CASES)


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


def summarize(values: list[int], first_call: int, warm_max: int) -> dict[str, object]:
    ordered = sorted(values)
    return {
        "count": len(values),
        "min": ordered[0],
        "median": percentile(ordered, 0.5),
        "p95": percentile(ordered, 0.95),
        "p99": percentile(ordered, 0.99),
        "max": ordered[-1],
        "first_call": first_call,
        "warm_max": warm_max,
    }


def load_cycles(path: Path) -> list[int]:
    payload = path.read_bytes()
    expected = JOB_COUNT * MEASURED_COUNT * 4
    if len(payload) != expected:
        raise ValueError(
            f"cycle RAW size mismatch: expected {expected}, got {len(payload)}"
        )
    return list(struct.unpack(f"<{JOB_COUNT * MEASURED_COUNT}I", payload))


def analyze(result_dir: Path) -> dict[str, object]:
    board_path = result_dir / "dynamic_clarity_benchmark_board.json"
    board = json.loads(board_path.read_text(encoding="utf-8"))
    if not board.get("pass"):
        raise ValueError(f"board benchmark failed: {board.get('error')}")
    if int(board["job_count"]) != JOB_COUNT:
        raise ValueError("board benchmark job count does not match analyzer")
    if int(board["measured_count"]) != MEASURED_COUNT:
        raise ValueError("board measured count does not match analyzer")

    cycles = load_cycles(result_dir / "dynamic_clarity_benchmark_cycles.raw")
    first_calls = [int(value) for value in board["first_call"]]
    warm_maxes = [int(value) for value in board["warm_max"]]
    if len(first_calls) != JOB_COUNT or len(warm_maxes) != JOB_COUNT:
        raise ValueError("board first-call or warm-max metadata is incomplete")

    jobs: list[dict[str, object]] = []
    by_key: dict[tuple[str, str, str], dict[str, object]] = {}
    job = 0
    for module in MODULES:
        for input_name in INPUTS:
            for case in CASES:
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
                    **metrics,
                }
                jobs.append(row)
                by_key[(module, input_name, case)] = row
                job += 1

    comparisons = []
    for input_name in INPUTS:
        for case in CASES:
            dynamic = by_key[("dynamic_clarity", input_name, case)]
            smart = by_key[("smart_bass", input_name, case)]
            comparisons.append(
                {
                    "input": input_name,
                    "case": case,
                    "dynamic_to_smart_median_ratio": (
                        float(dynamic["median"]) / max(float(smart["median"]), 1.0)
                    ),
                    "dynamic_to_smart_p99_ratio": (
                        float(dynamic["p99"]) / max(float(smart["p99"]), 1.0)
                    ),
                    "dynamic_to_smart_max_ratio": (
                        float(dynamic["max"]) / max(float(smart["max"]), 1.0)
                    ),
                }
            )

    safety = board["state"]
    for name in (
        "dynamic_saturation",
        "dynamic_nonfinite",
        "smart_saturation",
        "smart_nonfinite",
    ):
        if int(safety[name]) != 0:
            raise ValueError(f"non-zero benchmark safety counter: {name}")

    return {
        "evidence_label": "BOARD_ISOLATED_FUNCTION_MICROBENCHMARK",
        "pass": True,
        "analyzer_version": SCRIPT_VERSION,
        "percentile_definition": "linear interpolation at (count-1)*p",
        "board": board,
        "jobs": jobs,
        "comparisons": comparisons,
        "root_cause_classification": "PENDING_REALTIME_TIMING_COMPARISON",
        "subjective_observation": "NOT_PERFORMED",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_dir", type=Path)
    args = parser.parse_args()
    result_dir = args.result_dir.resolve()
    report = analyze(result_dir)
    output = result_dir / "dynamic_clarity_benchmark.json"
    output.write_text(
        json.dumps(report, indent=2, ensure_ascii=True) + "\n",
        encoding="ascii",
    )
    print(json.dumps({"pass": True, "report": str(output)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
