#include "System.h"
#include "sensor_control.h"
#include "app_comm.h"
#include "input_follow.h"
#include "run_led.h"

/**
 * @brief  系统主函数
 * @param  无
 * @retval 无
 */
void main(void)
{
    bit system_ready;

    EA = 0;
    system_ready = Sys_Init();
    InputFollow_Init();
    RunLed_Init();

    if (system_ready)
    {
        SensorControl_Init();
        AppComm_Init();
    }

    EA = 1;
    while (1)
    {
        InputFollow_Run();
        RunLed_Poll();

        if (system_ready)
        {
            AppComm_Poll();
        }
    }
}
