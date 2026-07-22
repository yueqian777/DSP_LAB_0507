import csv
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
DSS = ROOT / "tools" / "dss" / "dss_equalizer_final_acceptance.js"
RUNNER = ROOT / "tools" / "run_equalizer_final_acceptance.ps1"
PROCESSOR = ROOT / "tools" / "process_equalizer_final_acceptance.py"
BUILD_EXPORTER = ROOT / "tools" / "export_project33_h_build_evidence.py"

REQUIRED_OUTPUTS = (
    "README.md",
    "evidence_manifest.json",
    "build_summary.json",
    "memory_sections.csv",
    "framebuffer_map.json",
    "production_symbol_audit.txt",
    "touch_no_action_120s.json",
    "touch_hitbox_trials.csv",
    "touch_hitbox_summary.json",
    "page_switch_30_rounds.json",
    "operator_visual_observation.md",
    "lcd_job_timing.csv",
    "lcd_job_timing_summary.json",
    "final_interactive_endurance_300s.json",
    "analyzer_reset_board_summary.json",
    "external_file_inventory.json",
    "sha256_manifest.txt",
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


def powershell_executable():
    executable = shutil.which("powershell.exe") or shutil.which("pwsh.exe")
    if executable is None:
        raise unittest.SkipTest("PowerShell is unavailable")
    return executable


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


def append_jsonl(path, values):
    path.write_text(
        "".join(json.dumps(value) + "\n" for value in values),
        encoding="utf-8",
    )


class EqualizerFinalAcceptanceToolingTest(unittest.TestCase):
    def test_only_expected_new_tool_files_are_named(self):
        for path in (
            DSS,
            RUNNER,
            PROCESSOR,
            BUILD_EXPORTER,
            pathlib.Path(__file__),
        ):
            self.assertTrue(path.is_file(), path)

    def test_runner_default_is_hardware_inert(self):
        for arguments in ((), ("-Stage", "DryRun")):
            with self.subTest(arguments=arguments):
                result = subprocess.run(
                    [
                        powershell_executable(),
                        "-NoProfile",
                        "-ExecutionPolicy",
                        "Bypass",
                        "-File",
                        str(RUNNER),
                        *arguments,
                    ],
                    cwd=ROOT,
                    text=True,
                    capture_output=True,
                    timeout=20,
                    check=False,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), "NOT_RUN_NO_HARDWARE")
                self.assertNotIn("PASS", result.stdout.upper())

    def test_runner_has_staged_guards_and_clean_h_build_contract(self):
        source = RUNNER.read_text(encoding="utf-8")
        for stage in (
            "DryRun",
            "Prepare",
            "Build",
            "NoTouch120s",
            "Hitbox27",
            "PageSwitch30",
            "LcdTiming",
            "Endurance300s",
            "AnalyzerReset",
            "Process",
        ):
            self.assertIn(f'"{stage}"', source)
        for token in (
            "C6748_CONNECTED_FINAL_ACCEPTANCE",
            "I_ACCEPT_MANUAL_LONG_STAGE",
            "run_equalizer_ui_build_matrix.ps1",
            'H_project33_full',
            "warning_count",
            "error_count",
            "link_errors",
            "Get-FileHash",
            "process_equalizer_final_acceptance.py",
            "export_project33_h_build_evidence.py",
            "operator_visual_observation_raw.json",
            "endurance_operator_quota_raw.json",
        ):
            self.assertIn(token, source)
        guard = source.index('if ($Stage -eq "DryRun")')
        self.assertLess(guard, source.index("Resolve-Path"))
        self.assertLess(guard, source.index("Start-Process"))
        self.assertNotIn("NoTouch120s,Hitbox27", source)

    def test_runner_powershell_parses(self):
        quoted = str(RUNNER).replace("'", "''")
        command = (
            "$tokens=$null;$errors=$null;"
            f"[System.Management.Automation.Language.Parser]::ParseFile('{quoted}',"
            "[ref]$tokens,[ref]$errors)|Out-Null;"
            "if($errors.Count){$errors|ForEach-Object{$_.Message};exit 1}"
        )
        result = subprocess.run(
            [powershell_executable(), "-NoProfile", "-Command", command],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=20,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_dss_requires_authorization_and_never_fakes_physical_pages(self):
        source = DSS.read_text(encoding="utf-8")
        guard = source.index(
            'hardwareAuthorization != "C6748_CONNECTED_FINAL_ACCEPTANCE"'
        )
        self.assertLess(guard, source.index("ScriptingEnvironment.instance()"))
        self.assertIn('System.out.println("NOT_RUN_NO_HARDWARE")', source)
        self.assertNotIn("EQ_DebugUiRequestedPage =", source)
        self.assertNotIn("EQ_DebugUiDisplayedPage =", source)
        self.assertNotIn("setRequestedPage", source)
        self.assertIn('lcd_page_sync_jobs: "EQ_DebugLcdJobTypeCount[24]"', source)
        self.assertIn(
            'writeTarget("EQ_DebugAnalyzerResetRequest", 1)', source
        )

    def test_dss_covers_exact_physical_and_continuous_contracts(self):
        source = DSS.read_text(encoding="utf-8")
        hitbox_start = source.index("var HITBOXES = [")
        hitbox_end = source.index("];", hitbox_start)
        hitbox_block = source[hitbox_start:hitbox_end]
        self.assertEqual(hitbox_block.count("id:"), 27)
        for token in (
            "PAGE_ROUND_TRIPS = 30",
            "runContinuousWindow(120000)",
            "runContinuousWindow(300000)",
            "NO_TOUCH_START",
            "NO_TOUCH_END",
            "HITBOX_TOUCH_REQUIRED",
            "PAGE_TOUCH_REQUIRED",
            "ENDURANCE_INTERACTION_QUOTAS",
            "Touch_DebugDownCount",
            "Touch_DebugReleaseCount",
            "Touch_DebugI2cErrorCount",
            "EQ_DebugTouchInvalidCoordinateCount",
            "EQ_DebugTouchDuplicateActionCount",
            "EQ_DebugTouchActionHistogramSize",
            "EQ_DebugTouchActionHistogram[",
            "EQ_DebugLcdEof0Count",
            "EQ_DebugLcdEof1Count",
            "EQ_DebugLcdWritebackFailureCount",
            "EQ_DebugLcdFramebufferCanaryFailureCount",
            "EQ_DebugAnalyzerEpoch",
            "EQ_DebugAnalyzerPublicationCount",
            "EQ_DebugStaticDynamicTransitionOverlapFrameCount",
            "EQ_DebugLcdPendingDirtyRegionCount",
            "EQ_DebugLcdMaxPendingDirtyRegionCount",
            "EQ_DebugLcdTimingSamples[",
            "EQ_DebugLcdTimingSampleCount[",
            "EOF descriptor update",
            "requireEnduranceHistogramQuotas",
        ):
            self.assertIn(token, source)
        for class_name in TIMING_CLASSES:
            self.assertIn(f'"{class_name}"', source)

        continuous_start = source.index("function runContinuousWindow(")
        continuous_end = source.index("\n}", continuous_start)
        continuous = source[continuous_start:continuous_end]
        self.assertEqual(continuous.count("target.runAsynch()"), 1)
        self.assertEqual(continuous.count("target.halt()"), 1)
        self.assertNotIn("while", continuous)
        self.assertNotIn("for (", continuous)

        endurance_start = source.index("function runEndurance300s()")
        endurance_end = source.index("function anyDynamicActive", endurance_start)
        endurance = source[endurance_start:endurance_end]
        self.assertEqual(endurance.count("snapshot()"), 2)
        self.assertEqual(endurance.count("runContinuousWindow(300000)"), 1)
        self.assertNotIn("runSlice(", endurance)
        self.assertNotIn("waitForState(", endurance)

    def test_pending_processing_creates_complete_honest_skeleton(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()
            result = run_processor(raw, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for name in REQUIRED_OUTPUTS:
                self.assertTrue((output / name).is_file(), name)

            for name in (
                "touch_no_action_120s.json",
                "touch_hitbox_summary.json",
                "page_switch_30_rounds.json",
                "lcd_job_timing_summary.json",
                "final_interactive_endurance_300s.json",
                "analyzer_reset_board_summary.json",
            ):
                value = json.loads((output / name).read_text(encoding="utf-8"))
                self.assertIn(value["status"], {"NOT_MEASURED", "PENDING_HARDWARE"})
                self.assertNotEqual(value.get("result"), "PASS")

            readme = (output / "README.md").read_text(encoding="utf-8")
            self.assertIn("NOT_MEASURED", readme)
            self.assertIn("PENDING_HARDWARE", readme)
            self.assertNotIn("MEASURED: PASS", readme)

    def test_processor_preserves_strict_build_exporter_outputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()
            output.mkdir()
            strict_summary = {
                "schema_version": 1,
                "stage": "build",
                "status": "MEASURED",
                "measurement_label": "BUILD_EVIDENCE",
                "evidence_class": "BUILD_EVIDENCE",
                "result": "PASS",
                "profile": "H_project33_full",
                "warning_count": 0,
                "error_count": 0,
                "link_errors": "0x0",
            }
            write_json(output / "build_summary.json", strict_summary)
            (output / "memory_sections.csv").write_text(
                "section_name,size_bytes\n.text,1\n", encoding="utf-8"
            )
            write_json(
                output / "framebuffer_map.json",
                {"result": "PASS", "non_overlapping": True},
            )
            audit = (
                "result=PASS\n"
                "generic_capture_buffers=DISCLOSED_NOT_FAILURE\n"
                "generic_capture_total_bytes=81920\n"
            )
            (output / "production_symbol_audit.txt").write_text(
                audit, encoding="ascii"
            )
            expected = {
                path.name: path.read_bytes()
                for path in output.iterdir()
                if path.name in {
                    "build_summary.json",
                    "memory_sections.csv",
                    "framebuffer_map.json",
                    "production_symbol_audit.txt",
                }
            }

            result = run_processor(raw, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for name, content in expected.items():
                self.assertEqual((output / name).read_bytes(), content, name)
            manifest = json.loads(
                (output / "evidence_manifest.json").read_text(encoding="utf-8")
            )
            self.assertEqual(manifest["stage_results"]["build"]["result"], "PASS")

    def test_processor_derives_no_touch_hitbox_and_timing_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()
            counters = {
                "touch_down": 10,
                "touch_release": 10,
                "touch_actions": 4,
                "touch_rejected": 0,
                "touch_i2c_errors": 0,
                "touch_invalid_coordinates": 0,
                "touch_page_requests": 1,
                "touch_preset_requests": 1,
                "touch_dynamic_enable_requests": 1,
                "touch_dynamic_strength_requests": 1,
                "touch_editor_actions": 0,
                "touch_duplicate_actions": 0,
                "touch_action_histogram": [0] * 27,
            }
            write_json(
                raw / "no_touch_120s_raw_summary.json",
                {
                    "stage": "no_touch_120s",
                    "completed": True,
                    "measurement_label": "BOARD_UI_COUNTER",
                    "host_elapsed_seconds": 120.1,
                    "before": counters,
                    "after": counters,
                },
            )

            hitboxes = []
            expected_actions = [*range(1, 13), *range(13, 27), 12]
            for index, action in enumerate(expected_actions):
                hitboxes.append(
                    {
                        "record_type": "hitbox_trial",
                        "trial_id": index + 1,
                        "hitbox_id": f"hitbox_{index + 1}",
                        "page": "STATUS" if index < 12 else "EDITOR",
                        "raw_x": 100 + index,
                        "raw_y": 200 + index,
                        "screen_x": 100 + index,
                        "screen_y": 200 + index,
                        "expected_action": action,
                        "expected_action_name": f"ACTION_{action}",
                        "actual_action": action,
                        "actual_action_name": f"ACTION_{action}",
                        "accepted": True,
                        "rejected": False,
                        "action_count_delta": 1,
                        "action_histogram_bin": action,
                        "action_histogram_bin_delta": 1,
                        "duplicate_action": False,
                        "out_of_range": False,
                        "long_hold_verified": action == 7,
                        "release_rearm_verified": True,
                    }
                )
            append_jsonl(raw / "touch_hitbox_27.jsonl", hitboxes)
            write_json(
                raw / "touch_hitbox_27_raw_summary.json",
                {"stage": "touch_hitbox_27", "completed": True},
            )

            timing = []
            for class_name in TIMING_CLASSES:
                for index, cycles in enumerate((100, 200, 300), start=1):
                    timing.append(
                        {
                            "record_type": "timing_sample",
                            "class_name": class_name,
                            "sample_index": index,
                            "cycles": cycles,
                            "job_count_delta": 1,
                            "deferred_by_audio_delta": 0,
                            "audio_arrived_during_draw_delta": 0,
                        }
                    )
            append_jsonl(raw / "lcd_job_timing.jsonl", timing)
            write_json(
                raw / "lcd_job_timing_raw_summary.json",
                {"stage": "lcd_job_timing", "completed": True},
            )

            result = run_processor(raw, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            no_touch = json.loads(
                (output / "touch_no_action_120s.json").read_text(encoding="utf-8")
            )
            self.assertEqual(no_touch["status"], "MEASURED")
            self.assertEqual(no_touch["result"], "PASS")

            hitbox_summary = json.loads(
                (output / "touch_hitbox_summary.json").read_text(encoding="utf-8")
            )
            self.assertEqual(hitbox_summary["reachable_count"], 27)
            self.assertEqual(hitbox_summary["result"], "PASS")
            with (output / "touch_hitbox_trials.csv").open(
                newline="", encoding="utf-8"
            ) as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 27)

            timing_summary = json.loads(
                (output / "lcd_job_timing_summary.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(timing_summary["status"], "MEASURED")
            self.assertEqual(len(timing_summary["classes"]), 9)
            for item in timing_summary["classes"]:
                self.assertEqual(item["count"], 3)
                self.assertEqual(item["median_cycles"], 200)
                self.assertEqual(item["max_cycles"], 300)

            manifest = json.loads(
                (output / "evidence_manifest.json").read_text(encoding="utf-8")
            )
            self.assertEqual(
                manifest["measurement_boundaries"]["EXTERNAL_ANALOG_THD"],
                "NOT_MEASURED",
            )
            checksums = (output / "sha256_manifest.txt").read_text(
                encoding="ascii"
            )
            digest = hashlib.sha256(
                (output / "touch_no_action_120s.json").read_bytes()
            ).hexdigest()
            self.assertIn(digest, checksums)

    def test_failed_raw_data_is_retained_and_never_promoted_to_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()
            before = {
                "touch_actions": 1,
                "touch_page_requests": 0,
                "touch_preset_requests": 0,
                "touch_dynamic_enable_requests": 0,
                "touch_dynamic_strength_requests": 0,
                "touch_editor_actions": 0,
            }
            after = dict(before, touch_actions=2, touch_preset_requests=1)
            write_json(
                raw / "no_touch_120s_raw_summary.json",
                {
                    "stage": "no_touch_120s",
                    "completed": True,
                    "before": before,
                    "after": after,
                },
            )
            result = run_processor(raw, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            evidence = json.loads(
                (output / "touch_no_action_120s.json").read_text(encoding="utf-8")
            )
            self.assertEqual(evidence["status"], "MEASURED")
            self.assertEqual(evidence["result"], "FAIL")
            self.assertTrue(evidence["stop_reasons"])
            inventory = json.loads(
                (output / "external_file_inventory.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertTrue(
                any(
                    item["file_name"] == "no_touch_120s_raw_summary.json"
                    for item in inventory["files"]
                )
            )

    def test_endurance_quotas_are_proven_by_start_end_histograms(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = pathlib.Path(temporary)
            raw = base / "raw"
            output = base / "evidence"
            raw.mkdir()
            counter_names = (
                "process_frames", "ad_frames", "da_frames",
                "algorithm_frames", "analyzer_publications",
                "smart_decisions", "clarity_decisions", "guard_decisions",
                "lcd_swap_completes", "touch_actions", "touch_rejected",
                "lcd_jobs", "static_dynamic_overlap_frames",
                "deadline_misses", "latency_misses", "frame_overlaps",
                "dropped_frames", "clip_count", "invalid_count",
                "smart_saturation", "smart_nonfinite", "clarity_saturation",
                "clarity_nonfinite", "guard_saturation", "guard_nonfinite",
                "lcd_unexpected_full_redraw", "lcd_writeback_failures",
                "lcd_raster_faults", "lcd_fifo_underflows", "lcd_sync_errors",
                "lcd_frame_address_mismatches", "lcd_bounds_failures",
                "lcd_canary_failures", "lcd_raster_stop_timeouts",
                "touch_invalid_coordinates", "touch_duplicate_actions",
            )
            before = {name: 0 for name in counter_names}
            before.update(
                {
                    "frame_latency_max_cycles": 1000,
                    "algorithm_max_cycles": 1000,
                    "frame_service_max_cycles": 1000,
                    "lcd_max_job_cycles": 1000,
                    "touch_action_histogram": [0] * 27,
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
                    "lcd_swap_completes": 40,
                    "lcd_jobs": 500,
                    "static_dynamic_overlap_frames": 1,
                }
            )
            histogram = [0] * 27
            for action in range(1, 6):
                histogram[action] = 3
            for action in (6, 7, 8, 9, 10, 11):
                histogram[action] = 2
            histogram[12] = 40
            for action in range(13, 23):
                histogram[action] = 1
            histogram[25] = 3
            histogram[26] = 2
            after["touch_action_histogram"] = histogram
            after["touch_actions"] = sum(histogram)
            write_json(
                raw / "endurance_300s_raw_summary.json",
                {
                    "stage": "endurance_300s",
                    "completed": True,
                    "host_elapsed_seconds": 300.1,
                    "continuous_window_halt_count": 2,
                    "before": before,
                    "after": after,
                },
            )
            result = run_processor(raw, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            evidence = json.loads(
                (output / "final_interactive_endurance_300s.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(evidence["result"], "PASS")
            self.assertEqual(evidence["action_quota_status"], "PASS")
            self.assertEqual(evidence["action_quota_data"]["page_action_count"], 40)

            for key in (
                "analyzer_publications",
                "smart_decisions",
                "clarity_decisions",
                "guard_decisions",
            ):
                after[key] = 0
            write_json(
                raw / "endurance_300s_raw_summary.json",
                {
                    "stage": "endurance_300s",
                    "completed": True,
                    "host_elapsed_seconds": 300.1,
                    "continuous_window_halt_count": 2,
                    "before": before,
                    "after": after,
                },
            )
            result = run_processor(raw, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            stalled = json.loads(
                (output / "final_interactive_endurance_300s.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(stalled["result"], "FAIL")
            self.assertTrue(
                any("did not advance" in item for item in stalled["stop_reasons"])
            )


if __name__ == "__main__":
    unittest.main()
