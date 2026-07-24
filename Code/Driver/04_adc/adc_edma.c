/**
 * @file adc_edma.c
 * @brief ADC EDMA控制实现文件
 * @details 实现ADC的EDMA控制功能，包括初始化、启动和停止操作。
 * @ingroup ADC_EDMA
 */

/* 头文件包含 */
#include "adc_edma.h"
#include "adc_api.h"
#include "adc_drv.h"
#include "soc_C6748.h"
#include "edma.h"
#include "edma_event.h"
#include "edma0_common.h"

/* EDMA程序参数 */
unsigned int chType     = EDMA3_CHANNEL_TYPE_DMA;
unsigned int chNum      = EDMA3_CHA_GPIO_BNKINT5;
unsigned int tccNum     = EDMA3_CHA_GPIO_BNKINT5;
unsigned int edmaTC     = 0;
unsigned int syncType   = EDMA3_SYNC_AB;
unsigned int trigMode   = EDMA3_TRIG_MODE_EVENT;
unsigned int evtQ_adc   = 0;
volatile unsigned int ADC_EDMA_CallbackRegistrationError = 0u;

typedef char ADC_EDMA_TccRangeCheck[
    (EDMA3_NUM_TCC > EDMA3_CHA_GPIO_BNKINT5) ? 1 : -1];

static ADC_BLOCK_CALLBACK s_adcBlockCallback = (ADC_BLOCK_CALLBACK)0;
static unsigned int s_adcBlockSampleLen = 0u;

/* EDMA参数配置 */
static struct EDMA3CCPaRAMEntry dmaPar[3] = {
    {
        (unsigned int)(1<<2 | 1<<20 | \
        (EDMA3_CHA_GPIO_BNKINT5 << EDMA3CC_OPT_TCC_SHIFT)),// Opt
        (unsigned int)SOC_EMIFA_CS2_ADDR,                   // 源地址
        (unsigned short)(MAX_ACOUNT),                       // aCnt
        (unsigned short)(MAX_BCOUNT),                       // bCnt
        (unsigned int) AD_Buffer_Ping,                      // 目标地址
        (short)(MAX_ACOUNT),                                // 源 bIdx
        (short)(BUF_SIZE_MAX),                              // 目标 bIdx
        (unsigned short)(32u * 40u),                        // 链接地址
        (unsigned short)(MAX_BCOUNT),                       // bCnt 重装值
        (short)(MAX_ACOUNT * MAX_BCOUNT),                   // 源 cIdx
        (short)(MAX_ACOUNT),                                // 目标 cIdx
        (unsigned short)MAX_CCOUNT                          // cCnt
    },
    {
        (unsigned int)(1<<2 | 1<<20 | \
        (EDMA3_CHA_GPIO_BNKINT5 << EDMA3CC_OPT_TCC_SHIFT)),// Opt
        (unsigned int)SOC_EMIFA_CS2_ADDR,                   // 源地址
        (unsigned short)(MAX_ACOUNT),                       // aCnt
        (unsigned short)(MAX_BCOUNT),                       // bCnt
        (unsigned int) AD_Buffer_Pong,                      // 目标地址
        (short)(MAX_ACOUNT),                                // 源 bIdx
        (short)(BUF_SIZE_MAX),                              // 目标 bIdx
        (unsigned short)(32u * 41u),                        // 链接地址
        (unsigned short)(MAX_BCOUNT),                       // bCnt 重装值
        (short)(MAX_ACOUNT * MAX_BCOUNT),                   // 源 cIdx
        (short)(MAX_ACOUNT),                                // 目标 cIdx
        (unsigned short)MAX_CCOUNT                          // cCnt
    },
    {
        (unsigned int)(1<<2 | 1<<20 | \
        (EDMA3_CHA_GPIO_BNKINT5 << EDMA3CC_OPT_TCC_SHIFT)),// Opt
        (unsigned int)SOC_EMIFA_CS2_ADDR,                   // 源地址
        (unsigned short)(MAX_ACOUNT),                       // aCnt
        (unsigned short)(MAX_BCOUNT),                       // bCnt
        (unsigned int) AD_Buffer_Ping,                      // 目标地址
        (short)(MAX_ACOUNT),                                // 源 bIdx
        (short)(BUF_SIZE_MAX),                              // 目标 bIdx
        (unsigned short)(32u * 40u),                        // 链接地址
        (unsigned short)(MAX_BCOUNT),                       // bCnt 重装值
        (short)(MAX_ACOUNT * MAX_BCOUNT),                   // 源 cIdx
        (short)(MAX_ACOUNT),                                // 目标 cIdx
        (unsigned short)MAX_CCOUNT                          // cCnt
    }
};

/* 函数声明 */
// 申请DMA通道和TCC
static void RequestEDMA3Channels(void);
// 注册回调函数、初始化DMA参数、使能传输
static void AD7606Edma3Init(unsigned int sampleLen);
// EDMA回调函数
static void callback_adc(unsigned int tccNum, unsigned int status, void *appData);


// 初始化ADC EDMA传输
void Adc_EDMA_Init(unsigned int sampleLen)
{
    s_adcBlockSampleLen = sampleLen;
    RequestEDMA3Channels();
    AD7606Edma3Init(sampleLen);
}

void Adc_RegisterBlockCallback(ADC_BLOCK_CALLBACK callback)
{
    s_adcBlockCallback = callback;
}

// 申请DMA通道和TCC
static void RequestEDMA3Channels(void)
{
    // 初始化事件队列
    EDMA3Init(SOC_EDMA30CC_0_REGS, evtQ_adc);
    
    // 申请DMA通道和TCC
    EDMA3RequestChannel(SOC_EDMA30CC_0_REGS, EDMA3_CHANNEL_TYPE_DMA, \
            EDMA3_CHA_GPIO_BNKINT5, EDMA3_CHA_GPIO_BNKINT5, evtQ_adc);
}

// 注册回调函数、初始化DMA参数、使能传输
// sampleLen 采样长度
static void AD7606Edma3Init(unsigned int sampleLen)
{
    unsigned int i;
    unsigned int *pCcount;

    if (tccNum >= EDMA3_NUM_TCC)
    {
        ADC_EDMA_CallbackRegistrationError = 1u;
        return;
    }

    ADC_EDMA_CallbackRegistrationError = 0u;

    // 注册回调函数
    cb_Fxn[tccNum] = &callback_adc;
    Edma0_Common_EnableDspTcc(tccNum);

    // 在调用EDMA3SetPaRAM之前，修改dmaPar中的CCOUNT
    for (i = 0; i < 3; i++) {
        // 获取当前结构体中CCOUNT的地址
        pCcount = (unsigned int *)(&(dmaPar[i].cCnt));
        memcpy(pCcount, &sampleLen, sizeof(unsigned int));
    }

    // 初始化DMA参数
    EDMA3SetPaRAM(SOC_EDMA30CC_0_REGS, chNum,
                (struct EDMA3CCPaRAMEntry *)(&(dmaPar[0])));
    EDMA3SetPaRAM(SOC_EDMA30CC_0_REGS, 40,
                (struct EDMA3CCPaRAMEntry *)(&(dmaPar[1])));
    EDMA3SetPaRAM(SOC_EDMA30CC_0_REGS, 41,
                (struct EDMA3CCPaRAMEntry *)(&(dmaPar[2])));

    // 按照计算的次数使能传输
    EDMA3EnableTransfer(SOC_EDMA30CC_0_REGS, chNum, trigMode);
}

// EDMA回调函数
// tccNum TCC编号
// status 传输状态
// appData 应用数据
static void callback_adc(unsigned int tccNum, unsigned int status, void *appData)
{
    (void)tccNum;
    (void)appData;

    if(EDMA3_XFER_COMPLETE == status)
    {
        // 传输成功
        FLAG_AD = 1;
        
        AD_Ping_Pong = !AD_Ping_Pong;

        if (s_adcBlockCallback != (ADC_BLOCK_CALLBACK)0)
        {
            unsigned char completedBuffer =
                (AD_Ping_Pong == AD_BUFFER_PONG) ?
                AD_BUFFER_PING : AD_BUFFER_PONG;
            s_adcBlockCallback(completedBuffer, s_adcBlockSampleLen);
        }
    }

    else if(EDMA3_CC_DMA_EVT_MISS == status)
    {
        // 传输导致DMA事件丢失错误
        FLAG_AD = 2;
    }

    else if(EDMA3_CC_QDMA_EVT_MISS == status)
    {
        // 传输导致QDMA事件丢失错误
        FLAG_AD = 3;
    }
}
