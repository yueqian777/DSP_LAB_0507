/**
 * @file touch_drv.c
 * @brief GT1151 touch controller initialization.
 */

#include "touch_api.h"
#include "touch_drv.h"
#include "touch_pin.h"
#include "sys_iic.h"

#include "delay.h"

#ifndef TOUCH_ENABLE_INIT_PRINTF
#define TOUCH_ENABLE_INIT_PRINTF 0
#endif

#if TOUCH_ENABLE_INIT_PRINTF
#include "uartStdio.h"
#endif

extern volatile unsigned int AckRolling;

volatile unsigned char Touch_DebugProductId[5] = {0U};
volatile unsigned char Touch_DebugProductIdReadOk = 0U;
volatile unsigned char Touch_DebugConfigVersion = 0U;
volatile unsigned char Touch_DebugConfigSent = 0U;
volatile unsigned char Touch_DebugCtrlWriteOk = 0U;

unsigned char GT1151_CFG_TBL[] =
{
    0x44,0x20,0x03,0xE0,0x01,0x01,0x35,0x04,0x00,0x08,
    0x09,0x0F,0x55,0x37,0x33,0x11,0x00,0x03,0x08,0x56,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x48,0x00,0x00,
    0x3C,0x08,0x0A,0x28,0x1E,0x50,0x00,0x00,0x82,0xB4,
    0xD2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x85,0x25,0x11,0x41,0x43,0x31,
    0x0D,0x00,0xAD,0x22,0x24,0x7D,0x1D,0x1D,0x32,0xDF,
    0x4F,0x44,0x0F,0x80,0x2C,0x50,0x50,0x00,0x00,0x00,
    0x00,0xD3,0x00,0x00,0x00,0x00,0x0F,0x28,0x1E,0xFF,
    0xF0,0x37,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x50,0xB4,0xC0,0x94,0x53,0x2D,
    0x0A,0x02,0xBE,0x60,0xA2,0x71,0x8F,0x82,0x80,0x92,
    0x74,0xA3,0x6B,0x01,0x0F,0x14,0x03,0x1E,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x0C,0x0D,0x0E,0x0F,0x10,
    0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,
    0x1D,0x1F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x19,0x18,0x17,
    0x15,0x14,0x13,0x12,0x0C,0x08,0x06,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,
    0xC4,0x09,0x23,0x23,0x50,0x5D,0x54,0x4B,0x3C,0x0F,
    0x32,0xFF,0xE4,0x04,0x40,0x00,0x8A,0x05,0x40,0x00,
    0xAA,0x00,0x22,0x22,0x00,0x00,0xAA,0x05,0x01,
};

void GT1151_Init(void);
static unsigned char GT1151_Send_Cfg(unsigned char mode);

static unsigned char TouchDrv_ReadReg(unsigned int reg,
                                      unsigned char *data,
                                      unsigned int len)
{
    AckRolling = 0U;
    GT1151_RD_Reg(reg, data, len);
    if (AckRolling != 0U)
    {
        Touch_DebugI2cErrorCount++;
        return 0U;
    }
    return 1U;
}

static unsigned char TouchDrv_WriteReg(unsigned int reg,
                                       unsigned char *data,
                                       unsigned int len)
{
    AckRolling = 0U;
    GT1151_WR_Reg(reg, data, len);
    if (AckRolling != 0U)
    {
        Touch_DebugI2cErrorCount++;
        return 0U;
    }
    return 1U;
}

static void TouchDrv_SaveProductId(const unsigned char *id)
{
    unsigned int i;

    for (i = 0U; i < 5U; i++)
    {
        Touch_DebugProductId[i] = id[i];
    }
}

void Touch_Init(void)
{
    Sys_IIC_Init();
    Touch_Pin_Init_Out();
    GT1151_Init();
    Touch_Pin_Init_In();
}

void GT1151_Init(void)
{
    unsigned char id[5] = {0U};
    unsigned char buff[1];
    unsigned char ctrl_ok;

    IIC_SelectDevice(IIC_DEVICE_TOUCH);

    Touch_Int_PinSet(1U);
    delay(5);
    Touch_Int_PinSet(0U);

    if (TouchDrv_ReadReg(GT_PID_REG, id, 4U) != 0U)
    {
        id[4] = 0U;
        Touch_DebugProductIdReadOk = 1U;
    }
    else
    {
        id[0] = 0U;
        id[1] = 0U;
        id[2] = 0U;
        id[3] = 0U;
        id[4] = 0U;
        Touch_DebugProductIdReadOk = 0U;
    }
    TouchDrv_SaveProductId(id);

#if TOUCH_ENABLE_INIT_PRINTF
    printf("ID:%.4s\r\n", id);
#endif

    buff[0] = 0x02U;
    ctrl_ok = TouchDrv_WriteReg(GT_CTRL_REG, buff, 1U);

    buff[0] = 0U;
    if (TouchDrv_ReadReg(GT_CFGS_REG, buff, 1U) != 0U)
    {
        Touch_DebugConfigVersion = buff[0];
    }

#if TOUCH_ENABLE_INIT_PRINTF
    printf("Previous version: %x\r\n", buff[0]);
#endif

    if (buff[0] < 0x83U)
    {
        Touch_DebugConfigSent = GT1151_Send_Cfg(1U);
    }
    else
    {
        Touch_DebugConfigSent = 0U;
    }

    buff[0] = 0U;
    if (TouchDrv_ReadReg(GT_CFGS_REG, buff, 1U) != 0U)
    {
        Touch_DebugConfigVersion = buff[0];
    }

#if TOUCH_ENABLE_INIT_PRINTF
    printf("Current version: %x\r\n", buff[0]);
#endif

    delay(10);

    buff[0] = 0x00U;
    if ((ctrl_ok != 0U) && (TouchDrv_WriteReg(GT_CTRL_REG, buff, 1U) != 0U))
    {
        Touch_DebugCtrlWriteOk = 1U;
    }
    else
    {
        Touch_DebugCtrlWriteOk = 0U;
    }
}

static unsigned char GT1151_Send_Cfg(unsigned char mode)
{
    unsigned short checksum;
    unsigned char buf[3];
    unsigned int cfg_len;
    unsigned int i;
    unsigned char ok;

    checksum = 0U;
    cfg_len = (unsigned int)(sizeof(GT1151_CFG_TBL) - 3U);
    for (i = 0U; i < cfg_len; i += 2U)
    {
        unsigned short word_value;

        word_value = (unsigned short)((unsigned short)GT1151_CFG_TBL[i] << 8);
        if ((i + 1U) < cfg_len)
        {
            word_value |= GT1151_CFG_TBL[i + 1U];
        }
        checksum = (unsigned short)(checksum + word_value);
    }
    checksum = (unsigned short)((~checksum) + 1U);

    buf[0] = (unsigned char)(checksum >> 8);
    buf[1] = (unsigned char)(checksum & 0xFFU);
    buf[2] = mode;

    ok = TouchDrv_WriteReg(GT_CFGS_REG, GT1151_CFG_TBL, cfg_len);
    if (TouchDrv_WriteReg(GT_CHECK_REG, buf, 3U) == 0U)
    {
        ok = 0U;
    }
    return ok;
}
