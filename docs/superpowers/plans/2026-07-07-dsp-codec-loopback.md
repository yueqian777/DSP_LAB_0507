# DSP Codec Loopback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a lightweight frequency-domain codec encode/decode loopback inside the existing WOLA path for DSP speaker-listening verification.

**Architecture:** Keep the current PCM codec eval unchanged. Add a new `user_subband_codec_loopback` module that operates directly on WOLA `fft_re/fft_im` after denoise and before band gain, then add modes 8/9/10 that enable it without changing modes 0-7.

**Tech Stack:** C89-style C for C6748/CCS, existing WOLA constants, MSYS2 GCC host tests, CCS Debug gmake build.

---

### Task 1: RED Test Hook

**Files:**
- Modify: `Code/User/user_dsp/user_subband_test.c`
- Create: `Code/User/user_dsp/user_subband_codec_loopback_eval.h`
- Create: `Code/User/user_dsp/user_subband_codec_loopback_eval.c`

- [ ] Add an offline eval entry point named `SubbandCodecLoopbackEval_OfflineTest_All`.
- [ ] Call it from `SubbandWOLA_OfflineTest_All`.
- [ ] Compile the host test and confirm it fails before the production loopback module exists.

### Task 2: Frequency-Domain Loopback Module

**Files:**
- Create: `Code/User/user_dsp/user_subband_codec_loopback.h`
- Create: `Code/User/user_dsp/user_subband_codec_loopback.c`
- Modify: `Code/User/user_dsp/user_subband_wola.c`

- [ ] Implement enable/reset/target-kbps APIs and Watch variables.
- [ ] Process only positive frequency bins, compute energy/scalar counts/max/rms/robust scale, allocate bits with perceptual weights, quantize/dequantize, restore conjugate symmetry, and update debug bitrate/compression counters.
- [ ] Insert `SubbandCodecLoopback_ProcessSpectrum()` after `SubbandDenoise_ProcessSpectrum()` and before `Apply_Band_Gain()`.

### Task 3: Real-Time Demo Modes

**Files:**
- Modify: `Code/User/user_dsp/user_subband_flow.h`
- Modify: `Code/User/user_dsp/user_subband_flow.c`

- [ ] Add modes 8/9/10 for MCRA+codec loopback, strong MCRA+codec loopback, and fixed denoise+codec loopback.
- [ ] Disable codec loopback for every existing mode 0-7.
- [ ] Allow CCS Watch to set `SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps` to 160/240/320.

### Task 4: Offline Eval, WAV Export, Docs, Build Graph

**Files:**
- Modify: `Code/User/user_dsp/user_subband_audio_compare.c`
- Modify: `Debug/Code/User/user_dsp/subdir_vars.mk`
- Modify: `Debug/Code/User/user_dsp/subdir_rules.mk`
- Modify: `Debug/ccsObjs.opt`
- Modify: `Debug/sources.mk`
- Modify: `Debug/makefile`
- Update docs or README with mode 8/9/10 meaning.

- [ ] Generate `subband_codec_loopback_eval_report.csv` with required columns.
- [ ] Export compare WAVs 12/13/14 for 160/240/320 kbps MCRA codec loopback.
- [ ] Ensure CCS Debug build includes the new sources.
- [ ] Run host offline eval, compare export, and CCS Debug build without connecting DSP.
