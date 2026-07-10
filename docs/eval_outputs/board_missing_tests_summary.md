# Project 3.2 Board Missing Tests Summary

Generated: 2026-07-10T11:24:46

## Measurement Status

- Raw DSS rows: 21 (`board_missing_tests_raw.csv`).
- Rows with an input-silent or no-frame condition were normalized to `NOT_MEASURED` in `board_missing_tests.csv`.
- `subjective_result` is automated-only. It cannot claim absence of blast, tearing, or audible distortion.

## Newly Executed Method Comparison

| Backend | Valid timing repeats | Median MaxMs | Worst MaxMs | Output status | Conclusion |
|---|---:|---:|---:|---|---|
| legacy FIR | 2/3 | 2.000 ms | 2.000 ms | nonzero | timing valid |
| project1 polyphase | 2/3 | 17.000 ms | 17.000 ms | below output threshold | reconstruction listening test required |
| WOLA-DFT | 2/3 | 2.000 ms | 2.000 ms | nonzero | timing valid |

The third repeat for every method had no ADC/input frames. It is not used in the medians or worst-case values, so the requested three valid repetitions have not been completed.

## Codec-only 160 / 240 / 320 kbps

- Newly executed valid codec-only repetitions: 0/9. All current rows are `NOT_MEASURED_INPUT_SILENT`.
- Therefore target bitrate response, bit allocation, clamp ratio, and audible quality were not re-claimed from this run.
- Historical board reference: `board_codec_loopback_runtime.csv` (2026-07-08) recorded MCRA+codec, not codec-only: 160->158 kbps, 240->233 kbps, 320->321 kbps; MaxMs 14/15/16 ms; clamp ratios 0.270/0.410/0.472%; invalid_count=0. It has a traceable CSV/configuration but no commit SHA, so it is reference-only and needs current-code rerun.

## Full Chain WOLA + MCRA + 240 kbps

- Newly executed valid full-chain repetitions: 0/3. Current runs are `NOT_MEASURED_INPUT_SILENT`; learning values are not accepted as a 2 s learning result.
- Historical logged C6748 results from 2026-07-06 are traceable: 487 AD/DA/WOLA frames in 10 s, 391/391 learning hops, ready=1, WOLA+early denoise MaxMs=3.512 ms, CPU=17%. They are historical measurements, not current-code replacements.
- Historical 2026-07-08 mode 8 / 240 kbps reference: MaxMs=15 ms, CPU=76%, learning=391/391, ready=1, estimated bitrate=233 kbps, clamp ratio=0.410%, invalid_count=0. It must be re-run after continuous input is restored.

## Required Rerun Condition

Keep a continuous, fixed-amplitude CH1 source active for the complete test sequence. For the full-chain case, feed noise-only for the first 2 s and speech plus stationary noise afterwards. Then re-run `tmp/dss_board_missing_tests.js`; it will produce three repetitions per requested condition.

## Generated Files

- `docs/eval_outputs/board_missing_tests.csv`
- `docs/eval_outputs/board_missing_tests_raw.csv`
- `docs/eval_outputs/plots/board_runtime_compare.png`
- `docs/eval_outputs/plots/codec_bitrate_compare.png`
- `docs/eval_outputs/plots/codec_band_bits_compare.png`
