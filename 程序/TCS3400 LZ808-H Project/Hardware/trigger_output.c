#include "trigger_output.h"

/*==============================================================================
 内部定义
 *============================================================================*/

sbit Y1_CONTROL_PIN = P3 ^ 2;
sbit Y2_CONTROL_PIN = P3 ^ 3;
sbit Y3_CONTROL_PIN = P3 ^ 4;
sbit Y4_CONTROL_PIN = P5 ^ 0;

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化四路压力触发输出
 * @param  无
 * @retval 无
 */
void TriggerOutput_Init(void)
{
    // MCU低电平使ULN2003截止，外部Y端保持不触发
    Y1_CONTROL_PIN = 0;
    Y2_CONTROL_PIN = 0;
    Y3_CONTROL_PIN = 0;
    Y4_CONTROL_PIN = 0;
}

/**
 * @brief  设置指定压力通道的触发状态
 * @param  channel 压力逻辑通道1~4，对应Y1~Y4
 * @param  active 1：外部Y端输出低电平；0：释放外部Y端
 * @retval 无
 */
void TriggerOutput_Set(uint8 channel, uint8 active)
{
    uint8 pin_level;

    pin_level = (active != 0u) ? 1u : 0u;
    switch (channel)
    {
        case 1u: Y1_CONTROL_PIN = pin_level; break;
        case 2u: Y2_CONTROL_PIN = pin_level; break;
        case 3u: Y3_CONTROL_PIN = pin_level; break;
        case 4u: Y4_CONTROL_PIN = pin_level; break;
        default: break;
    }
}
