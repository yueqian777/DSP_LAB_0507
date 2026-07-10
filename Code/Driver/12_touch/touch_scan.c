/**
 * @file touch_scan.c
 * @brief Touch扫描实现文件
 * @details 实现Touch扫描功能，读取触摸坐标并更新状态。
 * @ingroup TOUCH_SCAN
 */

/* 头文件包含 */
#include "touch_scan.h"           // Touch扫描
#include "touch_drv.h"            // Touch设备驱动
#include "touch_btn.h"            // Touch按钮
#include "sys_iic.h"              // 系统IIC接口


// 触摸状态和坐标
unsigned char Touch_Sta = 0;    // 触摸状态 0: 未触摸 1: 触摸
unsigned int Touch_X;           // 触摸点的X坐标
unsigned int Touch_Y;           // 触摸点的Y坐标
volatile unsigned char Touch_DebugRawState = 0;
volatile unsigned long Touch_DebugScanCount = 0;
volatile unsigned long Touch_DebugTouchSampleCount = 0;
volatile unsigned long Touch_DebugReleaseSampleCount = 0;
volatile unsigned int Touch_DebugLastX = 0;
volatile unsigned int Touch_DebugLastY = 0;


/* 函数定义 */
// 扫描触摸并处理按钮响应
void Touch_Scan(void)
{
    GT1151_Scan();
    Widget_Btn_App(Touch_Sta, Touch_X, Touch_Y);
}

// 扫描GT1151触摸屏
void GT1151_Scan(void)
{
    unsigned char State = 0;
    unsigned char touch_count;
    unsigned char Temp_XY[4] = {0};

    // 读取触摸状态寄存器
    GT1151_RD_Reg(GT_GSTID_REG, &State, 1);
    Touch_DebugRawState = State;
    Touch_DebugScanCount++;

    if((State & 0X80) != 0) {
        touch_count = State & 0X0F;
        if(touch_count > 0) {
            // Read coordinates before acknowledging the ready flag.
            GT1151_RD_Reg(GT_TP1_REG, Temp_XY, 4);
            Touch_X = ((unsigned int)Temp_XY[1] << 8) + Temp_XY[0];
            Touch_Y = ((unsigned int)Temp_XY[3] << 8) + Temp_XY[2];
            Touch_DebugLastX = Touch_X;
            Touch_DebugLastY = Touch_Y;
            Touch_DebugTouchSampleCount++;
            Touch_Sta = 1;
        }
        else {
            Touch_DebugReleaseSampleCount++;
            Touch_Sta = 0;
        }
        // Acknowledge only after all data for this report has been read.
        GT1151_WR_Reg(GT_GSTID_REG, 0, 1);
    }
}
