/**
 * user_subband_flow.h
 *
 * 描述: 子频带分析-合成实时测试例程头文件
 * 功能: 提供 1024 点 short 帧处理接口和板端 AD/DA 测试入口
 */

#ifndef _USER_SUBBAND_FLOW_H_
#define _USER_SUBBAND_FLOW_H_

#ifndef ADDA_SUBBAND_BYPASS
#define ADDA_SUBBAND_BYPASS 0
#endif

#define SUBBAND_D        8

#ifdef SUBBAND_FLOW_ALGO_ONLY
#define SUBBAND_FRM_LEN  1024
#else
#include "adc_api.h"
#define SUBBAND_FRM_LEN  ADC_SAMPLE_1024
#endif

#define SUBBAND_LEN      (SUBBAND_FRM_LEN / SUBBAND_D)

void Subband_Process_1024(short *in, short *out);

#ifndef SUBBAND_FLOW_ALGO_ONLY
void Subband_Flow_Example(void);
#endif

#endif /* _USER_SUBBAND_FLOW_H_ */
