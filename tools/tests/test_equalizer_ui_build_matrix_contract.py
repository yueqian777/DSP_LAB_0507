from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
MATRIX = ROOT / "tools/run_equalizer_ui_build_matrix.ps1"


class EqualizerUiBuildMatrixContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.script = MATRIX.read_text(encoding="utf-8")

    def profile(self, name: str, next_name: str | None = None) -> str:
        start = self.script.index(f'name = "{name}"')
        if next_name is None:
            end = self.script.index("\n)\n", start)
        else:
            end = self.script.index(f'name = "{next_name}"', start)
        return self.script[start:end]

    def test_profiles_are_exactly_a_through_h(self) -> None:
        names = re.findall(r'^\s*name = "([A-H]_[^"]+)"',
                           self.script, re.MULTILINE)
        self.assertEqual(names, [
            "A_project32",
            "B_project33_lcd_off",
            "C_project33_static",
            "D_project33_dynamic",
            "E_project33_touch",
            "F_project33_editor_readonly",
            "G_project33_editor_touch",
            "H_project33_full",
        ])
        self.assertIn("$profiles.Count -ne 8", self.script)

    def test_editor_profile_defines_and_runtime_masks(self) -> None:
        cases = (
            ("D_project33_dynamic", "E_project33_touch", 1, 0, 0, 0, 15),
            ("E_project33_touch", "F_project33_editor_readonly", 1, 1, 0, 0, 15),
            ("F_project33_editor_readonly", "G_project33_editor_touch", 1, 0, 1, 0, 49),
            ("G_project33_editor_touch", "H_project33_full", 1, 1, 1, 1, 49),
            ("H_project33_full", None, 1, 1, 1, 1, 63),
        )
        for name, next_name, lcd, touch, editor, editor_touch, mask in cases:
            with self.subTest(profile=name):
                block = self.profile(name, next_name)
                for token in (
                    f"--define=EQ_ENABLE_LCD_DISPLAY={lcd}",
                    f"--define=EQ_ENABLE_PROJECT33_TOUCH={touch}",
                    f"--define=EQ_ENABLE_TEN_BAND_EDITOR={editor}",
                    f"--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH={editor_touch}",
                    f"--define=EQ_UI_RUNTIME_DEFAULT_MASK={mask}",
                    f"runtime_mask = {mask}",
                ):
                    self.assertIn(token, block)

    def test_readonly_mask_contains_only_preset_editor_and_page(self) -> None:
        block = self.profile("F_project33_editor_readonly",
                             "G_project33_editor_touch")
        self.assertIn("runtime_mask = 49", block)
        self.assertEqual(49, 0x01 | 0x10 | 0x20)

    def test_framebuffer_expectation_matches_each_profile(self) -> None:
        names = (
            "A_project32",
            "B_project33_lcd_off",
            "C_project33_static",
            "D_project33_dynamic",
            "E_project33_touch",
            "F_project33_editor_readonly",
            "G_project33_editor_touch",
            "H_project33_full",
        )
        for index, name in enumerate(names):
            next_name = names[index + 1] if index + 1 < len(names) else None
            block = self.profile(name, next_name)
            expected = 0 if name == "B_project33_lcd_off" else 1
            self.assertIn(f"expected_framebuffer_count = {expected}", block)

    def test_link_and_memory_evidence_fields_are_collected(self) -> None:
        for token in (
            "warning_count", "error_count", "link_errors",
            "out_sha256", "text_bytes", "const_bytes", "bss_bytes",
            "subband_l2_bytes", "ui_state_bytes", "editor_state_bytes",
            "touch_state_bytes", "ui_total_state_bytes", "job_count",
            "combined_ui_touch_state_bytes",
            "dynamic_hitbox_count", "editor_hitbox_count",
            "editor_runtime_symbol_hits", "ui_subband_symbol_hits",
            "ui_subband_object_hits", "framebuffer_symbol_count",
            "framebuffer_bytes", "offscreen_buffer_bytes",
            "offscreen_buffer_object_count", "second_framebuffer_symbol_hits",
        ):
            self.assertIn(token, self.script)

    def test_negative_contracts_fail_the_matrix(self) -> None:
        for token in (
            "Editor runtime symbols retained while Editor is OFF",
            "UI symbols or objects placed in .subband_l2",
            "Unexpected linked framebuffer count",
            "Second framebuffer candidate detected",
            "Unexpected UI job count",
            "Unexpected dynamic hitbox count",
            "Unexpected editor hitbox count",
        ):
            self.assertIn(token, self.script)

    def test_nm_long_format_and_map_are_the_evidence_sources(self) -> None:
        self.assertIn("& $NmPath -l $outPath", self.script)
        self.assertIn("Get-NmSymbols", self.script)
        self.assertIn("Get-MapSectionInfo", self.script)
        self.assertIn(
            "user_equalizer_(?:display|ui_logic)\\.obj",
            self.script,
        )


if __name__ == "__main__":
    unittest.main()
