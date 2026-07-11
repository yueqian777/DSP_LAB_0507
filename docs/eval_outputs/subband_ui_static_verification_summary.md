# Subband UI Static Verification

- Static checks: 21/21 PASS
- Python contract suite: run separately with `python -B -m unittest tools.tests.test_subband_touch_ui_contract -v`
- Host chain logic: run separately under MSYS2 using `tools/tests/subband_ui_logic_test.c.host`
- CCS Debug configuration (`-O3`): PASS
- Link errors: 0x0
- Linked sections: `.text`=120064 B, `.bss`=43 B, `.far`=19748 B, `.fardata`=2013 B, `.const`=14632 B, `.cinit`=2784 B, `.stack`=2048 B
- Board audio, touch, frame continuity, and LCD timing: `NOT_MEASURED_BOARD_UNAVAILABLE`
