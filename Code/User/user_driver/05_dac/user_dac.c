/**
 * @file user_dac.c
 * @brief DAC 用户使用示例文件
 * @details 展示 DAC 的使用方法和示例，包括初始化、启动、停止和数据输出。
 * @ingroup DAC_EXAMPLE
 */

#include "dac_api.h"
#include "system.h"
#include "math.h"
#include "key_api.h"
#include "string.h"

// 定义用户缓冲区
#define DAC_TEST_TONE_CYCLES  (9u)
#define DAC_TEST_AMPLITUDE    (22000.0f)
#define DAC_TEST_PI           (3.14159265358979323846f)
#define DAC_TEST_BYTES        (2u * DAC_SAMPLE_1024)

static short User_Buffer[DAC_SAMPLE_1024];

static void Build_Dac_Test_Tone(void)
{
    unsigned int i;
    float phaseStep;

    phaseStep = (2.0f * DAC_TEST_PI * (float)DAC_TEST_TONE_CYCLES) / (float)DAC_SAMPLE_1024;

    for(i = 0; i < DAC_SAMPLE_1024; i++)
    {
        User_Buffer[i] = (short)(DAC_TEST_AMPLITUDE * sin(phaseStep * (float)i));
    }
}

static void Fill_Dac_Ping_Buffer(void)
{
    memcpy(DA_CH1_Buf0, User_Buffer, DAC_TEST_BYTES);
    memcpy(DA_CH2_Buf0, User_Buffer, DAC_TEST_BYTES);
}

static void Fill_Dac_Pong_Buffer(void)
{
    memcpy(DA_CH1_Buf1, User_Buffer, DAC_TEST_BYTES);
    memcpy(DA_CH2_Buf1, User_Buffer, DAC_TEST_BYTES);
}

static void Fill_Dac_Test_Buffers(void)
{
    Fill_Dac_Ping_Buffer();
    Fill_Dac_Pong_Buffer();
}

// DAC 输出示例
// 展示如何初始化和使用DAC进行数据输出
void Dac_Example(void)
{
    Build_Dac_Test_Tone();

    // 初始化系统
    Sys_Init();
    
    // 初始化按键
    Key_Init();
    
    // 初始化 DAC，设置输出频率为 50kHz，输出长度为 1024，通道选择为功放模块 IN1/IN2
    Dac_Init(DAC_50KHZ, DAC_SAMPLE_1024, DAC_CHANNEL_12);

    // 启动前先填充 Ping/Pong，避免第一帧输出全 0 导致误判为无输出
    Fill_Dac_Test_Buffers();

    // 开始 DAC 输出
    Dac_Start();

    while(1)
    {
        // 检查 DAC 输出完成标志
        if (FLAG_DA == 1)
        {
            // 清除标志位
            FLAG_DA = 0;
            
            // 根据 Ping-Pong 标志确定当前使用的缓冲区
            if (DA_Ping_Pong == DA_BUFFER_PONG)
            {
                // 当前输出 Pong，则更新 Ping 缓冲区
                Fill_Dac_Ping_Buffer();
            }
            else
            {
                // 当前输出 Ping，则更新 Pong 缓冲区
                Fill_Dac_Pong_Buffer();
            }
        }
        // 检查按键标志位
        if (FLAG_KEY1 == 1)
        {
            FLAG_KEY1 = 0;
            // 开始 DAC 输出
            Dac_Start();
        }
        // 检查按键标志位
        if (FLAG_KEY2 == 1)
        {
            FLAG_KEY2 = 0;
            // 停止 DAC 输出
            Dac_Stop();
        }
    }
}
