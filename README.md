# DSP_LAB_0507

CCS 20.5 C6748 DSP course project collection.

## Project Selection

Set `DSP_LAB_PROJECT_SELECT` in `Code/main.c` before building:

| Value | Runtime |
| --- | --- |
| `32` | Project 3.2 subband analysis, denoise and codec loopback |
| `33` | Project 3.3 ten-band dynamic equalizer and LCD control |
| `34` | Project 3.4 BC-ResNet keyword recognition and music control |

The submitted default is Project 3.3 (`33`). Project 3.4 was integrated from
`ZitanGong/dsp_lab` at commit `9d29e9e0ecc254bc90d799a9e1c7d1393b3da6f7`.

## Project Layout

- `Code/User/user_dsp/`: Project 3.2, 3.3 and 3.4 application source
- `Code/Driver/`: shared C6748 board drivers
- `BuildSupport/`: reproducible build-fingerprint generation
- `Include/`: project headers
- `Library/`: board/device support files
- `TargetConfig/`: CCS target configuration
- `.ccsproject`, `.cproject`, `.project`: CCS project metadata

Generated CCS outputs, host test tools, measurement data and temporary reports
are intentionally excluded from the submitted source tree.
