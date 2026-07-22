import csv
import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
PROCESSOR = ROOT / "tools" / "process_equalizer_final_acceptance.py"
DSS_COLLECTOR = ROOT / "tools" / "dss" / "dss_equalizer_final_acceptance.js"

REQUIRED_OUTPUTS = (
    "README.md",
    "evidence_manifest.json",
    "build_summary.json",
    "memory_sections.csv",
    "framebuffer_map.json",
    "production_symbol_audit.txt",
    "production_boot_summary.json",
    "page_switch_30_scripted.json",
    "operator_visual_observation.md",
    "lcd_job_timing.csv",
    "lcd_job_timing_summary.json",
    "final_non_touch_endurance_300s.json",
    "analyzer_reset_board_summary.json",
    "external_file_inventory.json",
    "final_acceptance_summary.csv",
    "sha256_manifest.txt",
)

OBSOLETE_TOUCH_OUTPUTS = (
    "touch_no_action_120s.json",
    "touch_hitbox_trials.csv",
    "touch_hitbox_summary.json",
    "page_switch_30_rounds.json",
    "final_interactive_endurance_300s.json",
)

TIMING_CLASSES = (
    "preset",
    "analyzer_strip",
    "dynamic_enabled",
    "dynamic_strength",
    "dynamic_active",
    "editor_band",
    "editor_field",
    "page_sync",
    "page_swap",
)

WAIVER_REASON = "USER_REMOVED_TOUCH_RELIABILITY_FROM_ACCEPTANCE_SCOPE"


def load_processor():
    spec = importlib.util.spec_from_file_location(
        "project33_final_acceptance_processor", PROCESSOR
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load final-acceptance processor")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PROCESSOR_MODULE = load_processor()


def run_processor(raw_dir, output_dir):
    return subprocess.run(
        [
            sys.executable,
            "-B",
            str(PROCESSOR),
            "--raw-dir",
            str(raw_dir),
            "--output-dir",
            str(output_dir),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=30,
        check=False,
    )


def write_json(path, value):
    path.write_text(json.dumps(value) + "\n", encoding="utf-8")


def write_jsonl(path, values):
    path.write_text(
        "".join(json.dumps(value) + "\n" for value in values),
        encoding="utf-8",
    )


def valid_boot_summary():
    return {
        "pass": True,
        "init_stage": 11,
        "deadline": 0,
        "latency_miss": 0,
        "overlap": 0,
        "dropped": 0,
        "clip": 0,
        "final_target_state": "RUNNING_DISCONNECTED",
    }


class EqualizerFinalAcceptanceToolingTest(unittest.TestCase):
    def test_endurance_uses_production_preset_api_for_overlap(self):
        source = DSS_COLLECTOR.read_text(encoding="utf-8")
        self.assertIn('requestPreset(1, "endurance overlap BASS")', source)
        self.assertIn("ENDURANCE_WINDOW_MILLISECONDS = 302000", source)
        self.assertIn(
            "runContinuousWindow(ENDURANCE_WINDOW_MILLISECONDS)", source
        )
        self.assertNotIn(
            'writeTarget("EQ_DebugFourWayTransitionArmMode", 1)', source
        )

    def test_processor_generates_non_touch_skeleton_and_fixed_waivers(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()

            # Legacy raw Touch records cannot promote or fail waived stages.
            write_json(
                raw / "no_touch_120s_raw_summary.json",
                {"completed": True, "host_elapsed_seconds": 1.0},
            )
            write_json(
                raw / "touch_hitbox_27_raw_summary.json",
                {"completed": False, "error": "legacy stage failed"},
            )

            result = run_processor(raw, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(
                {path.name for path in output.iterdir()}, set(REQUIRED_OUTPUTS)
            )
            for name in OBSOLETE_TOUCH_OUTPUTS:
                self.assertFalse((output / name).exists(), name)

            manifest = json.loads(
                (output / "evidence_manifest.json").read_text(encoding="utf-8")
            )
            expected_waiver = {
                "status": "WAIVED",
                "result": "NOT_REQUIRED",
                "reason": WAIVER_REASON,
            }
            self.assertEqual(
                manifest["stage_results"]["no_touch_120s"], expected_waiver
            )
            self.assertEqual(
                manifest["stage_results"]["touch_hitbox_27"], expected_waiver
            )
            self.assertEqual(
                manifest["waived_stages"], ["no_touch_120s", "touch_hitbox_27"]
            )
            self.assertNotIn("no_touch_120s", manifest["pending_stages"])
            self.assertNotIn("touch_hitbox_27", manifest["pending_stages"])
            self.assertEqual(
                manifest["stage_results"]["production_boot"]["status"],
                "PENDING_HARDWARE",
            )

            with (output / "final_acceptance_summary.csv").open(
                newline="", encoding="utf-8"
            ) as stream:
                summary = {row["stage"]: row for row in csv.DictReader(stream)}
            for stage in ("no_touch_120s", "touch_hitbox_27"):
                self.assertEqual(summary[stage]["status"], "WAIVED")
                self.assertEqual(summary[stage]["result"], "NOT_REQUIRED")
                self.assertEqual(summary[stage]["reason"], WAIVER_REASON)

            visual = (output / "operator_visual_observation.md").read_text(
                encoding="utf-8"
            )
            self.assertIn("PENDING_OPERATOR", visual)
            self.assertNotIn("touch_coordinate_drift", visual)
            self.assertIn("final_acceptance_summary.csv", (
                output / "sha256_manifest.txt"
            ).read_text(encoding="ascii"))

    def test_waived_stages_do_not_block_manifest_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = pathlib.Path(temporary)
            measured_pass = {"status": "MEASURED", "result": "PASS"}
            stage_results = {
                "build": measured_pass,
                "production_boot": measured_pass,
                "no_touch_120s": PROCESSOR_MODULE.waived_touch_stage(
                    "no_touch_120s"
                ),
                "touch_hitbox_27": PROCESSOR_MODULE.waived_touch_stage(
                    "touch_hitbox_27"
                ),
                "page_switch_30": measured_pass,
                "operator_visual": {
                    "status": "OPERATOR_VISUAL_OBSERVATION",
                    "result": "PASS",
                },
                "lcd_job_timing": measured_pass,
                "endurance_300s": measured_pass,
                "analyzer_reset": measured_pass,
            }
            PROCESSOR_MODULE.write_manifest(output, stage_results)
            manifest = json.loads(
                (output / "evidence_manifest.json").read_text(encoding="utf-8")
            )
            self.assertEqual(manifest["overall_result"], "PASS")
            self.assertEqual(manifest["pending_stages"], [])
            self.assertEqual(
                manifest["waived_stages"], ["no_touch_120s", "touch_hitbox_27"]
            )

    def test_production_boot_requires_every_board_contract_field(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()
            output.mkdir()

            pending = PROCESSOR_MODULE.process_production_boot(raw, output)
            self.assertEqual(pending["status"], "PENDING_HARDWARE")
            self.assertEqual(pending["result"], "NOT_RUN")

            write_json(raw / "production_boot_summary.json", valid_boot_summary())
            passed = PROCESSOR_MODULE.process_production_boot(raw, output)
            self.assertEqual(passed["result"], "PASS")
            self.assertEqual(passed["stop_reasons"], [])

            bad_values = (
                ("pass", "pass", False),
                ("init_stage", "init_stage", 10),
                ("deadline", "deadline", 1),
                ("fractional_deadline", "deadline", 0.5),
                ("latency_miss", "latency_miss", 1),
                ("overlap", "overlap", 1),
                ("dropped", "dropped", 1),
                ("clip", "clip", 1),
                ("final_target_state", "final_target_state", "HALTED"),
            )
            for label, key, bad_value in bad_values:
                with self.subTest(label=label):
                    value = valid_boot_summary()
                    value[key] = bad_value
                    write_json(raw / "production_boot_summary.json", value)
                    failed = PROCESSOR_MODULE.process_production_boot(raw, output)
                    self.assertEqual(failed["result"], "FAIL")
                    self.assertTrue(failed["stop_reasons"])

    def test_page_switch_requires_thirty_scripted_api_round_trips(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()
            output.mkdir()

            before = {name: 0 for name in PROCESSOR_MODULE.PAGE_COUNTERS}
            after = dict(before)
            after.update(
                {
                    "lcd_page_sync_jobs": 60,
                    "lcd_swap_requests": 60,
                    "lcd_swap_completes": 60,
                    "lcd_eof0": 30,
                    "lcd_eof1": 30,
                    "lcd_writeback_count": 60,
                    "lcd_writeback_bytes": 4096,
                }
            )
            rounds = [
                {
                    "record_type": "page_round",
                    "round": index,
                    "status_to_editor_scripted": True,
                    "editor_to_status_scripted": True,
                }
                for index in range(1, 31)
            ]
            write_jsonl(raw / "page_switch_30.jsonl", rounds)
            write_json(
                raw / "page_switch_30_raw_summary.json",
                {
                    "completed": True,
                    "completed_round_trips": 30,
                    "before": before,
                    "after": after,
                },
            )

            evidence = PROCESSOR_MODULE.process_page_switch(raw, output)
            self.assertEqual(evidence["result"], "PASS")
            self.assertEqual(
                evidence["request_contract"], "SCRIPTED_PAGE_REQUEST_API"
            )
            self.assertEqual(evidence["scripted_record_error_count"], 0)
            self.assertNotIn("touch_page_requests", evidence["deltas"])
            self.assertTrue((output / "page_switch_30_scripted.json").is_file())

            rounds[0]["status_to_editor_scripted"] = False
            write_jsonl(raw / "page_switch_30.jsonl", rounds)
            failed = PROCESSOR_MODULE.process_page_switch(raw, output)
            self.assertEqual(failed["result"], "FAIL")
            self.assertEqual(failed["scripted_record_error_count"], 1)

    def test_lcd_timing_requires_twenty_valid_samples_per_class(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()
            output.mkdir()

            records = [
                {
                    "record_type": "timing_sample",
                    "class_name": class_name,
                    "sample_index": index,
                    "cycles": 1000 + index,
                    "job_count_delta": 1,
                    "deferred_by_audio_delta": 0,
                    "audio_arrived_during_draw_delta": 0,
                }
                for class_name in TIMING_CLASSES
                for index in range(1, 21)
            ]
            zero_faults = {name: 0 for name in PROCESSOR_MODULE.LCD_FAULT_COUNTERS}
            write_jsonl(raw / "lcd_job_timing.jsonl", records)
            write_json(
                raw / "lcd_job_timing_raw_summary.json",
                {"completed": True, "before": zero_faults, "after": zero_faults},
            )

            evidence = PROCESSOR_MODULE.process_timing(raw, output)
            self.assertEqual(evidence["result"], "PASS")
            self.assertEqual(
                evidence["thresholds"]["minimum_valid_samples_per_class"], 20
            )
            self.assertTrue(all(item["count"] == 20 for item in evidence["classes"]))

            records.pop(19)
            write_jsonl(raw / "lcd_job_timing.jsonl", records)
            incomplete = PROCESSOR_MODULE.process_timing(raw, output)
            self.assertEqual(incomplete["result"], "INCOMPLETE")
            self.assertIn("preset", incomplete["missing_classes"])

    def test_endurance_passes_without_touch_histogram_or_operator_quota(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()
            output.mkdir()

            self.assertFalse(
                any("touch" in name for name in PROCESSOR_MODULE.ENDURANCE_COUNTERS)
            )
            before = {name: 0 for name in PROCESSOR_MODULE.ENDURANCE_COUNTERS}
            before.update(
                {
                    "frame_latency_max_cycles": 1000,
                    "algorithm_max_cycles": 1000,
                    "frame_service_max_cycles": 1000,
                    "lcd_max_job_cycles": 1000,
                }
            )
            after = dict(before)
            after.update(
                {
                    "process_frames": 14649,
                    "ad_frames": 14649,
                    "da_frames": 14649,
                    "algorithm_frames": 14649,
                    "analyzer_publications": 300,
                    "smart_decisions": 300,
                    "clarity_decisions": 300,
                    "guard_decisions": 300,
                    "lcd_swap_completes": 30,
                    "lcd_jobs": 500,
                    "static_dynamic_overlap_frames": 1,
                }
            )
            write_json(
                raw / "endurance_300s_raw_summary.json",
                {
                    "completed": True,
                    "host_elapsed_seconds": 300.1,
                    "continuous_window_halt_count": 2,
                    "before": before,
                    "after": after,
                },
            )

            evidence = PROCESSOR_MODULE.process_endurance(raw, output)
            self.assertEqual(evidence["result"], "PASS")
            self.assertEqual(evidence["interaction_contract"], "NON_TOUCH_SCRIPTED_API")
            self.assertNotIn("action_quota_status", evidence)
            self.assertNotIn("operator_declaration", evidence)
            serialized = json.dumps(evidence)
            self.assertNotIn("touch_action_histogram", serialized)
            self.assertTrue(
                (output / "final_non_touch_endurance_300s.json").is_file()
            )

            after["analyzer_publications"] = 0
            write_json(
                raw / "endurance_300s_raw_summary.json",
                {
                    "completed": True,
                    "host_elapsed_seconds": 300.1,
                    "continuous_window_halt_count": 2,
                    "before": before,
                    "after": after,
                },
            )
            failed = PROCESSOR_MODULE.process_endurance(raw, output)
            self.assertEqual(failed["result"], "FAIL")
            self.assertTrue(
                any("did not advance" in reason for reason in failed["stop_reasons"])
            )

    def test_operator_visual_has_four_items_and_is_never_auto_filled(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()
            output.mkdir()

            pending = PROCESSOR_MODULE.process_visual(raw, output)
            self.assertEqual(pending["status"], "PENDING_OPERATOR")
            self.assertEqual(pending["result"], "NOT_RECORDED")
            pending_text = (output / "operator_visual_observation.md").read_text(
                encoding="utf-8"
            )
            self.assertEqual(pending_text.count("[PENDING]"), 4)
            self.assertNotIn("touch_coordinate_drift", pending_text)

            write_json(
                raw / "operator_visual_observation_raw.json",
                {
                    "result_label": "OPERATOR_VISUAL_OBSERVATION",
                    "operator": "human",
                    "observed_at": "2026-07-22T00:00:00Z",
                    "observations": {
                        "circular_offset": "PASS",
                        "full_screen_white_flash": "PASS",
                        "old_page_ghosting": "PASS",
                        "element_misalignment": "PASS",
                        "touch_coordinate_drift": "FAIL",
                    },
                },
            )
            observed = PROCESSOR_MODULE.process_visual(raw, output)
            self.assertEqual(observed["result"], "PASS")
            observed_text = (output / "operator_visual_observation.md").read_text(
                encoding="utf-8"
            )
            self.assertNotIn("touch_coordinate_drift", observed_text)


if __name__ == "__main__":
    unittest.main()
