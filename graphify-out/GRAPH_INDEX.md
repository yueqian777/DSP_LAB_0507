# DSP Lab Project Graphs

Use the focused graph that matches the requested project. Both graphs are built from the current working tree and contain only structural code extraction, so every relationship is marked `EXTRACTED` or `INFERRED` in the graph data.

## Project 3.2: subband analysis, denoise, codec loopback, and LCD/touch

- Graph: `project32/graph.json`
- Visual map: `project32/graph.html`
- Audit report: `project32/GRAPH_REPORT.md`
- Corpus: `Code/main.c`, `Code/User/user_include.h`, all `user_subband*` C/header files, and `tools/tests/test_subband_touch_ui_contract.py`.
- Use this for `Subband_Flow_Example`, WOLA, denoise learning, codec loopback, LCD scheduler, touch handling, and 3.2 validation work.

## Project 3.3: equalizer runtime, RBJ bank, LCD display, and evaluation

- Graph: `project33/graph.json`
- Visual map: `project33/graph.html`
- Audit report: `project33/GRAPH_REPORT.md`
- Corpus: `Code/main.c`, `Code/User/user_include.h`, and all `user_equalizer*` C/header files.
- Use this for `Equalizer_Flow_Example`, DSP frame processing, RBJ/legacy modes, transition state, display rendering, and offline equalizer evaluation.

## Targeted lookup

Use the interactive HTML map for browsing. For a shortest dependency path with Graphify, pass the selected graph explicitly:

```powershell
C:\Python314\Scripts\graphify.exe path "Subband_Flow_Example" "SubbandUI_ServiceDisplay" --graph graphify-out\project32\graph.json
C:\Python314\Scripts\graphify.exe path "Equalizer_Flow_Example" "Equalizer_ProcessFrame" --graph graphify-out\project33\graph.json
```

Treat `INFERRED` edges as review leads that require source confirmation; only `EXTRACTED` edges are direct structural evidence.
