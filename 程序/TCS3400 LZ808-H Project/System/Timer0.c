#include "Timer0.h"

/* P5.4 运行指示灯计数器，单位ms */
static uint16 g_run_led_ms = 0;

/* 系统节拍计数器，供App层等待和后台采样使用 */
static volatile uint32 g_system_ms = 0;

void TM0_Init(void)
{
    /* P5.4 初始化为推挽输出，驱动运行指示灯 */
    P54 = 0;
    P5M1 &= ~0x10;
    P5M0 |= 0x10;

    /* Timer0 使用 1T 模式16位自动重装*/
    AUXR |= 0x80;
    TMOD &= 0xF0;

    /*
     * 内部 22.1184MHz,1T模式下，1ms约22118计数脉冲     * 重装值 = 65536 - 22118 = 0xA99A     */
    TH0 = 0xA9;
    TL0 = 0x9A;

    /* 清零溢出标志，开启Timer0中断，启动Timer0 */
    TF0 = 0;
    ET0 = 1;
    TR0 = 1;
}

uint32 Timer0_GetMs(void)
{
    uint32 now;
    bit old_ea;

    /* 读取32位系统运行毫秒数时关中断，防止中断中途修改*/
    old_ea = EA;
    EA = 0;
    now = g_system_ms;
    EA = old_ea;

    return now;
}

void TM0_Isr(void) interrupt 1
{
    /* 每次中断累加1ms系统节拍*/
    g_system_ms++;

    /* P5.4 每500ms翻转一次，用于确认程序正常运行 */
    if (++g_run_led_ms >= 1000)
    {
        g_run_led_ms = 0;
        P54 = !P54;
    }
}
