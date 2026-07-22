# Project 3.3 Final Non-Touch Acceptance

## Scope

- Validated code commit: `ca594e2f0a4ebe0ed83c5a93fdb5b091d91a7270`
- Board evidence commit: `ba18ae89855f66147c6d04ad3966ac334036b2d8`
- Board result directory: `C:\Users\zhangyueqian\AppData\Local\Temp\DSP_LAB_0507\project33_final_acceptance\ba18ae89855f66147c6d04ad3966ac334036b2d8_20260722_113257`
- Overall result: `PASS`

The commits between the board evidence and validated code change only the
default project/build selection and acceptance tooling. They do not modify
Project 3.3 audio algorithms, dynamic parameters, LCD layout, or scheduling.
The ordinary CCS Clean/Build at the validated commit selects the
`H_project33_full` macro contract.

## Results

- Production boot: `PASS`
- 30 scripted page round trips: `PASS`
- Nine LCD job timing classes: `PASS`
- Non-touch endurance: `PASS` with `301.60896 s` DSP audio time
- Analyzer reset: `PASS`
- Operator visual observation: `PASS`
- `no_touch_120s`: `WAIVED / NOT_REQUIRED`
- `touch_hitbox_27`: `WAIVED / NOT_REQUIRED`

## Artifact Policy

No `.out`, `.map`, full build log, or WAV is stored here. Their absolute
paths, sizes, and SHA-256 values remain recorded in
`external_file_inventory.json` and the compact build summaries.
