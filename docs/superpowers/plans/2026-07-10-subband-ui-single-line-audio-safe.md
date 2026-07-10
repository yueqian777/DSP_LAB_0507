# Project 3.2 Single-Line LCD And Audio-Safe Scheduler Plan

> **For Codex:** Execute each task test-first. Do not touch WOLA, denoiser,
> codec, ADC/DAC, mode numbering, or codec target-control behavior.

**Goal:** Make the Project 3.2 processing chain a reliable single-line ASCII
display while bounding runtime LCD work so audio remains the highest-priority
path.

**Architecture:** Keep chain construction in the host-testable UI logic module.
The board UI snapshots applied state, renders one narrowly bounded rectangle,
and schedules at most one draw job with frame gap and mode-change holdoff. Raw
algorithm counters update internal state but do not directly trigger LCD work.

**Toolchain:** C99 host tests under MSYS2, Python `unittest` contract tests,
TI CCS C6748 Debug/O3 builds where available.

---

## Task 1: Lock The New Contract With Failing Tests

**Files:**
- Modify: `tools/tests/test_subband_touch_ui_contract.py`
- Modify: `tools/tests/subband_ui_logic_test.c.host`

1. Replace the old test that accepts a missing `FLAG_AD_DONE` guard with a test
   that requires `FLAG_AD_DONE == 0U` alongside AD, DA, audio, and touch idle.
2. Add source contracts for one chain line, ASCII arrows, no legacy line-2
   fields/job, dynamic mode 8/11 rates, dirty lifecycle, job priority, one job
   per frame, two-frame gap, three-frame holdoff, coarse progress policy, and
   the lightweight per-frame load recorder.
3. Add host C cases for all eight exact mode strings, mode 8/11 rates, fallback,
   and bounded tiny buffers.
4. Run the focused tests and record the expected failures against the old code.

## Task 2: Add The Pure Bounded Chain Builder

**Files:**
- Modify: `Code/User/user_dsp/user_subband_ui_logic.h`
- Modify: `Code/User/user_dsp/user_subband_ui_logic.c`
- Test: `tools/tests/subband_ui_logic_test.c.host`

1. Add a bounded append helper with no allocation.
2. Implement `SubbandUI_BuildProcessingChain()` using applied mode and applied
   target kbps only.
3. Return the safe fallback `ADC -> PROCESSING -> DAC` for unsupported state or
   insufficient output capacity where the fallback itself fits.
4. Run the host C logic test until all chain cases pass.

## Task 3: Replace Two-Line Rendering With One Atomic Chain Job

**Files:**
- Modify: `Code/User/user_dsp/user_subband_ui.c`
- Modify: `Code/User/user_dsp/user_subband_ui.h`
- Test: `tools/tests/test_subband_touch_ui_contract.py`

1. Replace Y0/Y1 and line1/line2 structures with the fixed single-line chain
   rectangle `(28,80)-(770,106)` and a single chain job.
2. Select the first existing 16/14/12 pixel ASCII font that fits according to
   `GrStringWidthGet()`; never wrap or truncate `WOLA SYN`/`DAC`.
3. Remove the duplicate standalone rate text job.
4. Add a chain-dirty marker helper and the required set/draw/wait/debug state.
5. Clear chain dirty and update displayed mode/rate only after the draw call.
6. Run the chain and source-contract tests.

## Task 4: Make Runtime Scheduling Audio-Safe

**Files:**
- Modify: `Code/User/user_dsp/user_subband_ui.c`
- Modify: `Code/User/user_dsp/user_subband_ui.h`
- Modify: `Code/User/user_dsp/user_subband_flow.c`
- Test: `tools/tests/test_subband_touch_ui_contract.py`

1. Restore `FLAG_AD_DONE == 0U` in the main-loop LCD guard.
2. Add a last-draw frame sentinel and skip repeated service calls in the same
   algorithm frame.
3. Enforce a default two-algorithm-frame draw gap after every effective job.
4. Start a default three-frame draw holdoff after a mode is applied.
5. Preserve one-job dispatch and enforce priority: mode buttons, status, chain,
   rate buttons, learning/countdown, optional progress, load.
6. Add the requested scheduler debug counters and verify the tests.

## Task 5: Reduce Learning And Load Refresh Work

**Files:**
- Modify: `Code/User/user_dsp/user_subband_ui.c`
- Modify: `Code/User/user_dsp/user_subband_ui.h`
- Modify: `Code/User/user_dsp/user_subband_ui_logic.c`
- Modify: `Code/User/user_dsp/user_subband_ui_logic.h`
- Test: `tools/tests/test_subband_touch_ui_contract.py`
- Test: `tools/tests/subband_ui_logic_test.c.host`

1. Add disabled/coarse/ten-block compile-time policies with coarse as default.
2. Compute rounded-up remaining seconds and mark the combined learning text job
   only for `2 s`, `1 s`, and ready transitions; raw LearnHops changes must not
   set countdown/progress dirty.
3. Retain the ten-block code only behind its explicit non-default policy.
4. Restrict `SubbandUI_RecordAlgoCycles()` to integer EMA and sample count.
5. Move frame-budget division, percentage conversion, and dirty decisions to
   the 50-frame low-frequency update.
6. Run all host and Python tests.

## Task 6: Produce Reproducible Static Verification Artifacts

**Files:**
- Add: `tools/generate_subband_ui_static_verification.py`
- Add: `docs/subband_ui_audio_safe_scheduler_design.md`
- Add: `docs/subband_ui_single_line_chain_design.md`
- Add: `docs/subband_ui_no_hardware_verification.md`
- Add: `docs/subband_ui_manual_board_test_plan.md`
- Generate: `docs/eval_outputs/subband_ui_static_verification.csv`
- Generate: `docs/eval_outputs/subband_ui_static_verification_summary.md`

1. Implement a deterministic static-verification script for the requested
   contracts and generated CSV/Markdown outputs.
2. Document arrow/font rationale, all mode strings, dirty lifecycle, scheduler,
   coarse learning behavior, and board-only test steps.
3. Mark every unavailable hardware conclusion exactly
   `NOT_MEASURED_BOARD_UNAVAILABLE`.
4. Run the generator and inspect generated artifacts.

## Task 7: Full Verification And Build

**Files:** all modified files

1. Run `python -B -m unittest tools.tests.test_subband_touch_ui_contract -v`.
2. Run the host C logic executable under MSYS2.
3. Run `git diff --check` and scan for Unicode arrows, legacy line-2 symbols,
   delays, and accidental algorithm edits.
4. Build CCS Debug clean/all. Build O3 if an independent configuration exists.
5. Record errors, warnings, link errors, and text/data/bss delta where the build
   artifacts expose them; report unavailable build configurations honestly.
6. Review the final diff, commit as
   `fix: make LCD chain single-line and audio-safe`, and do not connect a DSP.
