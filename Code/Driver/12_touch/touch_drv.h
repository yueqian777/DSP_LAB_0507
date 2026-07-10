/**
 * @file touch_drv.h
 * @brief GT1151 touch controller driver interface.
 */

#ifndef _TOUCH_DRV_H_
#define _TOUCH_DRV_H_

#include "sys_iic.h"
#include "touch_scan.h"

#define GT_CTRL_REG     0x8040U
#define GT_CFGS_REG     0x8050U
#define GT_CHECK_REG    0x813CU
#define GT_PID_REG      0x8140U
#define GT_FW_REG       0x8145U
#define GT_GSTID_REG    0x814EU
#define GT_TP1_REG      0x8150U
#define GT_TP2_REG      0x8158U
#define GT_TP3_REG      0x8160U
#define GT_TP4_REG      0x8168U
#define GT_TP5_REG      0x8170U

#define TP_PRES_DOWN    0x80U
#define TP_CATH_PRES    0x40U
#define CT_MAX_TOUCH    5U

#define GT1151_WR_Reg(REG_ADDR, Data, len) \
    IIC_SeqWrite_Reg(REG16, REG_ADDR, Data, len)
#define GT1151_RD_Reg(REG_ADDR, Data, len) \
    IIC_SeqRead_Reg(REG16, REG_ADDR, Data, len)

void Touch_Init(void);
void GT1151_Init(void);
TouchScanResult GT1151_Scan(void);

#endif /* _TOUCH_DRV_H_ */
