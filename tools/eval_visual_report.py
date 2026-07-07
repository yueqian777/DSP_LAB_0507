#!/usr/bin/env python3
from __future__ import annotations

import argparse
import math
import re
import sys
import wave
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Iterable, Sequence

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


CSV_FILES = [
    "subband_eval_report.csv",
    "subband_codec_eval_report.csv",
    "subband_denoise_ms_eval_report.csv",
    "subband_denoise_mcra_eval_report.csv",
    "compare_audio_metrics.csv",
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
    "wola_spectrum_compare.png",
    "wola_frequency_response.png",
    "denoise_spectrogram_compare.png",
    "denoise_snr_bar.png",
    "denoise_delta_vs_fixed.png",
    "codec_bitrate_compression_ratio.png",
    "codec_quality_snr.png",
    "realtime_budget_compare_template.png",
]

FIGSIZE = (10.0, 5.625)
DPI = 160
PCM_BITRATE_KBPS = 800.0
FRAME_BUDGET_MS = 20.48
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
    summary_files: list[str] = field(default_factory=list)
    found_map: Path | None = None

    def __post_init__(self) -> None:
        self.plots_dir = self.out_dir / "plots"
        self.summary_dir = self.out_dir / "summary_tables"
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
        root / "docs" / "eval_baseline",
        root / "docs" / "eval_outputs" / "input",
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


def read_csv_safe(path: Path | None, ctx: ReportContext, logical_name: str) -> pd.DataFrame:
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


def build_realtime_template(ctx: ReportContext) -> pd.DataFrame:
    modes = ["WOLA only", "fixed denoise", "HYBRID-MS", "MCRA normal", "MCRA strong"]
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
    warn(ctx, "No standalone realtime CSV was used; generated a CCS Watch fill-in template.")
    return df


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


def save_fig(ctx: ReportContext, fig: plt.Figure, filename: str) -> None:
    path = ctx.plots_dir / filename
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


def save_spectrogram(ctx: ReportContext, label: str, sample_rate: int, samples: np.ndarray, filename: str) -> None:
    fig, ax = plt.subplots(figsize=FIGSIZE)
    ax.specgram(samples, NFFT=1024, Fs=sample_rate, noverlap=768, cmap="magma")
    ax.set_title(f"Spectrogram: {label}")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Frequency (Hz)")
    save_fig(ctx, fig, filename)


def plot_denoise_spectrograms(ctx: ReportContext) -> None:
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
        ax.set_ylabel(f"{label}\nHz")
    axes[-1].set_xlabel("Time (s)")
    fig.suptitle("Denoise spectrogram compare")
    save_fig(ctx, fig, "denoise_spectrogram_compare.png")


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
    filename = "realtime_budget_compare_template.png"
    df = realtime_df.copy()
    df["max_ms"] = pd.to_numeric(df["max_ms"], errors="coerce").fillna(0.0)
    fig, ax = plt.subplots(figsize=FIGSIZE)
    x = np.arange(len(df))
    ax.bar(x, df["max_ms"], label="max_ms (fill from CCS Watch)")
    ax.axhline(FRAME_BUDGET_MS, color="#b23a48", linestyle="--", label=f"frame budget {FRAME_BUDGET_MS:.2f} ms")
    ax.set_xticks(x)
    ax.set_xticklabels(df["mode"].astype(str), rotation=20, ha="right")
    ax.set_title("Realtime frame budget compare template")
    ax.set_ylabel("Frame time (ms)")
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend()
    ax.text(0.01, 0.95, "Template: fill max_ms from CCS Watch before final reporting.", transform=ax.transAxes, va="top")
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
    single_tone_candidates = [
        path
        for directory in ctx.audio_dirs
        for path in directory.glob("*.wav")
        if any(term in path.name.lower() for term in ["tone", "sine", "thd"])
    ]
    filename = "thd_spectrum_example.png"
    if not single_tone_candidates:
        skip_plot(ctx, filename, "missing single-tone test WAV")
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
    for n in range(2, 8):
        harmonic = fundamental_freq * n
        if harmonic >= sample_rate / 2:
            break
        idx = int(np.argmin(np.abs(freq - harmonic)))
        harmonic_power += float(10 ** (mag[idx] / 10.0))
    fundamental_power = float(10 ** (mag[fundamental_idx] / 10.0))
    thd = math.sqrt(harmonic_power / max(fundamental_power, EPS))
    fig, ax = plt.subplots(figsize=FIGSIZE)
    mask = freq <= min(10000, sample_rate / 2)
    ax.plot(freq[mask], mag[mask], linewidth=1.0)
    ax.axvline(fundamental_freq, color="#b23a48", label=f"fundamental {fundamental_freq:.1f} Hz")
    ax.set_title("THD spectrum example")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB)")
    ax.text(0.01, 0.95, f"THD={20 * math.log10(thd + EPS):.2f} dB ({thd * 100:.3f}%)", transform=ax.transAxes, va="top")
    ax.grid(True, alpha=0.3)
    ax.legend()
    save_fig(ctx, fig, filename)


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

    conclusion_lines = [
        f"- {summarize_wola_text(wola_summary)}",
        f"- {summarize_denoise_text(denoise_summary)}",
        f"- {summarize_codec_text(codec_summary)}",
        "- 实时性结果当前以模板形式输出，需要把 CCS Watch 的 last/max frame time 手动填入后再作为最终验收数据。",
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

    wola_summary = build_wola_summary(wola_raw, ctx)
    denoise_summary = build_denoise_summary(ms_raw, mcra_raw, ctx)
    codec_summary = build_codec_summary(codec_raw, ctx)
    realtime_summary = build_realtime_template(ctx)

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
    plot_denoise_spectrograms(ctx)
    denoise_plot_df = preferred_denoise_plot_df(ms_raw, mcra_raw, denoise_summary)
    plot_denoise_snr_bar(ctx, denoise_plot_df)
    plot_denoise_delta(ctx, denoise_plot_df)
    plot_denoise_preservation(ctx, denoise_plot_df)
    plot_noise_tracking(ctx, denoise_plot_df)

    plot_codec_bitrate_compression(ctx, codec_raw if not codec_raw.empty else codec_summary)
    plot_codec_actual_bitrate(ctx, codec_raw if not codec_raw.empty else codec_summary)
    plot_codec_quality(ctx, codec_raw if not codec_raw.empty else codec_summary)
    plot_codec_spectrogram_compare(ctx)

    plot_realtime_template(ctx, realtime_summary)
    plot_memory_usage(ctx, memory_summary)

    report_path = write_report(ctx, wola_summary, denoise_summary, codec_summary, realtime_summary, memory_summary)
    return ctx, report_path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate local CSV/WAV evaluation plots for DSP_LAB_0507.")
    parser.add_argument("--csv-dir", action="append", default=[], help="Additional directory to search for CSV inputs.")
    parser.add_argument("--audio-dir", action="append", default=[], help="Additional directory to search for WAV inputs.")
    parser.add_argument("--out-dir", default="docs/eval_outputs", help="Output directory for plots, summaries, and report.")
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
    print(f"Report overview: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
