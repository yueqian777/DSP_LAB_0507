# Project 3.3 Final Internal-Digital Metrics

## Provenance

- Source commit: `b64282660e686a79eb4ba838f5918f0b18dec2fb`
- Target: TMS320C6748 with XDS100v3
- Sample rate and frame length: 50000 Hz, 1024 samples
- Dedicated `.out` SHA256: `78520eff999ba604f5adef5498944c4814f3bdeae28a0bfc81e51457976744a3`
- Build state: clean `main`, `dirty=0`, warning count 0, link errors `0x0`
- Measurement scope: static ten-band RBJ EQ only; ADC, DAC analog, LCD, Touch,
  Analyzer, Smart Bass, Dynamic Clarity, and HF Guard were excluded.

## Results

| Check | Result |
| --- | --- |
| Completed board cases | 60 / 60 |
| Normal-mode clipping | 0 |
| FLAT impulse | PCM16 byte-exact identity |
| Impulse amplitude | 29490 PCM16, -0.915238 dBFS |
| Magnitude error P95 / Max | 0.017883 / 0.077579 dB, limit 0.15 dB |
| Group-delay error P95 / Max | 0.517325 / 12.176229 samples, reported without a fabricated limit |
| THD | 30 measurable cases passed the -60 dB limit; worst -82.025591 dB |
| High-frequency THD boundary | 6 cases at 15.966796875 kHz marked `NOT_MEASURABLE_NO_IN_BAND_HARMONICS` |
| Reference SNR | minimum finite 54.269450 dB; all FLAT signals byte-exact |
| Maximum EQ frame cycles | 1407106 / 9338880, ratio 0.150672 |
| Cycle capture | continuous target run to `SWBP`, with no periodic DSS halt |
| Metric state placement | `0x00800000`, `DSPL2RAM/.subband_l2` |
| Project 3.2 isolated clean build | warning 0, link errors `0x0` |
| Restored Project 3.3 H build | warning 0, link errors `0x0`, `RUNNING_DISCONNECTED` |

Host verification after the board run completed 159 Project 3.3 Python tests with
`OK`; the original algo-only evaluator reported
`EqualizerEval_OfflineTest_All failures=0`.

## Evidence Boundary

- `HOST_MODEL`: high-precision response and PCM16 reference calculations.
- `BOARD_INTERNAL_DIGITAL`: JTAG capture before DAC analog output.
- `BOARD_REALTIME_COUNTER`: uninterrupted TSCL frame-cycle maximum.
- `OPERATOR_VISUAL_OR_LISTENING`: not performed for this exact build.

The following remain `NOT_MEASURED`: external analog THD, external analog SNR,
external analog frequency response, SPL, and ADC ENOB.

RAW captures, `.out`, map, link XML, and complete logs are not committed. Their
sizes and SHA256 values are recorded in `external_file_inventory.json`.
