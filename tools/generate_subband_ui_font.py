"""Generate the small static Chinese bitmap subset used by Project 3.2 UI."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT_H = ROOT / "Code/User/user_dsp/user_subband_ui_font.h"
OUT_C = ROOT / "Code/User/user_dsp/user_subband_ui_font.c"
FONT_PATH = Path(r"C:\Windows\Fonts\msyh.ttc")
HEIGHT = 16
PHRASES = [
    ("TITLE", "实时子带音频处理系统"),
    ("MODE_RAW", "原始直通"),
    ("MODE_WOLA", "分析合成"),
    ("MODE_BASIC", "基础降噪"),
    ("MODE_MINSTAT", "最小统计"),
    ("MODE_MCRA", "智能降噪"),
    ("MODE_STRONG", "强力降噪"),
    ("MODE_FULL", "完整处理"),
    ("MODE_CODEC", "感知压缩"),
    ("LABEL_MODE", "模式"),
    ("LABEL_RUNNING", "运行中"),
    ("LABEL_LEARNING", "学习中"),
    ("LABEL_READY", "学习完成"),
    ("LABEL_REMAINING", "剩余"),
    ("LABEL_BITRATE", "码率"),
    ("LABEL_LOAD", "算法负载"),
]


def bitmap(text: str) -> tuple[int, list[int]]:
    font = ImageFont.truetype(str(FONT_PATH), HEIGHT)
    width = HEIGHT * len(text)
    image = Image.new("L", (width, HEIGHT), 0)
    draw = ImageDraw.Draw(image)
    draw.text((0, -2), text, font=font, fill=255, spacing=0)
    bits = [1 if pixel >= 96 else 0 for pixel in image.getdata()]
    packed = []
    for start in range(0, len(bits), 8):
        byte = 0
        for offset, bit in enumerate(bits[start:start + 8]):
            byte |= bit << (7 - offset)
        packed.append(byte)
    return width, packed


def main() -> None:
    header = [
        "#ifndef _USER_SUBBAND_UI_FONT_H_",
        "#define _USER_SUBBAND_UI_FONT_H_",
        "",
        '#include "grlib.h"',
        "",
        "typedef enum",
        "{",
    ]
    for index, (name, _) in enumerate(PHRASES):
        header.append(f"    SUBBAND_UI_PHRASE_{name} = {index},")
    header.extend([
        f"    SUBBAND_UI_PHRASE_COUNT = {len(PHRASES)}",
        "} SubbandUIPhrase;",
        "",
        "void SubbandUIFont_DrawPhrase(tContext *context, SubbandUIPhrase phrase,",
        "                               int x, int y, unsigned long color);",
        "unsigned int SubbandUIFont_PhraseWidth(SubbandUIPhrase phrase);",
        "unsigned long SubbandUIFont_StorageBytes(void);",
        "",
        "#endif",
        "",
    ])

    source = [
        '#include "user_subband_ui_font.h"',
        "",
        "typedef struct",
        "{",
        "    const unsigned char *bits;",
        "    unsigned short width;",
        "} SubbandUIFontEntry;",
        "",
    ]
    entries = []
    total_bytes = 0
    for index, (name, text) in enumerate(PHRASES):
        width, data = bitmap(text)
        total_bytes += len(data)
        source.append(f"static const unsigned char Glyph_{index}[] =")
        source.append("{")
        for start in range(0, len(data), 16):
            source.append("    " + ", ".join(f"0x{item:02X}" for item in data[start:start + 16]) + ",")
        source.append("};")
        source.append("")
        entries.append((index, width))
    source.append("static const SubbandUIFontEntry Font_Entries[SUBBAND_UI_PHRASE_COUNT] =")
    source.append("{")
    for index, width in entries:
        source.append(f"    {{Glyph_{index}, {width}U}},")
    source.extend([
        "};",
        "",
        "void SubbandUIFont_DrawPhrase(tContext *context, SubbandUIPhrase phrase,",
        "                               int x, int y, unsigned long color)",
        "{",
        "    unsigned int row;",
        "    unsigned int col;",
        "    unsigned int bit_index;",
        "    unsigned int run_start;",
        "    const SubbandUIFontEntry *entry;",
        "",
        "    if ((context == 0) || ((unsigned int)phrase >= SUBBAND_UI_PHRASE_COUNT))",
        "    {",
        "        return;",
        "    }",
        "    entry = &Font_Entries[(unsigned int)phrase];",
        "    GrContextForegroundSet(context, color);",
        "    for (row = 0U; row < 16U; row++)",
        "    {",
        "        col = 0U;",
        "        while (col < entry->width)",
        "        {",
        "            bit_index = row * entry->width + col;",
        "            if ((entry->bits[bit_index >> 3] &",
        "                 (unsigned char)(0x80U >> (bit_index & 7U))) == 0U)",
        "            {",
        "                col++;",
        "                continue;",
        "            }",
        "            run_start = col;",
        "            do",
        "            {",
        "                col++;",
        "                if (col >= entry->width)",
        "                {",
        "                    break;",
        "                }",
        "                bit_index = row * entry->width + col;",
        "            }",
        "            while ((entry->bits[bit_index >> 3] &",
        "                    (unsigned char)(0x80U >> (bit_index & 7U))) != 0U);",
        "            GrLineDrawH(context, x + (int)run_start,",
        "                        x + (int)col - 1, y + (int)row);",
        "        }",
        "    }",
        "}",
        "",
        "unsigned int SubbandUIFont_PhraseWidth(SubbandUIPhrase phrase)",
        "{",
        "    if ((unsigned int)phrase >= SUBBAND_UI_PHRASE_COUNT)",
        "    {",
        "        return 0U;",
        "    }",
        "    return Font_Entries[(unsigned int)phrase].width;",
        "}",
        "",
        "unsigned long SubbandUIFont_StorageBytes(void)",
        "{",
        f"    return {total_bytes}UL;",
        "}",
        "",
    ])
    OUT_H.write_text("\n".join(header), encoding="ascii")
    OUT_C.write_text("\n".join(source), encoding="ascii")
    print(f"generated {len(PHRASES)} phrases, {total_bytes} bitmap bytes")


if __name__ == "__main__":
    main()
