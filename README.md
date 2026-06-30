# DSP_LAB_0507

CCS 20.5 C6748 DSP lab project.

## Current State

- Main entry: `Code/main.c`
- Normal runtime path: `Adda_Example()` for AD/DA audio passthrough
- DAC self-test path remains available through `RUN_DAC_SELF_TEST`

## Project Layout

- `Code/`: application and driver source
- `Include/`: project headers
- `Library/`: board/device support files
- `TargetConfig/`: CCS target configuration
- `.ccsproject`, `.cproject`, `.project`: CCS project metadata

Generated CCS build outputs under `Debug/` are intentionally ignored.
