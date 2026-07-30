#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

static volatile uint32 g_system_ms; // 保存环境光、压力、MTF超时和运行灯共用的毫秒时基

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
    // 本项目中该函数把Timer0配置成所有非阻塞任务共用的1ms系统时基
    g_system_ms = 0;   // 主板上电时将所有非阻塞任务的毫秒起点清零
    AUXR |= 0x80u;     // Timer0使用1T模式，匹配本项目1ms重装值
    TMOD &= 0xF0u;     // Timer0使用16位自动重装，持续产生1ms中断
    TH0 = 0xA9u;       // 写入22.1184MHz下1ms计时的高字节重装值
    TL0 = 0x9Au;       // 写入22.1184MHz下1ms计时的低字节重装值
    TF0 = 0;           // 启动前清除旧溢出标志，防止系统毫秒数误加一次
    ET0 = 1;           // 允许Timer0每1ms进入中断并更新项目时基
    TR0 = 1;           // 启动Timer0，使状态机超时和运行灯开始计时
}

/**
 * @brief  获取系统毫秒计数
 * @param  无
 * @retval 当前毫秒计数
 */
uint32 Timer0_GetMs(void)
{
    // 本项目中该函数在临界区原子读取32位毫秒数，避免8位MCU读到撕裂数据
    bit ea_state;
    uint32 current_ms;

    ea_state = EA;
    EA = 0; // 暂停中断，防止8位MCU读取32位项目时基时被Timer0拆分
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
    // 本项目中该函数统一判断MTF、TCS3400、Modbus和运行灯的等待时间是否到达
    // 使用无符号时间差，使环境光、压力和运行灯在32位计数回绕后仍能正确计时
    return ((uint32)(Timer0_GetMs() - start_ms) >= (uint32)period_ms);
}

/**
 * @brief  定时器0中断服务函数
 * @param  无
 * @retval 无
 */
void TM0_Isr(void) interrupt 1
{
    // 本项目中Timer0中断只累加毫秒时基，不在中断内执行任何传感器业务
    g_system_ms++;
}
