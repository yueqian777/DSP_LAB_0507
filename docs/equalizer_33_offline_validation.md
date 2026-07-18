# Project 3.3 Ten-Band Editor Offline Validation

## Provenance and labels

Baseline is `b67f755`; tested feature source is
`6c3daca0cfd645704446a60c5fe189ffeb0b8645`. This phase uses the labels
`HOST_CONTRACT_PASS`, `HOST_CONTROL_STRESS_PASS`, `OFFLINE_PCM16_PASS`,
`OFFLINE_RESPONSE_MODEL_PASS`, `UI_RENDER_TRACE_PASS`,
`TARGET_CLEAN_BUILD_PASS`, and `TARGET_LINK_MAP_AUDIT_PASS`.

No DSP board, sound card, microphone, ADC, DAC, or physical Touch was used.
Nothing in this report is `MEASURED_ON_CURRENT_BOARD`, `BOARD_PASS`,
`LCD_VISUAL_PASS`, or `REALTIME_BOARD_PASS`.

## Host and Project 3.2 regression

The complete `tools/tests` discovery passed 219 tests with zero failures.
Direct C89/C99 builds used MSYS2 GCC 15.2.0 with `-Wall -Wextra -Werror`.
Editor control, UI logic, and actual renderer harnesses each reported
`failures=0`; the original equalizer evaluator also reported zero failures.

The shared spectral kernel regression passed eight frozen 512-point FFT cases,
12 consecutive Project 3.2 WOLA output hops bit-exactly, 18 invalid-input
checks, and a 1024-point sanity case. No Project 3.2 algorithm source or
`Code/main.c` was changed from the baseline.

## Control and scheduler stress

The actual mailbox, builder, safe-boundary installer, Equalizer, UI action
mapping, Analyzer publication, LCD job selector, and background-budget APIs
ran for 12000 deterministic actions with seed `0x33E71801`. The run observed
145 stale targets, 145 cancellations, 129 applied targets, three builder
services, 1126 Analyzer services, and 2640 LCD services. Stale installs,
nonfinite values, clipping, and failures were zero; sequence wrap was covered.

The Host scheduler model proves ordering and mutual exclusion, not C6748
cycles or deadline margin. The event snapshot gate limits actual flow requests
to one per processed frame; a same-frame post-snapshot Touch change is carried
to the next frame.

## PCM16 response

`tools/equalizer_33_editor_offline.py` compiles the real Equalizer and Control
sources and generates deterministic mono PCM16 at 50 kHz. Six CUSTOM patterns
times 16 signals produce 96 static cases; 60 frequency points compare a
single-bin DFT ratio with `Equalizer_GetSystemResponse`, including preamp.

After 10000 settling samples, each center measurement uses 40000 samples. The
maximum center error was 0.001727647 dB against a 0.15 dB limit; maximum
complex-response error was 0.000102050 against 0.025. Maximum output peak was
0.899963379 FS. Clip, nonfinite, identity mismatch, unexpected repeated frame,
and failure counts were zero.

Four transitions covered FLAT to CUSTOM, CUSTOM A to CUSTOM B, CUSTOM to
VOCAL, and VOCAL to RESET FLAT. Every transition was exactly 6000 samples at
50 kHz, or 120 ms. Progress was monotonic across 192 checks; applied sequence
never advanced early; clip, nonfinite, and repeated-frame counts were zero.
This is `OFFLINE_DIGITAL_TRANSITION_INTEGRITY`, not a claim that switching is
inaudible.

## Renderer trace

The actual Project 3.3 renderer produced 19406 ordered primitive records and
555 Chinese glyph records for ten fixed 800x480 previews. Bounds failures were
zero. The replay uses the C source's 16x16 glyph bitmaps, not a system Chinese
font. Full trace, per-preview traces, and PNGs remain under `%TEMP%`; only
checksums and compact metadata are committed.

These images are `OFFLINE_RENDER_PREVIEW`. They do not prove physical LCD
glyph appearance, Touch alignment, refresh timing, or absence of dynamic-page
circular drift.

## Sanitizer and artifacts

The available MinGW GCC links ordinary warnings-as-errors Host harnesses but
does not provide `libasan` or `libubsan`; the linker rejected `-lasan` and
`-lubsan`. Status is therefore `SANITIZER_NOT_AVAILABLE`, not PASS.

Large WAV, PNG, JSONL, map, XML, and `.out` artifacts are not committed. Their
logical temporary paths, sizes, and SHA-256 values are recorded in
`docs/evidence/equalizer_editor_offline_6c3daca/`.

## CCS clean build and map audit

Eight clean `-B` profiles cover Project 3.2; Project 3.3 LCD OFF; static;
Dynamic; Dynamic plus Touch; Editor read-only; Editor plus Touch; and the full
dual-page mask. All eight have warning=0, error=0, and `link_errors=0x0`, and
each produced an `.out` with a recorded SHA-256.

Editor read-only relative to Dynamic adds 15424 bytes `.text`, 4080 `.const`,
zero `.bss`, and zero `.subband_l2`. Editor plus Touch relative to Touch adds
16160, 4080, zero, and zero. Editor state is 64 bytes; UI plus editor is 620
bytes; with Touch it is 656 bytes. Editor-OFF profiles retain zero editor
runtime symbols, UI `.subband_l2` hits are zero, and second-framebuffer hits
are zero. These are build and map facts only, not target execution evidence.

## Hardware boundary

The preserved `67a22ef` static alignment page ran ten minutes with objective
counters and operator visual observation passing. Hardened Dynamic Status,
this editor, physical Touch, real LCD, target cycles, and combined endurance
were not run after hardware removal and remain `PENDING_HARDWARE`. Offline
PCM16 also does not measure analog THD, SNR, frequency response, or SPL.
