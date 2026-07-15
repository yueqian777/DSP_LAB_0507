# Project 3.2 THD test plan

## Scope and evidence boundary

This test has two separate evidence layers:

1. **PC WOLA algorithm-level THD** measures deterministic mono PCM16 input,
   the production WOLA analysis/synthesis implementation, and mono PCM16 output.
2. **Board end-to-end THD** measures the signal source, ADC, Mode 1 WOLA,
   DAC, analog path, and capture device together.

PC results must not be described as ADC/DAC performance. Board rows remain
`NOT_MEASURED` until real captured WAV files are analyzed. No fixed THD
pass/fail threshold is assumed; input and output absolute values are reported.

## Audit of the existing path

- `Code/User/user_dsp/user_subband_wola.h` defines 50 kHz,
  `SUBBAND_FRAME_LEN=1024`, `SUBBAND_NFFT=512`, and `SUBBAND_HOP=256`.
- `SubbandWOLA_Init()` constructs a periodic sqrt-Hann window.
- `Subband_Process_1024()` selects `SubbandWOLA_ProcessFrame()` for the WOLA
  backend.
- Mode 1 (`SUBBAND_DEMO_MODE_WOLA_ONLY`) disables codec loopback and denoise,
  resets WOLA streaming state, and resets all eight band gains to `1.0`.
- `user_subband_audio_compare.c` is the existing Host WAV/evaluation example.
  The THD runner uses the same WOLA-only initialization sequence while calling
  the production WOLA source directly; it does not copy the DSP algorithm.

The Host THD wrapper also explicitly disables codec loopback and denoise and
resets every band gain to `1.0`. Project 3.3 EQ is not linked into this Host
binary. No UI code is linked or executed.

## Deterministic PC test

The five tones are 500, 1000, 3000, 4000, and 8000 Hz, each 10 seconds at
50 kHz, mono PCM16, -12 dBFS peak, with 50 ms cosine fade-in and fade-out.
The generator adds no noise.

Run from the repository root:

```powershell
C:\Python314\python.exe -m unittest tools.tests.test_thd_tools -v
C:\Python314\python.exe tools\thd\run_wola_thd_suite.py
```

The runner uses MSYS2 GCC (`C:\msys64\mingw64\bin\gcc.exe` by default) to
compile `tools/thd/wola_thd_host.c` together with the production WOLA, denoise,
and codec-loopback sources. Only their disabled WOLA-only path is exercised.

Generated evidence is under `results/thd/pc_wola/` and is intentionally
ignored by Git. Each frequency has input/output spectrum PNGs, harmonic CSVs,
metric JSON files, and the aligned WOLA output WAV. The root contains
`thd_summary.csv`, `thd_summary.md`, and `pc_wola_suite_metadata.json`.

## Tail, padding, and delay policy

WOLA has a fixed `NFFT-HOP=256` sample delay. A 10-second file contains
500000 samples, which is not an integer multiple of the 1024-sample outer
frame.

For an input of `N` samples, the Host wrapper processes

```text
ceil((N + 256) / 1024) * 1024
```

samples. Samples after `N` are zero, so this operation both pads the final
outer frame and flushes at least 256 WOLA tail samples. WOLA state is initialized
once and remains continuous across every outer frame. The full raw output keeps
startup and tail evidence. The published WAV drops the first 256 delayed
samples and saves the following `N` samples. The metadata records input count,
processed count, zero fill, delay, alignment slice, and saved output count.

THD analysis uses seconds 1 through 9 after this fixed-delay alignment. The
delay is therefore not treated as distortion.

## Metric definition

Pure THD uses the fitted RMS amplitudes of the fundamental and legal harmonics:

```text
THD = sqrt(V2^2 + ... + VN^2) / V1
```

The analyzer refines the actual fundamental near the requested value, then
fits DC plus sine/cosine terms for all harmonics simultaneously. THD+N is the
RMS residual after removing only DC and the fitted fundamental, divided by the
fundamental RMS. SINAD is `-20 log10(THD+N ratio)`.

H2 through H5 are requested. Orders at or above Nyquist are excluded; at
8 kHz and 50 kHz sampling, the report therefore uses only H2 and H3. Input
PCM16 quantization is measured as the input baseline rather than treated as
zero distortion.

## Board test prerequisites

Do not start a board test while another CCS, DSS, JTAG, or Codex task owns the
same DSP. Confirm the other task is complete, its DSS/debug processes have
exited, and the board is explicitly released before reset, load, or run.

Record before capture:

- commit SHA and clean/dirty state;
- Project 3.2 build flags and Mode 1;
- 50 kHz sample rate;
- signal source, interface/adapter, output level, and channel;
- capture device, channel, sample rate, and input gain;
- cable routing and test timestamp.

Keep the UI static. Do not touch the screen or run dynamic/high-frequency UI
refresh during recording. Confirm Mode 1, denoise off, codec loopback off,
all band gains `1.0`, and no EQ path.

## Board-executed JTAG digital path (no PC soundcard)

When use of the PC playback/recording device is prohibited, run the dedicated
compile-time harness:

```powershell
.\tools\run_wola_thd_board_suite.ps1
```

The runner requires a clean commit and matching generated build identity. It
builds Project 3.2 with `SUBBAND_THD_BOARD_TEST=1`; the C6748 generates and
captures the deterministic PCM16 stimulus, runs 489 consecutive frames through
the real C6748 `SubbandWOLA_ProcessFrame()` implementation, and exports both
input and output DDR captures through DSS/JTAG. The normal production build keeps
`SUBBAND_THD_BOARD_TEST=0`.

The digital board result is labelled
`MEASURED_BOARD_DIGITAL_NO_ADC_DAC_ANALOG`. It proves execution on the C6748
without using any Windows audio endpoint, but it intentionally bypasses the
ADC, DAC, analog filters, connectors, and cables. It must not be relabelled as
the full analog board result below.

## Signal-source/capture baseline

Use the same source and capture setup later used for the DSP path:

```text
signal source or high-quality audio output
-> capture device
-> baseline WAV
```

Start at -12 dBFS and confirm `clip_count=0`. Capture 10 seconds for every
frequency. Use names such as:

```text
board_baseline_1000Hz_run1.wav
```

At least three repeats per frequency are recommended for both baseline and
board paths so repeatability can be reported rather than inferred.

## DSP board path

After the baseline, change only the cable routing:

```text
same signal source
-> DSP ADC
-> Project 3.2 Mode 1 WOLA analysis/synthesis
-> DSP DAC
-> same capture device
-> board WAV
```

Use names such as:

```text
board_wola_1000Hz_run1.wav
board_wola_1000Hz_run2.wav
board_wola_1000Hz_run3.wav
```

Check for clipping, dropped frames, audible bursts, and unexpected mode/UI
changes. Do not tune WOLA, MCRA, codec, EQ, ADC/DAC, or UI parameters during
the measurement.

Analyze baseline and board WAV files with the same command and settings:

```powershell
C:\Python314\python.exe tools\thd\analyze_thd.py `
  board_baseline_1000Hz_run1.wav board_wola_1000Hz_run1.wav `
  --f0 1000 --start 1 --duration 8 --harmonics 5 `
  --out-dir results\thd\board\1000Hz_run1
```

Compare baseline and board measurements side by side. Do not subtract one THD
number from the other as if distortion powers were known to be independent.

## Board record fields

For each capture retain: filename, repeat number, commit SHA, build flags,
Mode, source level, source device, capture device/gain, sample rate,
fundamental estimate, THD ratio/percent/dB, THD+N ratio/percent/dB, SINAD,
fundamental RMS, harmonic order/amplitude table, peak dBFS, `clip_count`, and
operator notes.
