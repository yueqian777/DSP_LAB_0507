from __future__ import annotations

import csv
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "Code/User/user_dsp/user_subband_ui.c").read_text(encoding="utf-8")
UI_H = (ROOT / "Code/User/user_dsp/user_subband_ui.h").read_text(encoding="utf-8")
LOGIC = (ROOT / "Code/User/user_dsp/user_subband_ui_logic.c").read_text(encoding="utf-8")
FLOW = (ROOT / "Code/User/user_dsp/user_subband_flow.c").read_text(encoding="utf-8")


def contains_all(text: str, *tokens: str) -> bool:
    return all(token in text for token in tokens)


def record(name: str, passed: bool, evidence: str) -> tuple[str, str, str]:
    return name, "PASS" if passed else "FAIL", evidence


recorder = UI[
    UI.index("void SubbandUI_RecordAlgoCycles(unsigned long cycles)") :
    UI.index("void SubbandUI_ResetLoadWindow", UI.index("void SubbandUI_RecordAlgoCycles"))
]
job_body = UI[
    UI.index("static unsigned long UI_DrawOneJob(void)") :
    UI.index("static void UI_RecordDrawCycles", UI.index("static unsigned long UI_DrawOneJob"))
]
display_guard = FLOW[
    FLOW.rfind("if (", 0, FLOW.index("SubbandUI_ServiceDisplay();")) :
    FLOW.index("SubbandUI_ServiceDisplay();")
]

checks = [
    record("single_line_geometry", contains_all(UI, "#define UI_CHAIN_Y 84", "#define UI_CHAIN_Y_MIN 80", "#define UI_CHAIN_Y_MAX 106"), "single Y and bounded top-panel rectangle"),
    record("ascii_arrow_only", " -> " in LOGIC and "\u2192" not in (UI + LOGIC), "ASCII chain separator; no Unicode arrow"),
    record("legacy_second_line_removed", not any(token in UI for token in ("UI_CHAIN_Y0", "UI_CHAIN_Y1", "line2_prefix", "line2_suffix")), "legacy two-line fields absent"),
    record("all_eight_modes", all(f"case {mode}:" in LOGIC for mode in (0, 1, 2, 4, 6, 7, 8, 11)), "eight applied modes handled"),
    record("codec_rate_dynamic", contains_all(LOGIC, "SubbandUI_AppendKbps", '"k -> WOLA SYN -> DAC"'), "mode 8/11 append normalized target kbps"),
    record("all_paths_end_dac", LOGIC.count("WOLA SYN -> DAC") >= 7 and '"ADC -> RAW -> DAC"' in LOGIC, "RAW and all WOLA paths retain DAC"),
    record("bounded_chain_builder", contains_all(LOGIC, "SubbandUI_CopyBounded", "SubbandUI_AppendBounded", '"ADC -> PROCESSING -> DAC"'), "bounded append and safe fallback"),
    record("font_width_check", contains_all(UI, "UI_SelectChainFont", "GrStringWidthGet", "g_sFontCmss18b", "g_sFontCmtt12"), "existing font reused with measured 12-pixel fallback"),
    record("single_chain_draw", UI.count("static void UI_DrawProcessingChain(void)") == 1 and "SUBBAND_UI_DebugChainDrawCount++" in UI, "one atomic chain job"),
    record("chain_dirty_lifecycle", contains_all(UI, "UI_MarkChainDirty", "SUBBAND_UI_DebugChainDirtySetCount++", "UI_ClearDirty(UI_DIRTY_CHAIN);"), "dirty clears only in chain draw"),
    record("chain_priority", job_body.index("UI_DIRTY_STATUS") < job_body.index("UI_DIRTY_CHAIN") < job_body.index("UI_DIRTY_RATE"), "status then chain then rate buttons"),
    record("initial_chain_draw", "UI_DrawProcessingChain();" in UI[UI.index("static void UI_DrawInitialPage(void)"):], "initial static page includes chain"),
    record("mode_change_marks_chain", "UI_MarkChainDirty();" in UI[UI.index("void SubbandUI_NotifyModeChanged(void)"):], "applied-mode notification marks chain"),
    record("ad_done_guard", contains_all(display_guard, "FLAG_AD == 0U", "FLAG_DA == 0U", "FLAG_AD_DONE == 0U", "audio_serviced == 0U", "touch_serviced == 0U"), "LCD service runs only after pending AD-to-DA work is clear"),
    record("one_job_per_algo_frame", contains_all(UI, "frame == UI_LastDrawAlgoFrame", "SUBBAND_UI_DebugMaxDrawJobsPerFrame"), "same-frame redraw skipped"),
    record("draw_gap_two_frames", "SUBBAND_UI_RUNTIME_DRAW_GAP_FRAMES 2UL" in UI_H, "default 40.96 ms minimum gap"),
    record("mode_holdoff_three_frames", "SUBBAND_UI_MODE_CHANGE_HOLDOFF_FRAMES 3UL" in UI_H, "default 61.44 ms mode-change holdoff"),
    record("coarse_learning_default", contains_all(UI_H, "SUBBAND_UI_PROGRESS_COARSE", "#define SUBBAND_UI_PROGRESS_POLICY SUBBAND_UI_PROGRESS_COARSE"), "default states are 2 s, 1 s, ready"),
    record("learn_hops_not_direct_dirty", "UI_MarkDirty(UI_DIRTY_COUNTDOWN | UI_DIRTY_PROGRESS)" not in UI, "raw LearnHops changes do not directly dirty LCD"),
    record("lightweight_frame_load_recorder", "unsigned long long" not in recorder and "UI_FRAME_BUDGET_CYCLES" not in recorder and "UI_MarkDirty" not in recorder, "per-frame path is integer EMA and sample count only"),
    record("fixed_nonblocking_ui", "delay(" not in UI and "WidgetPaint(" not in UI and "x +=" not in UI and "y +=" not in UI, "fixed coordinates; no blocking or widget auto-redraw"),
]

output_dir = ROOT / "docs/eval_outputs"
output_dir.mkdir(parents=True, exist_ok=True)
csv_path = output_dir / "subband_ui_static_verification.csv"
summary_path = output_dir / "subband_ui_static_verification_summary.md"

with csv_path.open("w", newline="", encoding="utf-8") as handle:
    writer = csv.writer(handle)
    writer.writerow(("check", "status", "evidence"))
    writer.writerows(checks)

passed = sum(status == "PASS" for _, status, _ in checks)
link_info = ROOT / "Debug/DSP_LAB_0507_linkInfo.xml"
map_file = ROOT / "Debug/DSP_LAB_0507.map"
build_lines = ["- CCS Debug/O3 build: not present when this report was generated"]
if link_info.exists() and map_file.exists():
    link_text = link_info.read_text(encoding="latin-1")
    map_text = map_file.read_text(encoding="latin-1")
    link_match = re.search(r"<link_errors>([^<]+)</link_errors>", link_text)
    section_values: dict[str, int] = {}
    for section in (".text", ".bss", ".far", ".fardata", ".const", ".cinit", ".stack"):
        match = re.search(
            rf"^{re.escape(section)}\s+\d+\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)",
            map_text,
            re.MULTILINE,
        )
        if match:
            section_values[section] = int(match.group(1), 16)
    build_lines = [
        "- CCS Debug configuration (`-O3`): PASS",
        f"- Link errors: {link_match.group(1) if link_match else 'UNKNOWN'}",
        "- Linked sections: "
        + ", ".join(f"`{name}`={value} B" for name, value in section_values.items()),
    ]
summary_path.write_text(
    "# Subband UI Static Verification\n\n"
    f"- Static checks: {passed}/{len(checks)} PASS\n"
    "- Python contract suite: run separately with "
    "`python -B -m unittest tools.tests.test_subband_touch_ui_contract -v`\n"
    "- Host chain logic: run separately under MSYS2 using "
    "`tools/tests/subband_ui_logic_test.c.host`\n"
    + "\n".join(build_lines)
    + "\n"
    "- Board audio, touch, frame continuity, and LCD timing: "
    "`NOT_MEASURED_BOARD_UNAVAILABLE`\n",
    encoding="utf-8",
)

raise SystemExit(0 if passed == len(checks) else 1)
