# Project 3.3 Dynamic Clarity V1

## Scope

Dynamic Clarity is bounded content-aware low-mid de-masking. It is not speech
recognition, voice detection, VAD, source separation, denoising, or automatic
Presence boost.

The stage reads the existing pre-EQ Analyzer snapshot and computes:

```text
masking_db = Mud relative dB - Presence relative dB
```

It never changes the ten user EQ gains, preset, preamp, filter bank, builder,
mailbox, or control tokens. The audio chain is:

```text
ADC CH1 -> static 10-band EQ -> optional Smart Bass
        -> optional Dynamic Clarity -> DAC CH1
```

The Analyzer continues to observe the unprocessed `EQ_AD_Buffer1` frame.

## Compile And Runtime Gates

`EQ_ENABLE_DYNAMIC_CLARITY` defaults to `0`. Enabling it requires
`EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1`, but does not require Smart Bass. An OFF
build instantiates no Dynamic Clarity state or runtime filter path.

Runtime enable also defaults to zero. `EQ_DebugDynamicClarityEnabled` requests
enablement; the request becomes active only after the Analyzer is enabled and
a new valid snapshot has cleared the Analyzer-unavailable latch. Analyzer
disable, reset, service failure, or invalid data requests a smooth release.

## Control Contract

Attenuation is allowed only when all conditions hold:

- snapshot is valid and warm;
- RMS is at least `-45 dBFS`;
- Mud relative level is at least `0 dB`;
- Presence relative level is at least `-12 dB`;
- `masking_db` is greater than `4 dB`.

The mapping spans `4 dB` to `14 dB` and is clamped by runtime strength:

| Strength | Maximum level | Maximum cut |
|---|---:|---:|
| OFF | 0 | 0.0 dB |
| LOW | 2 | -1.0 dB |
| MEDIUM | 4 | -2.0 dB |
| HIGH | 6 | -3.0 dB |

The default strength is MEDIUM. Attack advances by at most one adjacent level
per new analysis. Release needs two supporting analyses per adjacent level.
Only the latest adjacent request is queued while an 80 ms transition is in
progress. Disable and strength reduction converge smoothly to the new bound.

## Filter And Identity

Seven 400 Hz, one-octave RBJ peaking coefficients are designed once during
initialization through `Equalizer_DesignRbjPeakingAt`. Levels are `0.0` through
`-3.0 dB` in `-0.5 dB` steps. No coefficient design runs per frame or sample.

Transitions crossfade independent active and pending DF2T paths for exactly
4000 samples at 50 kHz. Level zero is an exact direct path: no floating-point
filter is evaluated, an out-of-place call copies samples, and an in-place call
does not write them. Releasing to zero crossfades against direct input.

PCM16 conversion uses bounded rounding and saturation. Saturation and
nonfinite events have independent counters; a nonfinite filter result enters a
safe identity state.

## Independence From Smart Bass

Smart Bass and Dynamic Clarity consume the same immutable Analyzer snapshot,
but own separate coefficient tables, filter states, transitions, queues,
strength controls, reasons, and counters. Smart Bass uses Bass density and a
125 Hz low shelf. Dynamic Clarity uses Mud minus Presence and a 400 Hz peaking
cut. Neither module submits EQ control work.

## Evidence Boundary

Host response and control CSV files are generated only in a temporary
directory and are labeled `HOST_SIMULATED_CONTROL`. PCM16 chain tests are
labeled `HOST_DIGITAL_SIMULATION`. These labels are not board measurements or
analog THD evidence.

Board results require a clean, exact-SHA CCS build plus UART/Watch/RAW capture.
Until those steps are recorded in `equalizer_33_hardware_validation.md`, board
status remains `PENDING_HARDWARE`.

## Host Verification

The Host harness uses 1 kHz, 400 Hz, and 100 Hz sine inputs at approximately
`-18 dBFS`, plus a deterministic music-like multitone. Current verified
results are:

- 80 ms transition: 4000 samples;
- static one-octave peaking compatibility: 40 bit-exact coefficient cases;
- level 6 at 400 Hz: `-2.99999 dB`;
- maximum level-6 deviation at 100 Hz: `0.104896 dB`;
- maximum deviation at 8 kHz: `0.003212 dB`;
- response positive peak: `0.000000 dB`;
- both dynamics OFF: exact equality with the static EQ output;
- Dynamic Clarity OFF: exact equality with the Smart Bass output.

For the active Smart Bass plus active Dynamic Clarity PCM16 chain, the
intermediate rounding evaluation reports maximum error `1.005371 LSB`, RMS
error `0.411639 LSB`, residual `-98.018669 dBFS`, DC error `0.002443 LSB`, and
zero clipping. This is `HOST_DIGITAL_SIMULATION`, not analog or board THD.
