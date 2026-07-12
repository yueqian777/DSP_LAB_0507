# Project 3.2 Mode 6 MCRA Musical-Noise Adjustment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add reset-safe runtime tonal-guard tuning and diagnostics for Project 3.2 Mode 6 without changing Mode 8 or the MCRA equations.

**Architecture:** Store the three tonal-guard values in the existing singleton denoise state and restore legacy defaults during Init/Reset. Mode 6 overrides them through one new setter after Reset; Mode 8 relies on Reset defaults. Contract tests verify mode configuration and a small C host harness verifies state defaults, setter behavior, counter lifecycle, and switching.

**Tech Stack:** TI C6000 C89, MSYS2 MinGW GCC host tests, Python `unittest`, CCS generated Debug makefiles using `-O3`.

---

### Task 1: Add failing configuration and runtime tests

**Files:**
- Create: `tools/tests/test_subband_mcra_tonal_guard.py`
- Create: `tools/tests/subband_mcra_tonal_guard_test.c`
- Test: `Code/User/user_dsp/user_subband_denoise.c`
- Test: `Code/User/user_dsp/user_subband_flow.c`

- [ ] Write Python contract tests that extract the Mode 6 and Mode 8 switch bodies and assert the two Mode 6 setter tuples, unchanged MCRA tuple, no Mode 8 tonal setter, and Reset-first ordering.
- [ ] Write a C harness that initializes/resets the real denoiser, calls the new setter, drives deterministic spectra through learning and MCRA processing, and checks both debug counters.
- [ ] Compile and run the tests from MSYS2 with `/mingw64/bin/python` and `/mingw64/bin/gcc`; expect failure because the setter and counters do not exist yet.

### Task 2: Implement reset-safe tonal-guard state

**Files:**
- Modify: `Code/User/user_dsp/user_subband_denoise.h`
- Modify: `Code/User/user_dsp/user_subband_denoise.c`
- Test: `tools/tests/subband_mcra_tonal_guard_test.c`

- [ ] Add the three float fields to `SubbandDenoiseState` and initialize them to `3.0f`, `1.35f`, and `0.45f` inside the existing default-parameter initialization path.
- [ ] Declare and define `SubbandDenoise_SetMcraTonalGuardParams(float, float, float)` using the same Init-first setter pattern as existing denoise setters.
- [ ] Replace the three tonal-guard literals with state reads without changing PSD, Wiener, prior-SNR, WOLA, or codec logic.
- [ ] Add both `volatile unsigned long` counters, clear the per-frame count at each valid spectrum-call entry, increment both once per guarded bin, and clear both during Reset.
- [ ] Recompile and run the C harness; expect all assertions to pass.

### Task 3: Isolate Mode 6 configuration

**Files:**
- Modify: `Code/User/user_dsp/user_subband_flow.c`
- Test: `tools/tests/test_subband_mcra_tonal_guard.py`

- [ ] Add `SubbandDenoise_SetParams(0.96f, 0.15f, 0.85f, 0.80f)` and `SubbandDenoise_SetMcraTonalGuardParams(5.0f, 1.80f, 0.28f)` immediately after Mode 6 Reset.
- [ ] Leave Mode 6 `SubbandDenoise_SetMcraParams` values unchanged and leave Mode 8 without either new Mode 6 setter.
- [ ] Run the Python and C tests; expect Mode 6, Mode 8, and round-trip isolation checks to pass.

### Task 4: Document parameters and board A/B

**Files:**
- Create: `docs/project32_mode6_mcra_tonal_guard_ab.md`

- [ ] Document legacy defaults, Mode 6 overrides, Reset isolation, potential musical-noise/noise-retention/transient tradeoffs, watch variables, and repeatable board A/B steps.
- [ ] State explicitly that host/build checks do not prove audible improvement and no musical-noise fix is claimed without board listening.

### Task 5: Full verification and commit

**Files:**
- Verify: all files changed by Tasks 1-4

- [ ] From MSYS2, run the new tests and all existing `tools/tests/test_*.py` tests.
- [ ] From MSYS2, run CCS generated `Debug` clean/all using CCS `gmake.exe`; confirm compiler commands contain `-O3`, build has zero errors, and `Debug/DSP_LAB_0507_linkInfo.xml` reports `link_errors=0x0`.
- [ ] Run `git diff --check`, inspect the scoped diff, and confirm unrelated equalizer changes are not staged.
- [ ] Commit only the Mode 6 implementation, tests, plan, and A/B documentation; report both the design commit and implementation commit SHAs.
