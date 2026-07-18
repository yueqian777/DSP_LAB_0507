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
Commit `47337a0` completed objective A-H validation on the current board; the
exact evidence and its limits are recorded in
`equalizer_33_hardware_validation.md`. Human-ear judgments and external analog
THD, SNR, calibrated frequency response, and SPL remain unmeasured.

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

## Cycle Peak Diagnostic Closure

The 2026-07-17 diagnostic work did not change the 400 Hz filter, seven gain
levels, control thresholds, 80 ms transition, Analyzer, Smart Bass, preset,
headroom, or board I/O. The additional timing, transition-capture, and
microbenchmark paths all default to compile-time OFF.

The cycle peak is classified as
`C_INTERRUPT_OR_PREEMPTION_INCLUDED`. In the isolated C6000 benchmark,
Dynamic Clarity and Smart Bass had essentially the same warm distributions.
For the fixed music-like input:

| Module/case | Min | Median | P95 | P99 | Max | First | Warm max |
|---|---:|---:|---:|---:|---:|---:|---:|
| Clarity identity | 595 | 635 | 687 | 703 | 737 | 660 | 708 |
| Clarity stable 1 | 152401 | 152573 | 152617 | 152651.1 | 152763 | 154054 | 152736 |
| Clarity stable 4 | 152401 | 152573 | 152615 | 152653 | 152755 | 152860 | 152688 |
| Clarity 0 to 1 | 293331 | 308909 | 309640.25 | 309892.4 | 310291 | 309282 | 309681 |
| Clarity 1 to 2 | 317771 | 335535 | 336005 | 336177.2 | 336505 | 336160 | 336031 |
| Clarity 4 to 3 | 317567 | 335541.5 | 335991.25 | 336157 | 336441 | 336412 | 336180 |
| Clarity 1 to 0 | 287941 | 304407 | 304559 | 304592 | 304623 | 304842 | 304606 |
| Smart identity | 594 | 604 | 692 | 702 | 732 | 744 | 691 |
| Smart stable 1 | 152408 | 152574 | 152618 | 152668 | 152780 | 153474 | 152651 |
| Smart stable 4 | 152408 | 152574 | 152616 | 152668 | 152800 | 153442 | 152683 |
| Smart 0 to 1 | 293388 | 308886 | 309638.5 | 309885.05 | 310219 | 310324 | 310125 |
| Smart 1 to 2 | 317772 | 335543.5 | 335998 | 336144.15 | 336386 | 336180 | 336335 |
| Smart 4 to 3 | 317702 | 335552 | 336000 | 336170.1 | 336424 | 336792 | 336069 |
| Smart 1 to 0 | 287928 | 304411 | 304560 | 304592 | 304636 | 305428 | 304607 |

Across all four inputs and seven cases, the largest Dynamic-to-Smart P99
ratio was `1.007143`. The largest first call and warm maximum were comparable,
so the data does not support `TRUE_COMPUTE_COST` or
`COLD_CACHE_OR_FIRST_CALL`.

The real-time mode-tagged run produced the following maxima:

| Entry state | Max cycles | FLAG_AD | FLAG_DA |
|---|---:|---:|---:|
| Identity | 2199 | 0 | 1 |
| Stable filter | 253122 | 0 | 1 |
| 0 to filter | 2183186 | 0 | 1 |
| Filter to filter | 2190175 | 0 | 1 |
| Filter to 0 | 2183180 | 0 | 1 |

The same approximately 2.19 million-cycle transition peaks reproduced during
the corrected uninterrupted run, when there was no Watch, RAW, periodic UART,
or intermediate DSS access. The isolated maximum was `336505` cycles, while
the real-time function maximum was `2190175` cycles and every path maximum
coincided with `FLAG_DA=1`. This supports interrupt or audio-service
preemption being included in the TSCL interval, rather than additional filter
computation or a debugger-only artifact.

## Objective Transition And Runtime Evidence

The board-internal PCM16 transition test used deterministic dual-tone,
music-like, and 1024-sample-period pseudorandom inputs. All nine matched
transition captures passed digital integrity. The minimum matched-input
correlation was `0.966769`; the largest residual peak and RMS were
`-33.356119 dBFS` and `-46.877868 dBFS`; the largest first- and second-
difference peaks were `-30.615934 dBFS` and `-25.845146 dBFS`. The largest
4-20 kHz boundary-to-internal ratio was `0.857015 dB`. Clipped samples and
repeated adjacent frames were both zero. These are objective internal digital
metrics, not a listening result or an analog-output measurement.

The corrected continuous run used a 303-second host window. Host monotonic
elapsed time was `303.0231563 s`; 14764 processed frames correspond to
`302.36672 s` of DSP audio at 50 kHz and 1024 samples per frame. Analyzer,
Smart Bass, and Dynamic Clarity decision counts each advanced from 36 to
1882. There was no debugger access during the window, and all deadline,
latency, overlap, dropped, clip, saturation, and nonfinite counters were zero.

Compact evidence is committed under
`docs/evidence/dynamic_clarity_47337a0/`. Large WAV, RAW, map, and output
files remain under the `%TEMP%` paths indexed by `sha256_manifest.txt`.
Subjective listening remains `NOT_PERFORMED`; external analog THD, SNR,
calibrated frequency response, and SPL remain `UNMEASURED`.
