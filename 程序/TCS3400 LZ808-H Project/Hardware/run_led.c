#include "run_led.h"
#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

sbit RUN_LED_PIN = P5 ^ 4;
static uint32 g_run_led_last_ms; // 记录主板运行灯上一次翻转时的系统毫秒值

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
    // 本项目中该函数设置运行灯上电电平，并记录第一次1秒翻转的计时起点
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
    // 本项目中该函数用Timer0时基每1秒翻转运行灯，直观显示主循环仍在执行
    // 用Timer0时间差每1秒翻转运行灯，即使32位毫秒计数回绕也保持正确周期
    if (Timer0_HasElapsed(g_run_led_last_ms, 1000u))
    {
        g_run_led_last_ms = Timer0_GetMs();
        RUN_LED_PIN = !RUN_LED_PIN;
    }
}
