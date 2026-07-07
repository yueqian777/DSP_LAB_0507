from __future__ import annotations

import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd


ROOT = Path(__file__).resolve().parent
BOARD_DIR = ROOT / "board"
PC_DIR = ROOT / "pc_audio"
REPORT_DIR = ROOT / "report"
REPORT_DIR.mkdir(parents=True, exist_ok=True)


def read_board_summary() -> pd.DataFrame:
    path = BOARD_DIR / "board_mode_summary_normalized.csv"
    df = pd.read_csv(path)
    numeric_cols = [
        "mode",
        "applied",
        "ad_frames",
        "da_frames",
        "budget_ms",
        "last_us",
        "max_us",
        "cpu_percent",
        "track_mode",
        "strong",
        "ready",
        "learn_progress",
        "input_power",
        "output_power",
        "noise_psd",
    ]
    for col in numeric_cols:
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df["budget_us"] = df["budget_ms"] * 1000.0
    df["runtime_margin_us"] = df["budget_us"] - df["max_us"]
    df["realtime_pass"] = df["max_us"] < df["budget_us"]
    return df


def read_compare_metrics() -> pd.DataFrame:
    rows: list[pd.DataFrame] = []
    for metrics_path in sorted(PC_DIR.glob("*/compare_audio_metrics.csv")):
        case = metrics_path.parent.name
        df = pd.read_csv(metrics_path)
        df.insert(0, "case", case)
        rows.append(df)
    if not rows:
        return pd.DataFrame()
    out = pd.concat(rows, ignore_index=True)
    for col in [
        "target_bitrate_kbps",
        "actual_bitrate_kbps",
        "compression_ratio",
        "payload_bits",
        "avg_bits_per_scalar",
        "output_energy_ratio",
        "clipping_count",
        "invalid_count",
        "nonzero_output_count",
    ]:
        out[col] = pd.to_numeric(out[col], errors="coerce")
    return out


def save_csv(df: pd.DataFrame, name: str) -> Path:
    path = REPORT_DIR / name
    df.to_csv(path, index=False, encoding="utf-8-sig")
    return path


def plot_board_runtime(df: pd.DataFrame) -> None:
    fig, ax = plt.subplots(figsize=(10, 5.2), dpi=160)
    ax.bar(df["mode_name"], df["max_us"], label="max runtime")
    ax.axhline(float(df["budget_us"].iloc[0]), color="tab:red", linestyle="--", label="20.48 ms budget")
    ax.set_ylabel("microseconds per 1024-sample frame")
    ax.set_title("Board runtime by demo mode")
    ax.tick_params(axis="x", rotation=25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(REPORT_DIR / "board_runtime_by_mode.png")
    plt.close(fig)


def plot_energy(df: pd.DataFrame) -> None:
    pivot = df.pivot_table(index="case", columns="mode", values="output_energy_ratio", aggfunc="mean")
    fig, ax = plt.subplots(figsize=(11, 5.8), dpi=160)
    image = ax.imshow(pivot.to_numpy(dtype=float), aspect="auto", cmap="viridis")
    ax.set_yticks(range(len(pivot.index)))
    ax.set_yticklabels(pivot.index)
    ax.set_xticks(range(len(pivot.columns)))
    ax.set_xticklabels(pivot.columns, rotation=30, ha="right")
    ax.set_title("Output/input energy ratio by audio case")
    fig.colorbar(image, ax=ax, label="energy ratio")
    fig.tight_layout()
    fig.savefig(REPORT_DIR / "pc_audio_energy_ratio_heatmap.png")
    plt.close(fig)


def plot_codec_bitrate(df: pd.DataFrame) -> None:
    codec = df[df["target_bitrate_kbps"] > 0].copy()
    if codec.empty:
        return
    fig, ax = plt.subplots(figsize=(10, 5.2), dpi=160)
    grouped = codec.groupby(["mode", "target_bitrate_kbps"], as_index=False)["actual_bitrate_kbps"].mean()
    for mode, part in grouped.groupby("mode"):
        ax.plot(part["target_bitrate_kbps"], part["actual_bitrate_kbps"], marker="o", label=mode)
    ax.plot([160, 320], [160, 320], color="gray", linestyle="--", label="target")
    ax.set_xlabel("target bitrate (kbps)")
    ax.set_ylabel("actual bitrate (kbps)")
    ax.set_title("Average actual codec bitrate across audio cases")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(REPORT_DIR / "pc_audio_codec_actual_bitrate.png")
    plt.close(fig)


def write_markdown(board: pd.DataFrame, compare: pd.DataFrame) -> None:
    board_pass = int(board["realtime_pass"].sum())
    board_total = int(len(board))
    compare_runs = sorted(compare["case"].unique()) if not compare.empty else []
    lines = [
        "# Board + PC Audio Test Summary",
        "",
        "## Board Runtime",
        "",
        f"- Realtime pass: {board_pass}/{board_total}",
        f"- Worst max runtime: {board['max_us'].max():.0f} us",
        f"- Frame budget: {board['budget_us'].iloc[0]:.0f} us",
        "",
        "## PC Audio Compare",
        "",
        f"- Audio cases processed: {len(compare_runs)}",
        f"- Metrics rows: {len(compare)}",
        "",
        "## Notes",
        "",
        "- Board data comes from CCS DSS Watch variables while the target runs AD/DA in real time.",
        "- PC audio compare uses the current existing compare path. It exports raw input, WOLA, fixed denoise, direct codec, and denoise+codec WAVs.",
        "- Mode 6 and mode 7 are covered by board runtime Watch data. The current existing compare exporter does not emit separate mode 6/7 WAV files without changing project source.",
        "",
    ]
    lines.append("## Board Runtime Table")
    lines.append("")
    lines.append(board[["mode", "mode_name", "max_us", "budget_us", "cpu_percent", "realtime_pass"]].to_markdown(index=False))
    lines.append("")
    (REPORT_DIR / "summary.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    board = read_board_summary()
    compare = read_compare_metrics()
    save_csv(board, "board_mode_runtime_summary.csv")
    save_csv(compare, "aggregate_compare_audio_metrics.csv")
    if not compare.empty:
        mode_summary = (
            compare.groupby("mode", as_index=False)
            .agg(
                rows=("case", "count"),
                mean_energy_ratio=("output_energy_ratio", "mean"),
                max_clipping=("clipping_count", "max"),
                max_invalid=("invalid_count", "max"),
                mean_actual_bitrate_kbps=("actual_bitrate_kbps", "mean"),
                mean_compression_ratio=("compression_ratio", "mean"),
            )
            .sort_values("mode")
        )
        save_csv(mode_summary, "aggregate_compare_by_mode.csv")
        case_summary = (
            compare.groupby("case", as_index=False)
            .agg(
                rows=("mode", "count"),
                min_energy_ratio=("output_energy_ratio", "min"),
                max_clipping=("clipping_count", "max"),
                max_invalid=("invalid_count", "max"),
            )
            .sort_values("case")
        )
        save_csv(case_summary, "aggregate_compare_by_case.csv")
        plot_energy(compare)
        plot_codec_bitrate(compare)
    plot_board_runtime(board)
    write_markdown(board, compare)
    print(f"report_dir={REPORT_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
