#ifndef _USER_SUBBAND_UI_FONT_H_
#define _USER_SUBBAND_UI_FONT_H_

#include "grlib.h"

typedef enum
{
    SUBBAND_UI_PHRASE_TITLE = 0,
    SUBBAND_UI_PHRASE_MODE_RAW = 1,
    SUBBAND_UI_PHRASE_MODE_WOLA = 2,
    SUBBAND_UI_PHRASE_MODE_BASIC = 3,
    SUBBAND_UI_PHRASE_MODE_MINSTAT = 4,
    SUBBAND_UI_PHRASE_MODE_MCRA = 5,
    SUBBAND_UI_PHRASE_MODE_STRONG = 6,
    SUBBAND_UI_PHRASE_MODE_FULL = 7,
    SUBBAND_UI_PHRASE_MODE_CODEC = 8,
    SUBBAND_UI_PHRASE_LABEL_MODE = 9,
    SUBBAND_UI_PHRASE_LABEL_RUNNING = 10,
    SUBBAND_UI_PHRASE_LABEL_LEARNING = 11,
    SUBBAND_UI_PHRASE_LABEL_READY = 12,
    SUBBAND_UI_PHRASE_LABEL_REMAINING = 13,
    SUBBAND_UI_PHRASE_LABEL_BITRATE = 14,
    SUBBAND_UI_PHRASE_LABEL_LOAD = 15,
    SUBBAND_UI_PHRASE_COUNT = 16
} SubbandUIPhrase;

void SubbandUIFont_DrawPhrase(tContext *context, SubbandUIPhrase phrase,
                               int x, int y, unsigned long color);
unsigned int SubbandUIFont_PhraseWidth(SubbandUIPhrase phrase);
unsigned long SubbandUIFont_StorageBytes(void);

#endif
