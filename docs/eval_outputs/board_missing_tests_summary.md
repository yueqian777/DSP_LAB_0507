# Project 3.2 C6748 Board Test Summary

## Traceability

- Hardware: TMS320C6748, core clock 456 MHz.
- Configuration: 50 kHz, 1024 samples/frame, CH1, O3 Debug.
- Commit: `cf0db0781e6dae9a810bef3eb7f29bef1d261339`.
- Source CSV: `docs/eval_outputs/board_missing_tests_raw.csv`.
- Each row is a 10 s DSS hardware measurement after a 1 s input-frame preflight.
- `realtime_pass=1` means measured max processing time is below 20.48 ms. It does not prove audio quality.

## Method Comparison

| Backend | Repeats | Median max ms | Worst max ms | Median CPU | Output observation |
|---|---:|---:|---:|---:|---|
| legacy FIR | 3/3 | 2.073 | 2.073 | 10.12% | nonzero frames in all runs |
| project1 polyphase | 3/3 | 17.557 | 17.558 | 85.73% | below 64 LSB output threshold |
| WOLA-DFT | 3/3 | 2.198 | 2.198 | 10.73% | nonzero frames in all runs |

- WOLA reported 3 output-clip frame(s) across the three runs; this is source-level amplitude evidence and needs listening-level confirmation.
- The project1 polyphase wrapper met timing but its peak output/input ratio was only 0.112-0.128. It is not accepted as a verified audible reconstruction path.

## Codec-only WOLA

| Target kbps | Repeats | Measured kbps median | Compression median | Max ms worst | CPU median | Clamp ratio | Invalid count |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 160 | 3/3 | 158 | 5.0x | 4.306 | 21.02% | 0.000 | 0 |
| 240 | 3/3 | 233 | 3.0x | 5.012 | 24.47% | 0.000 | 0 |
| 320 | 3/3 | 321 | 2.0x | 5.744 | 28.03% | 0.000 | 0 |

## Full Chain: WOLA + MCRA + Codec 240 kbps

| Repeats | Median max ms | Worst max ms | Median CPU | Learning | Ready | Measured kbps | Output observation |
|---:|---:|---:|---:|---|---:|---:|---|
| 3/3 | 15.465 | 15.466 | 75.51% | 391/391 | 1 | 233 | below 64 LSB threshold; peak ratio 0.076 |

The full-chain measurements prove the DSP processing schedule and learning state completed in time. They do not prove acceptable sound: all three output peaks were 31 LSB, below the automated 64 LSB nonzero threshold. Repeat listening verification with a documented noise-only then speech-plus-stationary-noise source before making an audio-quality claim.

## Artifacts

- `docs/eval_outputs/board_missing_tests.csv`
- `docs/eval_outputs/board_missing_tests_raw.csv`
- `docs/eval_outputs/board_missing_tests_initial_input_lost_raw.csv` (archived incomplete earlier run)
- `docs/eval_outputs/plots/board_runtime_compare.png`
- `docs/eval_outputs/plots/codec_bitrate_compare.png`
- `docs/eval_outputs/plots/codec_band_bits_compare.png`
