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

    def test_optional_profile_filter_preserves_default_matrix(self) -> None:
        self.assertIn("[string[]]$ProfileNames = @()", self.script)
        self.assertIn("$profilesToBuild = @($profiles)", self.script)
        self.assertIn("if ($ProfileNames.Count -gt 0)", self.script)
        self.assertIn("Unknown build profile", self.script)
        self.assertIn("foreach ($profile in $profilesToBuild)", self.script)
        self.assertNotIn("$profiles = @($profiles | Where-Object", self.script)

    def test_build_id_generation_must_preserve_clean_state(self) -> None:
        generate = self.script.index("generate_equalizer_build_id.ps1")
        post_status = self.script.index("$statusAfterBuildId", generate)
        build_loop = self.script.index("foreach ($profile in $profilesToBuild)")
        self.assertLess(generate, post_status)
        self.assertLess(post_status, build_loop)
        self.assertIn(
            "Generated build identity changed the clean tracked worktree",
            self.script,
        )

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
            if name == "B_project33_lcd_off":
                expected = 0
            elif name.startswith(("F_", "G_", "H_")):
                expected = 2
            else:
                expected = 1
            self.assertIn(f"expected_framebuffer_count = {expected}", block)
            expected_second = 1 if expected == 2 else 0
            self.assertIn(
                "expected_second_framebuffer_symbol_hits = "
                f"{expected_second}",
                block,
            )

    def test_editor_profiles_require_exact_second_framebuffer_symbol(self) -> None:
        for name, next_name in (
            ("F_project33_editor_readonly", "G_project33_editor_touch"),
            ("G_project33_editor_touch", "H_project33_full"),
            ("H_project33_full", None),
        ):
            with self.subTest(profile=name):
                block = self.profile(name, next_name)
                self.assertIn("expected_framebuffer_count = 2", block)
                self.assertIn(
                    "expected_second_framebuffer_symbol_hits = 1", block
                )
        for token in (
            '$_.name -eq "EQ_LcdEditorBuffer"',
            '@("EQ_LcdEditorBuffer", "Lcd_Buffer")',
            "Unexpected framebuffer symbol set",
            "Unexpected EQ_LcdEditorBuffer count",
        ):
            self.assertIn(token, self.script)

    def test_offscreen_section_matches_all_large_framebuffers(self) -> None:
        self.assertIn("$_.size -ge 700000", self.script)
        self.assertIn(
            "$framebufferBytes += [long]$framebufferSymbol.size",
            self.script,
        )
        self.assertIn("$offscreenMatches.Count -ne", self.script)
        self.assertIn(
            "$offscreenBufferBytes -ne $framebufferBytes", self.script
        )

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
            "framebuffer_symbol_names", "framebuffer_bytes",
            "offscreen_buffer_bytes",
            "offscreen_buffer_object_count", "second_framebuffer_symbol_hits",
        ):
            self.assertIn(token, self.script)

    def test_negative_contracts_fail_the_matrix(self) -> None:
        for token in (
            "Editor runtime symbols retained while Editor is OFF",
            "UI symbols or objects placed in .subband_l2",
            "Unexpected linked framebuffer count",
            "Unexpected framebuffer symbol set",
            "Unexpected EQ_LcdEditorBuffer count",
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
