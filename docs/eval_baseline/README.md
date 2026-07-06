# Subband Eval Baseline

This directory stores example PC algo-only evaluation reports for presentation
and comparison. These CSV files are not required by the DSP realtime runtime.

The normal generated files are `subband_eval_report.csv` and
`subband_codec_eval_report.csv` in the repository root. They are ignored by Git
so repeated local test runs do not create source changes.

To reproduce a baseline, place the WAV test vectors under:

```text
test_vectors/subband_eval_audio/
```

or override the path during a PC test build:

```text
-DSUBBAND_EVAL_AUDIO_DIR=\"D:/NDM/subband_eval_audio_50k_with_2s_noise\"
```
