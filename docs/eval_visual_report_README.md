# 本地评测出图脚本说明

`tools/eval_visual_report.py` 用于在 PC 本地根据 CSV 和可选 WAV 自动生成报告/PPT 可用的汇总表、PNG 图像和 Markdown 总览报告。脚本只读取本地文件，不连接 DSP，不运行 CCS Debug，不调用串口、JTAG、DSS 或 XDS100v3。

## 推荐命令

当前机器的默认 `python` 缺少 `matplotlib`，已验证 `D:\Anaconda\python.exe` 可用：

```powershell
& 'D:\Anaconda\python.exe' tools\eval_visual_report.py `
  --csv-dir docs\eval_baseline `
  --audio-dir . `
  --out-dir docs\eval_outputs
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
- `docs/eval_baseline/`
- `docs/eval_outputs/input/`
- `compare/`
- `compare_unzip/`

CSV 支持精确文件名，也支持带日期后缀的同名前缀文件，例如 `subband_eval_report_2026_07_05.csv`。如果精确文件为空，脚本会优先使用非空的同名前缀候选。

## 输出内容

默认输出到 `docs/eval_outputs/`：

- `plots/`: PNG 图像
- `summary_tables/`: 汇总 CSV
- `report_overview.md`: 输入、缺失项、图像、结论和 PPT 推荐清单
- `copied_inputs_log.txt`: 输入文件识别日志；脚本不会复制大型 WAV

## 依赖

脚本使用：

- 标准库：`argparse`, `pathlib`, `wave`
- 第三方库：`numpy`, `pandas`, `matplotlib`

不使用 `seaborn`。

## 缺失输入处理

缺少某类 CSV 或 WAV 时，脚本不会整体失败。对应图像会跳过，并在 `report_overview.md` 的“未生成图像”和 “Warnings” 中说明原因。实时性数据若没有单独 CSV，会生成 `realtime_summary_template.csv` 和 `realtime_budget_compare_template.png`，需要根据 CCS Watch 手动填入。
