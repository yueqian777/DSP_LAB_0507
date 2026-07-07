# DSP Subband Evaluation Visual Report

Generated at: 2026-07-07 15:05:45

## 输入文件列表

### CSV found
- compare_audio_metrics.csv: `compare_audio_metrics.csv`
- subband_codec_eval_report.csv: `subband_codec_eval_report.csv`
- subband_denoise_mcra_eval_report.csv: `subband_denoise_mcra_eval_report.csv`
- subband_denoise_ms_eval_report.csv: `subband_denoise_ms_eval_report.csv`
- subband_eval_report.csv: `subband_eval_report.csv`

### WAV found
- compare_00_input.wav: `compare_00_input.wav`
- compare_01_wola_only.wav: `compare_01_wola_only.wav`
- compare_02_wola_denoise.wav: `compare_02_wola_denoise.wav`
- compare_03_codec_direct_160k.wav: `compare_03_codec_direct_160k.wav`
- compare_04_codec_direct_240k.wav: `compare_04_codec_direct_240k.wav`
- compare_05_codec_direct_320k.wav: `compare_05_codec_direct_320k.wav`
- compare_06_denoise_codec_160k.wav: `compare_06_denoise_codec_160k.wav`
- compare_07_denoise_codec_240k.wav: `compare_07_denoise_codec_240k.wav`
- compare_08_denoise_codec_320k.wav: `compare_08_denoise_codec_320k.wav`

## 缺失文件列表

### CSV missing
- <none>

### WAV missing
- compare_09_ms_denoise.wav
- compare_10_mcra_denoise.wav
- compare_11_strong_mcra_denoise.wav

## 成功生成的图像列表
- plots/codec_actual_bitrate.png
- plots/codec_bitrate_compression_ratio.png
- plots/codec_quality_snr.png
- plots/codec_spectrogram_compare.png
- plots/denoise_delta_vs_fixed.png
- plots/denoise_snr_bar.png
- plots/denoise_spectrogram_compare.png
- plots/denoise_spectrogram_fixed.png
- plots/denoise_spectrogram_input.png
- plots/denoise_spectrum_before_after.png
- plots/denoise_speech_preservation_corr.png
- plots/denoise_waveform_before_after.png
- plots/group_delay_impulse_or_correlation.png
- plots/memory_usage_template.png
- plots/noise_psd_gain_tracking.png
- plots/realtime_budget_compare_template.png
- plots/wola_frequency_response.png
- plots/wola_reconstruction_error.png
- plots/wola_rolloff_curve.png
- plots/wola_spectrum_compare.png
- plots/wola_waveform_compare.png

## 未生成图像
- thd_spectrum_example.png: missing single-tone test WAV

## 汇总表
- summary_tables\wola_summary.csv
- summary_tables\denoise_summary.csv
- summary_tables\codec_summary.csv
- summary_tables\realtime_summary_template.csv
- summary_tables\memory_usage_summary.csv

## WOLA 基础结果摘要

WOLA passthrough SNR=72.86 dB, energy ratio=0.999732, delay=256 samples, pass=PASS.

## 降噪结果摘要

rows=34; best delta=2.66 dB (noise_step_up_snr10 / mcra_normal); best output SNR=22.64 dB (stationary_snr20 / fixed).

## Codec 结果摘要

target bitrates=160, 240, 320 kbps; max compression ratio=5.17; best denoise+codec SNR=19.20 dB.

## 实时性模板或结果摘要

- frame_budget_ms = 20.48 ms
- realtime_summary rows = 5
- 若 max_ms 为空或为 0，说明该表仍是模板，需要根据 CCS Watch 手动填入。

## 指标定义

- Aligned SNR: 对齐输入输出延迟后，SNR = 10 log10(sum(x^2) / sum((y - x)^2))。
- Energy Ratio: output_energy / input_energy。
- Delay: 通过互相关峰值或 CSV 中 WOLA delay 估计，单位 samples 和 ms。
- THD: sqrt(sum(harmonic_power)) / fundamental_power。
- SNR Improvement: output_snr_db - input_snr_db。
- Delta vs Fixed: mode_output_snr_db - fixed_output_snr_db。
- Speech Preservation Ratio: clean 与 output 的投影比例，或直接采用 CSV 字段。
- Clean-output correlation: clean 与 output 的相关系数。
- Compression Ratio: 原始 PCM 码率 / 实际编码码率；当前原始 PCM 码率按 800 kbps 计算。
- CPU Usage: max_frame_time_ms / frame_budget_ms * 100%；frame_budget_ms = 20.48 ms。

## 推荐放入 PPT 的图像清单
- wola_spectrum_compare.png
- wola_frequency_response.png
- denoise_spectrogram_compare.png
- denoise_snr_bar.png
- denoise_delta_vs_fixed.png
- codec_bitrate_compression_ratio.png
- codec_quality_snr.png
- realtime_budget_compare_template.png

## 当前数据支持的结论
- WOLA passthrough SNR=72.86 dB, energy ratio=0.999732, delay=256 samples, pass=PASS.
- rows=34; best delta=2.66 dB (noise_step_up_snr10 / mcra_normal); best output SNR=22.64 dB (stationary_snr20 / fixed).
- target bitrates=160, 240, 320 kbps; max compression ratio=5.17; best denoise+codec SNR=19.20 dB.
- 实时性结果当前以模板形式输出，需要把 CCS Watch 的 last/max frame time 手动填入后再作为最终验收数据。
- 已从 map 文件解析内存区域占用：C:\Users\zhangyueqian\lab8\DSP_LAB_0507\Debug\DSP_LAB_0507.map。

## Warnings
- No standalone realtime CSV was used; generated a CCS Watch fill-in template.
