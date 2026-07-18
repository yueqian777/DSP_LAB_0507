from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
HARNESS = ROOT / "tools/tests/equalizer_editor_pcm16_test.c"
CORE = ROOT / "Code/User/user_dsp/user_equalizer.c"
CONTROL = ROOT / "Code/User/user_dsp/user_equalizer_control.c"
CORE_HEADER = ROOT / "Code/User/user_dsp/user_equalizer.h"
CONTROL_HEADER = ROOT / "Code/User/user_dsp/user_equalizer_control.h"
DEFAULT_BASH = Path(r"C:\msys64\usr\bin\bash.exe")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def is_within(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
    except ValueError:
        return False
    return True


def to_msys_path(path: Path) -> str:
    resolved = path.resolve()
    drive = resolved.drive.rstrip(":").lower()
    if not drive:
        return resolved.as_posix()
    tail = resolved.as_posix()[2:].lstrip("/")
    return f"/{drive}/{tail}"


def compile_harness(executable: Path) -> str:
    bash = Path(os.environ.get("MSYS2_BASH", str(DEFAULT_BASH)))
    if not bash.is_file():
        raise RuntimeError(f"MSYS2 bash not found: {bash}")

    arguments = [
        "gcc",
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-DEQ_ALGO_ONLY",
        "-ICode/User/user_dsp",
        "tools/tests/equalizer_editor_pcm16_test.c",
        "Code/User/user_dsp/user_equalizer.c",
        "Code/User/user_dsp/user_equalizer_control.c",
        "-lm",
        "-o",
        to_msys_path(executable),
    ]
    command = (
        "export PATH=/mingw64/bin:/usr/bin:$PATH; "
        f"cd {shlex.quote(to_msys_path(ROOT))}; "
        + " ".join(shlex.quote(item) for item in arguments)
    )
    result = subprocess.run(
        [str(bash), "-lc", command],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"host compile failed\n{result.stdout}")
    return result.stdout


def run_harness(executable: Path, output_directory: Path) -> str:
    result = subprocess.run(
        [str(executable), str(output_directory)],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"PCM16 harness failed\n{result.stdout}")
    if "failures=0" not in result.stdout:
        raise RuntimeError(f"PCM16 harness omitted pass marker\n{result.stdout}")
    return result.stdout


def collect_artifacts(output_directory: Path) -> list[dict[str, object]]:
    artifacts: list[dict[str, object]] = []
    for path in sorted(output_directory.rglob("*")):
        if path.is_file() and path.name not in {
            "manifest.json",
            "SHA256SUMS.txt",
        }:
            artifacts.append(
                {
                    "path": path.relative_to(output_directory).as_posix(),
                    "bytes": path.stat().st_size,
                    "sha256": sha256_file(path),
                }
            )
    return artifacts


def validate_summary(summary: dict[str, object]) -> None:
    expected = {
        "sample_rate_hz": 50_000,
        "channels": 1,
        "sample_format": "PCM_S16LE",
        "deterministic": True,
        "pattern_count": 6,
        "signal_count": 16,
        "static_case_count": 96,
        "response_point_count": 60,
        "transition_case_count": 4,
        "transition_expected_samples": 6000,
        "transition_min_samples": 6000,
        "transition_max_samples": 6000,
        "total_clip_count": 0,
        "total_nonfinite_count": 0,
        "identity_mismatch_count": 0,
        "unexpected_repeated_frames": 0,
        "transition_repeated_frames": 0,
        "transition_sequence_early_updates": 0,
        "failures": 0,
    }
    for key, value in expected.items():
        if summary.get(key) != value:
            raise RuntimeError(
                f"summary contract failed for {key}: "
                f"{summary.get(key)!r} != {value!r}"
            )
    if float(summary["max_center_error_db"]) > float(
        summary["center_error_limit_db"]
    ):
        raise RuntimeError("center-response error exceeds its declared limit")
    if float(summary["max_complex_error"]) > float(
        summary["complex_error_limit"]
    ):
        raise RuntimeError("complex-response error exceeds its declared limit")


def write_manifest(
    output_directory: Path,
    harness_output: str,
    artifacts: list[dict[str, object]],
) -> Path:
    summary_path = output_directory / "equalizer_editor_pcm16_summary.json"
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    validate_summary(summary)
    sources = []
    for path in (HARNESS, CORE, CONTROL, CORE_HEADER, CONTROL_HEADER):
        sources.append(
            {
                "path": path.relative_to(ROOT).as_posix(),
                "sha256": sha256_file(path),
            }
        )
    manifest = {
        "schema": "project33_equalizer_editor_offline_manifest_v1",
        "hardware_or_dss_used": False,
        "sound_card_used": False,
        "output_directory": str(output_directory),
        "compiler": "MSYS2 MinGW64 gcc",
        "compiled_sources": [
            "Code/User/user_dsp/user_equalizer.c",
            "Code/User/user_dsp/user_equalizer_control.c",
            "tools/tests/equalizer_editor_pcm16_test.c",
        ],
        "harness_stdout": harness_output.strip(),
        "measurement_contract": {
            "pcm": "mono PCM16 at 50000 Hz",
            "center_error_limit_db": 0.15,
            "center_method": (
                "single-bin DFT and RMS over 40000 steady-state samples "
                "after 10000 settling samples"
            ),
            "threshold_reason": (
                "0.15 dB bounds amplitude error to about 1.7 percent and is "
                "well above PCM16 quantization while remaining below the "
                "editor's 0.5 dB adjustment step"
            ),
            "transition": (
                "6000 samples at 50000 Hz, exactly 120 ms; applied sequence "
                "is observed only at the safe boundary after completion"
            ),
        },
        "summary": summary,
        "source_sha256": sources,
        "artifacts": artifacts,
    }
    path = output_directory / "manifest.json"
    path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return path


def write_sha256_sums(output_directory: Path) -> Path:
    paths = sorted(
        path
        for path in output_directory.rglob("*")
        if path.is_file() and path.name != "SHA256SUMS.txt"
    )
    lines = [
        f"{sha256_file(path)}  {path.relative_to(output_directory).as_posix()}"
        for path in paths
    ]
    destination = output_directory / "SHA256SUMS.txt"
    destination.write_text("\n".join(lines) + "\n", encoding="ascii")
    return destination


def run(output_directory: Path) -> dict[str, object]:
    output_directory = output_directory.resolve()
    if is_within(output_directory, ROOT):
        raise RuntimeError(
            f"output directory must be outside the repository: {output_directory}"
        )
    output_directory.mkdir(parents=True, exist_ok=True)
    if any(output_directory.iterdir()):
        raise RuntimeError(
            f"output directory must be empty: {output_directory}"
        )

    with tempfile.TemporaryDirectory(prefix="equalizer_editor_pcm16_build_") as tmp:
        executable = Path(tmp) / "equalizer_editor_pcm16_test.exe"
        compile_harness(executable)
        harness_output = run_harness(executable, output_directory)

    required = {
        "equalizer_editor_pcm16_cases.csv",
        "equalizer_editor_pcm16_response.csv",
        "equalizer_editor_pcm16_transitions.csv",
        "equalizer_editor_pcm16_summary.json",
    }
    missing = sorted(
        name for name in required if not (output_directory / name).is_file()
    )
    if missing:
        raise RuntimeError(f"missing harness artifacts: {missing}")
    artifacts = collect_artifacts(output_directory)
    wav_count = sum(
        1 for artifact in artifacts if str(artifact["path"]).endswith(".wav")
    )
    if wav_count != 120:
        raise RuntimeError(f"expected 120 WAV files, found {wav_count}")
    manifest_path = write_manifest(output_directory, harness_output, artifacts)
    sums_path = write_sha256_sums(output_directory)
    summary = json.loads(
        (output_directory / "equalizer_editor_pcm16_summary.json").read_text(
            encoding="utf-8"
        )
    )
    return {
        "output_directory": str(output_directory),
        "manifest": str(manifest_path),
        "sha256_sums": str(sums_path),
        "wav_count": wav_count,
        "summary": summary,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Compile and run deterministic Project 3.3 CUSTOM PCM16 host "
            "validation without audio hardware."
        )
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        required=True,
        help="Empty temporary directory outside the repository",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_args()
    result = run(arguments.output_dir)
    summary = result["summary"]
    print(
        "equalizer_33_editor_offline PASS "
        f"max_center_error_db={summary['max_center_error_db']:.6f} "
        f"max_complex_error={summary['max_complex_error']:.8f} "
        f"wav_count={result['wav_count']} "
        f"output={result['output_directory']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
