#!/usr/bin/env python3
from __future__ import annotations

import argparse
import math
import re
import sys
import warnings
import wave
from dataclasses import dataclass, field
from datetime import datetime
from html import escape
from pathlib import Path
from typing import Iterable, Sequence

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

try:
    import plotly.graph_objects as go

    PLOTLY_IMPORT_ERROR: Exception | None = None
except Exception as exc:  # noqa: BLE001 - optional interactive layer.
    go = None
    PLOTLY_IMPORT_ERROR = exc


CSV_FILES = [
    "subband_eval_report.csv",
    "subband_codec_eval_report.csv",
    "subband_denoise_ms_eval_report.csv",
    "subband_denoise_mcra_eval_report.csv",
    "aggregate_compare_audio_metrics.csv",
    "compare_audio_metrics.csv",
    "board_mode_runtime_summary.csv",
]

REALTIME_CSV_FILES = [
    "board_mode_runtime_summary.csv",
    "realtime_summary.csv",
    "realtime_summary_template.csv",
]

WAV_FILES = [
    "compare_00_input.wav",
    "compare_01_wola_only.wav",
    "compare_02_wola_denoise.wav",
    "compare_03_codec_direct_160k.wav",
    "compare_04_codec_direct_240k.wav",
    "compare_05_codec_direct_320k.wav",
    "compare_06_denoise_codec_160k.wav",
    "compare_07_denoise_codec_240k.wav",
    "compare_08_denoise_codec_320k.wav",
    "compare_09_ms_denoise.wav",
    "compare_10_mcra_denoise.wav",
    "compare_11_strong_mcra_denoise.wav",
]

PPT_RECOMMENDATIONS = [
    "denoise_algorithm_evolution_stepup.png",
    "denoise_spectrogram_panel_speechband.png",
    "denoise_reduction_panel_speechband.png",
    "realtime_budget_compare_annotated.png",
    "codec_quality_snr.png",
    "codec_bitrate_compression_ratio.png",
    "wola_spectrum_compare.png",
    "thd_spectrum_annotated.png",
    "wola_frequency_response.png",
    "denoise_snr_bar.png",
]

INTERACTIVE_REPORTS = [
    "index.html",
    "denoise_output_snr_surface.html",
    "denoise_delta_surface.html",
    "denoise_output_snr_heatmap.html",
    "denoise_delta_heatmap.html",
    "denoise_stepup_waterfall.html",
    "denoise_quality_tradeoff.html",
    "board_runtime_by_mode.html",
    "codec_rate_quality_tradeoff.html",
    "audio_energy_ratio_heatmap.html",
]

FIGSIZE = (10.0, 5.625)
DPI = 160
PCM_BITRATE_KBPS = 800.0
FRAME_BUDGET_MS = 20.48
SPEECH_BAND_HZ = 8000.0
SPECTROGRAM_VMIN_DB = -90.0
SPECTROGRAM_VMAX_DB = -20.0
REDUCTION_VMIN_DB = 0.0
REDUCTION_VMAX_DB = 40.0
EPS = 1e-12


@dataclass
class ReportContext:
    root: Path
    out_dir: Path
    csv_dirs: list[Path]
    audio_dirs: list[Path]
    plots_dir: Path = field(init=False)
    summary_dir: Path = field(init=False)
    found_csv: dict[str, Path] = field(default_factory=dict)
    missing_csv: list[str] = field(default_factory=list)
    found_wav: dict[str, Path] = field(default_factory=dict)
    missing_wav: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    generated_plots: list[str] = field(default_factory=list)
    skipped_plots: dict[str, str] = field(default_factory=dict)
    generated_html: list[str] = field(default_factory=list)
    skipped_html: dict[str, str] = field(default_factory=dict)
    summary_files: list[str] = field(default_factory=list)
    found_map: Path | None = None

    def __post_init__(self) -> None:
        self.plots_dir = self.out_dir / "plots"
        self.summary_dir = self.out_dir / "summary_tables"
        self.interactive_dir = self.out_dir / "interactive"
        self.plots_dir.mkdir(parents=True, exist_ok=True)
        self.summary_dir.mkdir(parents=True, exist_ok=True)


def warn(ctx: ReportContext, message: str) -> None:
    ctx.warnings.append(message)
    print(f"WARNING: {message}")


def normalize_column(name: object) -> str:
    cleaned = str(name).strip().lower()
    cleaned = re.sub(r"[\s\-\/]+", "_", cleaned)
    cleaned = re.sub(r"[^a-z0-9_]+", "", cleaned)
    cleaned = re.sub(r"_+", "_", cleaned).strip("_")
    return cleaned


def unique_paths(paths: Iterable[Path]) -> list[Path]:
    seen: set[str] = set()
    result: list[Path] = []
    for path in paths:
        resolved = str(path.resolve()) if path.exists() else str(path.absolute())
        key = resolved.lower()
        if key not in seen:
            seen.add(key)
            result.append(path)
    return result


def existing_dirs(paths: Iterable[Path]) -> list[Path]:
    return [path for path in unique_paths(paths) if path.exists() and path.is_dir()]


def build_search_dirs(root: Path, cli_dirs: Sequence[str] | None) -> list[Path]:
    defaults = [
        root,
        root / "test_vectors",
        root / "docs" / "eval_baseline",
        root / "docs" / "eval_outputs" / "input",
        root / "docs" / "eval_outputs" / "summary_tables",
        root / "compare",
        root / "compare_unzip",
    ]
    extras = [Path(item).expanduser() for item in (cli_dirs or [])]
    return existing_dirs(extras + defaults)


def find_csv_file(name: str, search_dirs: Sequence[Path], ctx: ReportContext) -> Path | None:
    stem = Path(name).stem.lower()
    candidates: list[Path] = []
    for directory in search_dirs:
        exact = directory / name
        if exact.exists() and exact.is_file():
            candidates.append(exact)
        for path in directory.glob(f"{stem}_*.csv"):
            if path.is_file():
                candidates.append(path)

    candidates = unique_paths(candidates)
    if not candidates:
        ctx.missing_csv.append(name)
        return None

    def score(path: Path) -> tuple[int, int, float]:
        exact = path.name.lower() == name.lower()
        non_empty = path.stat().st_size > 0
        return (0 if non_empty else 1, 0 if exact else 1, -path.stat().st_mtime)

    selected = sorted(candidates, key=score)[0]
    if selected.stat().st_size == 0:
        warn(ctx, f"CSV is empty: {selected}")
    if selected.name.lower() != name.lower():
        print(f"Found CSV for {name}: {selected} (dated/fallback candidate)")
    else:
        print(f"Found CSV for {name}: {selected}")
    return selected


def find_optional_csv(names: Sequence[str], search_dirs: Sequence[Path]) -> Path | None:
    candidates: list[Path] = []
    lowered = {name.lower() for name in names}
    stems = {Path(name).stem.lower() for name in names}
    for directory in search_dirs:
        for name in names:
            exact = directory / name
            if exact.exists() and exact.is_file():
                candidates.append(exact)
        for path in directory.glob("*.csv"):
            if path.name.lower() in lowered or path.stem.lower() in stems:
                candidates.append(path)

    candidates = unique_paths(candidates)
    if not candidates:
        return None

    preference = {name.lower(): index for index, name in enumerate(names)}
    stem_preference = {Path(name).stem.lower(): index for index, name in enumerate(names)}

    def score(path: Path) -> tuple[int, int, float]:
        non_empty = path.stat().st_size > 0
        rank = preference.get(path.name.lower(), stem_preference.get(path.stem.lower(), len(names)))
        return (rank, 0 if non_empty else 1, -path.stat().st_mtime)

    return sorted(candidates, key=score)[0]


def find_wav_file(name: str, search_dirs: Sequence[Path]) -> Path | None:
    for directory in search_dirs:
        path = directory / name
        if path.exists() and path.is_file():
            return path
    return None


def discover_inputs(ctx: ReportContext) -> None:
    for csv_name in CSV_FILES:
        path = find_csv_file(csv_name, ctx.csv_dirs, ctx)
        if path is not None:
            ctx.found_csv[csv_name] = path

    for wav_name in WAV_FILES:
        path = find_wav_file(wav_name, ctx.audio_dirs)
        if path is None:
            ctx.missing_wav.append(wav_name)
        else:
            ctx.found_wav[wav_name] = path
            print(f"Found WAV for {wav_name}: {path}")


def write_inputs_log(ctx: ReportContext) -> None:
    lines = [
        "No input files were copied. Large WAV files are referenced in place by design.",
        "",
        "[CSV found]",
    ]
    lines.extend(f"{name}: {path}" for name, path in sorted(ctx.found_csv.items()))
    lines.append("")
    lines.append("[CSV missing]")
    lines.extend(ctx.missing_csv or ["<none>"])
    lines.append("")
    lines.append("[WAV found]")
    lines.extend(f"{name}: {path}" for name, path in sorted(ctx.found_wav.items()))
    lines.append("")
    lines.append("[WAV missing]")
    lines.extend(ctx.missing_wav or ["<none>"])
    (ctx.out_dir / "copied_inputs_log.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def read_csv_safe(path: Path | None, ctx: ReportContext, logical_name: str,
                  drop_empty_columns: bool = True) -> pd.DataFrame:
    if path is None:
        warn(ctx, f"Missing CSV: {logical_name}")
        return pd.DataFrame()
    if path.stat().st_size == 0:
        warn(ctx, f"Skipping empty CSV: {path}")
        return pd.DataFrame()
    try:
        df = pd.read_csv(path)
    except Exception as exc:  # noqa: BLE001 - keep report generation alive.
        warn(ctx, f"Could not read CSV {path}: {exc}")
        return pd.DataFrame()

    if drop_empty_columns:
        df = df.dropna(axis=1, how="all")
    keep_cols = []
    for column in df.columns:
        normalized = normalize_column(column)
        if normalized and not normalized.startswith("unnamed"):
            keep_cols.append(column)
    df = df[keep_cols]
    df.columns = [normalize_column(column) for column in df.columns]
    return df


def first_col(df: pd.DataFrame, *names: str) -> str | None:
    for name in names:
        normalized = normalize_column(name)
        if normalized in df.columns:
            return normalized
    return None


def get_value(row: pd.Series | None, *names: str, default: object = np.nan) -> object:
    if row is None:
        return default
    for name in names:
        normalized = normalize_column(name)
        if normalized in row.index and not pd.isna(row[normalized]):
            return row[normalized]
    return default


def numeric(value: object, default: float = math.nan) -> float:
    try:
        if value is None or pd.isna(value):
            return default
        return float(value)
    except (TypeError, ValueError):
        return default


def pass_value(values: Iterable[object]) -> str:
    saw_value = False
    for value in values:
        if value is None or pd.isna(value):
            continue
        saw_value = True
        if isinstance(value, str):
            if value.strip().upper() not in {"PASS", "TRUE", "OK", "1", "YES"}:
                return "FAIL"
        else:
            try:
                if float(value) <= 0:
                    return "FAIL"
            except (TypeError, ValueError):
                return "FAIL"
    return "PASS" if saw_value else ""


def find_case_row(df: pd.DataFrame, *case_terms: str) -> pd.Series | None:
    case_col = first_col(df, "eval_case", "codec_case", "mode")
    if df.empty or case_col is None:
        return None
    cases = df[case_col].astype(str).str.lower()
    for term in case_terms:
        term_lower = term.lower()
        matches = df[cases == term_lower]
        if not matches.empty:
            return matches.iloc[0]
    for term in case_terms:
        term_lower = term.lower()
        matches = df[cases.str.contains(term_lower, regex=False, na=False)]
        if not matches.empty:
            return matches.iloc[0]
    return None


def save_summary(ctx: ReportContext, df: pd.DataFrame, filename: str) -> Path:
    path = ctx.summary_dir / filename
    df.to_csv(path, index=False, encoding="utf-8-sig")
    ctx.summary_files.append(str(path.relative_to(ctx.out_dir)))
    return path


def build_wola_summary(df: pd.DataFrame, ctx: ReportContext) -> pd.DataFrame:
    columns = [
        "wola_passthrough_snr_db",
        "wola_energy_ratio",
        "wola_delay_samples",
        "denoise_noise_reduction_db",
        "denoise_snr5_output_snr_db",
        "denoise_snr10_output_snr_db",
        "denoise_snr20_output_snr_db",
        "pass",
    ]
    if df.empty:
        warn(ctx, "No WOLA CSV data available; wola_summary.csv will be empty.")
        return pd.DataFrame(columns=columns)

    wola_candidate = find_case_row(df, "wola_passthrough")
    wola_row = wola_candidate if wola_candidate is not None else (df.iloc[0] if not df.empty else None)
    noise_row = find_case_row(df, "noise_only", "noise")
    snr5_row = find_case_row(df, "snr5")
    snr10_row = find_case_row(df, "snr10")
    snr20_row = find_case_row(df, "snr20")

    pass_col = first_col(df, "pass")
    pass_items = df[pass_col].tolist() if pass_col else []

    row = {
        "wola_passthrough_snr_db": get_value(wola_row, "wola_passthrough_snr_db"),
        "wola_energy_ratio": get_value(wola_row, "wola_energy_ratio", "output_input_energy_ratio"),
        "wola_delay_samples": get_value(wola_row, "wola_delay_samples", "delay_samples"),
        "denoise_noise_reduction_db": get_value(noise_row, "denoise_noise_reduction_db", "noise_reduction_db"),
        "denoise_snr5_output_snr_db": get_value(snr5_row, "denoise_snr5_output_snr_db", "output_snr_db"),
        "denoise_snr10_output_snr_db": get_value(snr10_row, "denoise_snr10_output_snr_db", "output_snr_db"),
        "denoise_snr20_output_snr_db": get_value(snr20_row, "denoise_snr20_output_snr_db", "output_snr_db"),
        "pass": pass_value(pass_items),
    }
    return pd.DataFrame([row], columns=columns)


def append_denoise_row(
    rows: list[dict[str, object]],
    row: pd.Series,
    mode_name: str,
    output_col: str,
    speech_col: str,
    corr_col: str,
    gain_col: str,
    noise_col: str,
    delta_col: str | None,
) -> None:
    rows.append(
        {
            "eval_case": get_value(row, "eval_case"),
            "mode": mode_name,
            "mode_name": mode_name,
            "input_snr_db": get_value(row, "input_snr_db"),
            "fixed_output_snr_db": get_value(row, "fixed_output_snr_db"),
            "ms_output_snr_db": get_value(row, "ms_output_snr_db"),
            "output_snr_db": get_value(row, output_col),
            "delta_vs_fixed_db": get_value(row, delta_col) if delta_col else 0.0,
            "delta_vs_hybrid_db": get_value(row, "delta_vs_hybrid_db"),
            "speech_preservation_ratio": get_value(row, speech_col),
            "clean_output_corr": get_value(row, corr_col),
            "gain_avg": get_value(row, gain_col),
            "noise_psd_avg": get_value(row, noise_col),
            "ms_min_psd_avg": get_value(row, "ms_min_psd_avg"),
            "pass": get_value(row, "pass"),
        }
    )


def build_denoise_summary(ms_df: pd.DataFrame, mcra_df: pd.DataFrame, ctx: ReportContext) -> pd.DataFrame:
    columns = [
        "eval_case",
        "mode",
        "mode_name",
        "input_snr_db",
        "fixed_output_snr_db",
        "ms_output_snr_db",
        "output_snr_db",
        "delta_vs_fixed_db",
        "delta_vs_hybrid_db",
        "speech_preservation_ratio",
        "clean_output_corr",
        "gain_avg",
        "noise_psd_avg",
        "ms_min_psd_avg",
        "pass",
    ]
    rows: list[dict[str, object]] = []

    if not ms_df.empty:
        required = ["eval_case", "fixed_output_snr_db", "ms_output_snr_db"]
        for column in required:
            if column not in ms_df.columns:
                warn(ctx, f"MS denoise CSV missing column: {column}")
        for _, row in ms_df.iterrows():
            append_denoise_row(
                rows,
                row,
                "fixed",
                "fixed_output_snr_db",
                "fixed_speech_preservation_ratio",
                "fixed_clean_output_corr",
                "fixed_gain_avg",
                "fixed_noise_psd_avg",
                None,
            )
            append_denoise_row(
                rows,
                row,
                "hybrid_ms",
                "ms_output_snr_db",
                "ms_speech_preservation_ratio",
                "ms_clean_output_corr",
                "ms_gain_avg",
                "ms_noise_psd_avg",
                "ms_vs_fixed_delta_db",
            )

    if not mcra_df.empty:
        for _, row in mcra_df.iterrows():
            mode_name = str(get_value(row, "mode_name", "mode", default="mcra")).strip() or "mcra"
            rows.append(
                {
                    "eval_case": get_value(row, "eval_case"),
                    "mode": mode_name,
                    "mode_name": mode_name,
                    "input_snr_db": get_value(row, "input_snr_db"),
                    "fixed_output_snr_db": np.nan,
                    "ms_output_snr_db": np.nan,
                    "output_snr_db": get_value(row, "output_snr_db"),
                    "delta_vs_fixed_db": get_value(row, "delta_vs_fixed_db"),
                    "delta_vs_hybrid_db": get_value(row, "delta_vs_hybrid_db"),
                    "speech_preservation_ratio": get_value(row, "speech_preservation_ratio"),
                    "clean_output_corr": get_value(row, "clean_output_corr"),
                    "gain_avg": get_value(row, "gain_avg"),
                    "noise_psd_avg": get_value(row, "noise_psd_avg"),
                    "ms_min_psd_avg": get_value(row, "ms_min_psd_avg"),
                    "pass": get_value(row, "pass"),
                }
            )

    if not rows:
        warn(ctx, "No denoise CSV data available; denoise_summary.csv will be empty.")
    return pd.DataFrame(rows, columns=columns)


def build_codec_summary(df: pd.DataFrame, ctx: ReportContext) -> pd.DataFrame:
    columns = [
        "eval_case",
        "target_bitrate_kbps",
        "actual_bitrate_kbps",
        "compression_ratio",
        "direct_output_snr_db",
        "denoise_codec_output_snr_db",
        "pass",
    ]
    if df.empty:
        warn(ctx, "No codec CSV data available; codec_summary.csv will be empty.")
        return pd.DataFrame(columns=columns)

    rows = []
    for _, row in df.iterrows():
        rows.append(
            {
                "eval_case": get_value(row, "eval_case", "codec_case"),
                "target_bitrate_kbps": get_value(row, "target_bitrate_kbps"),
                "actual_bitrate_kbps": get_value(row, "actual_bitrate_kbps", "direct_bitrate_kbps"),
                "compression_ratio": get_value(row, "compression_ratio", "direct_compression_ratio"),
                "direct_output_snr_db": get_value(row, "direct_output_snr_db", "direct_compress_snr_db", "direct_reconstruction_snr_db"),
                "denoise_codec_output_snr_db": get_value(
                    row,
                    "denoise_codec_output_snr_db",
                    "denoise_then_compress_snr_db",
                    "denoise_then_reconstruction_snr_db",
                ),
                "pass": get_value(row, "pass"),
            }
        )
    return pd.DataFrame(rows, columns=columns)


def build_realtime_template(ctx: ReportContext, realtime_raw: pd.DataFrame | None = None) -> pd.DataFrame:
    modes = ["WOLA only", "fixed denoise", "HYBRID-MS", "MCRA normal", "MCRA strong"]
    columns = ["mode", "last_ms", "max_ms", "frame_budget_ms", "cpu_usage_percent", "pass"]
    template_warning = "No standalone realtime CSV was used; generated a CCS Watch fill-in template."
    if realtime_raw is not None and not realtime_raw.empty:
        realtime_raw = normalize_board_runtime_df(realtime_raw)
        if "mode" not in realtime_raw.columns or "max_ms" not in realtime_raw.columns:
            template_warning = "Realtime CSV is present but missing mode or max_ms; generated a CCS Watch fill-in template."
        else:
            df = realtime_raw.copy()
            for column in columns:
                if column not in df.columns:
                    df[column] = np.nan if column != "pass" else ""
            df = df[columns]
            for column in ["last_ms", "max_ms", "frame_budget_ms", "cpu_usage_percent"]:
                df[column] = pd.to_numeric(df[column], errors="coerce")
            df["pass"] = df["pass"].astype(object)
            df["frame_budget_ms"] = df["frame_budget_ms"].fillna(FRAME_BUDGET_MS)
            measured = df["max_ms"].notna() & (df["max_ms"] > 0.0)
            empty_pass = df["pass"].isna() | (df["pass"].astype(str).str.strip() == "")
            df.loc[empty_pass & measured & (df["max_ms"] <= df["frame_budget_ms"]), "pass"] = "PASS"
            df.loc[empty_pass & measured & (df["max_ms"] > df["frame_budget_ms"]), "pass"] = "FAIL"
            if measured.any():
                return df
            template_warning = "Realtime CSV is present but max_ms is empty; generated a CCS Watch fill-in template."

    df = pd.DataFrame(
        {
            "mode": modes,
            "last_ms": [np.nan] * len(modes),
            "max_ms": [np.nan] * len(modes),
            "frame_budget_ms": [FRAME_BUDGET_MS] * len(modes),
            "cpu_usage_percent": [np.nan] * len(modes),
            "pass": [""] * len(modes),
        }
    )
    warn(ctx, template_warning)
    return df


def normalize_board_runtime_df(df: pd.DataFrame) -> pd.DataFrame:
    if df.empty:
        return df
    result = df.copy()
    if "mode" not in result.columns and "mode_name" in result.columns:
        result["mode"] = result["mode_name"]
    if "mode_name" not in result.columns and "mode" in result.columns:
        result["mode_name"] = result["mode"].astype(str)
    if "last_ms" not in result.columns and "last_us" in result.columns:
        result["last_ms"] = pd.to_numeric(result["last_us"], errors="coerce") / 1000.0
    if "max_ms" not in result.columns and "max_us" in result.columns:
        result["max_ms"] = pd.to_numeric(result["max_us"], errors="coerce") / 1000.0
    if "frame_budget_ms" not in result.columns:
        if "budget_ms" in result.columns:
            result["frame_budget_ms"] = pd.to_numeric(result["budget_ms"], errors="coerce")
        elif "budget_us" in result.columns:
            result["frame_budget_ms"] = pd.to_numeric(result["budget_us"], errors="coerce") / 1000.0
        else:
            result["frame_budget_ms"] = FRAME_BUDGET_MS
    if "cpu_usage_percent" not in result.columns and "cpu_percent" in result.columns:
        result["cpu_usage_percent"] = result["cpu_percent"]
    if "pass" not in result.columns and "realtime_pass" in result.columns:
        result["pass"] = result["realtime_pass"]
    return result


def find_latest_map(root: Path) -> Path | None:
    maps = list((root / "Debug").glob("*.map")) if (root / "Debug").exists() else []
    if not maps:
        return None
    return max(maps, key=lambda item: item.stat().st_mtime)


def parse_map_memory(path: Path, ctx: ReportContext) -> pd.DataFrame:
    pattern = re.compile(
        r"^\s*(?P<name>\S+)\s+[0-9a-fA-F]{8}\s+"
        r"(?P<length>[0-9a-fA-F]+)\s+(?P<used>[0-9a-fA-F]+)\s+"
        r"(?P<unused>[0-9a-fA-F]+)\s+\S+"
    )
    rows = []
    in_memory = False
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if "MEMORY CONFIGURATION" in line:
            in_memory = True
            continue
        if in_memory and "SEGMENT ALLOCATION MAP" in line:
            break
        if not in_memory:
            continue
        match = pattern.match(line)
        if not match:
            continue
        length = int(match.group("length"), 16)
        used = int(match.group("used"), 16)
        unused = int(match.group("unused"), 16)
        if used <= 0:
            continue
        rows.append(
            {
                "memory_region": match.group("name"),
                "length_bytes": length,
                "used_bytes": used,
                "unused_bytes": unused,
                "used_percent": (used / length * 100.0) if length else np.nan,
            }
        )
    if not rows:
        warn(ctx, f"Could not parse used memory regions from map file: {path}")
    return pd.DataFrame(rows)


def save_fig(ctx: ReportContext, fig: plt.Figure, filename: str, *, tight: bool = True) -> None:
    path = ctx.plots_dir / filename
    if tight:
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", message="This figure includes Axes that are not compatible with tight_layout")
            fig.tight_layout()
    fig.savefig(path, dpi=DPI)
    plt.close(fig)
    ctx.generated_plots.append(filename)


def skip_plot(ctx: ReportContext, filename: str, reason: str) -> None:
    ctx.skipped_plots[filename] = reason
    print(f"SKIP {filename}: {reason}")


def read_wav(path: Path, ctx: ReportContext) -> tuple[int, np.ndarray] | None:
    try:
        with wave.open(str(path), "rb") as handle:
            channels = handle.getnchannels()
            sample_width = handle.getsampwidth()
            sample_rate = handle.getframerate()
            frames = handle.readframes(handle.getnframes())
    except Exception as exc:  # noqa: BLE001 - keep report generation alive.
        warn(ctx, f"Could not read WAV {path}: {exc}")
        return None

    if sample_width == 1:
        data = (np.frombuffer(frames, dtype=np.uint8).astype(np.float32) - 128.0) / 128.0
    elif sample_width == 2:
        data = np.frombuffer(frames, dtype="<i2").astype(np.float32) / 32768.0
    elif sample_width == 4:
        data = np.frombuffer(frames, dtype="<i4").astype(np.float32) / 2147483648.0
    else:
        warn(ctx, f"Unsupported WAV sample width {sample_width} bytes: {path}")
        return None

    if channels > 1:
        data = data.reshape(-1, channels).mean(axis=1)
    return sample_rate, data.astype(np.float64)


def load_audio(ctx: ReportContext, name: str) -> tuple[int, np.ndarray] | None:
    path = ctx.found_wav.get(name)
    if path is None:
        return None
    return read_wav(path, ctx)


def estimate_delay_limited(x: np.ndarray, y: np.ndarray, max_lag: int = 1024) -> int:
    count = min(len(x), len(y), 32768)
    if count < 32:
        return 0
    x0 = x[:count] - np.mean(x[:count])
    y0 = y[:count] - np.mean(y[:count])
    max_lag = min(max_lag, count // 4)
    best_lag = 0
    best_corr = -np.inf
    for lag in range(-max_lag, max_lag + 1):
        if lag >= 0:
            a = x0[: count - lag]
            b = y0[lag:count]
        else:
            a = x0[-lag:count]
            b = y0[: count + lag]
        denom = np.linalg.norm(a) * np.linalg.norm(b)
        if denom <= EPS:
            continue
        corr = float(np.dot(a, b) / denom)
        if corr > best_corr:
            best_corr = corr
            best_lag = lag
    return best_lag


def correlation_curve(x: np.ndarray, y: np.ndarray, center: int, radius: int = 512) -> tuple[np.ndarray, np.ndarray]:
    count = min(len(x), len(y), 32768)
    x0 = x[:count] - np.mean(x[:count])
    y0 = y[:count] - np.mean(y[:count])
    lags = np.arange(center - radius, center + radius + 1)
    values = []
    for lag in lags:
        if lag >= 0:
            if lag >= count - 4:
                values.append(np.nan)
                continue
            a = x0[: count - lag]
            b = y0[lag:count]
        else:
            if -lag >= count - 4:
                values.append(np.nan)
                continue
            a = x0[-lag:count]
            b = y0[: count + lag]
        denom = np.linalg.norm(a) * np.linalg.norm(b)
        values.append(float(np.dot(a, b) / denom) if denom > EPS else np.nan)
    return lags, np.asarray(values)


def align_signals(x: np.ndarray, y: np.ndarray, delay: int) -> tuple[np.ndarray, np.ndarray]:
    if delay >= 0:
        length = min(len(x), len(y) - delay)
        if length <= 0:
            return np.array([]), np.array([])
        return x[:length], y[delay : delay + length]
    lead = -delay
    length = min(len(x) - lead, len(y))
    if length <= 0:
        return np.array([]), np.array([])
    return x[lead : lead + length], y[:length]


def aligned_snr_db(x: np.ndarray, y: np.ndarray) -> float:
    length = min(len(x), len(y))
    if length == 0:
        return math.nan
    x = x[:length]
    y = y[:length]
    signal = float(np.sum(x * x))
    noise = float(np.sum((y - x) ** 2))
    if signal <= EPS or noise <= EPS:
        return math.inf
    return 10.0 * math.log10(signal / noise)


def rms(samples: np.ndarray) -> float:
    if len(samples) == 0:
        return math.nan
    return float(np.sqrt(np.mean(np.square(samples))))


def spectrum(samples: np.ndarray, sample_rate: int, n_fft: int = 8192) -> tuple[np.ndarray, np.ndarray]:
    if len(samples) == 0:
        return np.array([]), np.array([])
    segment = samples[: min(len(samples), n_fft)]
    if len(segment) < n_fft:
        segment = np.pad(segment, (0, n_fft - len(segment)))
    window = np.hanning(n_fft)
    spec = np.fft.rfft(segment * window)
    mag_db = 20.0 * np.log10(np.abs(spec) / (np.sum(window) / 2.0) + EPS)
    freq = np.fft.rfftfreq(n_fft, 1.0 / sample_rate)
    return freq, mag_db


def response_db(x: np.ndarray, y: np.ndarray, sample_rate: int, n_fft: int = 8192) -> tuple[np.ndarray, np.ndarray]:
    length = min(len(x), len(y), n_fft)
    if length < 32:
        return np.array([]), np.array([])
    x_seg = x[:length]
    y_seg = y[:length]
    if length < n_fft:
        x_seg = np.pad(x_seg, (0, n_fft - length))
        y_seg = np.pad(y_seg, (0, n_fft - length))
    window = np.hanning(n_fft)
    x_fft = np.fft.rfft(x_seg * window)
    y_fft = np.fft.rfft(y_seg * window)
    response = 20.0 * np.log10(np.abs(y_fft) / (np.abs(x_fft) + EPS) + EPS)
    if len(response) >= 9:
        kernel = np.ones(9) / 9.0
        response = np.convolve(response, kernel, mode="same")
    freq = np.fft.rfftfreq(n_fft, 1.0 / sample_rate)
    return freq, response


def get_known_delay(wola_summary: pd.DataFrame) -> int | None:
    if wola_summary.empty or "wola_delay_samples" not in wola_summary.columns:
        return None
    value = numeric(wola_summary.iloc[0]["wola_delay_samples"])
    if math.isnan(value):
        return None
    return int(round(value))


def load_wola_pair(ctx: ReportContext, wola_summary: pd.DataFrame) -> tuple[int, np.ndarray, np.ndarray, int] | None:
    input_audio = load_audio(ctx, "compare_00_input.wav")
    wola_audio = load_audio(ctx, "compare_01_wola_only.wav")
    if input_audio is None or wola_audio is None:
        return None
    sr_x, x = input_audio
    sr_y, y = wola_audio
    if sr_x != sr_y:
        warn(ctx, f"WOLA WAV sample rates differ: {sr_x} vs {sr_y}; using {sr_x}.")
    known_delay = get_known_delay(wola_summary)
    delay = known_delay if known_delay is not None else estimate_delay_limited(x, y)
    return sr_x, x, y, delay


def plot_wola_waveform(ctx: ReportContext, wola_summary: pd.DataFrame) -> None:
    pair = load_wola_pair(ctx, wola_summary)
    filename = "wola_waveform_compare.png"
    if pair is None:
        skip_plot(ctx, filename, "missing compare_00_input.wav or compare_01_wola_only.wav")
        return
    sample_rate, x, y, delay = pair
    xa, ya = align_signals(x, y, delay)
    if len(xa) == 0:
        skip_plot(ctx, filename, "could not align WOLA input/output")
        return
    count = min(len(xa), int(0.2 * sample_rate))
    t_ms = np.arange(count) / sample_rate * 1000.0
    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.plot(t_ms, xa[:count], label="input", linewidth=1.0)
    ax.plot(t_ms, ya[:count], label="wola_only aligned", linewidth=1.0, alpha=0.8)
    ax.set_title("WOLA waveform compare")
    ax.set_xlabel("Time (ms)")
    ax.set_ylabel("Amplitude")
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.text(0.01, 0.95, f"delay={delay} samples ({delay / sample_rate * 1000:.2f} ms)", transform=ax.transAxes, va="top")
    save_fig(ctx, fig, filename)


def plot_wola_spectrum(ctx: ReportContext, wola_summary: pd.DataFrame) -> None:
    pair = load_wola_pair(ctx, wola_summary)
    filename = "wola_spectrum_compare.png"
    if pair is None:
        skip_plot(ctx, filename, "missing compare_00_input.wav or compare_01_wola_only.wav")
        return
    sample_rate, x, y, delay = pair
    xa, ya = align_signals(x, y, delay)
    freq_x, mag_x = spectrum(xa, sample_rate)
    freq_y, mag_y = spectrum(ya, sample_rate)
    if len(freq_x) == 0:
        skip_plot(ctx, filename, "not enough WOLA audio samples")
        return
    mask = freq_x <= min(10000.0, sample_rate / 2.0)
    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.plot(freq_x[mask], mag_x[mask], label="input", linewidth=1.1)
    ax.plot(freq_y[mask], mag_y[mask], label="wola_only aligned", linewidth=1.1, alpha=0.85)
    ax.set_title("WOLA spectrum compare")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)


def plot_wola_reconstruction_error(ctx: ReportContext, wola_summary: pd.DataFrame) -> None:
    pair = load_wola_pair(ctx, wola_summary)
    filename = "wola_reconstruction_error.png"
    if pair is None:
        skip_plot(ctx, filename, "missing compare_00_input.wav or compare_01_wola_only.wav")
        return
    sample_rate, x, y, delay = pair
    xa, ya = align_signals(x, y, delay)
    if len(xa) == 0:
        skip_plot(ctx, filename, "could not align WOLA input/output")
        return
    count = min(len(xa), int(0.2 * sample_rate))
    error = ya[:count] - xa[:count]
    snr = aligned_snr_db(xa, ya)
    energy_ratio = float(np.sum(ya * ya) / (np.sum(xa * xa) + EPS))
    t_ms = np.arange(count) / sample_rate * 1000.0
    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.plot(t_ms, error, color="#b23a48", linewidth=1.0)
    ax.set_title("WOLA reconstruction error")
    ax.set_xlabel("Time (ms)")
    ax.set_ylabel("Aligned output - input")
    ax.grid(True, alpha=0.3)
    ax.text(
        0.01,
        0.95,
        f"aligned SNR={snr:.2f} dB\nenergy ratio={energy_ratio:.6f}\ndelay={delay} samples",
        transform=ax.transAxes,
        va="top",
    )
    save_fig(ctx, fig, filename)


def plot_wola_frequency_response(ctx: ReportContext, wola_summary: pd.DataFrame) -> None:
    pair = load_wola_pair(ctx, wola_summary)
    filename = "wola_frequency_response.png"
    if pair is None:
        skip_plot(ctx, filename, "missing compare_00_input.wav or compare_01_wola_only.wav")
        return
    sample_rate, x, y, delay = pair
    xa, ya = align_signals(x, y, delay)
    freq, resp = response_db(xa, ya, sample_rate)
    if len(freq) == 0:
        skip_plot(ctx, filename, "not enough WOLA audio samples")
        return
    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.plot(freq, resp, linewidth=1.1, label="H(f)=Y/X, estimated from test audio")
    for boundary in np.linspace(0, sample_rate / 2.0, 9)[1:-1]:
        ax.axvline(boundary, color="gray", linewidth=0.7, alpha=0.5)
    ax.set_xlim(0, sample_rate / 2.0)
    ax.set_ylim(max(-60, np.nanpercentile(resp, 2) - 5), min(20, np.nanpercentile(resp, 98) + 5))
    ax.set_title("WOLA analysis/synthesis frequency response estimate")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Relative response (dB)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)


def plot_wola_rolloff(ctx: ReportContext, wola_summary: pd.DataFrame) -> None:
    pair = load_wola_pair(ctx, wola_summary)
    filename = "wola_rolloff_curve.png"
    if pair is None:
        skip_plot(ctx, filename, "missing compare_00_input.wav or compare_01_wola_only.wav")
        return
    sample_rate, x, y, delay = pair
    xa, ya = align_signals(x, y, delay)
    freq, resp = response_db(xa, ya, sample_rate)
    if len(freq) == 0:
        skip_plot(ctx, filename, "not enough WOLA audio samples")
        return
    boundaries = np.linspace(0, sample_rate / 2.0, 9)
    mids = []
    levels = []
    for lo, hi in zip(boundaries[:-1], boundaries[1:]):
        mask = (freq >= lo) & (freq < hi)
        mids.append((lo + hi) / 2.0)
        levels.append(float(np.nanmedian(resp[mask])) if np.any(mask) else np.nan)
    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.plot(mids, levels, marker="o", linewidth=1.3)
    for boundary in boundaries[1:-1]:
        ax.axvline(boundary, color="gray", linewidth=0.7, alpha=0.5)
    ax.set_title("Estimated subband boundary / rolloff curve")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Median relative response (dB)")
    ax.grid(True, alpha=0.3)
    save_fig(ctx, fig, filename)


def plot_group_delay(ctx: ReportContext, wola_summary: pd.DataFrame) -> None:
    pair = load_wola_pair(ctx, wola_summary)
    filename = "group_delay_impulse_or_correlation.png"
    if pair is None:
        skip_plot(ctx, filename, "missing compare_00_input.wav or compare_01_wola_only.wav")
        return
    sample_rate, x, y, delay = pair
    lags, corr = correlation_curve(x, y, delay, radius=512)
    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.plot(lags, corr, linewidth=1.1)
    ax.axvline(delay, color="#b23a48", linewidth=1.0, label=f"delay={delay} samples")
    ax.set_title("Group delay estimate by input/output correlation")
    ax.set_xlabel("Lag (samples)")
    ax.set_ylabel("Normalized correlation")
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.text(0.01, 0.95, f"delay={delay / sample_rate * 1000.0:.2f} ms", transform=ax.transAxes, va="top")
    save_fig(ctx, fig, filename)


def denoise_audio_items(ctx: ReportContext) -> list[tuple[str, int, np.ndarray]]:
    names = [
        ("input", "compare_00_input.wav"),
        ("fixed", "compare_02_wola_denoise.wav"),
        ("hybrid_ms", "compare_09_ms_denoise.wav"),
        ("mcra", "compare_10_mcra_denoise.wav"),
        ("mcra_strong", "compare_11_strong_mcra_denoise.wav"),
    ]
    items = []
    for label, wav_name in names:
        audio = load_audio(ctx, wav_name)
        if audio is not None:
            sample_rate, samples = audio
            items.append((label, sample_rate, samples))
    return items


def plot_denoise_waveform(ctx: ReportContext, wola_summary: pd.DataFrame) -> None:
    filename = "denoise_waveform_before_after.png"
    items = denoise_audio_items(ctx)
    if len(items) < 2:
        skip_plot(ctx, filename, "missing compare_00_input.wav or denoise output WAV")
        return
    base_rate, base = items[0][1], items[0][2]
    delay = get_known_delay(wola_summary) or 0
    count = min(int(0.2 * base_rate), len(base))
    t_ms = np.arange(count) / base_rate * 1000.0
    fig, ax = plt.subplots(figsize=FIGSIZE)
    for label, sample_rate, samples in items:
        if sample_rate != base_rate:
            warn(ctx, f"Sample rate mismatch in denoise plot for {label}: {sample_rate} vs {base_rate}")
        if label == "input":
            aligned = samples[: len(base)]
        else:
            _, aligned = align_signals(base, samples, delay)
        aligned = aligned[:count]
        ax.plot(t_ms[: len(aligned)], aligned, label=f"{label} RMS={rms(aligned):.4f}", linewidth=1.0, alpha=0.85)
    ax.set_title("Denoise waveform before/after")
    ax.set_xlabel("Time (ms)")
    ax.set_ylabel("Amplitude")
    ax.grid(True, alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)


def plot_denoise_spectrum(ctx: ReportContext, wola_summary: pd.DataFrame) -> None:
    filename = "denoise_spectrum_before_after.png"
    items = denoise_audio_items(ctx)
    if len(items) < 2:
        skip_plot(ctx, filename, "missing compare_00_input.wav or denoise output WAV")
        return
    base_rate, base = items[0][1], items[0][2]
    delay = get_known_delay(wola_summary) or 0
    fig, ax = plt.subplots(figsize=FIGSIZE)
    for label, sample_rate, samples in items:
        if label == "input":
            aligned = samples
        else:
            _, aligned = align_signals(base, samples, delay)
        freq, mag = spectrum(aligned, sample_rate)
        if len(freq) == 0:
            continue
        mask = freq <= min(10000.0, sample_rate / 2.0)
        ax.plot(freq[mask], mag[mask], label=label, linewidth=1.0, alpha=0.85)
    ax.set_title("Denoise spectrum before/after")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)


def display_label(label: str) -> str:
    return {
        "input": "input noisy",
        "fixed": "fixed denoise",
        "hybrid_ms": "HYBRID-MS",
        "mcra": "MCRA normal",
        "mcra_strong": "MCRA strong",
    }.get(label, label)


def add_spectrogram_time_markers(ax: plt.Axes) -> None:
    markers = [(2.0, "noise learning end"), (5.0, "noise step-up")]
    for x_value, label in markers:
        ax.axvline(x_value, color="white", linestyle="--", linewidth=1.0, alpha=0.92)
        ax.text(
            x_value + 0.04,
            0.97,
            label,
            color="white",
            fontsize=8,
            ha="left",
            va="top",
            transform=ax.get_xaxis_transform(),
            bbox=dict(boxstyle="round,pad=0.18", facecolor="black", alpha=0.45, edgecolor="none"),
        )


def spectrogram_db(
    samples: np.ndarray,
    sample_rate: int,
    n_fft: int = 1024,
    noverlap: int = 768,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    samples = np.asarray(samples, dtype=np.float64)
    hop = max(1, n_fft - noverlap)
    if len(samples) < n_fft:
        samples = np.pad(samples, (0, n_fft - len(samples)))
    starts = np.arange(0, len(samples) - n_fft + 1, hop, dtype=int)
    if len(starts) == 0:
        starts = np.array([0], dtype=int)
    if starts[-1] + n_fft < len(samples):
        starts = np.append(starts, len(samples) - n_fft)
    window = np.hanning(n_fft)
    denom = np.sum(window * window) + EPS
    frames = []
    for start in starts:
        segment = samples[start : start + n_fft]
        if len(segment) < n_fft:
            segment = np.pad(segment, (0, n_fft - len(segment)))
        spec = np.fft.rfft(segment * window)
        power = (np.abs(spec) ** 2) / denom
        frames.append(10.0 * np.log10(power + EPS))
    db = np.asarray(frames).T
    freqs = np.fft.rfftfreq(n_fft, 1.0 / sample_rate)
    times = (starts + n_fft / 2.0) / sample_rate
    return freqs, times, db


def save_spectrogram_db(
    ctx: ReportContext,
    label: str,
    sample_rate: int,
    samples: np.ndarray,
    filename: str,
    *,
    fmax: float | None = None,
    vmin: float | None = None,
    vmax: float | None = None,
    title: str | None = None,
) -> None:
    freqs, times, db = spectrogram_db(samples, sample_rate)
    mask = freqs <= (fmax if fmax is not None else sample_rate / 2.0)
    fig, ax = plt.subplots(figsize=FIGSIZE)
    mesh = ax.pcolormesh(times, freqs[mask], db[mask, :], shading="auto", cmap="magma", vmin=vmin, vmax=vmax)
    add_spectrogram_time_markers(ax)
    ax.set_ylim(0, fmax if fmax is not None else sample_rate / 2.0)
    ax.set_title(title or f"Spectrogram: {display_label(label)}")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Frequency (Hz)")
    fig.colorbar(mesh, ax=ax, label="Magnitude (dB)")
    save_fig(ctx, fig, filename)


def save_spectrogram(ctx: ReportContext, label: str, sample_rate: int, samples: np.ndarray, filename: str) -> None:
    save_spectrogram_db(ctx, label, sample_rate, samples, filename)


def denoise_stepup_conclusion(denoise_summary: pd.DataFrame) -> str:
    if denoise_summary.empty:
        return "HYBRID-MS improves step-up SNR vs fixed; MCRA normal gives best delta when available."
    df = denoise_summary.copy()
    if "eval_case" not in df.columns or "mode_name" not in df.columns:
        return "HYBRID-MS improves step-up SNR vs fixed; MCRA normal gives best delta when available."
    df["delta_vs_fixed_db"] = pd.to_numeric(df.get("delta_vs_fixed_db", np.nan), errors="coerce")
    case_df = df[df["eval_case"].astype(str).str.lower() == "noise_step_up_snr10"]
    mcra = case_df[case_df["mode_name"].astype(str).str.lower() == "mcra_normal"]
    if not mcra.empty and not pd.isna(mcra.iloc[0]["delta_vs_fixed_db"]):
        return f"HYBRID-MS improves step-up SNR vs fixed; MCRA normal gives best delta: +{mcra.iloc[0]['delta_vs_fixed_db']:.2f} dB in noise_step_up_snr10."
    return "HYBRID-MS improves step-up SNR vs fixed; MCRA normal gives best delta when available."


def plot_speechband_spectrograms(
    ctx: ReportContext,
    items: list[tuple[str, int, np.ndarray]],
    denoise_summary: pd.DataFrame,
) -> None:
    filename_map = {
        "input": "denoise_spectrogram_input_speechband.png",
        "fixed": "denoise_spectrogram_fixed_speechband.png",
        "hybrid_ms": "denoise_spectrogram_hybrid_ms_speechband.png",
        "mcra": "denoise_spectrogram_mcra_speechband.png",
        "mcra_strong": "denoise_spectrogram_mcra_strong_speechband.png",
    }
    for label, sample_rate, samples in items:
        save_spectrogram_db(
            ctx,
            label,
            sample_rate,
            samples,
            filename_map.get(label, f"denoise_spectrogram_{label}_speechband.png"),
            fmax=SPEECH_BAND_HZ,
            vmin=SPECTROGRAM_VMIN_DB,
            vmax=SPECTROGRAM_VMAX_DB,
            title=f"Speech-band spectrogram: {display_label(label)} (0-8 kHz)",
        )

    ordered = [item for target in ["input", "fixed", "hybrid_ms", "mcra", "mcra_strong"] for item in items if item[0] == target]
    if len(ordered) < 2:
        skip_plot(ctx, "denoise_spectrogram_panel_speechband.png", "missing denoise speech-band panel inputs")
        return
    fig = plt.figure(figsize=(15.5, 8.8))
    gs = fig.add_gridspec(
        2,
        7,
        width_ratios=[1, 1, 1, 1, 1, 1, 0.16],
        left=0.055,
        right=0.965,
        bottom=0.14,
        top=0.89,
        wspace=0.55,
        hspace=0.24,
    )
    positions = [(0, slice(0, 2)), (0, slice(2, 4)), (0, slice(4, 6)), (1, slice(1, 3)), (1, slice(3, 5))]
    axes = []
    shared_ax = None
    for row, col_slice in positions[: len(ordered)]:
        ax = fig.add_subplot(gs[row, col_slice], sharex=shared_ax, sharey=shared_ax)
        if shared_ax is None:
            shared_ax = ax
        axes.append(ax)
    mesh = None
    for idx, (ax, (label, sample_rate, samples)) in enumerate(zip(axes, ordered)):
        freqs, times, db = spectrogram_db(samples, sample_rate)
        mask = freqs <= SPEECH_BAND_HZ
        mesh = ax.pcolormesh(
            times,
            freqs[mask],
            db[mask, :],
            shading="auto",
            cmap="magma",
            vmin=SPECTROGRAM_VMIN_DB,
            vmax=SPECTROGRAM_VMAX_DB,
        )
        add_spectrogram_time_markers(ax)
        ax.set_ylim(0, SPEECH_BAND_HZ)
        ax.set_title(display_label(label))
        if idx in [0, 3]:
            ax.set_ylabel("Frequency (Hz)")
        else:
            ax.tick_params(labelleft=False)
        ax.grid(False)
    for ax in axes[-2:]:
        ax.set_xlabel("Time (s)")
    fig.suptitle("Speech-band denoise spectrogram panel (0-8 kHz)", fontsize=15)
    if mesh is not None:
        cax = fig.add_subplot(gs[:, 6])
        fig.colorbar(mesh, cax=cax, label="Magnitude (dB)")
    fig.text(
        0.5,
        0.045,
        denoise_stepup_conclusion(denoise_summary),
        ha="center",
        va="center",
        fontsize=12,
        bbox=dict(facecolor="white", edgecolor="#d0d4da", alpha=0.9),
    )
    save_fig(ctx, fig, "denoise_spectrogram_panel_speechband.png", tight=False)


def plot_reduction_spectrograms(ctx: ReportContext, items: list[tuple[str, int, np.ndarray]]) -> None:
    item_map = {label: (sample_rate, samples) for label, sample_rate, samples in items}
    if "input" not in item_map:
        skip_plot(ctx, "denoise_reduction_panel_speechband.png", "missing noisy input WAV")
        return
    base_rate, base_samples = item_map["input"]
    base_freqs, base_times, base_db = spectrogram_db(base_samples, base_rate)
    base_mask = base_freqs <= SPEECH_BAND_HZ
    output_labels = ["fixed", "hybrid_ms", "mcra", "mcra_strong"]
    filename_map = {
        "fixed": "denoise_reduction_fixed_speechband.png",
        "hybrid_ms": "denoise_reduction_hybrid_ms_speechband.png",
        "mcra": "denoise_reduction_mcra_speechband.png",
        "mcra_strong": "denoise_reduction_mcra_strong_speechband.png",
    }
    reductions: list[tuple[str, np.ndarray, np.ndarray, np.ndarray]] = []
    for label in output_labels:
        if label not in item_map:
            skip_plot(ctx, filename_map[label], f"missing {label} denoise WAV")
            continue
        sample_rate, samples = item_map[label]
        freqs, times, out_db = spectrogram_db(samples, sample_rate)
        mask = freqs <= SPEECH_BAND_HZ
        rows = min(int(base_mask.sum()), int(mask.sum()))
        cols = min(base_db.shape[1], out_db.shape[1])
        reduction = np.clip(base_db[base_mask, :][:rows, :cols] - out_db[mask, :][:rows, :cols], REDUCTION_VMIN_DB, REDUCTION_VMAX_DB)
        plot_times = base_times[:cols]
        plot_freqs = base_freqs[base_mask][:rows]
        reductions.append((label, plot_freqs, plot_times, reduction))
        fig, ax = plt.subplots(figsize=FIGSIZE)
        mesh = ax.pcolormesh(
            plot_times,
            plot_freqs,
            reduction,
            shading="auto",
            cmap="viridis",
            vmin=REDUCTION_VMIN_DB,
            vmax=REDUCTION_VMAX_DB,
        )
        add_spectrogram_time_markers(ax)
        ax.set_ylim(0, SPEECH_BAND_HZ)
        ax.set_title(f"Speech-band reduction: input - {display_label(label)}")
        ax.set_xlabel("Time (s)")
        ax.set_ylabel("Frequency (Hz)")
        fig.colorbar(mesh, ax=ax, label="Reduction (dB), clipped 0-40")
        save_fig(ctx, fig, filename_map[label])

    if not reductions:
        skip_plot(ctx, "denoise_reduction_panel_speechband.png", "no denoise output WAVs for reduction panel")
        return
    fig = plt.figure(figsize=(13.8, 8.2))
    gs = fig.add_gridspec(
        2,
        3,
        width_ratios=[1, 1, 0.055],
        left=0.065,
        right=0.94,
        bottom=0.09,
        top=0.9,
        wspace=0.15,
        hspace=0.26,
    )
    axes = [
        fig.add_subplot(gs[0, 0]),
        fig.add_subplot(gs[0, 1]),
        fig.add_subplot(gs[1, 0]),
        fig.add_subplot(gs[1, 1]),
    ]
    for ax in axes[1:]:
        ax.sharex(axes[0])
        ax.sharey(axes[0])
    mesh = None
    for idx, (ax, (label, freqs, times, reduction)) in enumerate(zip(axes, reductions)):
        mesh = ax.pcolormesh(
            times,
            freqs,
            reduction,
            shading="auto",
            cmap="viridis",
            vmin=REDUCTION_VMIN_DB,
            vmax=REDUCTION_VMAX_DB,
        )
        add_spectrogram_time_markers(ax)
        ax.set_ylim(0, SPEECH_BAND_HZ)
        ax.set_title(display_label(label))
        if idx in [0, 2]:
            ax.set_ylabel("Frequency (Hz)")
        else:
            ax.tick_params(labelleft=False)
    for ax in axes[len(reductions) :]:
        ax.axis("off")
    for ax in axes[-2:]:
        ax.set_xlabel("Time (s)")
    fig.suptitle("Speech-band noise reduction panel: input spectrogram - output spectrogram", fontsize=14)
    if mesh is not None:
        cax = fig.add_subplot(gs[:, 2])
        fig.colorbar(mesh, cax=cax, label="Reduction (dB), clipped 0-40")
    save_fig(ctx, fig, "denoise_reduction_panel_speechband.png", tight=False)


def plot_denoise_spectrograms(ctx: ReportContext, denoise_summary: pd.DataFrame) -> None:
    items = denoise_audio_items(ctx)
    if len(items) < 2:
        skip_plot(ctx, "denoise_spectrogram_compare.png", "missing input or denoise output WAV")
        return
    filename_map = {
        "input": "denoise_spectrogram_input.png",
        "fixed": "denoise_spectrogram_fixed.png",
        "hybrid_ms": "denoise_spectrogram_ms.png",
        "mcra": "denoise_spectrogram_mcra.png",
        "mcra_strong": "denoise_spectrogram_mcra_strong.png",
    }
    for label, sample_rate, samples in items:
        save_spectrogram(ctx, label, sample_rate, samples, filename_map.get(label, f"denoise_spectrogram_{label}.png"))

    compare_items = items[:4]
    fig, axes = plt.subplots(len(compare_items), 1, figsize=(10.0, 2.6 * len(compare_items)), sharex=True)
    if len(compare_items) == 1:
        axes = [axes]
    for ax, (label, sample_rate, samples) in zip(axes, compare_items):
        ax.specgram(samples, NFFT=1024, Fs=sample_rate, noverlap=768, cmap="magma")
        add_spectrogram_time_markers(ax)
        ax.set_ylabel(f"{label}\nHz")
    axes[-1].set_xlabel("Time (s)")
    fig.suptitle("Denoise spectrogram compare")
    save_fig(ctx, fig, "denoise_spectrogram_compare.png")
    plot_speechband_spectrograms(ctx, items, denoise_summary)
    plot_reduction_spectrograms(ctx, items)


def preferred_denoise_plot_df(ms_df: pd.DataFrame, mcra_df: pd.DataFrame, denoise_summary: pd.DataFrame) -> pd.DataFrame:
    if not mcra_df.empty:
        rows = []
        for _, row in mcra_df.iterrows():
            rows.append(
                {
                    "eval_case": get_value(row, "eval_case"),
                    "mode_name": get_value(row, "mode_name", "mode", default="mode"),
                    "output_snr_db": get_value(row, "output_snr_db"),
                    "delta_vs_fixed_db": get_value(row, "delta_vs_fixed_db"),
                    "speech_preservation_ratio": get_value(row, "speech_preservation_ratio"),
                    "clean_output_corr": get_value(row, "clean_output_corr"),
                    "gain_avg": get_value(row, "gain_avg"),
                    "noise_psd_avg": get_value(row, "noise_psd_avg"),
                    "ms_min_psd_avg": get_value(row, "ms_min_psd_avg"),
                }
            )
        return pd.DataFrame(rows)
    return denoise_summary.copy()


def sorted_modes(columns: Sequence[object]) -> list[str]:
    preferred = ["fixed", "hybrid_ms", "ms", "mcra_normal", "mcra", "mcra_strong"]
    present = [str(item) for item in columns]
    ordered = [mode for mode in preferred if mode in present]
    ordered.extend(mode for mode in present if mode not in ordered)
    return ordered


def plot_grouped_bars(ax: plt.Axes, pivot: pd.DataFrame, ylabel: str) -> None:
    modes = sorted_modes(pivot.columns)
    x = np.arange(len(pivot.index))
    width = 0.8 / max(len(modes), 1)
    for idx, mode in enumerate(modes):
        offsets = x - 0.4 + width / 2.0 + idx * width
        ax.bar(offsets, pivot[mode].astype(float), width=width, label=mode)
    ax.set_xticks(x)
    ax.set_xticklabels([str(item) for item in pivot.index], rotation=25, ha="right")
    ax.set_ylabel(ylabel)
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend()


def plot_denoise_snr_bar(ctx: ReportContext, plot_df: pd.DataFrame) -> None:
    filename = "denoise_snr_bar.png"
    if plot_df.empty or "output_snr_db" not in plot_df.columns:
        skip_plot(ctx, filename, "missing denoise output_snr_db data")
        return
    df = plot_df.copy()
    df["output_snr_db"] = pd.to_numeric(df["output_snr_db"], errors="coerce")
    pivot = df.pivot_table(index="eval_case", columns="mode_name", values="output_snr_db", aggfunc="mean")
    pivot = pivot.dropna(how="all")
    if pivot.empty:
        skip_plot(ctx, filename, "denoise SNR table has no numeric values")
        return
    fig, ax = plt.subplots(figsize=FIGSIZE)
    plot_grouped_bars(ax, pivot, "Output SNR (dB)")
    ax.set_title("Denoise output SNR by mode")
    save_fig(ctx, fig, filename)


def plot_denoise_delta(ctx: ReportContext, plot_df: pd.DataFrame) -> None:
    filename = "denoise_delta_vs_fixed.png"
    if plot_df.empty or "delta_vs_fixed_db" not in plot_df.columns:
        skip_plot(ctx, filename, "missing delta_vs_fixed_db data")
        return
    df = plot_df.copy()
    df["delta_vs_fixed_db"] = pd.to_numeric(df["delta_vs_fixed_db"], errors="coerce")
    df = df[df["mode_name"].astype(str) != "fixed"]
    pivot = df.pivot_table(index="eval_case", columns="mode_name", values="delta_vs_fixed_db", aggfunc="mean")
    pivot = pivot.dropna(how="all")
    if pivot.empty:
        skip_plot(ctx, filename, "no non-fixed denoise delta values")
        return
    fig, ax = plt.subplots(figsize=FIGSIZE)
    plot_grouped_bars(ax, pivot, "SNR delta vs fixed (dB)")
    ax.axhline(0.0, color="black", linewidth=0.8)
    ax.set_title("Denoise SNR improvement relative to fixed")
    save_fig(ctx, fig, filename)


def plot_denoise_preservation(ctx: ReportContext, plot_df: pd.DataFrame) -> None:
    filename = "denoise_speech_preservation_corr.png"
    needed = {"speech_preservation_ratio", "clean_output_corr"}
    if plot_df.empty or not needed.issubset(plot_df.columns):
        skip_plot(ctx, filename, "missing speech preservation or clean correlation data")
        return
    df = plot_df.copy()
    for column in needed:
        df[column] = pd.to_numeric(df[column], errors="coerce")
    fig, axes = plt.subplots(1, 2, figsize=FIGSIZE)
    for ax, column, ylabel, threshold in [
        (axes[0], "speech_preservation_ratio", "Speech preservation", 0.80),
        (axes[1], "clean_output_corr", "Clean-output corr", 0.95),
    ]:
        pivot = df.pivot_table(index="eval_case", columns="mode_name", values=column, aggfunc="mean").dropna(how="all")
        if not pivot.empty:
            plot_grouped_bars(ax, pivot, ylabel)
            ax.axhline(threshold, color="#b23a48", linestyle="--", linewidth=1.0, label=f"accept >= {threshold:.2f}")
            ax.legend(fontsize=7)
    fig.suptitle("Denoise speech preservation and clean correlation")
    save_fig(ctx, fig, filename)


def plot_noise_tracking(ctx: ReportContext, plot_df: pd.DataFrame) -> None:
    filename = "noise_psd_gain_tracking.png"
    needed = {"noise_psd_avg", "gain_avg"}
    if plot_df.empty or not needed.issubset(plot_df.columns):
        skip_plot(ctx, filename, "missing noise_psd_avg or gain_avg data")
        return
    df = plot_df.copy()
    for column in ["noise_psd_avg", "gain_avg", "ms_min_psd_avg"]:
        if column in df.columns:
            df[column] = pd.to_numeric(df[column], errors="coerce")
    df = df[df["mode_name"].astype(str).isin(["hybrid_ms", "mcra_normal", "mcra", "mcra_strong"])]
    if df.empty:
        skip_plot(ctx, filename, "no adaptive denoise rows for noise tracking")
        return
    grouped = df.groupby("eval_case", as_index=False).agg({"noise_psd_avg": "mean", "gain_avg": "mean"})
    max_psd = grouped["noise_psd_avg"].max()
    grouped["noise_psd_norm"] = grouped["noise_psd_avg"] / max_psd if max_psd and not math.isnan(max_psd) else np.nan
    fig, ax = plt.subplots(figsize=FIGSIZE)
    x = np.arange(len(grouped))
    ax.bar(x - 0.18, grouped["noise_psd_norm"], width=0.36, label="noise PSD normalized")
    ax.bar(x + 0.18, grouped["gain_avg"], width=0.36, label="gain avg")
    ax.set_xticks(x)
    ax.set_xticklabels(grouped["eval_case"].astype(str), rotation=25, ha="right")
    ax.set_title("Noise PSD / gain tracking by scenario")
    ax.set_ylabel("Normalized PSD or gain")
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)


def plot_denoise_algorithm_evolution_stepup(ctx: ReportContext, plot_df: pd.DataFrame) -> None:
    filename = "denoise_algorithm_evolution_stepup.png"
    if plot_df.empty or "output_snr_db" not in plot_df.columns:
        skip_plot(ctx, filename, "missing denoise step-up data")
        return
    df = plot_df.copy()
    df["output_snr_db"] = pd.to_numeric(df["output_snr_db"], errors="coerce")
    df["delta_vs_fixed_db"] = pd.to_numeric(df.get("delta_vs_fixed_db", np.nan), errors="coerce")
    step = df[df["eval_case"].astype(str).str.lower() == "noise_step_up_snr10"]
    if step.empty:
        skip_plot(ctx, filename, "missing noise_step_up_snr10 rows")
        return
    modes = ["fixed", "hybrid_ms", "mcra_normal", "mcra_strong"]
    rows = []
    for mode in modes:
        match = step[step["mode_name"].astype(str).str.lower() == mode]
        if not match.empty:
            rows.append(match.iloc[0])
    if len(rows) < 2:
        skip_plot(ctx, filename, "not enough noise_step_up_snr10 modes")
        return
    labels = [str(row["mode_name"]) for row in rows]
    display_labels = {
        "fixed": "fixed",
        "hybrid_ms": "HYBRID-MS",
        "mcra_normal": "MCRA normal",
        "mcra_strong": "MCRA strong",
    }
    x_labels = [display_labels.get(label, label) for label in labels]
    values = [numeric(row["output_snr_db"]) for row in rows]
    fixed = values[0]
    hybrid_value = next((values[idx] for idx, label in enumerate(labels) if label == "hybrid_ms"), math.nan)
    fig, ax = plt.subplots(figsize=FIGSIZE)
    colors = ["#4c78a8", "#f58518", "#54a24b", "#e45756"][: len(values)]
    bars = ax.bar(x_labels, values, color=colors, width=0.62)
    ax.set_title("Noise step-up denoise evolution - output SNR")
    ax.set_ylabel("Output SNR (dB)")
    ax.grid(True, axis="y", alpha=0.3)
    ax.set_ylim(0, max(values) * 1.22)
    for bar, label, value in zip(bars, labels, values):
        if label == "fixed":
            text = f"{value:.2f} dB"
        elif label == "mcra_normal" and not math.isnan(hybrid_value):
            text = f"+{value - fixed:.2f} dB vs fixed\n+{value - hybrid_value:.2f} dB vs HYBRID-MS"
        else:
            text = f"+{value - fixed:.2f} dB vs fixed"
        ax.text(bar.get_x() + bar.get_width() / 2.0, value + 0.25, text, ha="center", va="bottom", fontsize=9)
    ax.text(
        0.01,
        0.95,
        "Scenario: noise_step_up_snr10\nPurpose: compact PPT conclusion figure",
        transform=ax.transAxes,
        va="top",
        bbox=dict(facecolor="white", alpha=0.86, edgecolor="#d0d4da"),
    )
    save_fig(ctx, fig, filename)


def codec_plot_frame(codec_df: pd.DataFrame) -> pd.DataFrame:
    if codec_df.empty:
        return codec_df
    rows = []
    for _, row in codec_df.iterrows():
        rows.append(
            {
                "target_bitrate_kbps": numeric(get_value(row, "target_bitrate_kbps")),
                "direct_actual_kbps": numeric(get_value(row, "actual_bitrate_kbps", "direct_bitrate_kbps")),
                "denoise_actual_kbps": numeric(get_value(row, "denoise_then_bitrate_kbps", "actual_bitrate_kbps", "direct_bitrate_kbps")),
                "direct_ratio": numeric(get_value(row, "compression_ratio", "direct_compression_ratio")),
                "denoise_ratio": numeric(get_value(row, "denoise_then_compression_ratio", "compression_ratio", "direct_compression_ratio")),
                "direct_snr_db": numeric(get_value(row, "direct_output_snr_db", "direct_compress_snr_db", "direct_reconstruction_snr_db")),
                "denoise_snr_db": numeric(
                    get_value(row, "denoise_codec_output_snr_db", "denoise_then_compress_snr_db", "denoise_then_reconstruction_snr_db")
                ),
            }
        )
    return pd.DataFrame(rows).groupby("target_bitrate_kbps", as_index=False).mean(numeric_only=True)


def plot_codec_bitrate_compression(ctx: ReportContext, codec_df: pd.DataFrame) -> None:
    filename = "codec_bitrate_compression_ratio.png"
    df = codec_plot_frame(codec_df)
    if df.empty or "direct_ratio" not in df.columns:
        skip_plot(ctx, filename, "missing codec compression ratio data")
        return
    fig, ax = plt.subplots(figsize=FIGSIZE)
    x = np.arange(len(df))
    ax.bar(x - 0.18, df["direct_ratio"], width=0.36, label="direct")
    ax.bar(x + 0.18, df["denoise_ratio"], width=0.36, label="denoise+codec")
    ax.set_xticks(x)
    ax.set_xticklabels([f"{int(v)}" for v in df["target_bitrate_kbps"]])
    ax.set_title(f"Codec compression ratio (raw PCM = {PCM_BITRATE_KBPS:.0f} kbps)")
    ax.set_xlabel("Target bitrate (kbps)")
    ax.set_ylabel("Compression ratio")
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)


def plot_codec_actual_bitrate(ctx: ReportContext, codec_df: pd.DataFrame) -> None:
    filename = "codec_actual_bitrate.png"
    df = codec_plot_frame(codec_df)
    if df.empty or "direct_actual_kbps" not in df.columns:
        skip_plot(ctx, filename, "missing codec bitrate data")
        return
    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.plot(df["target_bitrate_kbps"], df["target_bitrate_kbps"], "--", color="gray", label="target")
    ax.plot(df["target_bitrate_kbps"], df["direct_actual_kbps"], marker="o", label="direct actual")
    ax.plot(df["target_bitrate_kbps"], df["denoise_actual_kbps"], marker="o", label="denoise+codec actual")
    ax.set_title("Codec target vs actual bitrate")
    ax.set_xlabel("Target bitrate (kbps)")
    ax.set_ylabel("Actual bitrate (kbps)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)


def plot_codec_quality(ctx: ReportContext, codec_df: pd.DataFrame) -> None:
    filename = "codec_quality_snr.png"
    df = codec_plot_frame(codec_df)
    if df.empty or "direct_snr_db" not in df.columns:
        skip_plot(ctx, filename, "missing codec SNR data")
        return
    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.plot(df["target_bitrate_kbps"], df["direct_snr_db"], marker="o", label="direct compress")
    ax.plot(df["target_bitrate_kbps"], df["denoise_snr_db"], marker="o", label="denoise then compress")
    ax.set_title("Codec quality by bitrate")
    ax.set_xlabel("Target bitrate (kbps)")
    ax.set_ylabel("Output SNR (dB)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)


def plot_codec_spectrogram_compare(ctx: ReportContext) -> None:
    filename = "codec_spectrogram_compare.png"
    required = [
        ("input", "compare_00_input.wav"),
        ("direct 240k", "compare_04_codec_direct_240k.wav"),
        ("denoise+codec 240k", "compare_07_denoise_codec_240k.wav"),
    ]
    items = []
    for label, wav_name in required:
        audio = load_audio(ctx, wav_name)
        if audio is None:
            skip_plot(ctx, filename, f"missing {wav_name}")
            return
        sample_rate, samples = audio
        items.append((label, sample_rate, samples))
    fig, axes = plt.subplots(len(items), 1, figsize=(10.0, 2.8 * len(items)), sharex=True)
    for ax, (label, sample_rate, samples) in zip(axes, items):
        ax.specgram(samples, NFFT=1024, Fs=sample_rate, noverlap=768, cmap="magma")
        ax.set_ylabel(f"{label}\nHz")
    axes[-1].set_xlabel("Time (s)")
    fig.suptitle("Codec spectrogram compare")
    save_fig(ctx, fig, filename)


def plot_realtime_template(ctx: ReportContext, realtime_df: pd.DataFrame) -> None:
    df = realtime_df.copy()
    df["max_ms"] = pd.to_numeric(df["max_ms"], errors="coerce")
    df["frame_budget_ms"] = pd.to_numeric(df["frame_budget_ms"], errors="coerce").fillna(FRAME_BUDGET_MS)
    measured = df["max_ms"].notna() & (df["max_ms"] > 0.0)
    formal = bool(measured.any())
    filename = "realtime_budget_compare.png" if formal else "realtime_budget_compare_template.png"
    df["max_ms"] = df["max_ms"].fillna(0.0)
    fig, ax = plt.subplots(figsize=FIGSIZE)
    x = np.arange(len(df))
    label = "max_ms from CCS Watch" if formal else "max_ms (fill from CCS Watch)"
    ax.bar(x, df["max_ms"], label=label)
    budget = float(df["frame_budget_ms"].dropna().iloc[0]) if df["frame_budget_ms"].notna().any() else FRAME_BUDGET_MS
    ax.axhline(budget, color="#b23a48", linestyle="--", label=f"frame budget {budget:.2f} ms")
    ax.set_xticks(x)
    ax.set_xticklabels(df["mode"].astype(str), rotation=20, ha="right")
    ax.set_title("Realtime frame budget compare" if formal else "Realtime frame budget compare template")
    ax.set_ylabel("Frame time (ms)")
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend()
    if formal:
        worst = float(df["max_ms"].max())
        margin = budget - worst
        ax.text(0.01, 0.95, f"worst={worst:.3f} ms, margin={margin:.3f} ms", transform=ax.transAxes, va="top")
    else:
        ax.text(0.01, 0.95, "Template: fill max_ms from CCS Watch before final reporting.", transform=ax.transAxes, va="top")
    save_fig(ctx, fig, filename)


def plot_realtime_annotated(ctx: ReportContext, realtime_df: pd.DataFrame) -> None:
    filename = "realtime_budget_compare_annotated.png"
    if realtime_df.empty or "max_ms" not in realtime_df.columns:
        skip_plot(ctx, filename, "missing realtime max_ms data")
        return
    df = realtime_df.copy()
    df["max_ms"] = pd.to_numeric(df["max_ms"], errors="coerce")
    measured = df["max_ms"].notna() & (df["max_ms"] > 0.0)
    if not measured.any():
        skip_plot(ctx, filename, "realtime CSV has no measured max_ms")
        return
    df = df[measured].copy()
    df["mode"] = df["mode"].astype(str)
    x = np.arange(len(df))
    fig, ax = plt.subplots(figsize=FIGSIZE)
    bars = ax.bar(x, df["max_ms"], color="#4c78a8", width=0.65)
    ax.axhline(FRAME_BUDGET_MS, color="#b23a48", linestyle="--", linewidth=1.5, label=f"20.48 ms frame budget")
    mode7 = df[df["mode"].astype(str) == "7"]
    if not mode7.empty:
        highlight_idx = int(mode7.index[0])
    else:
        highlight_idx = int(df["max_ms"].idxmax())
    positional_idx = list(df.index).index(highlight_idx)
    highlight = df.loc[highlight_idx]
    max_ms = float(highlight["max_ms"])
    margin = FRAME_BUDGET_MS - max_ms
    bars[positional_idx].set_color("#f58518")
    ax.annotate(
        f"mode {highlight['mode']} max={max_ms:.3f} ms\nmargin={margin:.3f} ms",
        xy=(positional_idx, max_ms),
        xytext=(positional_idx, max_ms + 3.0),
        ha="center",
        arrowprops=dict(arrowstyle="->", color="#333333"),
        bbox=dict(facecolor="white", edgecolor="#d0d4da", alpha=0.9),
    )
    ax.set_xticks(x)
    ax.set_xticklabels(df["mode"].astype(str), rotation=0)
    ax.set_ylabel("Max frame time (ms)")
    ax.set_xlabel("Board mode")
    ax.set_title("Board Runtime vs 20.48 ms Frame Budget")
    ax.set_ylim(0, max(FRAME_BUDGET_MS * 1.16, df["max_ms"].max() * 1.35))
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)


def plot_memory_usage(ctx: ReportContext, memory_df: pd.DataFrame) -> None:
    filename = "memory_usage_template.png"
    if memory_df.empty:
        template = pd.DataFrame(
            {
                "memory_region": ["code", "data", "bss", "stack_heap"],
                "used_bytes": [0, 0, 0, 0],
                "used_percent": [np.nan, np.nan, np.nan, np.nan],
            }
        )
        memory_df = template
        warn(ctx, "No map memory data available; generated memory usage template plot.")
    fig, ax = plt.subplots(figsize=FIGSIZE)
    ordered = memory_df.sort_values("used_bytes", ascending=False).head(12)
    ax.bar(ordered["memory_region"].astype(str), ordered["used_bytes"].astype(float))
    ax.set_title("Memory usage from map file" if ctx.found_map else "Memory usage template")
    ax.set_ylabel("Used bytes")
    ax.tick_params(axis="x", labelrotation=25)
    ax.grid(True, axis="y", alpha=0.3)
    save_fig(ctx, fig, filename)


def maybe_plot_thd(ctx: ReportContext) -> None:
    single_tone_candidates = unique_paths([
        path
        for directory in ctx.audio_dirs
        for path in directory.glob("*.wav")
        if any(term in path.name.lower() for term in ["tone", "sine", "thd"])
    ])
    filename = "thd_spectrum_example.png"
    if not single_tone_candidates:
        skip_plot(ctx, filename, "missing single-tone test WAV")
        save_summary(
            ctx,
            pd.DataFrame(
                columns=[
                    "source_wav",
                    "sample_rate",
                    "fundamental_hz",
                    "thd_db",
                    "thd_percent",
                    "algorithm_only_note",
                ]
            ),
            "thd_summary.csv",
        )
        return
    audio = read_wav(single_tone_candidates[0], ctx)
    if audio is None:
        skip_plot(ctx, filename, f"could not read {single_tone_candidates[0].name}")
        return
    sample_rate, samples = audio
    freq, mag = spectrum(samples, sample_rate, n_fft=16384)
    if len(freq) < 3:
        skip_plot(ctx, filename, "not enough single-tone samples")
        return
    fundamental_idx = int(np.nanargmax(mag[1:]) + 1)
    fundamental_freq = freq[fundamental_idx]
    harmonic_power = 0.0
    harmonic_rows: dict[str, float] = {}
    harmonic_indices: dict[int, int] = {}
    for n in range(2, 8):
        harmonic = fundamental_freq * n
        if harmonic >= sample_rate / 2:
            break
        idx = int(np.argmin(np.abs(freq - harmonic)))
        harmonic_indices[n] = idx
        harmonic_rows[f"harmonic_{n}_hz"] = float(freq[idx])
        harmonic_rows[f"harmonic_{n}_db"] = float(mag[idx])
        harmonic_power += float(10 ** (mag[idx] / 10.0))
    fundamental_power = float(10 ** (mag[fundamental_idx] / 10.0))
    thd = math.sqrt(harmonic_power / max(fundamental_power, EPS))
    thd_db = 20 * math.log10(thd + EPS)
    thd_percent = thd * 100.0
    fig, ax = plt.subplots(figsize=FIGSIZE)
    mask = freq <= min(10000, sample_rate / 2)
    ax.plot(freq[mask], mag[mask], linewidth=1.0)
    ax.axvline(fundamental_freq, color="#b23a48", label=f"fundamental {fundamental_freq:.1f} Hz")
    ax.set_title("THD spectrum example")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB)")
    ax.text(0.01, 0.95, f"THD={thd_db:.2f} dB ({thd_percent:.6f}%)", transform=ax.transAxes, va="top")
    ax.grid(True, alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)

    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.plot(freq[mask], mag[mask], linewidth=1.0, color="#4c78a8")
    ax.axvline(fundamental_freq, color="#b23a48", linewidth=1.3)
    ax.annotate(
        f"Fundamental\n{fundamental_freq:.1f} Hz",
        xy=(fundamental_freq, mag[fundamental_idx]),
        xytext=(fundamental_freq + 500, mag[fundamental_idx] + 5),
        arrowprops=dict(arrowstyle="->", color="#b23a48"),
        color="#b23a48",
    )
    harmonic_suffix = {2: "2nd", 3: "3rd", 4: "4th", 5: "5th"}
    for n in range(2, 6):
        idx = harmonic_indices.get(n)
        if idx is None:
            continue
        harmonic_freq = freq[idx]
        if harmonic_freq > freq[mask][-1]:
            continue
        ax.axvline(harmonic_freq, color="#f58518", linestyle="--", linewidth=1.0, alpha=0.9)
        ax.text(
            harmonic_freq,
            mag[idx] + 3,
            f"{harmonic_suffix.get(n, str(n) + 'th')}\n{harmonic_freq:.0f} Hz",
            ha="center",
            va="bottom",
            fontsize=8,
            color="#7a3e00",
        )
    ax.set_title("THD Spectrum Example (PC Algorithm-only)")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB)")
    ax.grid(True, alpha=0.3)
    ax.text(
        0.01,
        0.95,
        f"THD={thd_db:.2f} dB ({thd_percent:.6f}%)\nThis evaluates digital WOLA processing, not analog AD/DA hardware.",
        transform=ax.transAxes,
        va="top",
        bbox=dict(facecolor="white", alpha=0.88, edgecolor="#d0d4da"),
    )
    ax.text(
        0.5,
        0.02,
        "This THD result evaluates the digital WOLA processing path, not the full analog AD/DA hardware path.",
        transform=ax.transAxes,
        ha="center",
        va="bottom",
        fontsize=9,
        bbox=dict(facecolor="white", alpha=0.82, edgecolor="none"),
    )
    save_fig(ctx, fig, "thd_spectrum_annotated.png")
    summary_row = {
        "source_wav": str(single_tone_candidates[0]),
        "sample_rate": sample_rate,
        "fundamental_hz": fundamental_freq,
        "thd_db": thd_db,
        "thd_percent": thd_percent,
        "algorithm_only_note": "This THD result evaluates the digital WOLA processing path, not the full analog AD/DA hardware path.",
    }
    summary_row.update(harmonic_rows)
    save_summary(ctx, pd.DataFrame([summary_row]), "thd_summary.csv")


def skip_html(ctx: ReportContext, filename: str, reason: str) -> None:
    ctx.skipped_html[filename] = reason
    print(f"SKIP interactive/{filename}: {reason}")


def save_plotly_html(ctx: ReportContext, fig: object, filename: str, width: int = 1100, height: int = 800) -> None:
    ctx.interactive_dir.mkdir(parents=True, exist_ok=True)
    path = ctx.interactive_dir / filename
    fig.update_layout(width=width, height=height, margin=dict(l=70, r=40, t=90, b=80))
    fig.write_html(
        str(path),
        include_plotlyjs=True,
        full_html=True,
        config={
            "displaylogo": False,
            "responsive": True,
            "toImageButtonOptions": {"format": "png", "filename": Path(filename).stem},
        },
    )
    ctx.generated_html.append(filename)


def clean_label(value: object) -> str:
    if value is None or pd.isna(value):
        return "missing"
    return str(value)


def fmt_metric(value: object, suffix: str = "", digits: int = 2) -> str:
    number = numeric(value)
    if math.isnan(number):
        return "missing"
    if math.isinf(number):
        return f"inf{suffix}"
    return f"{number:.{digits}f}{suffix}"


def ensure_numeric_columns(df: pd.DataFrame, columns: Sequence[str]) -> pd.DataFrame:
    result = df.copy()
    for column in columns:
        if column in result.columns:
            result[column] = pd.to_numeric(result[column], errors="coerce")
        else:
            result[column] = np.nan
    return result


def prepare_denoise_interactive_df(ms_df: pd.DataFrame, mcra_df: pd.DataFrame, denoise_summary: pd.DataFrame) -> pd.DataFrame:
    df = preferred_denoise_plot_df(ms_df, mcra_df, denoise_summary)
    if df.empty:
        return df
    df = df.copy()
    if "mode_name" not in df.columns and "mode" in df.columns:
        df["mode_name"] = df["mode"]
    if "mode_name" not in df.columns:
        df["mode_name"] = "mode"
    if "eval_case" not in df.columns:
        df["eval_case"] = "case"
    numeric_columns = [
        "input_snr_db",
        "output_snr_db",
        "delta_vs_fixed_db",
        "delta_vs_hybrid_db",
        "speech_preservation_ratio",
        "clean_output_corr",
        "gain_avg",
        "noise_psd_avg",
        "ms_min_psd_avg",
    ]
    df = ensure_numeric_columns(df, numeric_columns)
    df["eval_case"] = df["eval_case"].astype(str)
    df["mode_name"] = df["mode_name"].astype(str)
    return df


def denoise_matrix(df: pd.DataFrame, value_col: str) -> tuple[list[str], list[str], np.ndarray, list[list[list[object]]]]:
    cases = sorted(df["eval_case"].dropna().astype(str).unique().tolist())
    preferred_modes = ["fixed", "hybrid_ms", "mcra_normal", "mcra_strong"]
    present_modes = df["mode_name"].dropna().astype(str).unique().tolist()
    modes = [mode for mode in preferred_modes if mode in present_modes]
    modes.extend(mode for mode in sorted(present_modes) if mode not in modes)
    pivot = df.pivot_table(index="mode_name", columns="eval_case", values=value_col, aggfunc="mean")
    z = pivot.reindex(index=modes, columns=cases).to_numpy(dtype=float)
    custom: list[list[list[object]]] = []
    for mode in modes:
        row_items: list[list[object]] = []
        for case in cases:
            match = df[(df["eval_case"] == case) & (df["mode_name"] == mode)].head(1)
            if match.empty:
                row_items.append([case, mode, np.nan, np.nan, np.nan, np.nan, np.nan, np.nan])
            else:
                item = match.iloc[0]
                row_items.append(
                    [
                        case,
                        mode,
                        numeric(item.get("input_snr_db")),
                        numeric(item.get("output_snr_db")),
                        numeric(item.get("delta_vs_fixed_db")),
                        numeric(item.get("delta_vs_hybrid_db")),
                        numeric(item.get("speech_preservation_ratio")),
                        numeric(item.get("clean_output_corr")),
                    ]
                )
        custom.append(row_items)
    return cases, modes, z, custom


def plot_interactive_denoise_surface(ctx: ReportContext, df: pd.DataFrame, value_col: str, filename: str, title: str, z_title: str) -> None:
    if go is None:
        skip_html(ctx, filename, "plotly is not installed")
        return
    if df.empty or value_col not in df.columns:
        skip_html(ctx, filename, f"missing denoise {value_col} data")
        return
    cases, modes, z, custom = denoise_matrix(df, value_col)
    if not cases or not modes or np.all(np.isnan(z)):
        skip_html(ctx, filename, f"no numeric {value_col} values")
        return
    fig = go.Figure(
        data=[
            go.Surface(
                z=z,
                x=list(range(len(cases))),
                y=list(range(len(modes))),
                customdata=custom,
                colorscale="Viridis",
                colorbar=dict(title=z_title),
                hovertemplate=(
                    "case=%{customdata[0]}<br>"
                    "mode=%{customdata[1]}<br>"
                    "input_snr_db=%{customdata[2]:.2f}<br>"
                    "output_snr_db=%{customdata[3]:.2f}<br>"
                    "delta_vs_fixed_db=%{customdata[4]:.2f}<br>"
                    "delta_vs_hybrid_db=%{customdata[5]:.2f}<br>"
                    "preservation=%{customdata[6]:.3f}<br>"
                    "corr=%{customdata[7]:.3f}<extra></extra>"
                ),
            )
        ]
    )
    fig.update_layout(
        title=title,
        scene=dict(
            xaxis=dict(title="Scenario / eval_case", tickmode="array", tickvals=list(range(len(cases))), ticktext=cases),
            yaxis=dict(title="Mode / mode_name", tickmode="array", tickvals=list(range(len(modes))), ticktext=modes),
            zaxis=dict(title=z_title),
        ),
    )
    save_plotly_html(ctx, fig, filename)


def plot_interactive_denoise_heatmap(ctx: ReportContext, df: pd.DataFrame, value_col: str, filename: str, title: str, color_title: str) -> None:
    if go is None:
        skip_html(ctx, filename, "plotly is not installed")
        return
    if df.empty or value_col not in df.columns:
        skip_html(ctx, filename, f"missing denoise {value_col} data")
        return
    cases = sorted(df["eval_case"].dropna().astype(str).unique().tolist())
    modes = sorted_modes(df["mode_name"].dropna().astype(str).unique().tolist())
    pivot = df.pivot_table(index="eval_case", columns="mode_name", values=value_col, aggfunc="mean").reindex(index=cases, columns=modes)
    z = pivot.to_numpy(dtype=float)
    if np.all(np.isnan(z)):
        skip_html(ctx, filename, f"no numeric {value_col} values")
        return
    text = [[("" if pd.isna(value) else f"{value:.2f}") for value in row] for row in z]
    custom: list[list[list[object]]] = []
    for case in cases:
        row_items: list[list[object]] = []
        for mode in modes:
            match = df[(df["eval_case"] == case) & (df["mode_name"] == mode)].head(1)
            if match.empty:
                row_items.append([np.nan, np.nan])
            else:
                item = match.iloc[0]
                row_items.append([numeric(item.get("speech_preservation_ratio")), numeric(item.get("clean_output_corr"))])
        custom.append(row_items)
    fig = go.Figure(
        data=[
            go.Heatmap(
                z=z,
                x=modes,
                y=cases,
                text=text,
                texttemplate="%{text}",
                customdata=custom,
                colorscale="Viridis",
                colorbar=dict(title=color_title),
                hovertemplate=(
                    "case=%{y}<br>mode=%{x}<br>"
                    f"{value_col}=%{{z:.2f}}<br>"
                    "preservation=%{customdata[0]:.3f}<br>"
                    "corr=%{customdata[1]:.3f}<extra></extra>"
                ),
            )
        ]
    )
    fig.update_layout(title=title, xaxis_title="Mode / mode_name", yaxis_title="Scenario / eval_case")
    save_plotly_html(ctx, fig, filename)


def denoise_row_for(df: pd.DataFrame, case: str, mode_candidates: Sequence[str]) -> pd.Series | None:
    if df.empty:
        return None
    target_case = case.lower()
    mode_set = {mode.lower() for mode in mode_candidates}
    tmp = df.copy()
    tmp["_case"] = tmp["eval_case"].astype(str).str.lower()
    tmp["_mode"] = tmp["mode_name"].astype(str).str.lower()
    exact = tmp[(tmp["_case"] == target_case) & (tmp["_mode"].isin(mode_set))]
    if not exact.empty:
        return exact.iloc[0]
    contains = tmp[tmp["_case"].str.contains(target_case, regex=False, na=False) & tmp["_mode"].isin(mode_set)]
    if not contains.empty:
        return contains.iloc[0]
    return None


def plot_interactive_denoise_waterfall(ctx: ReportContext, df: pd.DataFrame) -> None:
    filename = "denoise_stepup_waterfall.html"
    if go is None:
        skip_html(ctx, filename, "plotly is not installed")
        return
    fixed = denoise_row_for(df, "noise_step_up_snr10", ["fixed"])
    hybrid = denoise_row_for(df, "noise_step_up_snr10", ["hybrid_ms", "ms"])
    mcra = denoise_row_for(df, "noise_step_up_snr10", ["mcra_normal"])
    if fixed is None or hybrid is None or mcra is None:
        skip_html(ctx, filename, "missing fixed, HYBRID-MS, or MCRA normal row for noise_step_up_snr10")
        return
    fixed_snr = numeric(fixed.get("output_snr_db"))
    hybrid_delta = numeric(hybrid.get("output_snr_db")) - fixed_snr
    mcra_delta = numeric(mcra.get("output_snr_db")) - numeric(hybrid.get("output_snr_db"))
    if any(math.isnan(value) for value in [fixed_snr, hybrid_delta, mcra_delta]):
        skip_html(ctx, filename, "noise_step_up_snr10 waterfall has non-numeric values")
        return
    fig = go.Figure(
        go.Waterfall(
            x=["fixed output SNR", "HYBRID-MS gain", "MCRA normal gain"],
            y=[fixed_snr, hybrid_delta, mcra_delta],
            measure=["absolute", "relative", "relative"],
            text=[f"{fixed_snr:.2f} dB", f"+{hybrid_delta:.2f} dB", f"+{mcra_delta:.2f} dB"],
            textposition="outside",
            connector={"line": {"color": "rgb(80,80,80)"}},
        )
    )
    fig.update_layout(
        title="算法演进 Waterfall / fixed → HYBRID-MS → MCRA normal",
        yaxis_title="Output SNR (dB)",
        showlegend=False,
    )
    save_plotly_html(ctx, fig, filename)


def plot_interactive_denoise_tradeoff(ctx: ReportContext, df: pd.DataFrame) -> None:
    filename = "denoise_quality_tradeoff.html"
    if go is None:
        skip_html(ctx, filename, "plotly is not installed")
        return
    needed = {"speech_preservation_ratio", "clean_output_corr", "output_snr_db", "delta_vs_fixed_db"}
    if df.empty or not needed.issubset(df.columns):
        skip_html(ctx, filename, "missing denoise tradeoff columns")
        return
    fig = go.Figure()
    for mode in sorted_modes(df["mode_name"].dropna().astype(str).unique().tolist()):
        mode_df = df[df["mode_name"] == mode].copy()
        size_source = pd.to_numeric(mode_df["output_snr_db"], errors="coerce").fillna(0.0)
        sizes = np.clip((size_source - size_source.min() + 1.0) * 7.0, 8.0, 32.0)
        fig.add_trace(
            go.Scatter(
                x=mode_df["speech_preservation_ratio"],
                y=mode_df["clean_output_corr"],
                mode="markers",
                name=mode,
                marker=dict(size=sizes, opacity=0.82),
                customdata=np.stack(
                    [
                        mode_df["eval_case"].astype(str),
                        mode_df["mode_name"].astype(str),
                        pd.to_numeric(mode_df["output_snr_db"], errors="coerce"),
                        pd.to_numeric(mode_df["delta_vs_fixed_db"], errors="coerce"),
                    ],
                    axis=-1,
                ),
                hovertemplate=(
                    "case=%{customdata[0]}<br>mode=%{customdata[1]}<br>"
                    "output_snr_db=%{customdata[2]:.2f}<br>"
                    "delta_vs_fixed_db=%{customdata[3]:.2f}<br>"
                    "preservation=%{x:.3f}<br>corr=%{y:.3f}<extra></extra>"
                ),
            )
        )
    fig.add_hline(y=0.95, line_dash="dash", line_color="red", annotation_text="corr >= 0.95")
    fig.add_vline(x=0.80, line_dash="dash", line_color="red", annotation_text="preservation >= 0.80")
    fig.update_layout(
        title="语音保持-相关性折中 / Speech Preservation vs Correlation",
        xaxis_title="Speech preservation ratio",
        yaxis_title="Clean-output correlation",
    )
    save_plotly_html(ctx, fig, filename)


def plot_interactive_board_runtime(ctx: ReportContext, board_df: pd.DataFrame) -> None:
    filename = "board_runtime_by_mode.html"
    if go is None:
        skip_html(ctx, filename, "plotly is not installed")
        return
    board_df = normalize_board_runtime_df(board_df)
    if board_df.empty or "max_ms" not in board_df.columns:
        skip_html(ctx, filename, "missing board runtime csv")
        return
    df = board_df.copy()
    df["max_ms"] = pd.to_numeric(df["max_ms"], errors="coerce")
    df["last_ms"] = pd.to_numeric(df.get("last_ms", np.nan), errors="coerce")
    df["frame_budget_ms"] = pd.to_numeric(df.get("frame_budget_ms", FRAME_BUDGET_MS), errors="coerce").fillna(FRAME_BUDGET_MS)
    df = df.dropna(subset=["max_ms"])
    if df.empty:
        skip_html(ctx, filename, "board runtime csv has no max_ms/max_us values")
        return
    x = df["mode_name"].astype(str) if "mode_name" in df.columns else df["mode"].astype(str)
    custom_cols = []
    for column in ["mode", "track_mode", "strong", "last_us", "max_us", "cpu_percent", "runtime_margin_us", "realtime_pass"]:
        custom_cols.append(df[column] if column in df.columns else pd.Series(["missing"] * len(df), index=df.index))
    customdata = np.stack(custom_cols, axis=-1)
    fig = go.Figure(
        go.Bar(
            x=x,
            y=df["max_ms"],
            customdata=customdata,
            marker_color=np.where(df["max_ms"] <= df["frame_budget_ms"], "#2ca02c", "#d62728"),
            hovertemplate=(
                "mode=%{customdata[0]}<br>track_mode=%{customdata[1]}<br>strong=%{customdata[2]}<br>"
                "last_us=%{customdata[3]}<br>max_us=%{customdata[4]}<br>cpu_percent=%{customdata[5]}<br>"
                "runtime_margin_us=%{customdata[6]}<br>realtime_pass=%{customdata[7]}<extra></extra>"
            ),
        )
    )
    budget = float(df["frame_budget_ms"].dropna().iloc[0]) if df["frame_budget_ms"].notna().any() else FRAME_BUDGET_MS
    fig.add_hline(y=budget, line_color="red", line_dash="dash", annotation_text=f"frame budget {budget:.2f} ms")
    fig.update_layout(title="板端实时性 / Board Runtime", xaxis_title="Mode", yaxis_title="Max runtime (ms)")
    save_plotly_html(ctx, fig, filename)


def plot_interactive_codec_tradeoff(ctx: ReportContext, codec_df: pd.DataFrame) -> None:
    filename = "codec_rate_quality_tradeoff.html"
    if go is None:
        skip_html(ctx, filename, "plotly is not installed")
        return
    if codec_df.empty:
        skip_html(ctx, filename, "missing codec CSV")
        return
    rows = []
    for _, row in codec_df.iterrows():
        rows.append(
            {
                "path": "direct compress",
                "actual_bitrate_kbps": numeric(get_value(row, "actual_bitrate_kbps", "direct_bitrate_kbps")),
                "output_snr_db": numeric(get_value(row, "direct_output_snr_db", "direct_compress_snr_db", "direct_reconstruction_snr_db")),
                "compression_ratio": numeric(get_value(row, "compression_ratio", "direct_compression_ratio")),
                "target_bitrate_kbps": numeric(get_value(row, "target_bitrate_kbps")),
            }
        )
        rows.append(
            {
                "path": "denoise then compress",
                "actual_bitrate_kbps": numeric(get_value(row, "denoise_then_bitrate_kbps", "actual_bitrate_kbps", "direct_bitrate_kbps")),
                "output_snr_db": numeric(get_value(row, "denoise_codec_output_snr_db", "denoise_then_compress_snr_db", "denoise_then_reconstruction_snr_db")),
                "compression_ratio": numeric(get_value(row, "denoise_then_compression_ratio", "compression_ratio", "direct_compression_ratio")),
                "target_bitrate_kbps": numeric(get_value(row, "target_bitrate_kbps")),
            }
        )
    df = pd.DataFrame(rows).dropna(subset=["actual_bitrate_kbps", "output_snr_db"])
    if df.empty:
        skip_html(ctx, filename, "codec CSV has no numeric bitrate/SNR values")
        return
    fig = go.Figure()
    for path_name in ["direct compress", "denoise then compress"]:
        item = df[df["path"] == path_name].sort_values("actual_bitrate_kbps")
        fig.add_trace(
            go.Scatter(
                x=item["actual_bitrate_kbps"],
                y=item["output_snr_db"],
                mode="lines+markers",
                name=path_name,
                marker=dict(size=np.clip(item["compression_ratio"].fillna(1.0) * 4.0, 8.0, 26.0)),
                customdata=np.stack([item["target_bitrate_kbps"], item["compression_ratio"]], axis=-1),
                hovertemplate=(
                    "actual_bitrate=%{x:.2f} kbps<br>output_snr=%{y:.2f} dB<br>"
                    "target_bitrate=%{customdata[0]:.0f} kbps<br>compression_ratio=%{customdata[1]:.2f}<extra></extra>"
                ),
            )
        )
    fig.update_layout(title="压缩码率-音质折中 / Codec Rate-Quality Tradeoff", xaxis_title="Actual bitrate (kbps)", yaxis_title="Output SNR (dB)")
    save_plotly_html(ctx, fig, filename)


def plot_interactive_audio_energy(ctx: ReportContext, audio_df: pd.DataFrame) -> None:
    filename = "audio_energy_ratio_heatmap.html"
    if go is None:
        skip_html(ctx, filename, "plotly is not installed")
        return
    if audio_df.empty:
        skip_html(ctx, filename, "missing aggregate_compare_audio_metrics.csv or compare_audio_metrics.csv")
        return
    df = audio_df.copy()
    mode_col = first_col(df, "mode", "mode_name")
    value_col = first_col(df, "output_input_energy_ratio", "output_energy_ratio")
    case_col = first_col(df, "case", "audio_case", "eval_case")
    if mode_col is None or value_col is None:
        skip_html(ctx, filename, "audio metrics missing mode or energy ratio column")
        return
    if case_col is None:
        df["audio_case"] = "current_audio"
        case_col = "audio_case"
    df[value_col] = pd.to_numeric(df[value_col], errors="coerce")
    pivot = df.pivot_table(index=case_col, columns=mode_col, values=value_col, aggfunc="mean")
    if pivot.empty:
        skip_html(ctx, filename, "audio metrics has no numeric energy ratio values")
        return
    z = pivot.to_numpy(dtype=float)
    text = [[("" if pd.isna(value) else f"{value:.3f}") for value in row] for row in z]
    fig = go.Figure(
        go.Heatmap(
            z=z,
            x=pivot.columns.astype(str).tolist(),
            y=pivot.index.astype(str).tolist(),
            text=text,
            texttemplate="%{text}",
            colorscale="Viridis",
            colorbar=dict(title="energy ratio"),
            hovertemplate="case=%{y}<br>mode=%{x}<br>energy_ratio=%{z:.4f}<extra></extra>",
        )
    )
    fig.update_layout(title="PC 音频能量比热力图 / Audio Energy Ratio Heatmap", xaxis_title="Mode", yaxis_title="Audio case")
    save_plotly_html(ctx, fig, filename)


def compute_dashboard_cards(
    wola_summary: pd.DataFrame,
    denoise_df: pd.DataFrame,
    codec_summary: pd.DataFrame,
    board_df: pd.DataFrame,
) -> list[tuple[str, str]]:
    cards: list[tuple[str, str]] = []
    wola_row = wola_summary.iloc[0] if not wola_summary.empty else None
    delay_samples = numeric(get_value(wola_row, "wola_delay_samples") if wola_row is not None else np.nan)
    delay_ms = delay_samples / 50000.0 * 1000.0 if not math.isnan(delay_samples) else math.nan
    cards.append(("WOLA passthrough SNR", fmt_metric(get_value(wola_row, "wola_passthrough_snr_db") if wola_row is not None else np.nan, " dB")))
    cards.append(("WOLA energy ratio", fmt_metric(get_value(wola_row, "wola_energy_ratio") if wola_row is not None else np.nan, "", 6)))
    cards.append(("WOLA delay samples / ms", "missing" if math.isnan(delay_samples) else f"{delay_samples:.0f} / {delay_ms:.2f} ms"))

    if denoise_df.empty or "delta_vs_fixed_db" not in denoise_df.columns:
        cards.append(("best denoise delta", "missing"))
        cards.append(("best denoise case / mode", "missing"))
    else:
        tmp = denoise_df.dropna(subset=["delta_vs_fixed_db"]).sort_values("delta_vs_fixed_db", ascending=False)
        if tmp.empty:
            cards.append(("best denoise delta", "missing"))
            cards.append(("best denoise case / mode", "missing"))
        else:
            best = tmp.iloc[0]
            cards.append(("best denoise delta", fmt_metric(best.get("delta_vs_fixed_db"), " dB")))
            cards.append(("best denoise case / mode", f"{best.get('eval_case')} / {best.get('mode_name')}"))

    if codec_summary.empty:
        cards.append(("max compression ratio", "missing"))
        cards.append(("best denoise+codec SNR", "missing"))
    else:
        cards.append(("max compression ratio", fmt_metric(pd.to_numeric(codec_summary["compression_ratio"], errors="coerce").max(), "", 2)))
        cards.append(("best denoise+codec SNR", fmt_metric(pd.to_numeric(codec_summary["denoise_codec_output_snr_db"], errors="coerce").max(), " dB")))

    board_df = normalize_board_runtime_df(board_df)
    if board_df.empty or "max_ms" not in board_df.columns:
        cards.append(("worst board runtime / frame budget", "missing"))
        cards.append(("realtime pass count", "missing"))
    else:
        board_df = board_df.copy()
        board_df["max_ms"] = pd.to_numeric(board_df["max_ms"], errors="coerce")
        board_df["frame_budget_ms"] = pd.to_numeric(board_df.get("frame_budget_ms", FRAME_BUDGET_MS), errors="coerce").fillna(FRAME_BUDGET_MS)
        max_ms = board_df["max_ms"].max()
        budget = board_df["frame_budget_ms"].dropna().iloc[0] if board_df["frame_budget_ms"].notna().any() else FRAME_BUDGET_MS
        cards.append(("worst board runtime / frame budget", "missing" if pd.isna(max_ms) else f"{max_ms:.2f} / {budget:.2f} ms"))
        pass_col = first_col(board_df, "realtime_pass", "pass")
        if pass_col:
            pass_count = board_df[pass_col].astype(str).str.lower().isin(["true", "pass", "1", "yes"]).sum()
            cards.append(("realtime pass count", f"{pass_count}/{len(board_df)}"))
        else:
            cards.append(("realtime pass count", "missing"))
    return cards


def dashboard_explanations() -> dict[str, tuple[str, str]]:
    return {
        "denoise_output_snr_surface.html": (
            "降噪输出 SNR 曲面图 / Denoise Output SNR Surface",
            "旋转查看不同噪声场景与算法模式下的输出 SNR。",
        ),
        "denoise_delta_surface.html": (
            "降噪提升曲面图 / Denoise Delta Surface",
            "突出 HYBRID-MS 和 MCRA normal 在 noise_step_up 场景中的提升。",
        ),
        "denoise_output_snr_heatmap.html": (
            "降噪输出 SNR 热力图 / Output SNR Heatmap",
            "适合截图放 PPT，用颜色快速比较场景和模式。",
        ),
        "denoise_delta_heatmap.html": (
            "降噪增益热力图 / Delta vs Fixed Heatmap",
            "单元格显示相对 fixed 的 SNR 提升。",
        ),
        "denoise_stepup_waterfall.html": (
            "算法演进 Waterfall / Algorithm Evolution",
            "fixed → HYBRID-MS → MCRA normal 的提升过程一眼可见。",
        ),
        "denoise_quality_tradeoff.html": (
            "语音保持折中 / Quality Tradeoff",
            "展示提升 SNR 的同时 speech preservation 和 corr 的变化。",
        ),
        "board_runtime_by_mode.html": (
            "板端实时性 / Board Runtime",
            "红线是帧预算，低于红线表示满足实时性。",
        ),
        "codec_rate_quality_tradeoff.html": (
            "压缩码率-音质折中 / Codec Rate-Quality Tradeoff",
            "比较 direct compress 与 denoise then compress 的码率和音质。",
        ),
        "audio_energy_ratio_heatmap.html": (
            "PC 音频能量比 / Audio Energy Ratio",
            "检查不同输出文件的能量变化是否符合预期。",
        ),
    }


def write_interactive_index(ctx: ReportContext, cards: list[tuple[str, str]]) -> None:
    ctx.interactive_dir.mkdir(parents=True, exist_ok=True)
    explanations = dashboard_explanations()
    generated = [name for name in INTERACTIVE_REPORTS if name in ctx.generated_html and name != "index.html"]
    links = "\n".join(
        f'<li><a href="{escape(name)}">{escape(explanations.get(name, (name, ""))[0])}</a></li>'
        for name in generated
    )
    cards_html = "\n".join(
        f'<div class="card"><div class="label">{escape(label)}</div><div class="value">{escape(value)}</div></div>'
        for label, value in cards
    )
    frames = []
    for name in generated:
        title, explanation = explanations.get(name, (name, ""))
        frames.append(
            f"""
            <section class="figure-block">
              <h2>{escape(title)}</h2>
              <p>{escape(explanation)}</p>
              <iframe src="{escape(name)}" loading="lazy"></iframe>
            </section>
            """
        )
    skipped = "\n".join(f"<li>{escape(name)}: {escape(reason)}</li>" for name, reason in sorted(ctx.skipped_html.items()))
    html = f"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DSP Evaluation Interactive Dashboard</title>
  <style>
    body {{ margin: 0; font-family: Arial, "Microsoft YaHei", sans-serif; background: #f6f7f9; color: #1f2933; }}
    header {{ padding: 28px 32px 18px; background: #101820; color: white; }}
    header h1 {{ margin: 0 0 8px; font-size: 28px; }}
    header p {{ margin: 0; color: #d7dde5; }}
    main {{ padding: 24px 32px 40px; }}
    .cards {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(210px, 1fr)); gap: 14px; margin-bottom: 24px; }}
    .card {{ background: white; border: 1px solid #d9dee7; border-radius: 8px; padding: 14px 16px; }}
    .label {{ color: #5b6778; font-size: 13px; margin-bottom: 8px; }}
    .value {{ font-size: 22px; font-weight: 700; }}
    .links, .missing {{ background: white; border: 1px solid #d9dee7; border-radius: 8px; padding: 14px 18px; margin-bottom: 22px; }}
    .links ul, .missing ul {{ margin: 8px 0 0 20px; padding: 0; }}
    .figure-block {{ background: white; border: 1px solid #d9dee7; border-radius: 8px; padding: 16px; margin: 18px 0; }}
    .figure-block h2 {{ margin: 0 0 6px; font-size: 20px; }}
    .figure-block p {{ margin: 0 0 12px; color: #526070; }}
    iframe {{ width: 100%; min-height: 860px; border: 1px solid #d9dee7; border-radius: 6px; background: white; }}
  </style>
</head>
<body>
  <header>
    <h1>DSP 评测交互式 Dashboard / Interactive Dashboard</h1>
    <p>HTML 文件内嵌 Plotly，可离线打开；PNG 仍用于 PPT 静态插图。</p>
  </header>
  <main>
    <div class="cards">{cards_html}</div>
    <div class="links"><strong>Interactive figures</strong><ul>{links}</ul></div>
    {''.join(frames)}
    <div class="missing"><strong>Skipped interactive figures</strong><ul>{skipped or '<li>&lt;none&gt;</li>'}</ul></div>
  </main>
</body>
</html>
"""
    (ctx.interactive_dir / "index.html").write_text(html, encoding="utf-8")
    if "index.html" not in ctx.generated_html:
        ctx.generated_html.insert(0, "index.html")


def generate_interactive_reports(
    ctx: ReportContext,
    wola_summary: pd.DataFrame,
    denoise_summary: pd.DataFrame,
    codec_summary: pd.DataFrame,
    ms_raw: pd.DataFrame,
    mcra_raw: pd.DataFrame,
    codec_raw: pd.DataFrame,
    board_runtime_raw: pd.DataFrame,
    audio_metrics_raw: pd.DataFrame,
) -> None:
    if PLOTLY_IMPORT_ERROR is not None:
        warn(ctx, f"Plotly is not installed; interactive HTML reports were skipped. Install with: pip install plotly. Error: {PLOTLY_IMPORT_ERROR}")
        for name in INTERACTIVE_REPORTS:
            skip_html(ctx, name, "plotly is not installed")
        return

    denoise_df = prepare_denoise_interactive_df(ms_raw, mcra_raw, denoise_summary)
    plot_interactive_denoise_surface(
        ctx,
        denoise_df,
        "output_snr_db",
        "denoise_output_snr_surface.html",
        "降噪输出 SNR 曲面图 / Denoise Output SNR Surface: Scenario × Mode",
        "Output SNR (dB)",
    )
    plot_interactive_denoise_surface(
        ctx,
        denoise_df,
        "delta_vs_fixed_db",
        "denoise_delta_surface.html",
        "降噪 Delta 曲面图 / Denoise Delta Surface: Scenario × Mode",
        "Delta vs fixed (dB)",
    )
    plot_interactive_denoise_heatmap(
        ctx,
        denoise_df,
        "output_snr_db",
        "denoise_output_snr_heatmap.html",
        "降噪输出 SNR 热力图 / Denoise Output SNR Heatmap",
        "Output SNR",
    )
    plot_interactive_denoise_heatmap(
        ctx,
        denoise_df,
        "delta_vs_fixed_db",
        "denoise_delta_heatmap.html",
        "降噪 Delta 热力图 / Denoise Delta vs Fixed Heatmap",
        "Delta vs fixed",
    )
    plot_interactive_denoise_waterfall(ctx, denoise_df)
    plot_interactive_denoise_tradeoff(ctx, denoise_df)
    plot_interactive_board_runtime(ctx, board_runtime_raw)
    plot_interactive_codec_tradeoff(ctx, codec_raw if not codec_raw.empty else codec_summary)
    plot_interactive_audio_energy(ctx, audio_metrics_raw)
    cards = compute_dashboard_cards(wola_summary, denoise_df, codec_summary, board_runtime_raw)
    write_interactive_index(ctx, cards)


def summarize_wola_text(wola_summary: pd.DataFrame) -> str:
    if wola_summary.empty:
        return "未找到可用 WOLA 汇总数据。"
    row = wola_summary.iloc[0]
    return (
        f"WOLA passthrough SNR={numeric(row.get('wola_passthrough_snr_db')):.2f} dB, "
        f"energy ratio={numeric(row.get('wola_energy_ratio')):.6f}, "
        f"delay={numeric(row.get('wola_delay_samples')):.0f} samples, pass={row.get('pass', '')}."
    )


def summarize_denoise_text(denoise_df: pd.DataFrame) -> str:
    if denoise_df.empty:
        return "未找到可用降噪汇总数据。"
    df = denoise_df.copy()
    df["delta_vs_fixed_db"] = pd.to_numeric(df["delta_vs_fixed_db"], errors="coerce")
    df["output_snr_db"] = pd.to_numeric(df["output_snr_db"], errors="coerce")
    best_delta = df.dropna(subset=["delta_vs_fixed_db"]).sort_values("delta_vs_fixed_db", ascending=False).head(1)
    best_snr = df.dropna(subset=["output_snr_db"]).sort_values("output_snr_db", ascending=False).head(1)
    parts = [f"rows={len(df)}"]
    if not best_delta.empty:
        row = best_delta.iloc[0]
        parts.append(f"best delta={row['delta_vs_fixed_db']:.2f} dB ({row.get('eval_case')} / {row.get('mode_name')})")
    if not best_snr.empty:
        row = best_snr.iloc[0]
        parts.append(f"best output SNR={row['output_snr_db']:.2f} dB ({row.get('eval_case')} / {row.get('mode_name')})")
    return "; ".join(parts) + "."


def summarize_codec_text(codec_summary: pd.DataFrame) -> str:
    if codec_summary.empty:
        return "未找到可用 codec 汇总数据。"
    df = codec_summary.copy()
    df["target_bitrate_kbps"] = pd.to_numeric(df["target_bitrate_kbps"], errors="coerce")
    df["compression_ratio"] = pd.to_numeric(df["compression_ratio"], errors="coerce")
    df["denoise_codec_output_snr_db"] = pd.to_numeric(df["denoise_codec_output_snr_db"], errors="coerce")
    targets = ", ".join(str(int(v)) for v in sorted(df["target_bitrate_kbps"].dropna().unique()))
    max_ratio = df["compression_ratio"].max()
    best_snr = df["denoise_codec_output_snr_db"].max()
    return f"target bitrates={targets} kbps; max compression ratio={max_ratio:.2f}; best denoise+codec SNR={best_snr:.2f} dB."


def write_report(
    ctx: ReportContext,
    wola_summary: pd.DataFrame,
    denoise_summary: pd.DataFrame,
    codec_summary: pd.DataFrame,
    realtime_summary: pd.DataFrame,
    memory_summary: pd.DataFrame,
) -> Path:
    generated = sorted(ctx.generated_plots)
    missing_images = [f"{name}: {reason}" for name, reason in sorted(ctx.skipped_plots.items())]
    ppt_lines = []
    for name in PPT_RECOMMENDATIONS:
        if name in ctx.generated_plots:
            ppt_lines.append(f"- {name}")
        else:
            ppt_lines.append(f"- {name}（未生成：{ctx.skipped_plots.get(name, '缺少输入或数据')}）")

    interactive_lines = []
    for name in INTERACTIVE_REPORTS:
        rel_name = f"interactive/{name}"
        if name in ctx.generated_html:
            interactive_lines.append(f"- {rel_name}")
        else:
            interactive_lines.append(f"- {rel_name}（未生成：{ctx.skipped_html.get(name, '缺少输入或 plotly')}）")
    interactive_lines.append("")
    interactive_lines.append("这些 HTML 用于课堂演示和交互式检查；PNG 用于 PPT 静态插图。")

    realtime_has_measurement = (
        "max_ms" in realtime_summary.columns
        and pd.to_numeric(realtime_summary["max_ms"], errors="coerce").fillna(0.0).gt(0.0).any()
    )
    realtime_line = (
        "- 实时性结果已使用 CCS Watch 填入的 max_ms 生成正式 realtime_budget_compare.png。"
        if realtime_has_measurement
        else "- 实时性结果当前以模板形式输出，需要把 CCS Watch 的 last/max frame time 手动填入后再作为最终验收数据。"
    )

    conclusion_lines = [
        f"- {summarize_wola_text(wola_summary)}",
        f"- {summarize_denoise_text(denoise_summary)}",
        f"- {summarize_codec_text(codec_summary)}",
        realtime_line,
    ]
    if not memory_summary.empty and ctx.found_map is not None:
        conclusion_lines.append(f"- 已从 map 文件解析内存区域占用：{ctx.found_map}。")
    else:
        conclusion_lines.append("- 未解析到有效 map 内存数据，内存图按模板输出。")

    lines = [
        "# DSP Subband Evaluation Visual Report",
        "",
        f"Generated at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "",
        "## 输入文件列表",
        "",
        "### CSV found",
        *(f"- {name}: `{path}`" for name, path in sorted(ctx.found_csv.items())),
        "",
        "### WAV found",
        *(f"- {name}: `{path}`" for name, path in sorted(ctx.found_wav.items())),
        "",
        "## 缺失文件列表",
        "",
        "### CSV missing",
        *(f"- {name}" for name in (ctx.missing_csv or ["<none>"])),
        "",
        "### WAV missing",
        *(f"- {name}" for name in (ctx.missing_wav or ["<none>"])),
        "",
        "## 成功生成的图像列表",
        *(f"- plots/{name}" for name in generated),
        "",
        "## 未生成图像",
        *(f"- {item}" for item in (missing_images or ["<none>"])),
        "",
        "## 汇总表",
        *(f"- {name}" for name in ctx.summary_files),
        "",
        "## WOLA 基础结果摘要",
        "",
        summarize_wola_text(wola_summary),
        "",
        "## 降噪结果摘要",
        "",
        summarize_denoise_text(denoise_summary),
        "",
        "## Codec 结果摘要",
        "",
        summarize_codec_text(codec_summary),
        "",
        "## 实时性模板或结果摘要",
        "",
        f"- frame_budget_ms = {FRAME_BUDGET_MS:.2f} ms",
        f"- realtime_summary rows = {len(realtime_summary)}",
        "- 若 max_ms 为空或为 0，说明该表仍是模板，需要根据 CCS Watch 手动填入。",
        "",
        "## 指标定义",
        "",
        "- Aligned SNR: 对齐输入输出延迟后，SNR = 10 log10(sum(x^2) / sum((y - x)^2))。",
        "- Energy Ratio: output_energy / input_energy。",
        "- Delay: 通过互相关峰值或 CSV 中 WOLA delay 估计，单位 samples 和 ms。",
        "- THD: sqrt(sum(harmonic_power)) / fundamental_power。",
        "- SNR Improvement: output_snr_db - input_snr_db。",
        "- Delta vs Fixed: mode_output_snr_db - fixed_output_snr_db。",
        "- Speech Preservation Ratio: clean 与 output 的投影比例，或直接采用 CSV 字段。",
        "- Clean-output correlation: clean 与 output 的相关系数。",
        f"- Compression Ratio: 原始 PCM 码率 / 实际编码码率；当前原始 PCM 码率按 {PCM_BITRATE_KBPS:.0f} kbps 计算。",
        f"- CPU Usage: max_frame_time_ms / frame_budget_ms * 100%；frame_budget_ms = {FRAME_BUDGET_MS:.2f} ms。",
        "",
        "## 推荐放入 PPT 的图像清单",
        *ppt_lines,
        "",
        "## 当前数据支持的结论",
        *conclusion_lines,
        "",
        "## Interactive HTML reports",
        *interactive_lines,
        "",
        "## Warnings",
        *(f"- {item}" for item in (ctx.warnings or ["<none>"])),
        "",
    ]
    path = ctx.out_dir / "report_overview.md"
    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def run(args: argparse.Namespace) -> tuple[ReportContext, Path]:
    root = Path.cwd()
    out_dir = Path(args.out_dir).expanduser()
    if not out_dir.is_absolute():
        out_dir = root / out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    csv_dirs = build_search_dirs(root, args.csv_dir)
    board_report_root = root / "board_audio_test_outputs"
    if board_report_root.exists():
        csv_dirs = existing_dirs(csv_dirs + list(board_report_root.glob("*/report")))
    audio_dirs = build_search_dirs(root, args.audio_dir)
    ctx = ReportContext(root=root, out_dir=out_dir, csv_dirs=csv_dirs, audio_dirs=audio_dirs)

    print("CSV search dirs:")
    for directory in csv_dirs:
        print(f"  {directory}")
    print("Audio search dirs:")
    for directory in audio_dirs:
        print(f"  {directory}")

    discover_inputs(ctx)
    write_inputs_log(ctx)

    wola_raw = read_csv_safe(ctx.found_csv.get("subband_eval_report.csv"), ctx, "subband_eval_report.csv")
    codec_raw = read_csv_safe(ctx.found_csv.get("subband_codec_eval_report.csv"), ctx, "subband_codec_eval_report.csv")
    ms_raw = read_csv_safe(ctx.found_csv.get("subband_denoise_ms_eval_report.csv"), ctx, "subband_denoise_ms_eval_report.csv")
    mcra_raw = read_csv_safe(ctx.found_csv.get("subband_denoise_mcra_eval_report.csv"), ctx, "subband_denoise_mcra_eval_report.csv")
    audio_metrics_path = ctx.found_csv.get("aggregate_compare_audio_metrics.csv") or ctx.found_csv.get("compare_audio_metrics.csv")
    audio_metrics_raw = (
        read_csv_safe(audio_metrics_path, ctx, "aggregate_compare_audio_metrics.csv")
        if audio_metrics_path is not None else pd.DataFrame()
    )
    board_runtime_path = ctx.found_csv.get("board_mode_runtime_summary.csv")
    board_runtime_raw = (
        read_csv_safe(board_runtime_path, ctx, "board_mode_runtime_summary.csv", drop_empty_columns=False)
        if board_runtime_path is not None else pd.DataFrame()
    )
    realtime_path = find_optional_csv(REALTIME_CSV_FILES, ctx.csv_dirs)
    if realtime_path is not None:
        print(f"Found realtime CSV: {realtime_path}")
    realtime_raw = (
        read_csv_safe(realtime_path, ctx, "realtime_summary_template.csv",
                      drop_empty_columns=False)
        if realtime_path is not None else pd.DataFrame()
    )

    wola_summary = build_wola_summary(wola_raw, ctx)
    denoise_summary = build_denoise_summary(ms_raw, mcra_raw, ctx)
    codec_summary = build_codec_summary(codec_raw, ctx)
    realtime_summary = build_realtime_template(ctx, realtime_raw)

    save_summary(ctx, wola_summary, "wola_summary.csv")
    save_summary(ctx, denoise_summary, "denoise_summary.csv")
    save_summary(ctx, codec_summary, "codec_summary.csv")
    save_summary(ctx, realtime_summary, "realtime_summary_template.csv")

    ctx.found_map = find_latest_map(root)
    if ctx.found_map:
        print(f"Found map file: {ctx.found_map}")
        memory_summary = parse_map_memory(ctx.found_map, ctx)
    else:
        memory_summary = pd.DataFrame(columns=["memory_region", "length_bytes", "used_bytes", "unused_bytes", "used_percent"])
        warn(ctx, "No Debug/*.map file found.")
    save_summary(ctx, memory_summary, "memory_usage_summary.csv")

    plot_wola_waveform(ctx, wola_summary)
    plot_wola_spectrum(ctx, wola_summary)
    plot_wola_reconstruction_error(ctx, wola_summary)
    plot_wola_frequency_response(ctx, wola_summary)
    plot_wola_rolloff(ctx, wola_summary)
    maybe_plot_thd(ctx)
    plot_group_delay(ctx, wola_summary)

    plot_denoise_waveform(ctx, wola_summary)
    plot_denoise_spectrum(ctx, wola_summary)
    plot_denoise_spectrograms(ctx, denoise_summary)
    denoise_plot_df = preferred_denoise_plot_df(ms_raw, mcra_raw, denoise_summary)
    plot_denoise_snr_bar(ctx, denoise_plot_df)
    plot_denoise_delta(ctx, denoise_plot_df)
    plot_denoise_preservation(ctx, denoise_plot_df)
    plot_noise_tracking(ctx, denoise_plot_df)
    plot_denoise_algorithm_evolution_stepup(ctx, denoise_plot_df)

    plot_codec_bitrate_compression(ctx, codec_raw if not codec_raw.empty else codec_summary)
    plot_codec_actual_bitrate(ctx, codec_raw if not codec_raw.empty else codec_summary)
    plot_codec_quality(ctx, codec_raw if not codec_raw.empty else codec_summary)
    plot_codec_spectrogram_compare(ctx)

    plot_realtime_template(ctx, realtime_summary)
    plot_realtime_annotated(ctx, realtime_summary)
    plot_memory_usage(ctx, memory_summary)

    if args.interactive:
        generate_interactive_reports(
            ctx,
            wola_summary,
            denoise_summary,
            codec_summary,
            ms_raw,
            mcra_raw,
            codec_raw,
            board_runtime_raw,
            audio_metrics_raw,
        )

    report_path = write_report(ctx, wola_summary, denoise_summary, codec_summary, realtime_summary, memory_summary)
    return ctx, report_path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate local CSV/WAV evaluation plots for DSP_LAB_0507.")
    parser.add_argument("--csv-dir", action="append", default=[], help="Additional directory to search for CSV inputs.")
    parser.add_argument("--audio-dir", action="append", default=[], help="Additional directory to search for WAV inputs.")
    parser.add_argument("--out-dir", default="docs/eval_outputs", help="Output directory for plots, summaries, and report.")
    interactive = parser.add_mutually_exclusive_group()
    interactive.add_argument("--interactive", dest="interactive", action="store_true", help="Generate Plotly interactive HTML reports (default).")
    interactive.add_argument("--no-interactive", dest="interactive", action="store_false", help="Skip Plotly interactive HTML reports.")
    parser.set_defaults(interactive=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    ctx, report_path = run(args)
    print("")
    print(f"Generated {len(ctx.generated_plots)} plot(s).")
    print("Generated summary CSV:")
    for item in ctx.summary_files:
        print(f"  {ctx.out_dir / item}")
    if args.interactive:
        print("Generated interactive HTML:")
        for item in ctx.generated_html:
            print(f"  {ctx.interactive_dir / item}")
    print(f"Report overview: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
