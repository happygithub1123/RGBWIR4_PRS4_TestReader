#include "System.h"
#include "sensor_control.h"
#include "app_comm.h"
#include "run_led.h"

/**
 * @brief  系统主函数
 * @param  无
 * @retval 无
 */
void main(void)
{
    EA = 0; // 关闭总中断
    Sys_Init(); // 初始化系统基础资源
    RunLed_Init(); // 初始化运行指示灯
    SensorControl_Init(); // 初始化统一传感器控制器
    AppComm_Init(); // 初始化MTF应用通信状态

    EA = 1; // 开启总中断
    // 循环轮询各模块
    while (1)
    {
        RunLed_Poll(); // 轮询运行指示灯
        AppComm_Poll(); // 轮询MTF应用通信
    }
}
