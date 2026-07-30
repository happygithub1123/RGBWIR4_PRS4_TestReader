#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

static volatile uint32 g_system_ms; //保存Timer0中断累加的系统毫秒时基

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化定时器0为1ms系统时基
 * @param  无
 * @retval 无
 */
void TM0_Init(void)
{
    g_system_ms = 0;   // 中断与主循环共享的毫秒时基
    AUXR |= 0x80u;     // Timer0使用1T模式
    TMOD &= 0xF0u;     // Timer0配置为16位自动重装模式
    TH0 = 0xA9u;       // 22.1184 MHz下1 ms重装高字节
    TL0 = 0x9Au;       // 22.1184 MHz下1 ms重装低字节
    TF0 = 0;           // 清除启动前溢出标志
    ET0 = 1;           // 允许Timer0中断
    TR0 = 1;           // 启动Timer0
}

/**
 * @brief  获取系统毫秒计数
 * @param  无
 * @retval 当前毫秒计数
 */
uint32 Timer0_GetMs(void)
{
    bit ea_state;
    uint32 current_ms;

    ea_state = EA;
    EA = 0; // 临界区保证32位计数读取不会被中断拆分
    current_ms = g_system_ms;
    EA = ea_state;
    return current_ms;
}

/**
 * @brief  判断指定时间间隔是否已经到达
 * @param  start_ms 起始毫秒计数
 * @param  period_ms 目标间隔，单位ms
 * @retval 1：已到达；0：未到达
 */
bit Timer0_HasElapsed(uint32 start_ms, uint16 period_ms)
{
    // 无符号时间差允许32位毫秒计数自然回绕
    return ((uint32)(Timer0_GetMs() - start_ms) >= (uint32)period_ms);
}

/**
 * @brief  定时器0中断服务函数
 * @param  无
 * @retval 无
 */
void TM0_Isr(void) interrupt 1
{
    g_system_ms++;
}
