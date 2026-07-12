# Project 3.3 Offline Quality Report

## Scope and Baseline

This report covers only Project 3.3 files. Project 3.2 sources, WOLA, denoise,
codec, Touch, ADC/DAC drivers, linker configuration, and the default
`DSP_LAB_PROJECT_SELECT=32` entry remain unchanged.

## Current Code Audit

The previous 3.3 core was a fixed `Q=3` parallel band-pass correction bank.
It used several `+5/+6 dB` legacy presets and its old host evaluation allowed
`clip_count=6659` to print PASS.  The old frequency checks were broad envelope
checks rather than a transparency or coefficient-reference test.

The historical implementation remains available as `EQ_CORE_LEGACY` and is
labeled `EQ_LEGACY_UNSAFE_PRESET`.  The default core is now
`EQ_CORE_RBJ_CASCADE` with low shelf, eight one-octave peaking sections, high
shelf, mild presets, coefficient validation, and automatic preamp.

## Hard Bypass and FLAT

`Equalizer_SetBypass()` provides a strict copy path.  RAW_COPY, FLOAT_COPY,
hard bypass, and FLAT bypass IIR, preamp, crossfade, limiter, saturation, and
gain smoothing.  The host transparency report covers impulse, 1 kHz, and
frame-boundary signals in short and float paths:

- max error: 0 LSB / exact float equality
- clip count: 0
- hard bypass state mutation: none

## Legacy and RBJ Responses

`equalizer_legacy_system_response.csv` contains 512 log-spaced response
points for FLAT, BASS, VOCAL, TREBLE, V_SHAPE, and SINGLE_1K_PLUS3DB.
`equalizer_legacy_summary.csv` records the legacy peak, minimum, group delay,
center interaction, and required headroom.

The legacy BASS peak is about 6.37 dB, demonstrating neighbor interaction
beyond a single slider command.  Its maximum reported group delay is about
857 samples; VOCAL reaches about 931 samples.  These are offline response
measurements, not board timing results.

`equalizer_system_response.csv` provides the corresponding legacy and RBJ
complex-response-derived magnitude, phase, and group-delay data.  The RBJ
mild preset preamps are exported in `equalizer_headroom_prediction.csv`.

## Headroom, Stability, and THD

RBJ predicted peaks for the normal presets are approximately 2.32 to 3.80 dB.
The applied preamp follows `-max(0, predicted_peak_db)-0.5 dB`; normal
multitone headroom tests reported zero clips for every preset.

Five independent 60-second-equivalent host runs (white, pink, multitone,
speech-like, music-like) completed with finite internal state and zero clips.
The coherent 50 kHz / 5000-sample / 1 kHz THD report also records RAW,
FLOAT_COPY, FLAT, a single band, and a mild preset separately.  Deliberate
legacy overload is isolated in `equalizer_expected_clip_report.csv` and only
passes when clipping occurs.

## Independent Python Check

`tools/equalizer_33_reference.py` independently evaluates the complex
z-domain response from `equalizer_coefficients.csv`.  It does not invoke C
functions.  The current comparison has 5120 rows, no non-finite coefficient,
all pole radii below one, and maximum magnitude difference about
0.0021205 dB, below the required 0.01 dB.

## WAV A/B

`tools/equalizer_33_process_wav.py` generated eight deterministic mono
16-bit 50 kHz signals and rendered nine modes per input through a temporary
host executable compiled from `user_equalizer.c`.  The 72 output rows passed.
Artifacts are under `docs/eval_outputs/equalizer_33_offline/`, including WAV
files, `equalizer_wav_metrics.csv`, `equalizer_wav_summary.md`, and five PNG
comparison plots.

## Build Status

- `BUILD_PASS`: default CCS Project 32 build with LCD disabled.
- `BUILD_PASS`: full CCS build with
  `DSP_LAB_PROJECT_SELECT=33` injected through `GEN_OPTS__FLAG`.
- `HOST_PASS`: strict MSYS2 C build and offline evaluator.
- `HOST_PASS`: LCD-off compilation and explicit LCD-on Chinese-glyph mock.
- `HOST_PASS`: independent Python reference and C-rendered WAV A/B.

The build fingerprint symbols are `EQ_DebugBuildMagic=0x33030003`,
`EQ_DebugBuildVersion="P33_FIX_V5"`, `EQ_DebugBuildGitSha`,
`EQ_DebugBuildTimestamp`, and `EQ_DebugBuildDirty`. The generated header reads
the short SHA and dirty state at build time; the source fallback reports Git
as unavailable rather than embedding a hand-maintained commit ID.

## PENDING_HARDWARE

Board audio quality, AD/DA frame continuity, actual CPU timing, actual clip
count, LCD refresh interference, LCD Chinese glyph appearance, download/run
success, and all Watch observations remain `PENDING_HARDWARE`.  Use
`equalizer_33_board_test_pending.md` for the required board sequence.
