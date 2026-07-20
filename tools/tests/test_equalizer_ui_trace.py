from __future__ import annotations

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
PYTHON = Path(r"D:\SoftwareDownload\python.exe")
PREVIEW_NAMES = {
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
}


class EqualizerUiTraceTest(unittest.TestCase):
    def test_real_renderer_trace_and_previews(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            result = subprocess.run(
                [str(PYTHON), "-B", "tools/render_equalizer_ui_trace.py",
                 "--output-dir", str(output),
                 "--source-commit", "TEST_COMMIT"],
                cwd=ROOT, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stdout)
            summary = json.loads(
                (output / "ui_render_trace_summary.json").read_text(
                    encoding="utf-8")
            )
            manifest = json.loads(
                (output / "ui_preview_manifest.json").read_text(
                    encoding="utf-8")
            )
            self.assertEqual(summary["status"], "UI_RENDER_TRACE_PASS")
            self.assertEqual(summary["bounds_failures"], 0)
            self.assertEqual(summary["preview_count"], 10)
            self.assertGreater(summary["trace_record_count"], 0)
            self.assertGreater(summary["chinese_glyph_record_count"], 0)
            names = {item["name"] for item in manifest["previews"]}
            self.assertEqual(names, PREVIEW_NAMES)
            checksums = set()
            for name in PREVIEW_NAMES:
                png = output / f"{name}.png"
                metadata = json.loads(
                    (output / f"{name}.json").read_text(encoding="utf-8")
                )
                with Image.open(png) as image:
                    self.assertEqual(image.size, (800, 480))
                self.assertEqual(metadata["bounds_failures"], 0)
                self.assertTrue(metadata["bounds_valid"])
                self.assertEqual(
                    metadata["evidence_label"], "OFFLINE_RENDER_PREVIEW")
                self.assertEqual(
                    metadata["chinese_glyph_source"],
                    "C_s_cn_bits_traced_lines")
                self.assertEqual(
                    metadata["framebuffer_model"],
                    "page_isolated_double_buffer")
                checksums.add(metadata["pixel_checksum"])
            self.assertGreaterEqual(len(checksums), 8)
            editor_trace = [
                json.loads(line)
                for line in (output / "eq_editor_flat.jsonl").read_text(
                    encoding="utf-8").splitlines()
            ]
            self.assertTrue(any(
                item["operation"] == "fill_rect" and
                item["job"] == 27 and item["page"] == 1 and
                item["x"] == 51 and item["y"] == 137 and
                item["w"] == 14 and item["h"] == 149
                for item in editor_trace
            ))
            self.assertFalse(any(
                item["operation"] == "fill_rect" and
                item["job"] == 27 and
                ((item["w"] == 800) or
                 (item["x"] == 24 and item["y"] == 112 and
                  item["w"] == 68 and item["h"] == 218))
                for item in editor_trace
            ))
            self.assertTrue(any(
                item["operation"] == "draw_rect" and
                item["job"] == 0 and item["page"] == 1 and
                item["x"] == 672 and item["y"] == 430 and
                item["w"] == 104 and item["h"] == 40
                for item in editor_trace
            ))
            self.assertFalse(any(
                item["operation"] == "draw_rect" and
                item["job"] == 27 and item["page"] == 1 and
                item["x"] == 672 and item["y"] == 430 and
                item["w"] == 104 and item["h"] == 40
                for item in editor_trace
            ))
            intermediate = json.loads(
                (output / "page_transition_intermediate.json").read_text(
                    encoding="utf-8")
            )
            self.assertEqual(intermediate["completed_jobs"], 9)
            self.assertEqual(intermediate["pending_jobs"], 1)
            self.assertEqual(intermediate["displayed_page"], 0)
            self.assertEqual(intermediate["page_building"], 1)
            with (output / "draw_trace.jsonl").open(encoding="utf-8") as source:
                first = json.loads(next(source))
            for field in (
                "operation", "x", "y", "w", "h", "color", "job",
                "page", "frame", "draw_sequence", "text_or_glyph_id",
            ):
                self.assertIn(field, first)


if __name__ == "__main__":
    unittest.main()
