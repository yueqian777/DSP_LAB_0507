# Project 3.3 Smart Bass Attenuation V1

## Scope and evidence boundary

Smart Bass V1 is a bounded attenuation stage after the existing ten-band RBJ
equalizer. It never adds gain and never changes a preset, band gain, preamp,
filter bank, mailbox token, or custom-builder state. The Analyzer still reads
the EQ-before ADC CH1 frame.

Current implementation status:

- `HOST_VERIFIED`: C harness and Python contracts pass.
- `CCS_BUILD_VERIFIED`: Project 33 Analyzer + Smart Bass, LCD OFF, compiles and
  links with warning=0 and link errors=0.
- `MEASURED_ON_CURRENT_BOARD_SMART_BASS_EB2EB1F`: the exact clean feature
  output passed Smart Bass A-G validation on the current board.
- `MEASURED_ON_CURRENT_BOARD_A490E80`: the Analyzer-only prerequisite build is
  recorded separately in `equalizer_33_hardware_validation.md`.

The measured source commit is
`eb2eb1f0e6274f253ac8d5e94704369f30955e6c`. Validation-report commit
`45558565e8943889d11c2109813d88f3ab2a2f13` differs from that source commit
only in `docs/equalizer_33_hardware_validation.md` and this document. It does
not change the loaded DSP program. This closeout also leaves all source and
build macros unchanged.

Generated response/control CSV files are Host simulation artifacts and are
written only to a temporary directory. They are not board measurements and
are not committed.

## Signal chain

```text
ADC CH1
  -> ten-band RBJ equalizer and static preamp
  -> Smart Bass low-shelf attenuation
  -> bounded PCM16 conversion
  -> DAC CH1
```

The stage is compiled only when both flags are enabled:

```text
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=1
```

`EQ_ENABLE_SMART_BASS` defaults to 0. Enabling Smart Bass without the Analyzer
is a compile error. With Smart Bass OFF, the module object contains no
`SmartBass_*` runtime symbols, and Project 3.3 instantiates no Smart Bass state
or coefficient table.

## Fixed filter bank

Init calls the common, stateless `Equalizer_DesignRbjLowShelfAt` interface once
for each of seven levels. That interface reuses the same normalized RBJ shelf
math as the graphic equalizer and does not access EQ state or run a headroom
scan.

| Level | Low-shelf gain |
| ---: | ---: |
| 0 | 0.0 dB |
| 1 | -0.5 dB |
| 2 | -1.0 dB |
| 3 | -1.5 dB |
| 4 | -2.0 dB |
| 5 | -2.5 dB |
| 6 | -3.0 dB |

The shelf frequency is 125 Hz, slope is 1.0, and sample rate is 50 kHz. No
coefficient-design math runs per sample, frame, or Analyzer update. Host
response evaluation measures level 6 at -2.92242 dB at 50 Hz. Across the
evaluated grid, maximum positive response is about 0.000002 dB, and the 8 kHz
absolute deviation is about 0.000002 dB.

## Control law

The default runtime strength is MEDIUM, but runtime enabled defaults to zero.
Strength ceilings are OFF=level 0, LOW=level 2, MEDIUM=level 4, and HIGH=level
6. A new decision is made only when `analysis_count` changes.

- invalid, not-warm, or RMS below -45 dBFS requests release;
- Bass relative dB <=4 requests level 0;
- Bass relative dB >=16 requests the strength ceiling;
- values between 4 and 16 dB map linearly and quantize to 0.5 dB levels;
- attack advances at most one adjacent level per new analysis;
- release needs two consecutive supporting analyses per adjacent level;
- a running transition is never overwritten; one latest adjacent request may
  be queued for after it completes;
- lowering strength clamps both the current target and any queued attack to
  the new ceiling, then converges through adjacent transitions;
- explicit runtime disable finishes the current transition, then returns to
  level 0 through adjacent 80 ms transitions.

The active and pending low-shelf paths have independent DF2T state. A level
transition is a 4000-sample output crossfade. Stable level 0 does not run a
floating-point filter and copies samples exactly. PCM16 output uses bounded
rounding and saturation. A nonfinite filter result enters safe identity.

Runtime enable is gated by the Analyzer's applied state, not only its Watch
request. Analyzer reset or service failure latches the source unavailable and
starts a smooth release. The latch clears only when a new Analyzer snapshot is
published. Disabling also invalidates the previous analysis-count epoch, so a
post-reset count that restarts at the same numeric value is still consumed.

Reason values are `DISABLED`, `NOT_WARM`, `INVALID`, `LOW_LEVEL`,
`BELOW_THRESHOLD`, `EXCESS_BASS`, and `RELEASING`. They explain the most recent
decision without changing the Analyzer snapshot or the base EQ control state.

## Watch variables

`EQ_DebugSmartBassCompiled` is read-only and reports the build flag. Runtime
control uses `EQ_DebugSmartBassEnabled` and `EQ_DebugSmartBassStrength`.
Strength writes are normalized to OFF through HIGH.

The remaining Watch surface reports processing state, Analyzer input values,
requested/applied/pending levels and gains, transition state/progress, reason,
decision/level/transition/invalid-release counters, process cycles, saturation,
and nonfinite events. No UART RX, periodic telemetry, LCD page, or Touch action
is added.

## Host evidence

The dedicated harness covers initialization, three identity cases, all
strength ceilings, monotonic mapping, adjacent attack, two-confirmation
release, threshold debounce, latest-wins transition handling, smooth disable,
non-OFF strength-ceiling convergence, Analyzer count-epoch reset, frequency
response, 0.9FS impulse, invalid arguments, NaN handling, immutable input, and
deterministic output. It also compares a BASS-preset EQ-only chain against the
same chain followed by disabled Smart Bass and requires sample-exact output.
Current result is `smart_bass failures=0`.

The evaluator writes:

```text
smart_bass_response.csv
smart_bass_control.csv
```

The control trace includes a 0 to 20 dB rise, hold, fall, threshold jitter,
then a second rise to level 4 followed immediately by invalid/silent release.
Every response and control row is labeled `HOST_SIMULATED_CONTROL`.

The Python contract requires all seven response levels to contain the same 129
strictly increasing frequency points, level 0 to be identity, 50/125 Hz
attenuation to increase monotonically by level, high-frequency response to
remain near 0 dB, and the control trace to truly reach level 4 before release.
The threshold-jitter region must cross the level 0/1 quantization boundary but
may change the applied level at most once.

The exact committed-SHA A-E CCS matrix passed for commit `eb2eb1f`, with zero
compiler diagnostics and zero linker errors. Smart Bass A-G board validation
also passed on 2026-07-17 for the exact LCD-OFF feature build. Objective UART,
Watch, RAW, and stability evidence plus the separately labeled subjective
operator observation are recorded in `docs/equalizer_33_hardware_validation.md`.
External analog THD, SNR, calibrated frequency response, and SPL remain
`PENDING_HARDWARE_MEASUREMENT`.

The measured five-minute stability run is explicitly
`SEGMENTED_30X10S`: thirty 10-second DSP windows with a short JTAG snapshot at
each boundary. A no-halt `CONTINUOUS_300S` rerun is not claimed and is deferred
because it adds limited evidence after the segmented run and the separate
18-second no-halt operator window.

A short supplemental combined-transition stress run is also measured on
eb2eb1f. It observed ten-band FLAT-to-BASS and BASS-to-VOCAL transitions
overlapping Smart Bass adjacent-level transitions with every safety counter
remaining zero. It is objective debugger-polled evidence, not a continuous or
subjective audio-quality claim.
