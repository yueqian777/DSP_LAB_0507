/**
 * @file touch_pin.c
 * @brief GT1151 interrupt pin setup.
 */

#include "touch_pin.h"

#include "soc_C6748.h"
#include "hw_types.h"
#include "hw_syscfg0_C6748.h"
#include "interrupt.h"
#include "psc.h"
#include "gpio.h"

volatile unsigned char FLAG_TOUCH = 0U;
volatile unsigned long Touch_DebugInterruptCount = 0UL;

static void PSCInit(void);
static void GPIOBankPinMuxSet(void);
static void GPIOBankPinInit_Out(void);
static void GPIOBankPinInit_In(void);
static void GPIOBankPinInterruptInit(void);
static void TouchIsr(void);

void Touch_Pin_Init_Out(void)
{
    PSCInit();
    GPIOBankPinMuxSet();
    GPIOBankPinInit_Out();
}

void Touch_Pin_Init_In(void)
{
    GPIOBankPinInit_In();
    GPIOBankPinInterruptInit();
}

static void PSCInit(void)
{
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_GPIO,
                     PSC_POWERDOMAIN_ALWAYS_ON,
                     PSC_MDCTL_NEXT_ENABLE);
}

static void GPIOBankPinMuxSet(void)
{
    volatile unsigned int savePinMux;

    savePinMux = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(18)) &
                 ~(TOUCH_INT_MASK);
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(18)) =
        (TOUCH_INT_ENABLE | savePinMux);
}

static void GPIOBankPinInit_Out(void)
{
    GPIODirModeSet(SOC_GPIO_0_REGS, TOUCH_INT_PIN, GPIO_DIR_OUTPUT);
}

static void GPIOBankPinInit_In(void)
{
    GPIODirModeSet(SOC_GPIO_0_REGS, TOUCH_INT_PIN, GPIO_DIR_INPUT);
}

void Touch_Int_PinSet(unsigned int bitState)
{
    if (bitState == 1U)
    {
        GPIOPinWrite(SOC_GPIO_0_REGS, TOUCH_INT_PIN, GPIO_PIN_HIGH);
    }
    else
    {
        GPIOPinWrite(SOC_GPIO_0_REGS, TOUCH_INT_PIN, GPIO_PIN_LOW);
    }
}

static void GPIOBankPinInterruptInit(void)
{
    GPIOIntTypeSet(SOC_GPIO_0_REGS, TOUCH_INT_PIN, GPIO_INT_TYPE_FALLEDGE);
    GPIOBankIntEnable(SOC_GPIO_0_REGS, 8U);
    IntRegister(C674X_MASK_INT13, TouchIsr);
    IntEventMap(C674X_MASK_INT13, SYS_INT_GPIO_B8INT);
    IntEnable(C674X_MASK_INT13);
}

static void TouchIsr(void)
{
    GPIOBankIntDisable(SOC_GPIO_0_REGS, 8U);
    IntEventClear(SYS_INT_GPIO_B8INT);
    GPIOPinIntClear(SOC_GPIO_0_REGS, TOUCH_INT_PIN);
    Touch_DebugInterruptCount++;
    FLAG_TOUCH = 1U;
    GPIOBankIntEnable(SOC_GPIO_0_REGS, 8U);
}
