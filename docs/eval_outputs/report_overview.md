# DSP Subband Evaluation Visual Report

Generated at: 2026-07-07 17:04:30

## 输入文件列表

### CSV found
- aggregate_compare_audio_metrics.csv: `C:\Users\zhangyueqian\lab8\DSP_LAB_0507\board_audio_test_outputs\run_20260707_155620\report\aggregate_compare_audio_metrics.csv`
- board_mode_runtime_summary.csv: `C:\Users\zhangyueqian\lab8\DSP_LAB_0507\board_audio_test_outputs\run_20260707_155620\report\board_mode_runtime_summary.csv`
- compare_audio_metrics.csv: `C:\Users\zhangyueqian\lab8\DSP_LAB_0507\compare_audio_metrics.csv`
- subband_codec_eval_report.csv: `C:\Users\zhangyueqian\lab8\DSP_LAB_0507\subband_codec_eval_report.csv`
- subband_denoise_mcra_eval_report.csv: `C:\Users\zhangyueqian\lab8\DSP_LAB_0507\subband_denoise_mcra_eval_report.csv`
- subband_denoise_ms_eval_report.csv: `C:\Users\zhangyueqian\lab8\DSP_LAB_0507\subband_denoise_ms_eval_report.csv`
- subband_eval_report.csv: `C:\Users\zhangyueqian\lab8\DSP_LAB_0507\subband_eval_report.csv`

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
- compare_09_ms_denoise.wav: `compare_09_ms_denoise.wav`
- compare_10_mcra_denoise.wav: `compare_10_mcra_denoise.wav`
- compare_11_strong_mcra_denoise.wav: `compare_11_strong_mcra_denoise.wav`

## 缺失文件列表

### CSV missing
- <none>

### WAV missing
- <none>

## 成功生成的图像列表
- plots/codec_actual_bitrate.png
- plots/codec_bitrate_compression_ratio.png
- plots/codec_quality_snr.png
- plots/codec_spectrogram_compare.png
- plots/denoise_algorithm_evolution_stepup.png
- plots/denoise_delta_vs_fixed.png
- plots/denoise_reduction_fixed_speechband.png
- plots/denoise_reduction_hybrid_ms_speechband.png
- plots/denoise_reduction_mcra_speechband.png
- plots/denoise_reduction_mcra_strong_speechband.png
- plots/denoise_reduction_panel_speechband.png
- plots/denoise_snr_bar.png
- plots/denoise_spectrogram_compare.png
- plots/denoise_spectrogram_fixed.png
- plots/denoise_spectrogram_fixed_speechband.png
- plots/denoise_spectrogram_hybrid_ms_speechband.png
- plots/denoise_spectrogram_input.png
- plots/denoise_spectrogram_input_speechband.png
- plots/denoise_spectrogram_mcra.png
- plots/denoise_spectrogram_mcra_speechband.png
- plots/denoise_spectrogram_mcra_strong.png
- plots/denoise_spectrogram_mcra_strong_speechband.png
- plots/denoise_spectrogram_ms.png
- plots/denoise_spectrogram_panel_speechband.png
- plots/denoise_spectrum_before_after.png
- plots/denoise_speech_preservation_corr.png
- plots/denoise_waveform_before_after.png
- plots/group_delay_impulse_or_correlation.png
- plots/memory_usage_template.png
- plots/noise_psd_gain_tracking.png
- plots/realtime_budget_compare.png
- plots/realtime_budget_compare_annotated.png
- plots/thd_spectrum_annotated.png
- plots/thd_spectrum_example.png
- plots/wola_frequency_response.png
- plots/wola_reconstruction_error.png
- plots/wola_rolloff_curve.png
- plots/wola_spectrum_compare.png
- plots/wola_waveform_compare.png

## 未生成图像
- <none>

## 汇总表
- summary_tables\wola_summary.csv
- summary_tables\denoise_summary.csv
- summary_tables\codec_summary.csv
- summary_tables\realtime_summary_template.csv
- summary_tables\memory_usage_summary.csv
- summary_tables\thd_summary.csv

## WOLA 基础结果摘要

WOLA passthrough SNR=72.86 dB, energy ratio=0.999732, delay=256 samples, pass=PASS.

## 降噪结果摘要

rows=34; best delta=2.66 dB (noise_step_up_snr10 / mcra_normal); best output SNR=22.64 dB (stationary_snr20 / fixed).

## Codec 结果摘要

target bitrates=160, 240, 320 kbps; max compression ratio=5.17; best denoise+codec SNR=19.20 dB.

## 实时性模板或结果摘要

- frame_budget_ms = 20.48 ms
- realtime_summary rows = 8
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
- denoise_algorithm_evolution_stepup.png
- denoise_spectrogram_panel_speechband.png
- denoise_reduction_panel_speechband.png
- realtime_budget_compare_annotated.png
- codec_quality_snr.png
- codec_bitrate_compression_ratio.png
- wola_spectrum_compare.png
- thd_spectrum_annotated.png
- wola_frequency_response.png
- denoise_snr_bar.png

## 当前数据支持的结论
- WOLA passthrough SNR=72.86 dB, energy ratio=0.999732, delay=256 samples, pass=PASS.
- rows=34; best delta=2.66 dB (noise_step_up_snr10 / mcra_normal); best output SNR=22.64 dB (stationary_snr20 / fixed).
- target bitrates=160, 240, 320 kbps; max compression ratio=5.17; best denoise+codec SNR=19.20 dB.
- 实时性结果已使用 CCS Watch 填入的 max_ms 生成正式 realtime_budget_compare.png。
- 已从 map 文件解析内存区域占用：C:\Users\zhangyueqian\lab8\DSP_LAB_0507\Debug\DSP_LAB_0507.map。

## Interactive HTML reports
- interactive/index.html
- interactive/denoise_output_snr_surface.html
- interactive/denoise_delta_surface.html
- interactive/denoise_output_snr_heatmap.html
- interactive/denoise_delta_heatmap.html
- interactive/denoise_stepup_waterfall.html
- interactive/denoise_quality_tradeoff.html
- interactive/board_runtime_by_mode.html
- interactive/codec_rate_quality_tradeoff.html
- interactive/audio_energy_ratio_heatmap.html

这些 HTML 用于课堂演示和交互式检查；PNG 用于 PPT 静态插图。

## Warnings
- <none>
