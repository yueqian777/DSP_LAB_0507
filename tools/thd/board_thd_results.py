#!/usr/bin/env python3
"""Prepare and analyze C6748 WOLA THD captures transferred only through JTAG."""

from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path
from typing import Any, Iterable

import numpy as np

from analyze_thd import analyze_wav, read_pcm16_mono, write_artifacts, write_pcm16_mono
from generate_thd_tones import FREQUENCIES_HZ, generate_suite, tone_filename
from run_wola_thd_suite import WOLA_DELAY, git_text, summary_row


FRAME_LENGTH = 1024
INPUT_SAMPLES = 500_000
PROCESSED_SAMPLES = 500_736
FRAME_COUNT = PROCESSED_SAMPLES // FRAME_LENGTH
LAYER = "BOARD_C6748_WOLA_JTAG_DIGITAL"
STATUS = "MEASURED_BOARD_DIGITAL_NO_ADC_DAC_ANALOG"


def prepare_inputs(output_dir: Path, frequencies: Iterable[int] = FREQUENCIES_HZ) -> None:
    frequencies = tuple(frequencies)
    tone_dir = output_dir / "tones"
    generate_suite(tone_dir)
    for frequency_hz in frequencies:
        input_wav = tone_dir / tone_filename(frequency_hz)
        _, pcm, _ = read_pcm16_mono(input_wav)
        if len(pcm) != INPUT_SAMPLES:
            raise RuntimeError(f"{input_wav}: expected {INPUT_SAMPLES} samples")
        frequency_dir = output_dir / f"{frequency_hz}Hz"
        frequency_dir.mkdir(parents=True, exist_ok=True)
        padded = np.zeros(PROCESSED_SAMPLES, dtype="<i2")
        padded[: len(pcm)] = pcm
        padded.tofile(frequency_dir / "input_full.pcm16le")


def decode_c6748_raw(path: Path) -> np.ndarray:
    """Read the explicit little-endian PCM16 stream written by the DSS script."""
    samples = np.fromfile(path, dtype="<i2")
    if len(samples) != PROCESSED_SAMPLES:
        raise RuntimeError(
            f"{path}: got {len(samples)} samples, expected {PROCESSED_SAMPLES}"
        )
    return samples


def board_summary_row(
    frequency_hz: int,
    input_result: dict[str, Any],
    output_result: dict[str, Any],
) -> dict[str, Any]:
    row = summary_row(frequency_hz, input_result, output_result)
    row["layer"] = LAYER
    row["board_status"] = STATUS
    return row


def write_board_summary(output_dir: Path, rows: list[dict[str, Any]]) -> None:
    with (output_dir / "thd_summary.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    lines = [
        "# C6748 board WOLA digital THD summary",
        "",
        "The deterministic PCM16 stimulus was loaded into DDR through JTAG, processed by "
        "the real C6748 WOLA implementation, and exported from DDR through JTAG. No PC "
        "playback or recording device was used.",
        "",
        "This is a board-executed digital-path measurement. It excludes ADC, DAC, analog "
        "filters, connectors, and cabling.",
        "",
        "| f0 | Input THD % | Board output THD % | Output THD dB | THD+N % | SINAD dB | Clip | Harmonics |",
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
            f"Measurement status: `{STATUS}`.",
            "",
            "Full ADC + DSP + DAC analog THD remains `NOT_MEASURED`.",
        ]
    )
    (output_dir / "thd_summary.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8"
    )


def analyze_outputs(
    output_dir: Path,
    frequencies: Iterable[int] = FREQUENCIES_HZ,
    repo_root: Path | None = None,
) -> list[dict[str, Any]]:
    frequencies = tuple(frequencies)
    rows: list[dict[str, Any]] = []
    dss_records: list[dict[str, Any]] = []
    tone_dir = output_dir / "tones"
    for frequency_hz in frequencies:
        frequency_dir = output_dir / f"{frequency_hz}Hz"
        input_wav = tone_dir / tone_filename(frequency_hz)
        output_raw = frequency_dir / "output_full.pcm16le"
        full_output = decode_c6748_raw(output_raw)
        aligned = full_output[WOLA_DELAY : WOLA_DELAY + INPUT_SAMPLES].copy()
        output_wav = frequency_dir / f"board_{tone_filename(frequency_hz)}"
        write_pcm16_mono(output_wav, 50_000, aligned)

        input_result, input_segment = analyze_wav(input_wav, frequency_hz)
        output_result, output_segment = analyze_wav(output_wav, frequency_hz)
        write_artifacts(input_result, input_segment, frequency_dir, "input")
        write_artifacts(output_result, output_segment, frequency_dir, "board_output")
        rows.append(board_summary_row(frequency_hz, input_result, output_result))

        dss_path = frequency_dir / "dss_summary.json"
        if dss_path.is_file():
            dss_records.append(json.loads(dss_path.read_text(encoding="utf-8-sig")))
        print(
            f"f0={frequency_hz:5d} Hz board_THD={output_result['thd_percent']:.9g}% "
            f"THD+N={output_result['thdn_percent']:.9g}% "
            f"SINAD={output_result['sinad_db']:.3f} dB "
            f"clips={output_result['clip_count']}"
        )

    write_board_summary(output_dir, rows)
    metadata: dict[str, Any] = {
        "layer": LAYER,
        "board_status": STATUS,
        "transport": "JTAG_DSS_MEMORY_LOAD_SAVE",
        "raw_packing": "PCM16LE_PACKED_BY_C6748_TWO_I16_PER_U32",
        "uses_pc_soundcard": False,
        "includes_adc": False,
        "includes_dac_analog": False,
        "sample_rate_hz": 50_000,
        "input_samples": INPUT_SAMPLES,
        "processed_samples": PROCESSED_SAMPLES,
        "frame_count": FRAME_COUNT,
        "startup_delay_samples": WOLA_DELAY,
        "analysis_interval_s": [1.0, 9.0],
        "dss_records": dss_records,
    }
    if repo_root is not None:
        metadata["commit_sha"] = git_text(repo_root, ["rev-parse", "HEAD"])
        metadata["git_status_porcelain"] = git_text(repo_root, ["status", "--short"])
    (output_dir / "board_suite_metadata.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    if sum(int(row["output_clip_count"]) for row in rows) != 0:
        raise RuntimeError("board WOLA output clipping detected")
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "analyze"))
    parser.add_argument("--output-dir", type=Path, required=True)
    arguments = parser.parse_args()
    output_dir = arguments.output_dir.resolve()
    if arguments.action == "prepare":
        prepare_inputs(output_dir)
        print(f"prepared={len(FREQUENCIES_HZ)} output_dir={output_dir}")
    else:
        analyze_outputs(output_dir, repo_root=Path(__file__).resolve().parents[2])
        print(f"summary={output_dir / 'thd_summary.csv'}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, RuntimeError, FileNotFoundError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1) from error
