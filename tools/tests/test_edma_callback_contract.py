from __future__ import annotations

import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
EDMA0_C = ROOT / "Code/Driver/21_edma/edma0_common.c"
EDMA0_H = ROOT / "Code/Driver/21_edma/edma0_common.h"
EDMA1_C = ROOT / "Code/Driver/21_edma/edma1_common.c"
EDMA1_H = ROOT / "Code/Driver/21_edma/edma1_common.h"
ADC_C = ROOT / "Code/Driver/04_adc/adc_edma.c"
MAP = ROOT / "Debug/DSP_LAB_0507.map"


def read(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8")


def callback_allocation(map_text: str, symbol: str) -> tuple[int, int]:
    pattern = re.compile(
        rf"^\s*([0-9a-fA-F]{{8}})\s+([0-9a-fA-F]{{8}})\s+.*"
        rf"\([^\r\n)]*\b{re.escape(symbol)}\)\s*$",
        re.MULTILINE,
    )
    match = pattern.search(map_text)
    if match is None:
        raise AssertionError(f"missing allocation for {symbol}")
    return int(match.group(1), 16), int(match.group(2), 16)


class EdmaCallbackContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.edma0_c = read(EDMA0_C)
        cls.edma0_h = read(EDMA0_H)
        cls.edma1_c = read(EDMA1_C)
        cls.edma1_h = read(EDMA1_H)
        cls.adc_c = read(ADC_C)

    def test_callback_tables_are_complete(self) -> None:
        for source in (self.edma0_c, self.edma0_h):
            self.assertNotRegex(source, r"\bcb_Fxn\s*\[\s*\]")
            self.assertIn("cb_Fxn[EDMA3_NUM_TCC]", source)
        for source in (self.edma1_c, self.edma1_h):
            self.assertNotRegex(source, r"\bedma1_cb_Fxn\s*\[\s*\]")
            self.assertIn("edma1_cb_Fxn[EDMA3_NUM_TCC]", source)
        self.assertIn("cb_Fxn[EDMA3_NUM_TCC] = {0};", self.edma0_c)
        self.assertIn("edma1_cb_Fxn[EDMA3_NUM_TCC] = {0};", self.edma1_c)
        self.assertIn("EDMA0_CallbackTableSizeCheck", self.edma0_c)
        self.assertIn("EDMA1_CallbackTableSizeCheck", self.edma1_c)

    def test_adc_registration_is_bounded(self) -> None:
        self.assertIn("EDMA3_NUM_TCC > EDMA3_CHA_GPIO_BNKINT5", self.adc_c)
        guard = self.adc_c.index("if (tccNum >= EDMA3_NUM_TCC)")
        registration = self.adc_c.index("cb_Fxn[tccNum] = &callback_adc;")
        self.assertLess(guard, registration)
        self.assertIn("ADC_EDMA_CallbackRegistrationError = 1u;",
                      self.adc_c[guard:registration])
        self.assertIn("return;", self.adc_c[guard:registration])
        region_enable = self.adc_c.index(
            "Edma0_Common_EnableDspTcc(tccNum);", guard)
        transfer_enable = self.adc_c.index("EDMA3EnableTransfer(", registration)
        self.assertLess(registration, region_enable)
        self.assertLess(region_enable, transfer_enable)

    def test_edma0_completion_uses_dsp_shadow_region(self) -> None:
        self.assertIn("void Edma0_Common_EnableDspTcc(unsigned int tccNum);",
                      self.edma0_h)
        self.assertIn("EDMA3CC_DRAE(EDMA0_DSP_REGION)", self.edma0_c)
        self.assertIn("EDMA3CC_IECR", self.edma0_c)
        self.assertIn("EDMA3CC_ICR", self.edma0_c)
        self.assertIn("EDMA3CC_S_IESR(EDMA0_DSP_REGION)", self.edma0_c)
        self.assertIn("EDMA3CC_S_IPR(EDMA0_DSP_REGION)", self.edma0_c)
        self.assertIn("EDMA3CC_S_ICR(EDMA0_DSP_REGION)", self.edma0_c)

        start = self.edma0_c.index("static void Edma3CCComplHandlerIsr(void)\n{")
        end = self.edma0_c.index("static void Edma3CCErrHandlerIsr(void)", start)
        isr = self.edma0_c[start:end]
        self.assertIn("EDMA0GetDspIntrStatus()", isr)
        self.assertIn("EDMA0ClearDspIntr(indexl);", isr)
        self.assertNotIn("EDMA3GetIntrStatus", isr)
        self.assertNotIn("EDMA3ClrIntr", isr)

    def test_completion_isrs_guard_bounds_and_null(self) -> None:
        for source, table, callback_type, clear_statement in (
            (self.edma0_c, "cb_Fxn", "EDMA0_CALLBACK",
             "EDMA0ClearDspIntr(indexl);"),
            (self.edma1_c, "edma1_cb_Fxn", "EDMA1_CALLBACK",
             "EDMA3ClrIntr(SOC_EDMA31CC_0_REGS, indexl);"),
        ):
            start = source.index("static void Edma3CCComplHandlerIsr(void)\n{")
            end = source.index("static void Edma3CCErrHandlerIsr(void)", start)
            isr = source[start:end]
            self.assertIn(f"{callback_type} callback;", isr)
            bounds = isr.index("if (indexl < EDMA3_NUM_TCC)")
            copy = isr.index(f"callback = {table}[indexl];", bounds)
            clear = isr.index(clear_statement, copy)
            null_guard = isr.index("if (callback != NULL)", clear)
            call = isr.index("callback(indexl, EDMA3_XFER_COMPLETE, NULL);",
                             null_guard)
            self.assertLess(bounds, copy)
            self.assertLess(copy, clear)
            self.assertLess(clear, null_guard)
            self.assertLess(null_guard, call)

    def test_ccs_map_callback_table_sizes_and_placement(self) -> None:
        map_text = read(MAP)
        expected_size = 32 * 4
        cb_base, cb_size = callback_allocation(map_text, "cb_Fxn")
        edma1_base, edma1_size = callback_allocation(map_text, "edma1_cb_Fxn")
        self.assertEqual(cb_size, expected_size)
        self.assertEqual(edma1_size, expected_size)
        tcc29 = cb_base + 29 * 4
        self.assertGreaterEqual(tcc29, cb_base)
        self.assertLess(tcc29 + 4, cb_base + cb_size + 1)

        debug_addresses = {
            int(match.group(1), 16): match.group(2)
            for match in re.finditer(
                r"^([0-9a-fA-F]{8})\s+(EQ_Debug\w+)\s*$",
                map_text,
                re.MULTILINE,
            )
        }
        for address, name in debug_addresses.items():
            self.assertFalse(cb_base <= address < cb_base + cb_size,
                             f"{name} overlaps cb_Fxn")
            self.assertFalse(edma1_base <= address < edma1_base + edma1_size,
                             f"{name} overlaps edma1_cb_Fxn")
        self.assertNotIn(tcc29, debug_addresses)


if __name__ == "__main__":
    unittest.main()
