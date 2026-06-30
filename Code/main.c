/**
 * main.c
 *
 * 描述: 主函数文件
 * 功能: 系统初始化和主循环
 */

#include "driver_include.h"
#include "user_include.h"

#define SW_BREAKPOINT     asm(" SWBP 0 ")

// 1: DAC 输出链路自测；0: ADC->DAC 回放电脑/麦克风输入
#define RUN_DAC_SELF_TEST 0


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
#if RUN_DAC_SELF_TEST
    Dac_Example();
#else
    Adda_Example();
#endif


    while (1)
    {

    }
}
