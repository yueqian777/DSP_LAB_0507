/**
 * main.c
 *
 * 描述: 主函数文件
 * 功能: 系统初始化和主循环
 */

#include "driver_include.h"
#include "user_include.h"

#define SW_BREAKPOINT     asm(" SWBP 0 ")

#ifndef DSP_LAB_PROJECT_SELECT
#define DSP_LAB_PROJECT_SELECT 33
#endif

/**
 * @brief 主函数
 * @return 无
 */
int main(void)
{
//    Led_Example();
//    Key_Example();
//    Timer_Example();
//    Uart_Example();
//    Uart_Example_NoInterrupt();
//    Uart_Example_Interrupt();
//    Rs485_Example();
//    Rs485_Example_Interrupt();
//    Flash_Example();
//    EEPROM_Example();
#if DSP_LAB_PROJECT_SELECT == 32
    Subband_Flow_Example();
#elif DSP_LAB_PROJECT_SELECT == 33
    Equalizer_Flow_Example();
#else
    while (1)
    {
    }
#endif


    while (1)
    {

    }
}
