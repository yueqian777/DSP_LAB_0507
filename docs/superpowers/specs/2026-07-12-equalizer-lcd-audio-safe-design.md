# Project 3.3 Audio-Safe LCD Scheduler Design

## Goal

Replace the blocking Project 3.3 LCD refresh path with an audio-first,
dirty-driven scheduler. Runtime drawing must perform at most one bounded LCD
job per completed audio frame. The RBJ equalizer, presets, automatic preamp,
crossfade, bypass behavior, sample rate, frame length, and ping-pong audio path
remain unchanged.

## Confirmed Hardware Boundary

- The PC audio output feeds DSP ADC CH1.
- DSP DAC CH1 feeds a loudspeaker for operator listening.
- ADC CH2 is not connected and is not used for capture.
- External analog recording is not permitted.
- Board-side digital capture stores CH1 equalizer input and digital algorithm
  output only. A clean digital output does not prove that the analog DAC output
  is clean.
- Actual-output conclusions require frame and LCD timing diagnostics plus
  loudspeaker observation. Exact ISR overwrite counters and ISR-to-DAC timing
  remain `PENDING_DIAGNOSTIC` unless they can be added without changing the
  common ADC driver or PRU program.

## Current Blocking Path

The current runtime path calls the drawing functions directly:

```text
Equalizer_Flow_Example
  -> EqualizerDisplay_UpdateGains
     -> draw all ten band slots
     -> redraw the full chart border and zero axis

Equalizer_Flow_Example
  -> EqualizerDisplay_UpdateStatus
     -> redraw Mode
     -> redraw Frames
     -> redraw Last
     -> redraw Max
     -> redraw Clip
```

With the default Chinese bitmap glyph path, one status refresh performs about
435 to 577 grlib primitives. One complete gain refresh performs about 22 grlib
primitives. A mode change can trigger gains and status back-to-back, and a
periodic status refresh can occur in the same idle pass. The outer audio-idle
check cannot preempt a drawing primitive after it starts.

## Architecture

Runtime display work is split into request and service phases:

```text
audio and mode service
  -> copy latest display snapshot
  -> set independent dirty bits
  -> round-robin select one job
  -> recheck all audio guards
  -> draw one fixed region
  -> record timing and post-draw audio flags
  -> return to the top of the main loop
```

Request functions perform no LCD operations. They only compare values, copy
the newest snapshot, and set dirty bits. A newer request replaces an older
undrawn snapshot.

The display module keeps separate requested and displayed caches. Values are
copied to the displayed cache only after their job completes.

## Dirty Bits And Jobs

The pending mask contains fifteen independent jobs:

- Mode
- Frames
- Algorithm last time
- Algorithm max time
- Clip count
- Band 0 through Band 9

The runtime mask is a Watch-writable variable:

```text
0                          static page only
EQ_LCD_RUNTIME_STATUS      status value jobs only
EQ_LCD_RUNTIME_GAINS       band jobs only
STATUS | GAINS             all cooperative runtime jobs
```

The default runtime mask is zero. Disabling a class cancels pending jobs in
that class.

A round-robin cursor selects the next dirty job so repeated Frames requests
cannot starve band jobs.

Mode requests carry requested, transition-target, and applied values. The
displayed Mode value follows the algorithm state rather than the latest knob:

- Steady state displays the applied mode.
- A transition displays `APPLIED>TARGET` or an equivalent explicit transition
  marker.
- A queued latest request is not displayed as applied before its bank becomes
  active.

## Drawing Granularity

Static layout is drawn once before `Adc_Start()` and `Dac_Start()`. It includes
the background, title, chart border, full zero axis, band labels, and fixed
status labels. After audio starts, a static-layout request increments
`EQ_DebugLcdUnexpectedFullRedrawCount` and returns without drawing.

Each band job:

1. Computes `slot_x = EQ_BAR_AREA_X + band * EQ_BAR_PITCH`.
2. Clears only that band's fixed slot.
3. Redraws only the local zero-axis segment inside the slot.
4. Restores any top, bottom, left, or right chart-border segment erased by the
   slot clear, without redrawing the full axes.
5. Draws one bar.
6. Updates the displayed gain cache.
7. Returns.

The bottom y coordinate is clamped to `EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 1` so
the -18 dB bar remains inside the slot.

Each status job clears and draws only the dynamic value sub-region for one
field. Fixed Chinese labels remain part of the static layout. Runtime values
use lightweight ASCII and integer formatting. Milliseconds are stored as
integer tenths; no floating-point printf, dynamic allocation, or unbounded
runtime formatting is used.
NaN, infinity, negative, and out-of-range timing inputs are clamped before any
float-to-integer conversion.

The initial design uses one complete band slot as one job. If a measured band
job exceeds 5 ms, it is split into clear-slot and draw-bar jobs. The blocking
ten-band refresh is never restored.

## Flow Scheduling

One LCD job may run only when all conditions are true:

```c
FLAG_AD == 0
FLAG_DA == 0
flag_ad_done == 0
EQ_FrameServicePending == 0
EQ_DebugProcessFrames != EQ_LastLcdServiceFrame
EqualizerDisplay_HasPendingJob() != 0
```

The flags are checked immediately before drawing. After one job returns,
`EQ_LastLcdServiceFrame` is updated and the loop immediately continues from
the top. No second job or other low-priority operation follows in the same
pass.

Audio flags and frame-pending state are sampled before and after the draw. If
audio work appears during the draw,
`EQ_DebugLcdAudioArrivedDuringDrawCount` increments. This indicates at least
one arrival; it is not an exact ISR event count.

`EQ_DebugLcdDeferredAudioCount` is counted at most once per processed frame
when a pending LCD job is deferred by audio work, avoiding inflation from a
busy-spin loop.

If ADC or DAC is stopped, `EQ_DebugProcessFrames` no longer advances and the
one-job-per-frame scheduler intentionally stops servicing runtime LCD jobs.
Pending jobs remain pending. The UI must not restart audio or modify the
driver to obtain display progress.

## Timing And Diagnostics

The existing algorithm time, active frame service time, and frame latency stay
separate. A new latency miss counter compares accepted-AD-to-DAC-buffer latency
against the 20.48 ms frame budget:

- `EQ_DebugDeadlineMissCount`: active ADC copy + EQ + DAC copy time.
- `EQ_DebugFrameLatencyDeadlineMissCount`: main-loop accepted AD event to DAC
  inactive-buffer completion.

LCD diagnostics include:

- pending mask and last job
- total job count
- deferred-by-audio count
- audio-arrived-during-draw count
- unexpected-full-redraw count
- budget-exceeded count
- last and max job cycles/ms
- category count/last/max for band, mode, frames, last, max, and clip jobs

The target is below 2 ms per job. Any job above 2 ms increments the budget
counter. Any job above 5 ms blocks completion until the job is split or its
primitive implementation is reduced.

Runtime refresh also has an automatic fail-safe. The display runtime mask is
set to zero when any of these events occurs:

- one LCD job exceeds 5 ms;
- the frame-latency deadline-miss count increases;
- the frame-service overlap count increases;
- the frame-service dropped count increases.

`EQ_DebugLcdAutoDisabledCount` increments once when a nonzero runtime mask is
automatically disabled. `EQ_DebugLcdAutoDisableReason` is a bit mask so
simultaneous causes are retained. Auto-disable cancels runtime dirty jobs but
does not stop audio and does not erase the startup static page.

`EQ_DebugLcdAudioArrivedDuringDrawCount` means that at least one audio event
was visible after a draw. It is not an ISR count and is not sufficient by
itself to claim a dropped frame.

Exact AD/DA ISR counts, flag-already-set counts, and ISR-to-DAC latency are not
implemented by modifying the common driver or PRU in this change. The AD flag
is written by an EDMA callback, while the DA flag is written by PRU firmware.
Those metrics are documented as `PENDING_DIAGNOSTIC` rather than approximated
with misleading names.

## Digital Capture

The manual one-shot capture records eight complete CH1 frames:

```text
EQ_CaptureInput[8192]   <- EQ_AD_Buffer1 after ADC copy
EQ_CaptureOutput[8192]  <- EQ_DA_Buffer1 after Equalizer_ProcessFrame
```

Watch variables request, track, and complete capture. A new request starts on
the next complete frame. The capture stops after eight frames, sets ready, and
does not overwrite old data. It performs no file I/O, printing, UART streaming,
allocation, resampling, normalization, or gain adjustment in the audio loop.

An independent event-triggered capture keeps four pre-trigger frames and eight
post-trigger frames for both CH1 input and digital algorithm output. Supported
one-shot triggers are:

- the next LCD job;
- the next accepted mode switch;
- the next audio-arrived-during-draw observation.

While armed, a four-frame circular pre-trigger buffer retains the newest
complete frames. When the selected event occurs, the trigger position is
frozen and the next eight complete frames are appended. A newer trigger does
not overwrite an active or ready capture. Trigger capture is disabled by
default.

All manual and trigger capture copies execute inside the measured frame active
service segments. The exported arrays are placed in a dedicated external
DDR/SDRAM linker section with sufficient capacity, not in constrained internal
memory. The expected storage is 32 KiB for the manual input/output pair and
48 KiB for the twelve-frame triggered input/output pair, excluding small
control metadata.

CCS/DSS debug capture exports signed little-endian 16-bit data after a bounded
run and one halt. It is labeled `DEBUG_CAPTURE` and cannot establish real-time
latency or frame-boundary continuity. A host tool converts it to
mono 50 kHz PCM WAV and reports RAW_COPY mismatch, peaks, clipping, correlation,
SNR, frame-boundary discontinuity, and spectrum. These results validate the
digital algorithm path only.

## Test Strategy

Test-driven implementation starts with failing host/mock contracts for:

1. Ten changed bands require ten service calls.
2. One changed band touches only its slot and never redraws full axes.
3. Five changed status fields require five service calls.
4. Unchanged requests create no dirty work.
5. A newer request replaces an undrawn snapshot.
6. One thousand mode changes keep coordinates stable and in bounds.
7. Runtime flow never services LCD while any audio guard is active.
8. Runtime service cannot invoke the static layout.
9. Manual capture starts, fills eight frames, stops, and never overwrites.
10. Trigger capture preserves four pre-event and eight post-event frames for
    LCD-job, mode-switch, and audio-during-draw triggers.
11. Capture copies are inside the active-service timing region and all capture
    arrays are assigned to external memory.
12. Auto-disable fires once for each defined safety cause, records the reason,
    clears runtime work, and leaves audio and the static page intact.

Regression includes the complete equalizer host suite, independent Python
reference, WAV A/B, Project 32 build, Project 33 LCD-off build, and Project 33
LCD-on build. `link_errors` must be zero and the source default remains Project
32. Project 3.3 is selected only through the build define
`DSP_LAB_PROJECT_SELECT=33`.

## Board Validation

The board matrix remains `PENDING_HARDWARE`. It runs LCD runtime mask 0, status only, gains only, and combined
status plus gains. Tests use a PC-generated 1 kHz -18 dBFS signal and music on
ADC CH1, with loudspeaker observation. Mode changes run at 5-second and
2-second intervals. Switching is manual or board-timed; debugger halt/run is
not used as a real-time switching method. Each matrix row runs for 60 seconds; a passing combined
fixed-mode row then runs for five minutes.

Passing requires zero active-service and frame-latency deadline misses, zero
overlap/dropped counts, zero clipping, zero unexpected full redraws, no LCD job
above 5 ms, and no sustained tearing, periodic clicks, or obvious switch pops.
Zero sample mismatch is required only for RAW_COPY; FLAT and preset rows are
judged against algorithm/reference and spectrum criteria. Frame counts should
differ by at most one.

Because external analog recording is forbidden, subjective loudspeaker
observations remain operator evidence. The final report must not claim that
the analog DAC waveform was objectively captured.

## Scope

Allowed changes are limited to Project 3.3 display and flow files, Project 3.3
host/mock/DSS/capture tooling, and the requested documentation. The equalizer
core, presets, preamp, crossfade, Project 3.2, Project 3.4, touch, sample rate,
frame length, and ping-pong behavior are not changed.
