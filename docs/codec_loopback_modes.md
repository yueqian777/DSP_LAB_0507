# DSP Codec Loopback Modes

This project now has three DSP demo modes for codec encode/decode loopback
listening tests:

- `SUBBAND_DebugDemoMode = 8`: MCRA denoise + codec loopback
- `SUBBAND_DebugDemoMode = 9`: strong MCRA denoise + codec loopback
- `SUBBAND_DebugDemoMode = 10`: fixed denoise + codec loopback

These modes insert `SubbandCodecLoopback_ProcessSpectrum()` inside the WOLA
frequency-domain path:

`FFT -> denoise -> codec quantize/dequantize loopback -> band gain -> IFFT -> OLA`

The codec loopback simulates compression damage before synthesis output. It is
for speaker listening verification only. It does not generate a communication
bitstream and does not implement Opus, AAC, MP3, G.722, UART, USB, or Ethernet
transport.

## CCS Watch Variables

- `SUBBAND_DebugDemoMode`: set to `8`, `9`, or `10` to select loopback mode.
- `SUBBAND_CODEC_LOOP_DebugCompressionLevel`: running-time compression level
  knob. Set to `1` for high quality/light compression (`320 kbps`), `2` for
  balanced/default compression (`240 kbps`), or `3` for strong compression
  (`160 kbps`). Invalid values are ignored and snapped back to the active
  level.
- `SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps`: set to `160`, `240`, or `320`
  to request a bitrate change while the mode is running. The request is
  automatically cleared to `0` after a valid target is applied. This one-shot
  kbps request takes priority over `SUBBAND_CODEC_LOOP_DebugCompressionLevel`
  for that WOLA hop.
- `SUBBAND_CODEC_LOOP_DebugTargetKbps`: active target bitrate.
- `SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps`: current estimated payload
  bitrate.
- `SUBBAND_CODEC_LOOP_DebugCompressionRatio`: estimated `800 kbps PCM / codec`
  ratio.
- `SUBBAND_CODEC_LOOP_DebugAvgBitsPerScalar`: current average scalar bits.
- `SUBBAND_CODEC_LOOP_DebugBandBits0` through
  `SUBBAND_CODEC_LOOP_DebugBandBits7`: current per-band scalar bit allocation.
- `SUBBAND_CODEC_LOOP_DebugInvalidCount`: NaN/Inf or invalid spectrum count.
- `SUBBAND_CODEC_LOOP_DebugQuantizerClampCount`: frequency-domain scalar
  quantizer clamp count. This is not output PCM waveform clipping.
- `SUBBAND_CODEC_LOOP_DebugTotalScalarCount`: cumulative frequency-domain
  scalar count used as the denominator for clamp ratio.
- `SUBBAND_CODEC_LOOP_DebugQuantizerClampRatio`: cumulative
  `quantizer_clamp_count / total_scalar_count`.
- `SUBBAND_CODEC_LOOP_DebugFrames`: processed WOLA hops with loopback enabled.

Modes `0` through `7` explicitly disable codec loopback and keep their previous
behavior.

`subband_codec_loopback_eval_report.csv` uses `quantizer_clamp_count` and
`quantizer_clamp_ratio` for the codec loopback path. These fields describe
frequency-domain quantizer saturation, not final speaker waveform clipping.
