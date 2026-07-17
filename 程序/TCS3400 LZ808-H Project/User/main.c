#include "GPIO.h"
#include "Timer0.h"
#include "tcs3400_app.h"
#include "pressure_sensor_app.h"
#include "action_control.h"

/**
 * @brief 系统主函数
 * @note 完成硬件模块初始化，并在主循环中执行输入输出控制和传感器任务。
 */
void main(void)
{
	 //XtalSelect_Init();  /* 关闭外部时钟采用内部 22.1184M */
    GPIO_Init();          /* 初始化 GPIO 端口工作模式 */

    Pxup_Init();          /* 开启 I2C 引脚内部上拉，确保 4 路 TCS3400 软件 I2C 通信可靠 */

    Yn_Init();            /* 初始化 Y 路输出，并将全部输出关闭 */

    Uart1_Init();         /* 初始化 UART1，用于当前 TCS3400 协议和数据输出 */

    TM0_Init();           /* 初始化 Timer0，提供 1 ms 系统节拍 */

    EA = 1;               /* 开启全局中断，允许 Timer0 和 UART1 中断运行 */

    TCS3400_App_Init();   /* 初始化全部 4 路 TCS3400 传感器 */
    Pressure_Sensor_App_Init(); /* 初始化压力传感器 UART2 + 485 通讯框架 */
	
	

    while (1)
    {
        XY_Control();       /* 检测 X 路输入，并控制对应的 Y 路输出 */

        TCS3400_App_Poll(); /* 处理串口命令并驱动 TCS3400 采样状态机 */
        Pressure_Sensor_App_Poll(); /* 处理压力传感器 485/Modbus 通讯任务 */
    }
}

