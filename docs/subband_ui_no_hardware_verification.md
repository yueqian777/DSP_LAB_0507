# Project 3.2 Verification Without Hardware

This revision was checked through source contracts, an executed host C
state-machine program, and the available CCS Project 3.2 Debug configuration.
No C6748 board or touch screen was used.

## Automated Results

- Python contract suite: 24 tests, 24 passed, 0 failed, 0 skipped.
- Host C scheduler test: executed with MSYS2 MinGW GCC, passed.
- Host scheduler scenarios: 5 of 5 passed.
- Scenario cancellation counts: normal=0, pending digit then Mode 0=1,
  direct ready=0, zero remaining=0, simultaneous state/digit=1.
- CCS clean build: passed.
- CCS link errors: `0x0`.
- PRU assembler diagnostics: 0 errors, 0 warnings.
- `git ls-files graphify-out`: no output after deletion.

The host compiler lookup checks `gcc`, `cc`, and `clang` on `PATH`, then common
MSYS2 GCC paths on Windows, then MSYS2 bash. A machine with no usable host C
compiler skips the host test with `HOST_C_COMPILER_UNAVAILABLE` instead of
failing all Python contracts.

## Build And Size Check

Baseline is HEAD `64c330d`, built with the same CCS Project 3.2 Debug
configuration before the source edits.

| Section | Baseline bytes | Current bytes | Delta bytes |
|---|---:|---:|---:|
| `.text` | 121024 | 121216 | +192 |
| `.const` | 14632 | 14632 | 0 |
| `.cinit` | 2784 | 2784 | 0 |
| `.bss` | 43 | 43 | 0 |
| `.neardata` | 872 | 876 | +4 |
| `.far` | 19748 | 19748 | 0 |
| `.fardata` | 2013 | 2013 | 0 |
| `.stack` | 2048 | 2048 | 0 |

The five-scenario scheduler model is protected by `SUBBAND_UI_HOST_TEST` and is
not present in the TI image. The board delta comes from the production zero
guard, precomputed remaining-value handoff, and one 4-byte debug counter.

## Board-Only Conclusions

The following remain `NOT_MEASURED_BOARD_UNAVAILABLE`:

- Audible distortion, clicks, silence, or dropouts.
- Real touch hit accuracy and one-touch activation.
- Real LCD visibility, colors, and absence of transient `0 s`.
- AD/DA/algo frame continuity during LCD updates.
- Actual LCD, chain, touch, full-state, digit, and algorithm cycle maxima.
- Whether any LCD transaction is below 2 ms.
- The 60-second Mode 8 / 320 kbps stability result.

The code now redraws a smaller region for a second-only transition and rejects
zero-second jobs, but this must not be reported as proof that audio distortion
is solved.
