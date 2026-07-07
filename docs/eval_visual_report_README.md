# 本地评测出图脚本说明

`tools/eval_visual_report.py` 用于在 PC 本地根据 CSV 和可选 WAV 自动生成报告/PPT 可用的汇总表、PNG 图像、Plotly 交互式 HTML 和 Markdown 总览报告。脚本只读取本地文件，不连接 DSP，不运行 CCS Debug，不调用串口、JTAG、DSS 或 XDS100v3。

## 推荐命令

当前机器的默认 `python` 缺少 `matplotlib`，已验证 `D:\Anaconda\python.exe` 可用：

```powershell
& 'D:\Anaconda\python.exe' tools\eval_visual_report.py `
  --csv-dir docs\eval_baseline `
  --audio-dir . `
  --out-dir docs\eval_outputs
```

默认会同时生成静态 PNG 和交互式 HTML。也可以显式打开交互层：

```powershell
& 'D:\Anaconda\python.exe' tools\eval_visual_report.py `
  --csv-dir . `
  --audio-dir . `
  --out-dir docs\eval_outputs `
  --interactive
```

如果只想生成 PNG 和 Markdown，可使用：

```powershell
& 'D:\Anaconda\python.exe' tools\eval_visual_report.py `
  --csv-dir docs\eval_baseline `
  --audio-dir . `
  --out-dir docs\eval_outputs `
  --no-interactive
```

也可以指定其他 CSV/WAV 目录：

```powershell
& 'D:\Anaconda\python.exe' tools\eval_visual_report.py `
  --csv-dir docs\eval_baseline `
  --csv-dir compare_unzip `
  --audio-dir compare_unzip `
  --out-dir docs\eval_outputs
```

## 自动搜索路径

脚本会搜索命令行目录，以及以下常见位置：

- 当前工作目录
- `test_vectors/`
- `docs/eval_baseline/`
- `docs/eval_outputs/input/`
- `docs/eval_outputs/summary_tables/`
- `compare/`
- `compare_unzip/`
- `board_audio_test_outputs/*/report/`

CSV 支持精确文件名，也支持带日期后缀的同名前缀文件，例如 `subband_eval_report_2026_07_05.csv`。如果精确文件为空，脚本会优先使用非空的同名前缀候选。

## 输出内容

默认输出到 `docs/eval_outputs/`：

- `plots/`: PNG 图像
- `summary_tables/`: 汇总 CSV
- `interactive/`: Plotly 交互式 HTML 图
- `interactive/index.html`: 课堂演示用 dashboard
- `report_overview.md`: 输入、缺失项、图像、结论和 PPT 推荐清单
- `copied_inputs_log.txt`: 输入文件识别日志；脚本不会复制大型 WAV

## 依赖

脚本使用：

- 标准库：`argparse`, `pathlib`, `wave`
- 第三方库：`numpy`, `pandas`, `matplotlib`, `plotly`

不使用 `seaborn`。如果当前 Python 环境缺少 Plotly：

```powershell
& 'D:\Anaconda\python.exe' -m pip install plotly
```

缺少 Plotly 时，脚本会跳过 interactive HTML，但 PNG 和 Markdown 仍会继续生成。

## 交互式 HTML

交互式输出默认写入 `docs/eval_outputs/interactive/`，其中 `index.html` 是总入口。每个 Plotly HTML 都使用 `include_plotlyjs=True` 写入本地文件，适合无网络教室离线打开。

主要交互图包括：

- `denoise_output_snr_surface.html`: 降噪输出 SNR 曲面图
- `denoise_delta_surface.html`: 相对 fixed 的 SNR 提升曲面图
- `denoise_output_snr_heatmap.html`: 降噪输出 SNR 热力图
- `denoise_delta_heatmap.html`: delta vs fixed 热力图
- `denoise_stepup_waterfall.html`: fixed 到 HYBRID-MS 到 MCRA normal 的演进图
- `denoise_quality_tradeoff.html`: speech preservation / correlation 散点图
- `board_runtime_by_mode.html`: 板端实时性图，依赖 `board_mode_runtime_summary.csv`
- `codec_rate_quality_tradeoff.html`: codec 码率-音质折中图
- `audio_energy_ratio_heatmap.html`: PC 音频能量比热力图

## 缺失输入处理

缺少某类 CSV 或 WAV 时，脚本不会整体失败。对应图像会跳过，并在 `report_overview.md` 的“未生成图像”“Interactive HTML reports”和 “Warnings” 中说明原因。实时性数据若没有 `board_mode_runtime_summary.csv` 或单独实时 CSV，会生成 `realtime_summary_template.csv` 和 `realtime_budget_compare_template.png`，需要根据 CCS Watch 手动填入。
