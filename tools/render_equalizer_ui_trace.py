"""Replay the real Project 3.3 Host renderer trace into offline PNG previews."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
BASH = Path(r"C:\msys64\usr\bin\bash.exe")
WIDTH = 800
HEIGHT = 480
PREVIEW_NAMES = (
    "dynamic_status_initial",
    "dynamic_status_all_armed",
    "dynamic_status_all_active",
    "eq_editor_flat",
    "eq_editor_vocal",
    "eq_editor_draft_125hz_plus2",
    "eq_editor_building_custom",
    "eq_editor_custom_applied",
    "eq_editor_gain_limits",
    "page_transition_intermediate",
)
COLORS = {
    0: "#000000",
    1: "#ffffff",
    2: "#b0c4de",
    3: "#808080",
    4: "#1e90ff",
    5: "#32cd32",
    6: "#00bfff",
    7: "#ff4500",
    8: "#ffd700",
}
REQUIRED_TRACE_FIELDS = {
    "draw_sequence", "operation", "x", "y", "w", "h", "color",
    "job", "page", "frame", "text_or_glyph_id",
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def current_commit() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, check=True,
        text=True, stdout=subprocess.PIPE,
    )
    return result.stdout.strip()


def build_and_capture(output_dir: Path, commit: str) -> None:
    env = os.environ.copy()
    env["EQ_TRACE_OUTPUT"] = str(output_dir.resolve())
    env["EQ_TRACE_COMMIT"] = commit
    command = (
        "export PATH=/mingw64/bin:/usr/bin:$PATH; "
        "cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507; "
        "gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
        "-DEQ_ENABLE_LCD_DISPLAY=1 -DEQ_ENABLE_TEN_BAND_EDITOR=1 "
        "-ICode/User/user_dsp "
        "tools/tests/equalizer_ui_trace_test.c "
        "Code/User/user_dsp/user_equalizer.c "
        "Code/User/user_dsp/user_equalizer_ui_logic.c "
        "Code/User/user_dsp/user_equalizer_display.c -lm "
        "-o /tmp/equalizer_ui_trace_test.exe && "
        "out=$(cygpath -u \"$EQ_TRACE_OUTPUT\") && mkdir -p \"$out\" && "
        "/tmp/equalizer_ui_trace_test.exe \"$out\" \"$EQ_TRACE_COMMIT\""
    )
    result = subprocess.run(
        [str(BASH), "-lc", command], cwd=ROOT, env=env,
        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        encoding="utf-8", errors="replace", check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stdout)


def read_trace(path: Path) -> list[dict[str, object]]:
    operations: list[dict[str, object]] = []
    with path.open(encoding="utf-8") as source:
        for line_number, line in enumerate(source, start=1):
            record = json.loads(line)
            missing = REQUIRED_TRACE_FIELDS - record.keys()
            if missing:
                raise ValueError(f"{path}:{line_number}: missing {sorted(missing)}")
            x, y = int(record["x"]), int(record["y"])
            w, h = int(record["w"]), int(record["h"])
            if x < 0 or y < 0 or w <= 0 or h <= 0:
                raise ValueError(f"{path}:{line_number}: invalid rectangle")
            if x + w > WIDTH or y + h > HEIGHT:
                raise ValueError(f"{path}:{line_number}: out of bounds")
            operations.append(record)
    if not operations:
        raise ValueError(f"empty trace: {path}")
    return operations


def replay(operations: list[dict[str, object]], output: Path) -> str:
    image = Image.new("RGB", (WIDTH, HEIGHT), COLORS[0])
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    for record in operations:
        operation = str(record["operation"])
        x, y = int(record["x"]), int(record["y"])
        w, h = int(record["w"]), int(record["h"])
        color = COLORS[int(record["color"])]
        right, bottom = x + w - 1, y + h - 1
        if operation == "fill_rect":
            draw.rectangle((x, y, right, bottom), fill=color)
        elif operation == "draw_rect":
            draw.rectangle((x, y, right, bottom), outline=color)
        elif operation == "line":
            draw.line((x, y, right, bottom), fill=color)
        elif operation == "ascii_text":
            text = str(record["text_or_glyph_id"])
            centered = int(record.get("centered", 0)) != 0
            if centered:
                bounds = draw.textbbox((0, 0), text, font=font)
                tx = x + (w - (bounds[2] - bounds[0])) // 2
            else:
                tx = x + 2
            ty = y + max(0, (h - 12) // 2)
            draw.text((tx, ty), text, fill=color, font=font)
        elif operation == "chinese_glyph":
            # The following traced line records are emitted directly from
            # s_cn_bits; replaying them preserves the C 16x16 bitmap exactly.
            continue
        else:
            raise ValueError(f"unknown trace operation: {operation}")
    image.save(output, optimize=True)
    return hashlib.sha256(image.tobytes()).hexdigest()


def render_all(output_dir: Path, commit: str) -> dict[str, object]:
    output_dir.mkdir(parents=True, exist_ok=True)
    build_and_capture(output_dir, commit)
    previews: list[dict[str, object]] = []
    combined_path = output_dir / "draw_trace.jsonl"
    total_records = 0
    glyph_records = 0
    with combined_path.open("w", encoding="utf-8", newline="\n") as combined:
        for name in PREVIEW_NAMES:
            trace_path = output_dir / f"{name}.jsonl"
            source_path = output_dir / f"{name}.source.json"
            png_path = output_dir / f"{name}.png"
            metadata_path = output_dir / f"{name}.json"
            operations = read_trace(trace_path)
            for record in operations:
                combined_record = {"preview": name, **record}
                combined.write(json.dumps(
                    combined_record, ensure_ascii=True,
                    separators=(",", ":"),
                ) + "\n")
            pixel_checksum = replay(operations, png_path)
            metadata = json.loads(source_path.read_text(encoding="utf-8"))
            metadata.update({
                "evidence_label": "OFFLINE_RENDER_PREVIEW",
                "trace_file": trace_path.name,
                "png_file": png_path.name,
                "trace_sha256": sha256_file(trace_path),
                "png_sha256": sha256_file(png_path),
                "pixel_checksum": pixel_checksum,
                "operation_count": len(operations),
                "bounds_valid": True,
                "chinese_glyph_source": "C_s_cn_bits_traced_lines",
            })
            metadata_path.write_text(
                json.dumps(metadata, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            source_path.unlink()
            glyph_count = sum(
                record["operation"] == "chinese_glyph"
                for record in operations
            )
            glyph_records += glyph_count
            total_records += len(operations)
            previews.append({
                "name": name,
                "png": png_path.name,
                "metadata": metadata_path.name,
                "trace": trace_path.name,
                "pixel_checksum": pixel_checksum,
                "operation_count": len(operations),
                "glyph_records": glyph_count,
            })
    manifest = {
        "source_commit": commit,
        "evidence_label": "OFFLINE_RENDER_PREVIEW",
        "screen": {"width": WIDTH, "height": HEIGHT},
        "preview_count": len(previews),
        "trace_record_count": total_records,
        "chinese_glyph_record_count": glyph_records,
        "bounds_failures": 0,
        "previews": previews,
    }
    (output_dir / "ui_preview_manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    summary = {
        "status": "UI_RENDER_TRACE_PASS",
        "source_commit": commit,
        "preview_count": len(previews),
        "trace_record_count": total_records,
        "chinese_glyph_record_count": glyph_records,
        "bounds_failures": 0,
        "combined_trace": combined_path.name,
        "combined_trace_sha256": sha256_file(combined_path),
    }
    (output_dir / "ui_render_trace_summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return summary


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--source-commit")
    args = parser.parse_args()
    commit = args.source_commit or current_commit()
    summary = render_all(args.output_dir, commit)
    print(json.dumps(summary, sort_keys=True))


if __name__ == "__main__":
    main()
