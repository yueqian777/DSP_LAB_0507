#!/usr/bin/env python3
"""Build the Host WOLA wrapper and run the five-frequency PC THD suite."""

from __future__ import annotations

import argparse
import csv
import json
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

import numpy as np

from analyze_thd import analyze_wav, read_pcm16_mono, write_artifacts, write_pcm16_mono
from generate_thd_tones import FREQUENCIES_HZ, generate_suite, tone_filename


FRAME_LENGTH = 1024
WOLA_DELAY = 256


def find_compiler(explicit: Path | None) -> Path:
    if explicit is not None:
        if not explicit.is_file():
            raise FileNotFoundError(f"compiler not found: {explicit}")
        return explicit.resolve()
    environment = os.environ.get("MSYS2_CC")
    candidates = [
        Path(environment) if environment else None,
        Path(r"C:\msys64\mingw64\bin\gcc.exe"),
        Path(r"C:\msys64\ucrt64\bin\gcc.exe"),
    ]
    for candidate in candidates:
        if candidate is not None and candidate.is_file():
            return candidate.resolve()
    found = shutil.which("gcc")
    if found:
        return Path(found).resolve()
    raise FileNotFoundError("MSYS2 GCC not found; set MSYS2_CC")


def compile_host(repo_root: Path, build_dir: Path, compiler: Path) -> tuple[Path, list[str]]:
    build_dir.mkdir(parents=True, exist_ok=True)
    executable = build_dir / ("wola_thd_host.exe" if os.name == "nt" else "wola_thd_host")
    dsp_dir = repo_root / "Code" / "User" / "user_dsp"
    compiler_arguments = [
        str(compiler),
        "-std=c99",
        "-O2",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-DSUBBAND_ALGO_ONLY",
        f"-I{dsp_dir}",
        str(repo_root / "tools" / "thd" / "wola_thd_host.c"),
        str(dsp_dir / "user_spectral_fft.c"),
        str(dsp_dir / "user_subband_wola.c"),
        str(dsp_dir / "user_subband_denoise.c"),
        str(dsp_dir / "user_subband_codec_loopback.c"),
        "-lm",
        "-o",
        str(executable),
    ]
    if os.name == "nt":
        bash_text = os.environ.get("MSYS2_BASH", r"C:\msys64\usr\bin\bash.exe")
        bash = Path(bash_text)
        if not bash.is_file():
            raise FileNotFoundError(f"MSYS2 bash not found: {bash}")

        def to_msys(path_text: str) -> str:
            path = Path(path_text)
            if not path.is_absolute():
                return path_text.replace("\\", "/")
            drive = path.drive.rstrip(":").lower()
            remainder = str(path)[len(path.drive) :].replace("\\", "/").lstrip("/")
            return f"/{drive}/{remainder}"

        shell_arguments: list[str] = []
        for argument in compiler_arguments:
            if argument.startswith("-I"):
                shell_arguments.append("-I" + to_msys(argument[2:]))
            elif Path(argument).is_absolute():
                shell_arguments.append(to_msys(argument))
            else:
                shell_arguments.append(argument.replace("\\", "/"))
        shell_command = "export PATH=/mingw64/bin:/usr/bin:$PATH; " + " ".join(
            shlex.quote(argument) for argument in shell_arguments
        )
        command = [str(bash), "-lc", shell_command]
    else:
        command = compiler_arguments
    subprocess.run(command, cwd=repo_root, check=True)
    return executable, command


def git_text(repo_root: Path, arguments: list[str]) -> str:
    completed = subprocess.run(
        ["git", *arguments],
        cwd=repo_root,
        check=True,
        capture_output=True,
        text=True,
    )
    return completed.stdout.strip()


def process_wola(
    executable: Path,
    input_wav: Path,
    output_wav: Path,
    raw_dir: Path,
) -> dict[str, Any]:
    sample_rate, input_pcm, _ = read_pcm16_mono(input_wav)
    raw_dir.mkdir(parents=True, exist_ok=True)
    input_raw = raw_dir / f"{input_wav.stem}_input.pcm16le"
    output_raw = raw_dir / f"{input_wav.stem}_wola_full.pcm16le"
    input_pcm.astype("<i2").tofile(input_raw)
    completed = subprocess.run(
        [str(executable), str(input_raw), str(output_raw)],
        check=True,
        capture_output=True,
        text=True,
    )
    full_output = np.fromfile(output_raw, dtype="<i2")
    stop = WOLA_DELAY + len(input_pcm)
    if len(full_output) < stop:
        raise RuntimeError(
            f"Host WOLA output has {len(full_output)} samples, expected at least {stop}"
        )
    aligned_output = full_output[WOLA_DELAY:stop].copy()
    write_pcm16_mono(output_wav, sample_rate, aligned_output)

    metadata = json.loads(completed.stdout.strip().splitlines()[-1])
    if metadata["input_samples"] != len(input_pcm):
        raise RuntimeError("Host WOLA metadata input length mismatch")
    if metadata["startup_delay_samples"] != WOLA_DELAY:
        raise RuntimeError("Host WOLA delay metadata mismatch")
    metadata.update(
        {
            "input_wav": str(input_wav.resolve()),
            "output_wav": str(output_wav.resolve()),
            "saved_output_samples": len(aligned_output),
            "alignment": f"full_output[{WOLA_DELAY}:{stop}]",
            "output_policy": "drop fixed startup delay, keep original input duration",
        }
    )
    return metadata


def summary_row(
    frequency_hz: int,
    input_result: dict[str, Any],
    output_result: dict[str, Any],
) -> dict[str, Any]:
    return {
        "f0_hz": frequency_hz,
        "input_thd_ratio": input_result["thd_ratio"],
        "input_thd_percent": input_result["thd_percent"],
        "input_thd_db": input_result["thd_db"],
        "output_thd_ratio": output_result["thd_ratio"],
        "output_thd_percent": output_result["thd_percent"],
        "output_thd_db": output_result["thd_db"],
        "output_thdn_ratio": output_result["thdn_ratio"],
        "output_thdn_percent": output_result["thdn_percent"],
        "output_thdn_db": output_result["thdn_db"],
        "output_sinad_db": output_result["sinad_db"],
        "input_fundamental_rms": input_result["fundamental_rms"],
        "output_fundamental_rms": output_result["fundamental_rms"],
        "input_peak_dbfs": input_result["peak_dbfs"],
        "output_peak_dbfs": output_result["peak_dbfs"],
        "input_clip_count": input_result["clip_count"],
        "output_clip_count": output_result["clip_count"],
        "harmonic_order_used": output_result["max_harmonic_used"],
        "analysis_start_s": output_result["analysis_start_s"],
        "analysis_end_s": output_result["analysis_end_s"],
        "layer": "PC_WOLA_ALGORITHM",
        "board_status": "NOT_MEASURED",
    }


def write_summary(output_dir: Path, rows: list[dict[str, Any]]) -> None:
    with (output_dir / "thd_summary.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    lines = [
        "# PC WOLA algorithm-level THD summary",
        "",
        "This table measures the PCM16 -> WOLA analysis/synthesis -> PCM16 path. "
        "It is not an ADC + DSP + DAC board measurement.",
        "",
        "| f0 | Input THD % | Output THD % | Output THD dB | Output THD+N % | SINAD dB | Clip | Harmonics |",
        "|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            f"| {row['f0_hz']} | {row['input_thd_percent']:.9g} | "
            f"{row['output_thd_percent']:.9g} | {row['output_thd_db']:.3f} | "
            f"{row['output_thdn_percent']:.9g} | {row['output_sinad_db']:.3f} | "
            f"{row['output_clip_count']} | H2-H{row['harmonic_order_used']} |"
        )
    lines.extend(
        [
            "",
            "Board baseline: `NOT_MEASURED`.",
            "",
            "Board ADC + Mode 1 WOLA + DAC: `NOT_MEASURED`.",
            "",
            "No fixed pass/fail THD threshold is applied; absolute input and output values are retained.",
        ]
    )
    (output_dir / "thd_summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=Path("results/thd/pc_wola"))
    parser.add_argument("--cc", type=Path)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--host-exe", type=Path)
    arguments = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    output_dir = arguments.output_dir
    if not output_dir.is_absolute():
        output_dir = repo_root / output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    tone_dir = output_dir / "tones"
    raw_dir = output_dir / "_raw"
    build_dir = output_dir / "_build"
    generate_suite(tone_dir)

    compile_command: list[str] = []
    if arguments.skip_build:
        if arguments.host_exe is None or not arguments.host_exe.is_file():
            raise ValueError("--skip-build requires an existing --host-exe")
        executable = arguments.host_exe.resolve()
        compiler_text = "not built by this run"
    else:
        compiler = find_compiler(arguments.cc)
        executable, compile_command = compile_host(repo_root, build_dir, compiler)
        compiler_text = str(compiler)

    rows: list[dict[str, Any]] = []
    processing_records: list[dict[str, Any]] = []
    for frequency_hz in FREQUENCIES_HZ:
        input_wav = tone_dir / tone_filename(frequency_hz)
        frequency_dir = output_dir / f"{frequency_hz}Hz"
        output_wav = frequency_dir / f"wola_{tone_filename(frequency_hz)}"
        processing_records.append(
            process_wola(executable, input_wav, output_wav, raw_dir)
        )
        input_result, input_segment = analyze_wav(input_wav, frequency_hz)
        output_result, output_segment = analyze_wav(output_wav, frequency_hz)
        write_artifacts(input_result, input_segment, frequency_dir, "input")
        write_artifacts(output_result, output_segment, frequency_dir, "wola_output")
        rows.append(summary_row(frequency_hz, input_result, output_result))
        print(
            f"f0={frequency_hz:5d} Hz input_THD={input_result['thd_percent']:.9g}% "
            f"output_THD={output_result['thd_percent']:.9g}% "
            f"THD+N={output_result['thdn_percent']:.9g}% "
            f"SINAD={output_result['sinad_db']:.3f} dB "
            f"clips={output_result['clip_count']} H2-H{output_result['max_harmonic_used']}"
        )

    write_summary(output_dir, rows)
    metadata = {
        "layer": "PC_WOLA_ALGORITHM",
        "board_status": "NOT_MEASURED",
        "repo_root": str(repo_root),
        "commit_sha": git_text(repo_root, ["rev-parse", "HEAD"]),
        "git_status_porcelain": git_text(repo_root, ["status", "--short"]),
        "compiler": compiler_text,
        "compile_command": compile_command,
        "sample_rate_hz": 50_000,
        "format": "mono PCM16",
        "outer_frame_samples": FRAME_LENGTH,
        "wola_fft_samples": 512,
        "wola_hop_samples": WOLA_DELAY,
        "window": "periodic sqrt-Hann",
        "analysis_interval_s": [1.0, 9.0],
        "processing": processing_records,
    }
    (output_dir / "pc_wola_suite_metadata.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8"
    )

    clipping = sum(int(row["output_clip_count"]) for row in rows)
    if clipping != 0:
        raise RuntimeError(f"WOLA output clipping detected: {clipping} samples")
    print(f"summary={output_dir / 'thd_summary.csv'}")
    print("board_status=NOT_MEASURED")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, RuntimeError, FileNotFoundError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1) from error
