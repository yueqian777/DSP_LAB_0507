/**
 * user_include.h
 *
 * 描述: 用户层统一头文件
 * 功能: 包含所有用户层模块的头文件
 */

#ifndef _USER_INCLUDE_H_
#define _USER_INCLUDE_H_

/* 用户驱动 */
// LED驱动
#include "user_led.h"
// KEY驱动
#include "user_key.h"
// TIMER驱动
#include "user_timer.h"
// ADC驱动
#include "user_adc.h"
// DAC驱动
#include "user_dac.h"
// UART驱动
#include "user_uart.h"
// RS485驱动
#include "user_rs485.h"
// FLASH驱动
#include "user_flash.h"
// EEPROM驱动
#include "user_eeprom.h"
// LCD驱动
#include "user_lcd.h"
// 屏幕触摸驱动
#include "user_touch.h"
// ADDA驱动
#include "user_adda.h"
// Project 3.2 subband processing
#include "user_subband_flow.h"
#include "user_subband_ui.h"
#include "user_subband_wola.h"
#include "user_subband_denoise.h"
#include "user_subband_diagnostics.h"
#include "user_subband_codec_loopback.h"
// Project 3.3 dynamic equalizer
#include "user_equalizer_flow.h"
#include "user_equalizer.h"
#include "user_equalizer_control.h"
#include "user_equalizer_response.h"
#include "user_equalizer_ui_logic.h"
#include "user_equalizer_display.h"
#include "user_audio_feature_analyzer.h"
#include "user_smart_bass.h"
#include "user_dynamic_clarity.h"
#include "user_harshness_guard.h"
#include "user_spectral_fft.h"
// Project 3.4 keyword recognition and music control
#include "user_project34_flow.h"



#endif /* __USER_INCLUDE_H__ */
