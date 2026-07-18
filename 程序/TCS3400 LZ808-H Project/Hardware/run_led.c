#include "run_led.h"
#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

sbit RUN_LED_PIN = P5 ^ 4;
static uint32 g_run_led_last_ms;

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化运行指示灯
 * @param  无
 * @retval 无
 */
void RunLed_Init(void)
{
    RUN_LED_PIN = 1;
    g_run_led_last_ms = Timer0_GetMs();
}

/**
 * @brief  轮询运行指示灯
 * @param  无
 * @retval 无
 */
void RunLed_Poll(void)
{
    // 使用时间差判断周期，允许32位毫秒计数自然回绕
    if (Timer0_HasElapsed(g_run_led_last_ms, 1000u))
    {
        g_run_led_last_ms = Timer0_GetMs();
        RUN_LED_PIN = !RUN_LED_PIN;
    }
}
