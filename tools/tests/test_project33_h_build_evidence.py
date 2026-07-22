from __future__ import annotations

import csv
import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "export_project33_h_build_evidence.py"


def load_exporter():
    spec = importlib.util.spec_from_file_location(
        "export_project33_h_build_evidence", SCRIPT
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load {SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Project33HBuildEvidenceTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        self.matrix_path = self.root / "build_matrix_summary.json"
        self.map_path = self.root / "H_project33_full.map"
        self.link_path = self.root / "H_project33_full_linkInfo.xml"
        self.out_path = self.root / "H_project33_full.out"
        self.log_path = self.root / "H_project33_full.log"
        self.nm_path = self.root / "nm6x.exe"
        self.output_dir = self.root / "evidence"
        self.module = load_exporter()

        self.out_path.write_bytes(b"synthetic-c6000-output")
        self.nm_path.write_bytes(b"synthetic-nm6x")
        self.log_path.write_text(
            "C6000 Compiler: user_equalizer.c\n"
            "Pass 2 : 0 Error(s), 0 Warning(s)\n"
            "Finished building target: DSP_LAB_0507.out\n",
            encoding="utf-8",
        )
        self.map_path.write_text(self.map_text(), encoding="utf-8")
        self.link_path.write_text(self.link_xml(), encoding="iso-8859-1")
        self.write_matrix()
        self.nm_output = self.valid_nm_output()

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    @staticmethod
    def required_defines() -> str:
        values = {
            "DSP_LAB_PROJECT_SELECT": 33,
            "EQ_ENABLE_AUDIO_FEATURE_ANALYZER": 1,
            "EQ_ENABLE_SMART_BASS": 1,
            "EQ_ENABLE_DYNAMIC_CLARITY": 1,
            "EQ_ENABLE_HARSHNESS_GUARD": 1,
            "EQ_ENABLE_LCD_DISPLAY": 1,
            "EQ_ENABLE_PROJECT33_TOUCH": 1,
            "EQ_ENABLE_TEN_BAND_EDITOR": 1,
            "EQ_ENABLE_TEN_BAND_EDITOR_TOUCH": 1,
            "EQ_ENABLE_FINAL_METRICS_BOARD_TEST": 0,
            "EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS": 0,
            "EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE": 0,
            "EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE": 0,
            "EQ_ENABLE_HARSHNESS_GUARD_KERNEL_DIAGNOSTICS": 0,
            "EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS": 0,
            "EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK": 0,
            "EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK": 0,
            "EQ_ENABLE_LCD_JOB_TIMING_CAPTURE": 0,
        }
        return " ".join(
            f"--define={name}={value}" for name, value in values.items()
        )

    def write_matrix(self, defines: str | None = None, **overrides) -> None:
        profile = {
            "profile": "H_project33_full",
            "exit_code": 0,
            "warning_count": 0,
            "error_count": 0,
            "link_errors": "0x0",
            "text_bytes": 0x1000,
            "const_bytes": 0x0200,
            "bss_bytes": 0x0080,
            "subband_l2_bytes": 0x4F9C,
            "framebuffer_symbol_count": 2,
            "framebuffer_symbol_names": [
                "EQ_LcdEditorBuffer",
                "Lcd_Buffer",
            ],
            "out_size": self.out_path.stat().st_size,
            "defines": defines if defines is not None else self.required_defines(),
        }
        profile.update(overrides)
        self.matrix_path.write_text(
            json.dumps([profile], indent=2), encoding="utf-8"
        )

    @staticmethod
    def map_text() -> str:
        return """******************************************************************************
               TMS320C6x Linker PC v8.5.0
******************************************************************************
>> Linked Wed Jul 22 12:00:00 2026

MEMORY CONFIGURATION

         name            origin    length      used     unused   attr    fill
----------------------  --------  ---------  --------  --------  ----  --------
  DSPL2RAM              00800000   00040000  00004f9c  0003b064  RWIX
  DDR2                  c0000000   02000000  0017a300  01e85d00  RWIX

SEGMENT ALLOCATION MAP

run origin  load origin   length   init length attrs members
----------  ----------- ---------- ----------- ----- -------
00800000    00800000    00004f9c   00000000    rw-
  00800000    00800000    00004f9c   00000000    rw- .subband_l2
c0000000    c0000000    0017704c   00000000    rw-
  c0000000    c0000000    0017704c   00000000    rw- offscreen_buffer
c0178000    c0178000    00001000   00001000    r-x .text
c0179000    c0179000    00000200   00000200    r-- .const
c0179200    c0179200    00000080   00000000    rw- .bss

SECTION ALLOCATION MAP
"""

    @staticmethod
    def link_xml(second_framebuffer: str = "0xc00bb828") -> str:
        return f"""<?xml version="1.0" encoding="ISO-8859-1" ?>
<link_info>
  <banner>TMS320C6x Linker PC v8.5.0.LTS</banner>
  <link_errors>0x0</link_errors>
  <output_file>C:\\build\\DSP_LAB_0507.out</output_file>
  <logical_group_list>
    <logical_group id="lg-1"><name>.subband_l2</name><run_address>0x00800000</run_address><size>0x4f9c</size><readonly>false</readonly><executable>false</executable></logical_group>
    <logical_group id="lg-2"><name>offscreen_buffer</name><run_address>0xc0000000</run_address><size>0x17704c</size><readonly>false</readonly><executable>false</executable></logical_group>
    <logical_group id="lg-3"><name>.text</name><run_address>0xc0178000</run_address><load_address>0xc0178000</load_address><size>0x1000</size><readonly>true</readonly><executable>true</executable></logical_group>
    <logical_group id="lg-4"><name>.const</name><run_address>0xc0179000</run_address><load_address>0xc0179000</load_address><size>0x200</size><readonly>true</readonly><executable>false</executable></logical_group>
    <logical_group id="lg-5"><name>.bss</name><run_address>0xc0179200</run_address><size>0x80</size><readonly>false</readonly><executable>false</executable></logical_group>
  </logical_group_list>
  <symbol_table>
    <symbol><name>Lcd_Buffer</name><value>0xc0000000</value></symbol>
    <symbol><name>EQ_LcdEditorBuffer</name><value>{second_framebuffer}</value></symbol>
  </symbol_table>
</link_info>
"""

    @staticmethod
    def nm_line(index: int, address: int, size: int, name: str) -> str:
        return (
            f"[{index}] |0x{address:08x}|{size}|GLOB |OBJT |HIDN |9     |{name}"
        )

    def valid_nm_output(self) -> str:
        symbols = [
            (0xC0000000, 768036, "Lcd_Buffer"),
            (0xC00BB828, 768036, "EQ_LcdEditorBuffer"),
            (0x00800000, 16488, "EQ_AudioAnalyzerState"),
            (0x00804080, 2048, "EQ_AnalyzerInput"),
            (0x00804880, 252, "EQ_SmartBassState"),
            (0x00804980, 260, "EQ_DynamicClarityState"),
            (0x00804A90, 260, "EQ_HarshnessGuardState"),
            (0xC0179300, 512, "s_ui_state"),
            (0xC0179500, 64, "EQ_UiEditorState"),
            (0xC0179540, 28, "EQ_TouchState"),
            (0xC017955C, 8, "EQ_TouchTransform"),
            (0xC0179100, 4, "EQ_DebugLcdTimingCaptureCompiled"),
            (0xC0180000, 16384, "EQ_CaptureInput"),
            (0xC0184000, 16384, "EQ_CaptureOutput"),
            (0xC0188000, 24576, "EQ_TriggerCaptureInput"),
            (0xC018E000, 24576, "EQ_TriggerCaptureOutput"),
        ]
        lines = [
            "[index]  value      size  bind  type  vis   shndx  symbol name",
        ]
        lines.extend(
            self.nm_line(index, address, size, name)
            for index, (address, size, name) in enumerate(symbols, 1)
        )
        return "\n".join(lines) + "\n"

    def run_export(self, nm_output: str | None = None):
        completed = subprocess.CompletedProcess(
            args=[str(self.nm_path), "-l", str(self.out_path)],
            returncode=0,
            stdout=self.nm_output if nm_output is None else nm_output,
            stderr="",
        )
        with mock.patch.object(
            self.module.subprocess, "run", return_value=completed
        ) as runner:
            result = self.module.export_evidence(
                build_matrix_path=self.matrix_path,
                map_path=self.map_path,
                link_info_path=self.link_path,
                out_path=self.out_path,
                build_log_path=self.log_path,
                nm6x_path=self.nm_path,
                output_dir=self.output_dir,
            )
        runner.assert_called_once()
        self.assertEqual(
            runner.call_args.args[0],
            [str(self.nm_path.resolve()), "-l", str(self.out_path.resolve())],
        )
        return result

    def test_exports_four_files_and_discloses_generic_capture(self) -> None:
        result = self.run_export()

        expected = {
            "build_summary.json",
            "memory_sections.csv",
            "framebuffer_map.json",
            "production_symbol_audit.txt",
        }
        self.assertEqual(
            {path.name for path in self.output_dir.iterdir()}, expected
        )
        self.assertEqual(result["result"], "PASS")
        self.assertEqual(result["status"], "MEASURED")
        self.assertEqual(result["measurement_label"], "BUILD_EVIDENCE")
        summary = json.loads(
            (self.output_dir / "build_summary.json").read_text(encoding="utf-8")
        )
        self.assertEqual(summary["warning_count"], 0)
        self.assertEqual(summary["link_errors"], "0x0")
        self.assertEqual(summary["sections_bytes"][".text"], 0x1000)
        self.assertEqual(summary["sections_bytes"][".subband_l2"], 0x4F9C)
        self.assertEqual(summary["ddr"]["used_bytes"], 0x17A300)
        self.assertEqual(summary["ddr"]["unused_bytes"], 0x1E85D00)
        self.assertEqual(summary["state_sizes_bytes"]["analyzer_state"], 16488)
        self.assertEqual(summary["state_sizes_bytes"]["analyzer_input"], 2048)
        self.assertEqual(summary["state_sizes_bytes"]["touch_total"], 36)

        framebuffer = json.loads(
            (self.output_dir / "framebuffer_map.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertTrue(framebuffer["non_overlapping"])
        self.assertEqual(framebuffer["gap_bytes"], 4)
        self.assertEqual(framebuffer["framebuffer_count"], 2)

        with (self.output_dir / "memory_sections.csv").open(
            newline="", encoding="utf-8-sig"
        ) as stream:
            sections = {row["section_name"]: row for row in csv.DictReader(stream)}
        self.assertEqual(sections[".const"]["memory_region"], "DDR2")
        self.assertEqual(sections[".subband_l2"]["memory_region"], "DSPL2RAM")

        audit = (self.output_dir / "production_symbol_audit.txt").read_text(
            encoding="utf-8"
        )
        self.assertIn("result=PASS", audit)
        self.assertIn("forbidden_measurement_symbols=NONE", audit)
        self.assertIn("lcd_timing_compiled_state=PASS", audit)
        self.assertIn(
            "EQ_DebugLcdTimingCaptureCompiled address=0xc0179100 "
            "size_bytes=4 readonly_section=.const",
            audit,
        )
        self.assertIn("generic_capture_buffers=DISCLOSED_NOT_FAILURE", audit)
        self.assertIn("EQ_CaptureInput address=0xc0180000 size_bytes=16384", audit)
        self.assertIn("generic_capture_total_bytes=81920", audit)

    def test_rejects_warning_found_in_build_log(self) -> None:
        self.log_path.write_text(
            '"file.c", line 9: warning #123-D: diagnostic\n', encoding="utf-8"
        )
        with self.assertRaisesRegex(self.module.EvidenceError, "warning"):
            self.run_export()
        self.assertFalse(self.output_dir.exists())

    def test_rejects_nonzero_link_errors_from_xml(self) -> None:
        self.link_path.write_text(
            self.link_xml().replace(
                "<link_errors>0x0</link_errors>",
                "<link_errors>0x2</link_errors>",
            ),
            encoding="iso-8859-1",
        )
        with self.assertRaisesRegex(self.module.EvidenceError, "link_errors"):
            self.run_export()

    def test_rejects_missing_or_enabled_measurement_macro(self) -> None:
        token = "--define=EQ_ENABLE_FINAL_METRICS_BOARD_TEST=0"
        for defines in (
            self.required_defines().replace(token, ""),
            self.required_defines().replace(token, token[:-1] + "1"),
        ):
            with self.subTest(defines=defines):
                self.write_matrix(defines=defines)
                with self.assertRaisesRegex(
                    self.module.EvidenceError,
                    "EQ_ENABLE_FINAL_METRICS_BOARD_TEST",
                ):
                    self.run_export()

    def test_rejects_offline_host_macro_even_when_set_to_zero(self) -> None:
        self.write_matrix(
            defines=self.required_defines() + " --define=EQ_ALGO_ONLY=0"
        )
        with self.assertRaisesRegex(self.module.EvidenceError, "EQ_ALGO_ONLY"):
            self.run_export()

    def test_rejects_overlapping_framebuffers(self) -> None:
        overlap_address = 0xC00BB820
        self.link_path.write_text(
            self.link_xml(second_framebuffer=f"0x{overlap_address:08x}"),
            encoding="iso-8859-1",
        )
        overlapping_nm = self.nm_output.replace(
            "|0xc00bb828|768036|", "|0xc00bb820|768036|"
        )
        with self.assertRaisesRegex(self.module.EvidenceError, "overlap"):
            self.run_export(overlapping_nm)

    def test_rejects_each_forbidden_measurement_array_category(self) -> None:
        names = (
            "EQ_FinalMetricsResponsePacked",
            "EQ_BenchmarkInput",
            "EQ_DebugDynamicClarityTransitionCaptureSamples",
            "EQ_DebugLcdJobTimingSamples",
            "EQ_DebugLcdTimingTotalCount",
        )
        for index, name in enumerate(names, 90):
            with self.subTest(name=name):
                forbidden = self.nm_output + self.nm_line(
                    index, 0xC0194000, 65536, name
                )
                with self.assertRaisesRegex(self.module.EvidenceError, name):
                    self.run_export(forbidden)

    def test_rejects_missing_lcd_timing_compiled_state(self) -> None:
        line = self.nm_line(
            12, 0xC0179100, 4, "EQ_DebugLcdTimingCaptureCompiled"
        )
        without_compiled_state = self.nm_output.replace(line + "\n", "")
        with self.assertRaisesRegex(
            self.module.EvidenceError, "EQ_DebugLcdTimingCaptureCompiled"
        ):
            self.run_export(without_compiled_state)

    def test_rejects_nonzero_build_matrix_diagnostics(self) -> None:
        for field, value in (
            ("warning_count", 1),
            ("error_count", 1),
            ("link_errors", "0x1"),
        ):
            with self.subTest(field=field):
                self.write_matrix(**{field: value})
                with self.assertRaises(self.module.EvidenceError):
                    self.run_export()

    def test_rejects_build_matrix_and_xml_section_mismatch(self) -> None:
        self.write_matrix(text_bytes=0x1004)
        with self.assertRaisesRegex(self.module.EvidenceError, "text_bytes"):
            self.run_export()

    def test_rejects_nm6x_failure(self) -> None:
        completed = subprocess.CompletedProcess(
            args=[str(self.nm_path), "-l", str(self.out_path)],
            returncode=1,
            stdout="",
            stderr="bad object",
        )
        with mock.patch.object(
            self.module.subprocess, "run", return_value=completed
        ):
            with self.assertRaisesRegex(self.module.EvidenceError, "nm6x"):
                self.module.export_evidence(
                    build_matrix_path=self.matrix_path,
                    map_path=self.map_path,
                    link_info_path=self.link_path,
                    out_path=self.out_path,
                    build_log_path=self.log_path,
                    nm6x_path=self.nm_path,
                    output_dir=self.output_dir,
                )


if __name__ == "__main__":
    unittest.main()
