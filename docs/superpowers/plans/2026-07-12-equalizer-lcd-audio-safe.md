# Project 3.3 Audio-Safe LCD Scheduler Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Project 3.3 blocking LCD refreshes with one-job-per-audio-frame cooperative drawing, add fail-safe diagnostics and CH1 digital capture, and verify LCD-off/on behavior without changing the equalizer core or Project 3.2.

**Architecture:** `EqualizerDisplay_Request*()` stores the newest snapshot and sets independent dirty bits; `EqualizerDisplay_ServiceOneJob()` consumes one round-robin job and returns immediately. The flow services one job only after all AD/DA/frame guards pass, records draw timing and audio arrivals, and automatically disables runtime LCD work on safety faults. Manual and event-triggered CH1 input/output capture live in DDR2-backed `.far` arrays and execute inside measured active frame service.

**Tech Stack:** TI C6000 C89 board code, MSYS2 GCC C99 host tests, Python unittest and audio analysis, CCS 20.5 managed build/gmake, DSS JavaScript, PowerShell board-test wrapper.

**Execution rule:** Do not create another branch and do not make intermediate commits. Preserve the user's uncommitted `Code/main.c` Project 33 selector. After every required verification passes, stage only the intended Project 3.3 paths and make one final commit on `main`.

---

### Task 1: Lock the cooperative display contract with failing mock tests

**Files:**
- Modify: `Code/User/user_dsp/user_equalizer_display.c`
- Modify: `Code/User/user_dsp/user_equalizer_display.h`

- [ ] **Step 1: Add the desired public contract to the mock test first**

Add test calls under `EQUALIZER_DISPLAY_TEST_MAIN` for these interfaces before implementing them:

```c
#define EQ_LCD_RUNTIME_STATUS 0x01U
#define EQ_LCD_RUNTIME_GAINS  0x02U

void EqualizerDisplay_BeginRuntime(void);
void EqualizerDisplay_RequestGains(const EQ_STATE *st);
void EqualizerDisplay_RequestStatus(unsigned long frames,
                                    float algo_last_ms,
                                    float algo_max_ms,
                                    unsigned long clip_count,
                                    int requested_mode,
                                    int transition_target_mode,
                                    int applied_mode);
int EqualizerDisplay_HasPendingJob(void);
int EqualizerDisplay_ServiceOneJob(void);
void EqualizerDisplay_CancelRuntimeJobs(void);
void EqualizerDisplay_AutoDisable(unsigned long reason);
```

The test must assert:

```c
/* Ten changed bands require exactly ten jobs. */
EqualizerDisplay_RequestGains(&st);
for (job_count = 0; job_count < EQ_NUM_BANDS; job_count++) {
    before = s_mock_bar_slot_clear_count;
    job = EqualizerDisplay_ServiceOneJob();
    require(job >= EQ_LCD_JOB_BAND_0 && job <= EQ_LCD_JOB_BAND_9);
    require(s_mock_bar_slot_clear_count == before + 1UL);
}
require(EqualizerDisplay_HasPendingJob() == 0);

/* Five changed status fields require exactly five jobs. */
EqualizerDisplay_RequestStatus(25UL, 1.2f, 3.4f, 7UL,
                               EQ_PRESET_TREBLE_BOOST,
                               EQ_PRESET_NONE,
                               EQ_PRESET_TREBLE_BOOST);
for (job_count = 0; job_count < 5; job_count++) {
    require(EqualizerDisplay_ServiceOneJob() != EQ_LCD_JOB_NONE);
}
require(EqualizerDisplay_HasPendingJob() == 0);
```

Also test one-band-only clearing, no full-axis redraw, unchanged snapshots, new-request overwrite, 1000 mode transitions with stable bounds, runtime static-layout rejection, runtime-mask cancellation, and one-call auto-disable semantics.

- [ ] **Step 2: Run the RED display mock**

Run with MSYS2:

```sh
export PATH=/mingw64/bin:/usr/bin:$PATH
gcc -std=c99 -Wall -Wextra -Werror \
  -DEQ_ALGO_ONLY -DEQ_ENABLE_LCD_DISPLAY=1 \
  -DEQUALIZER_DISPLAY_TEST_MAIN \
  -ICode/User/user_dsp \
  Code/User/user_dsp/user_equalizer.c \
  Code/User/user_dsp/user_equalizer_display.c \
  -lm -o /tmp/equalizer_display_red.exe
```

Expected: compile failure only because the request/service APIs, job constants, and new diagnostics do not yet exist.

### Task 2: Implement display dirty masks, jobs, formatting, and fail-safe

**Files:**
- Modify: `Code/User/user_dsp/user_equalizer_display.c`
- Modify: `Code/User/user_dsp/user_equalizer_display.h`

- [ ] **Step 1: Define jobs, dirty bits, masks, and diagnostics**

Use explicit job IDs and safety reasons:

```c
#define EQ_LCD_JOB_NONE       0
#define EQ_LCD_JOB_MODE       1
#define EQ_LCD_JOB_FRAMES     2
#define EQ_LCD_JOB_ALGO_LAST  3
#define EQ_LCD_JOB_ALGO_MAX   4
#define EQ_LCD_JOB_CLIP       5
#define EQ_LCD_JOB_BAND_0     6
#define EQ_LCD_JOB_BAND_9     (EQ_LCD_JOB_BAND_0 + 9)

#define EQ_LCD_AUTO_DISABLE_JOB_OVER_5MS 0x01UL
#define EQ_LCD_AUTO_DISABLE_LATENCY_MISS 0x02UL
#define EQ_LCD_AUTO_DISABLE_OVERLAP      0x04UL
#define EQ_LCD_AUTO_DISABLE_DROPPED      0x08UL

#define EQ_LCD_JOB_TARGET_CYCLES 912000UL
#define EQ_LCD_JOB_HARD_CYCLES   2280000UL
```

Export the required Watch variables, including pending mask, job count, deferred/audio-arrival/full-redraw/budget counters, last/max cycles/ms, last job, auto-disable count/reason, and six category arrays for count/last/max cycles.

- [ ] **Step 2: Implement requested/displayed caches and round-robin selection**

Maintain requested and displayed values separately. Gains set only their changed band bits when the difference from displayed data is at least `0.05f`. Status fields compare independently. A newer request overwrites the requested snapshot without queueing an obsolete value.

Use a cursor that checks at most fifteen bits to select one job, but never draws more than one:

```c
for (checked = 0; checked < EQ_LCD_JOB_COUNT; checked++) {
    int index = (s_job_cursor + checked) % EQ_LCD_JOB_COUNT;
    if ((s_dirty_mask & (1UL << index)) != 0UL) {
        s_job_cursor = (index + 1) % EQ_LCD_JOB_COUNT;
        return EQ_ServiceSelectedJob(index + 1);
    }
}
return EQ_LCD_JOB_NONE;
```

Dirty indices are `0..14`; public job IDs are `1..15`. The conversion is
always `job_id = dirty_index + 1`, so Mode cannot alias NONE and Band 9 remains
reachable.

- [ ] **Step 3: Split static labels from dynamic values**

`EqualizerDisplay_Init()` initializes state and the LCD backend but does not draw twice. `EqualizerDisplay_DrawStaticLayout()` performs the only full-screen clear and draws fixed Chinese labels, chart border, full zero axis, and band labels before audio starts. `EqualizerDisplay_BeginRuntime()` locks the static layout.

After runtime begins, `DrawStaticLayout()` increments `EQ_DebugLcdUnexpectedFullRedrawCount` and returns without drawing. Legacy `UpdateAll`, `UpdateGains`, and `UpdateStatus` must likewise become request-only or reject the call without any drawing primitive; test all four entry points after `BeginRuntime()`.

Runtime mask zero means static page only. Track the previous mask: a rising
STATUS or GAINS bit marks the current snapshot for that class dirty so Watch
enablement produces visible values; a falling bit cancels that class. Test
`0->STATUS`, `0->GAINS`, and `BOTH->0` explicitly.

- [ ] **Step 4: Implement one fixed region per job**

Band jobs use:

```c
slot_x = EQ_BAR_AREA_X + band * EQ_BAR_PITCH;
EQ_LcdClearRect(slot_x, EQ_BAR_AREA_Y, EQ_BAR_SLOT_W, EQ_BAR_SLOT_H);
EQ_LcdDrawLine(slot_x, EQ_BAR_ZERO_Y,
               slot_x + EQ_BAR_SLOT_W - 1, EQ_BAR_ZERO_Y,
               EQ_COLOR_AXIS);
EQ_RestoreLocalChartBorder(band, slot_x);
EQ_DrawBandBarOnly(band, requested_gain_db);
```

Clamp the lowest bar y to `EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 1`. Restore only
the chart-border segments erased by that slot clear, including the left border
for band 0 and any top/bottom overlap. Do not call `EQ_DrawAxes()` from a band
job.

Status jobs clear only their dynamic value rectangle. Draw the Mode value from applied and transition target state:

```text
steady:      BASS
transition:  BASS>VOCAL
dry return:  DRY>VOCAL
```

Do not show queued requested mode as applied.

- [ ] **Step 5: Replace runtime sprintf with integer append helpers**

Implement bounded ASCII helpers for unsigned integers and tenths of milliseconds. Request stores `algo_last_tenths` and `algo_max_tenths`; service formats one field only. Clamp NaN, infinity, negative, and values above the representable/format limit before conversion. Do not use `%f`, dynamic allocation, or multiple-field formatting.

- [ ] **Step 6: Add job timing and automatic disable**

Measure one service call with TSCL. Update total and category diagnostics. Above 2 ms increments budget exceeded. Above 5 ms calls:

```c
EqualizerDisplay_AutoDisable(EQ_LCD_AUTO_DISABLE_JOB_OVER_5MS);
```

Auto-disable increments its count only when a nonzero mask transitions to zero, ORs the reason, clears runtime dirty work, and leaves the static page intact.

- [ ] **Step 7: Run the GREEN display mock**

Run the Task 1 command. Expected: `equalizer_display_mock PASS`, warning count zero, every service call consumes at most one job, bounds failures zero, and runtime full redraw count zero.

### Task 3: Lock and implement the flow scheduling contract

**Files:**
- Modify: `tools/tests/test_equalizer_flow_contract.py`
- Modify: `Code/User/user_dsp/user_equalizer_flow.c`
- Modify: `Code/User/user_dsp/user_equalizer_flow.h`

- [ ] **Step 1: Add failing source-level flow contracts**

Add tests that require:

```python
required_guard = [
    "FLAG_AD == 0", "FLAG_DA == 0", "flag_ad_done == 0",
    "EQ_FrameServicePending == 0U",
    "EQ_DebugProcessFrames != EQ_LastLcdServiceFrame",
]
```

The service block must contain exactly one `EqualizerDisplay_ServiceOneJob();`, update `EQ_LastLcdServiceFrame` only for a non-NONE job, check audio state after drawing, and execute `continue;`. It must not contain `UpdateGains`, `UpdateStatus`, `UpdateAll`, or `DrawStaticLayout`.

Add contracts for `EQ_DebugFrameLatencyDeadlineMissCount`, auto-disable on increased latency/overlap/dropped counters, and the documented behavior that UI stops when process frames stop. Add a host-testable service policy proving the same processed-frame value can consume at most one job, frozen frames consume none, each audio guard blocks service, and the next completed frame permits exactly one job. Add a cadence test proving frames 1 through 24 create no periodic status job and frame 25 does.

- [ ] **Step 2: Run RED flow contracts**

```powershell
D:\SoftwareDownload\python.exe -B -m unittest tools.tests.test_equalizer_flow_contract -v
```

Expected: failures for missing one-job service, frame-pending guard, latency miss counter, and auto-disable checks.

- [ ] **Step 3: Replace runtime direct drawing with request and service**

Keep request generation inside the existing audio-idle block. Mode changes
request gains plus the affected mode/status fields immediately. Periodic
status is requested only after `EQ_LCD_REFRESH_FRAMES` additional processed
frames, preserving the existing roughly 0.5 second cadence; changing Frames
must not create a job every audio frame. Request calls are LCD-free.

Service only when every guard is true and a dirty job exists. Sample AD, DA, `flag_ad_done`, and frame-pending immediately before and after drawing. A nonzero post-draw audio mask increments `EQ_DebugLcdAudioArrivedDuringDrawCount` and can notify trigger capture.

After a real job:

```c
EQ_LastLcdServiceFrame = EQ_DebugProcessFrames;
continue;
```

- [ ] **Step 4: Add latency deadline and fail-safe monitoring**

At frame completion, compare both active service and accepted-AD-to-DAC latency with `EQ_FRAME_SERVICE_BUDGET_CYCLES`. Increment `EQ_DebugFrameLatencyDeadlineMissCount` only for latency.

Track prior latency-miss, overlap, and dropped counters. If any increases, OR the corresponding reason and call `EqualizerDisplay_AutoDisable(reason)` without stopping ADC or DAC.

Test each cause through the monitoring path, including injected job cycles over
5 ms, simultaneous reason OR-ing, one count per enabled-to-disabled transition,
dirty cancellation, and baseline refresh while disabled so stale faults do not
disable a later Watch re-enable.

- [ ] **Step 5: Count deferred display work once per frame**

When a job is pending but audio state prevents service, increment `EQ_DebugLcdDeferredAudioCount` only if `EQ_DebugProcessFrames` differs from a `last_deferred_frame` latch.

- [ ] **Step 6: Initialize the page once before audio**

Before `Adc_Start()` and `Dac_Start()`:

```c
EqualizerDisplay_Init();
EqualizerDisplay_DrawStaticLayout();
EqualizerDisplay_BeginRuntime();
```

With the default runtime mask zero, startup draws only the static page. Dynamic
values are requested when Watch enables STATUS or GAINS; runtime
`ServiceOneJob()` remains one job per completed frame.

- [ ] **Step 7: Run GREEN flow contracts and display mock**

Expected: all flow tests pass and display mock remains green.

### Task 4: Add manual and event-triggered CH1 digital capture

**Files:**
- Create: `tools/tests/equalizer_capture_state_test.c.host`
- Modify: `Code/User/user_dsp/user_equalizer_flow.c`
- Modify: `Code/User/user_dsp/user_equalizer_flow.h`

- [ ] **Step 1: Write the failing capture state test**

The host test calls exported capture helpers with frames whose samples encode the frame number. Test:

- manual request captures exactly eight frames and refuses overwrite;
- LCD-job, mode-switch, and audio-during-draw triggers each preserve four preceding frames and eight following frames;
- a trigger is ignored while capture is active or ready;
- the saved pre-ring index reconstructs chronological order;
- capture is inactive by default.

Compile:

```sh
export PATH=/mingw64/bin:/usr/bin:$PATH
gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY \
  -ICode/User/user_dsp \
  Code/User/user_dsp/user_equalizer_flow.c \
  -x c tools/tests/equalizer_capture_state_test.c.host -x none \
  -o /tmp/equalizer_capture_red.exe
```

Expected: compile failure for missing capture arrays, controls, and helpers.

- [ ] **Step 2: Define capture constants and Watch variables**

Use:

```c
#define EQ_CAPTURE_FRAMES 8
#define EQ_CAPTURE_SAMPLES (EQ_CAPTURE_FRAMES * EQ_FRAME_LEN)
#define EQ_TRIGGER_PRE_FRAMES 4
#define EQ_TRIGGER_POST_FRAMES 8
#define EQ_TRIGGER_TOTAL_FRAMES 12

#define EQ_CAPTURE_TRIGGER_LCD_JOB          0x01U
#define EQ_CAPTURE_TRIGGER_MODE_SWITCH      0x02U
#define EQ_CAPTURE_TRIGGER_AUDIO_DURING_LCD 0x04U
```

Export manual request/active/ready/index/frame count and trigger request/active/triggered/ready/source/pre-write-index/post-count variables.

Define explicit ready acknowledgement/reset controls and deterministic
manual-versus-trigger request priority. A new one-shot is accepted only after
the prior ready result is acknowledged; active or ready data is never
overwritten.

- [ ] **Step 3: Place arrays explicitly in DDR2-backed `.far`**

For TI builds:

```c
#pragma DATA_SECTION(EQ_CaptureInput, ".far")
#pragma DATA_SECTION(EQ_CaptureOutput, ".far")
#pragma DATA_SECTION(EQ_TriggerCaptureInput, ".far")
#pragma DATA_SECTION(EQ_TriggerCaptureOutput, ".far")
```

The existing `TargetConfig/C6748.cmd` maps `.far` to DDR2. Do not modify linker files.

- [ ] **Step 4: Implement frame and event helpers**

`EqualizerCapture_OnFrame(input, output)` handles manual capture, the four-frame circular pre-buffer, and eight post frames. `EqualizerCapture_NotifyEvent(source)` freezes the current pre-ring only when the armed source matches and no capture is active/ready.

Every event notification must execute before the next `OnFrame`; notify the
audio-during-draw event immediately after the post-draw audio snapshot. Add an
ordering test proving the first post-trigger frame is not retained as
pre-trigger history.

Maintain a prefill count only while trigger capture is armed. An event before
four complete pre-frames is ignored/deferred and cannot claim a 4+8 capture;
DSS waits for armed-ready before inducing the event. Test early events for all
three trigger sources. Capture remains disabled by default, so there is no
continuous pre-ring memcpy when unarmed.

Call `OnFrame` after `Equalizer_ProcessFrame()` and before `EQ_EndFrameActiveSegment()`. Therefore every capture memcpy contributes to active service cycles.

Notify events after one LCD job, when `EQ_ServiceMode()` accepts a real mode change, and when post-draw audio state is nonzero.

An accepted mode switch means the equalizer core's mode-change count actually
advanced. A queued request during an active transition is not accepted/applied
and must not be displayed as applied; test `APPLIED>TARGET` while a newer
queued request exists. Keep the capture state machine host-compilable under
`EQ_ALGO_ONLY`, or isolate it in a board-independent module, so the documented
host command links without ADC/DAC/LCD dependencies.

- [ ] **Step 5: Run GREEN capture tests and flow contracts**

Expected: capture test exits zero, flow contract confirms capture call is inside the active segment, and warning count is zero.

### Task 5: Add capture export analysis and Project 3.3 DSS tooling

**Files:**
- Create: `tools/equalizer_33_capture.py`
- Create: `tools/dss/dss_equalizer_lcd_audio_safe.js`
- Create: `tools/run_equalizer_lcd_audio_safe_test.ps1`
- Create: `tools/tests/test_equalizer_lcd_tooling.py`

- [ ] **Step 1: Write failing tooling tests**

Assert the capture tool reads little-endian signed 16-bit mono without normalization, emits 50 kHz WAV, reorders the four-frame trigger ring, and reports mismatch count, peaks, clipping, correlation, SNR, boundary delta, and spectrum CSV.

Assert the DSS script reads the required `EQ_Debug*` fields, supports runtime masks 0/STATUS/GAINS/BOTH, writes commit SHA and measurement status, and does not read CH2 capture arrays.

Assert capture export waits for ready, halts the target, exports only the CH1
input/output arrays as signed little-endian 16-bit raw data, and records source,
frame counts, pre-write index, and trigger metadata for the analyzer.

Test bounded timeout behavior: a trigger such as audio-during-draw may remain
`NOT_OBSERVED`; the script must not hang or convert absence into PASS.

- [ ] **Step 2: Run RED tooling tests**

```powershell
D:\SoftwareDownload\python.exe -B -m unittest tools.tests.test_equalizer_lcd_tooling -v
```

Expected: failure because the capture analyzer and Project 3.3 DSS files do not exist.

- [ ] **Step 3: Implement the capture analyzer**

Provide subcommands for manual and trigger captures. Output WAV without resampling, normalization, or gain changes. For trigger capture, rotate the first four frames using the exported pre-write index, then append frames 4 through 11.

- [ ] **Step 4: Implement the DSS snapshot matrix**

Reuse the existing load/run/halt/evaluate pattern. Each 60-second row records frame, timing, LCD, auto-disable, capture, clip, and mismatch variables. Tests requiring 5-second or 2-second operator mode changes are labeled `MEASURED_DSS_WATCH_PLUS_OPERATOR`; the script does not claim uninterrupted mode injection if it must halt the target.

For capture rows, arm the request, wait for ready with a bounded timeout, halt,
export the arrays and metadata, and pass those files to the capture analyzer.

- [ ] **Step 5: Implement the PowerShell wrapper**

Generate deterministic 1 kHz -18 dBFS and music-like 50 kHz WAV inputs, start PC playback, run the DSS script, and always stop the player in `finally`. Do not require or use an external recording input.

Generate audio at least as long as each measurement row or loop it for the
entire DSS interval. Verify playback remains active through the 60-second and
five-minute windows.

The wrapper may open only the PC default line-output playback path. Add a
source contract forbidding recording/input-device APIs, and keep loudspeaker
observations explicitly subjective in the report.

- [ ] **Step 6: Run GREEN tooling tests**

Expected: all tooling tests pass and no `__pycache__` is staged.

### Task 6: Run complete host and build regression

**Files:**
- Modify: `docs/equalizer_33_lcd_audio_safe.md`

- [ ] **Step 1: Run strict display, capture, and flow tests**

Run Tasks 2 through 5 test commands from clean temporary output directories. Expected: warning count zero and all tests green.

- [ ] **Step 2: Run the complete equalizer host suite**

```sh
export PATH=/mingw64/bin:/usr/bin:$PATH
gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY -DEQUALIZER_TEST_MAIN \
  -ICode/User/user_dsp \
  Code/User/user_dsp/user_equalizer.c \
  Code/User/user_dsp/user_equalizer_eval.c \
  -lm -o /tmp/equalizer_test.exe
/tmp/equalizer_test.exe
```

Expected: `EqualizerEval_OfflineTest_All failures=0` and all CSV rows pass.

- [ ] **Step 3: Run Python reference and WAV A/B**

```powershell
D:\SoftwareDownload\python.exe tools\equalizer_33_reference.py `
  --coefficients <temp>\equalizer_coefficients.csv `
  --system-response <temp>\equalizer_system_response.csv `
  --output <temp>\equalizer_reference_comparison.csv
D:\SoftwareDownload\python.exe tools\equalizer_33_process_wav.py `
  --generate-vectors --output-dir <temp>\wav
```

Expected: zero failures, reference error below 0.01 dB, and WAV A/B has 72 passing rows.

- [ ] **Step 4: Build Project 32 and Project 33 LCD OFF/ON**

Use full rebuilds so macro changes cannot reuse stale `main.obj`:

```powershell
$gmake = 'D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\utils\bin\gmake.exe'
& $gmake -B -C Debug all 'GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=32 --define=EQ_ENABLE_LCD_DISPLAY=0'
& $gmake -B -C Debug all 'GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=33 --define=EQ_ENABLE_LCD_DISPLAY=0'
& $gmake -B -C Debug all 'GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=33 --define=EQ_ENABLE_LCD_DISPLAY=1'
```

After each build, require `link_errors=0x0`, fresh nonempty `.out/.map/linkInfo`, and verify the selected flow through `ofd6x --call_graph --xml`.

For Project 33 builds, also verify from the map or symbol dump that all four
non-static exported capture arrays lie in DDR2 (`0xC0000000` range), with 8192
signed shorts per manual array and 12288 signed shorts per trigger array.

- [ ] **Step 5: Update the validation document**

Document the old/new call chains, dirty bits, jobs, runtime masks, auto-disable reasons, capture layout/export, Watch variables, build results, board matrix, and `PENDING_DIAGNOSTIC` ISR boundary.

### Task 7: Run real-board LCD OFF/ON validation

**Files:**
- Modify: `docs/equalizer_33_lcd_audio_safe.md`

- [ ] **Step 1: Establish the LCD-off baseline**

Build Project 33 with LCD enabled but runtime mask zero. Load the program, play the 1 kHz -18 dBFS file from the PC into ADC CH1, run RAW_COPY and FLAT for 60 seconds each, and collect DSS snapshots plus one manual eight-frame capture.

- [ ] **Step 2: Run status-only and gains-only rows**

Run STATUS-only RAW_COPY/FLAT and GAINS-only preset switching for 60 seconds. Arm the next-LCD-job trigger capture. Record speaker observations separately from automated counters.

- [ ] **Step 3: Run combined and fast-switch rows**

Run STATUS+GAINS with 5-second and 2-second operator mode changes. Arm next-mode-switch and audio-during-draw trigger captures in separate runs. If auto-disable fires, stop that row and record the reason; do not re-enable until the cause is understood.

- [ ] **Step 4: Run the five-minute stability row**

Only after all 60-second rows pass, run combined fixed-mode operation for five minutes with music-like input.

- [ ] **Step 5: Apply the pass criteria**

Require frame-count differences at most one, zero active and latency deadline
misses, zero overlap/dropped, zero clipping, zero unexpected full redraw, no
job above 5 ms, and no sustained tearing, periodic clicks, or obvious switch
pops. Require zero sample mismatch only in RAW_COPY; FLAT and preset rows use
their algorithm/reference, spectrum, correlation, and timing criteria instead.
`AudioArrivedDuringDrawCount` is contextual and cannot alone fail a row as a
dropped frame.

The board wrapper permits the known uncommitted `Code/main.c` selector used for
hardware testing. It records HEAD, dirty paths, generated build ID, and the
actual compile-time selector instead of requiring a fully clean worktree.

### Task 8: Final scope review and single main commit

**Files:**
- Review all changed paths

- [ ] **Step 1: Run final repository checks**

```powershell
git diff --check
git diff --stat
git status --short
```

Verify no `user_equalizer.c`, preset, preamp, crossfade, `user_subband_*`, Project 3.2, Project 3.4, Touch, ADC/DAC processing, PRU, linker, sample-rate, frame-length, or ping-pong file changed. Preserve but do not stage the user's `Code/main.c` selector change.

- [ ] **Step 2: Request final code review**

Review scheduler correctness, one-job enforcement, timing semantics, capture chronology, external-memory placement, auto-disable behavior, board evidence, and documentation claims. Fix every Critical or Important finding and rerun affected tests.

- [ ] **Step 3: Stage only intended paths and commit on main**

Stage the Project 3.3 display/flow files, Project 3.3 tests/tooling, and the two documentation files. Do not stage `Code/main.c`, `__pycache__`, graphify output, generated binaries, CSV/WAV artifacts, or unrelated files.

Create one commit only after all required checks and board rows pass.
