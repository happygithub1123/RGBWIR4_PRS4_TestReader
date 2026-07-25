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
    bit system_ready;

    EA = 0; // 所有中断源配置完成前保持总中断关闭
    system_ready = Sys_Init();
    RunLed_Init();

    if (system_ready)
    {
        SensorControl_Init();
        AppComm_Init();
    }

    EA = 1;
    // 主循环只执行短任务轮询，长等待由毫秒时基和状态机拆分
    while (1)
    {
        RunLed_Poll();

        if (system_ready)
        {
            AppComm_Poll();
        }
    }
}
