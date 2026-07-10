# Subband Touch UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a non-blocking 800×480 LCD/touch controller for Project 3.2 modes and codec bitrate without changing DSP algorithms.

**Architecture:** Keep deterministic UI state in a pure C logic module and hardware drawing/touch in a separate UI module. The existing flow remains the sole owner of algorithm mode transitions; UI writes only request variables and is serviced after audio events.

**Tech Stack:** TI C6000 C99, StarterWare grlib/widgets, GT1151 BSP, MSYS2 host GCC tests, CCS Debug/O3, DSS board scripts.

---

### Task 1: Pure UI state contract

**Files:**
- Create: `Code/User/user_dsp/user_subband_ui_logic.h`
- Create: `Code/User/user_dsp/user_subband_ui_logic.c`
- Create: `tools/tests/subband_ui_logic_test.c`

- [ ] Write tests asserting button mapping `{0,1,2,4,6,7,8,11}`, bitrate normalization to 240, codec-mode activation only for 8/11, learning-mode detection for 2/4/6/7/8, remaining-ms calculation, and one accepted press per press-release cycle.
- [ ] Compile before implementation with MSYS2 GCC and confirm missing-symbol failure.
- [ ] Implement pure functions with no hardware headers or dynamic allocation.
- [ ] Recompile and require all assertions to pass.

### Task 2: Persistent codec target in flow control

**Files:**
- Modify: `Code/User/user_dsp/user_subband_flow.h`
- Modify: `Code/User/user_dsp/user_subband_flow.c`
- Extend: `tools/tests/subband_ui_logic_test.c`

- [ ] Add a failing source contract test requiring a normalized persistent target and no fixed 240 assignment in Mode 8/11.
- [ ] Add `SUBBAND_DebugPersistentCodecKbps` and `Subband_Request_Codec_Target()` in the flow control layer.
- [ ] Make Mode 8/11 reset codec state then apply the persistent target; running requests use `SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps` without mode re-entry.
- [ ] Verify Mode 9/10 and all core codec functions are unchanged.

### Task 3: LCD/touch UI module

**Files:**
- Create: `Code/User/user_dsp/user_subband_ui.h`
- Create: `Code/User/user_dsp/user_subband_ui.c`
- Create: `Code/User/user_dsp/user_subband_ui_font.h`
- Create: `Code/User/user_dsp/user_subband_ui_font.c`
- Modify: `Code/User/user_include.h`

- [ ] Add source contract tests for fixed absolute coordinates, dirty bits, debug variables, and absence of malloc/free and blocking delays.
- [ ] Build eight mode widgets plus three manual bitrate hit regions at the approved coordinates.
- [ ] Implement static 16×16 Chinese glyphs and ASCII numeric rendering.
- [ ] Implement press-release locking, applied-mode highlighting, learning countdown from real hop counters, codec target display, and optional integer algorithm load.
- [ ] Limit each display service call to two dirty regions.

### Task 4: Main-loop integration and project build

**Files:**
- Modify: `Code/User/user_dsp/user_subband_flow.c`
- Modify: `Debug/Code/User/user_dsp/subdir_vars.mk`
- Modify: `Debug/ccsObjs.opt`

- [ ] Initialize LCD/touch/UI before ADC/DAC start.
- [ ] Service touch after AD/DA handling and display only when both audio flags are clear.
- [ ] Add new UI sources to CCS build metadata.
- [ ] Run Debug/O3 build and require `link_errors=0` with no new compiler warnings.

### Task 5: Documentation and board evaluation

**Files:**
- Create: `docs/subband_touch_ui_design.md`
- Create: `docs/subband_touch_ui_reference.md`
- Create: `tools/dss/dss_subband_touch_ui_test.js`
- Create: `docs/eval_outputs/subband_touch_ui_test.csv`
- Create: `docs/eval_outputs/subband_touch_ui_test_summary.md`

- [ ] Document resolution, touch range, layout, state machine, refresh policy, persistent bitrate and font storage.
- [ ] Record only TI official and local BSP references actually used.
- [ ] Build/load the current `.out`, inject mode flags and codec requests, and collect applied mode, frame counters, learning counters, target bitrate, invalid count and UI timing.
- [ ] Run Mode 8/320 for at least 60 seconds if the hardware/audio path remains available.
- [ ] Mark visual drift, physical long-press and subjective audio checks `NOT_MEASURED` when DSS cannot prove them.

### Task 6: Final verification

**Files:** all files above.

- [ ] Run host tests, source contract tests, CCS build, `git diff --check`, and result consistency checks.
- [ ] Compare map section sizes against the pre-change build and report text/data/bss and UI state/font footprint.
- [ ] Review the TXT requirements line by line and list any unmeasured physical checks without converting them into passes.
