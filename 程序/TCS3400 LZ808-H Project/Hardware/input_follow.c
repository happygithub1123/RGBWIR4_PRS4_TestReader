#include "input_follow.h"

/*==============================================================================
 内部定义
 *============================================================================*/

sbit INPUT_1_PIN = P5 ^ 1;
sbit INPUT_2_PIN = P3 ^ 5;
sbit INPUT_3_PIN = P3 ^ 6;
sbit INPUT_4_PIN = P3 ^ 7;
sbit OUTPUT_1_PIN = P3 ^ 2;
sbit OUTPUT_2_PIN = P3 ^ 3;
sbit OUTPUT_3_PIN = P3 ^ 4;
sbit OUTPUT_4_PIN = P5 ^ 0;

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化四路跟随输出
 * @param  无
 * @retval 无
 */
void InputFollow_Init(void)
{
    OUTPUT_1_PIN = 0;
    OUTPUT_2_PIN = 0;
    OUTPUT_3_PIN = 0;
    OUTPUT_4_PIN = 0;
}

/**
 * @brief  更新四路输入到输出跟随状态
 * @param  无
 * @retval 无
 */
void InputFollow_Run(void)
{
    OUTPUT_1_PIN = (INPUT_1_PIN == 0) ? 1 : 0;
    OUTPUT_2_PIN = (INPUT_2_PIN == 0) ? 1 : 0;
    OUTPUT_3_PIN = (INPUT_3_PIN == 0) ? 1 : 0;
    OUTPUT_4_PIN = (INPUT_4_PIN == 0) ? 1 : 0;
}
