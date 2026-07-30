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
    // 本项目中main只在22.1184MHz外部晶振稳定后启动传感器和ICT协议
    bit system_ready;

    EA = 0; // 本项目所有中断源配置完成前保持总中断关闭
    system_ready = Sys_Init(); // 初始化底层资源并保存外部晶振是否稳定
    RunLed_Init(); // 建立主板运行指示灯的初始电平和1秒计时起点

    if (system_ready)
    {
        SensorControl_Init(); // 晶振稳定后初始化四路TCS3400、压力仪表和Y1至Y4
        AppComm_Init(); // 晶振稳定后清空MTF收帧状态并准备接收第一条命令
    }

    EA = 1; // 开启UART1、UART2和Timer0中断，使通信与毫秒时基开始工作
    // 主循环只轮询运行灯和MTF任务，环境光积分及压力等待由状态机分步完成
    while (1)
    {
        RunLed_Poll(); // 根据Timer0时基每1秒翻转运行灯，表示主循环没有卡死

        if (system_ready)
        {
            AppComm_Poll(); // 仅在系统时钟可靠时推进环境光或压力命令
        }
    }
}
