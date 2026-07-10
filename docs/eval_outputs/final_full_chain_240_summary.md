# Final C6748 Full-Chain 240 kbps Rerun

- Measured commit: `62784775f9f2253652f935a67b9aa83e80e06d07`.
- Three repeats are valid according to frame, learning, codec, invalid-count, and real-time criteria.
- MaxMs: median 15.9700, worst 15.9769 ms; CPU: median 77.9786, worst 78.0123%.
- Estimated bitrate: median 233.9844, worst 233.9844 kbps; compression ratio: median 3.4190, worst 3.4219x.
- Clamp ratio: median 0.003176, worst 0.003178; invalid count worst: 0.
- Peak ratio: median 0.454936, worst 0.464363; RMS ratio: median 0.094451, worst 0.094567.
- Speech-probability change from the noise checkpoint: observed.
- Gain near MCRA floor: yes.
- Audio recording status: `NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE`; no audio-quality conclusion is made.

## Input-level diagnosis

- Generated source peak is 16384 LSB (-6 dBFS target); board input peak median is 932 LSB (-30.92 dBFS), only 5.69% of the source peak.
- Speech probability rises from 0.008261 at the noise checkpoint to 0.401329 after speech starts. The detector responds, but its steady-state average remains moderate.
- Gain average median is 0.085449; MCRA floor median is 0.082529. Gain is near the floor, and the board energy/RMS ratios show substantial attenuation.
- Therefore the low analog input level is a primary condition, and the active MCRA denoiser is the primary in-algorithm source of the low mode-8 output. This does not prove that MCRA parameters are intrinsically wrong; no parameter was changed in this diagnostic.
- Codec-only historical output was nonzero, while this mode adds MCRA before the same codec loopback. With invalid_count=0 and a valid bitrate here, the evidence points to denoise attenuation rather than a codec failure. That is a diagnosis, not an audio-quality claim.

## Final status

- Real-time status: PASS.
- Algorithm-state status: PASS (391/391 learning, ready=1, valid codec frames, invalid_count=0).
- Audio-quality status: pending. No board-output recording path was available, and the measured mode-8 output is strongly attenuated.

## Artifacts

- `docs/eval_outputs/final_full_chain_240_rerun.csv`
- `docs/eval_outputs/final_full_chain_240_diagnostics.csv`
- `docs/eval_outputs/final_full_chain_240_audio_metrics.csv`
- `docs/eval_outputs/dss_final_full_chain.log`
- `docs/eval_outputs/plots/final_full_chain_runtime.png`
- `docs/eval_outputs/plots/final_full_chain_gain.png`
- `docs/eval_outputs/plots/final_full_chain_input_output_level.png`
- `docs/eval_outputs/plots/final_full_chain_audio_metrics.png`
