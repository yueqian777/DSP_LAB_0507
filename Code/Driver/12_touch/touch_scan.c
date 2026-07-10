/**
 * @file touch_scan.c
 * @brief GT1151 touch scan state machine.
 */

#include "touch_scan.h"
#include "touch_drv.h"
#include "touch_btn.h"
#include "sys_iic.h"

extern volatile unsigned int AckRolling;

unsigned char Touch_Sta = 0U;
unsigned int Touch_X = 0U;
unsigned int Touch_Y = 0U;

volatile unsigned char Touch_DebugRawState = 0U;
volatile unsigned long Touch_DebugScanCount = 0UL;
volatile unsigned long Touch_DebugReadyCount = 0UL;
volatile unsigned long Touch_DebugNoDataCount = 0UL;
volatile unsigned long Touch_DebugDownCount = 0UL;
volatile unsigned long Touch_DebugReleaseCount = 0UL;
volatile unsigned long Touch_DebugI2cErrorCount = 0UL;
volatile unsigned long Touch_DebugTouchSampleCount = 0UL;
volatile unsigned long Touch_DebugReleaseSampleCount = 0UL;
volatile unsigned int Touch_DebugLastX = 0U;
volatile unsigned int Touch_DebugLastY = 0U;

static unsigned char Touch_ReadReg(unsigned int reg,
                                   unsigned char *data,
                                   unsigned int len)
{
    AckRolling = 0U;
    GT1151_RD_Reg(reg, data, len);
    return (AckRolling == 0U) ? 1U : 0U;
}

static unsigned char Touch_ClearReady(void)
{
    unsigned char clear_value;

    clear_value = 0U;
    AckRolling = 0U;
    GT1151_WR_Reg(GT_GSTID_REG, &clear_value, 1U);
    return (AckRolling == 0U) ? 1U : 0U;
}

TouchScanResult Touch_Scan(void)
{
    TouchScanResult result;

    result = GT1151_Scan();
    if ((result == TOUCH_SCAN_DOWN) || (result == TOUCH_SCAN_RELEASE))
    {
        Widget_Btn_App(Touch_Sta, Touch_X, Touch_Y);
    }
    return result;
}

TouchScanResult GT1151_Scan(void)
{
    unsigned char State;
    unsigned char ready;
    unsigned char touch_count;
    unsigned char Temp_XY[4];

    State = 0U;
    Temp_XY[0] = 0U;
    Temp_XY[1] = 0U;
    Temp_XY[2] = 0U;
    Temp_XY[3] = 0U;

    Touch_DebugScanCount++;
    if (Touch_ReadReg(GT_GSTID_REG, &State, 1U) == 0U)
    {
        Touch_DebugI2cErrorCount++;
        return TOUCH_SCAN_ERROR;
    }

    Touch_DebugRawState = State;
    ready = (unsigned char)(State & 0x80U);
    touch_count = (unsigned char)(State & 0x0FU);

    if (ready == 0U)
    {
        Touch_DebugNoDataCount++;
        return TOUCH_SCAN_NO_DATA;
    }

    Touch_DebugReadyCount++;

    if (touch_count > 0U)
    {
        if (Touch_ReadReg(GT_TP1_REG, Temp_XY, 4U) == 0U)
        {
            Touch_DebugI2cErrorCount++;
            return TOUCH_SCAN_ERROR;
        }

        Touch_X = ((unsigned int)Temp_XY[1] << 8) + Temp_XY[0];
        Touch_Y = ((unsigned int)Temp_XY[3] << 8) + Temp_XY[2];
        Touch_DebugLastX = Touch_X;
        Touch_DebugLastY = Touch_Y;
        Touch_Sta = 1U;
        Touch_DebugDownCount++;
        Touch_DebugTouchSampleCount++;

        if (Touch_ClearReady() == 0U)
        {
            Touch_DebugI2cErrorCount++;
        }
        return TOUCH_SCAN_DOWN;
    }

    Touch_Sta = 0U;
    Touch_DebugReleaseCount++;
    Touch_DebugReleaseSampleCount++;
    if (Touch_ClearReady() == 0U)
    {
        Touch_DebugI2cErrorCount++;
    }
    return TOUCH_SCAN_RELEASE;
}
