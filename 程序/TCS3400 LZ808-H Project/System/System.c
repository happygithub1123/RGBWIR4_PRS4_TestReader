#include "System.h"
#include "Xtalselect.h"
#include "GPIO.h"
#include "Uart1.h"
#include "Uart2.h"
#include "Rs485.h"
#include "Timer0.h"

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化系统基础资源
 * @param  无
 * @retval 1：初始化成功；0：外部晶振启动失败
 */
bit Sys_Init(void)
{
    // 本项目中该函数保存外部晶振状态，并按稳定main的顺序初始化其余系统资源
    bit clock_ready;

    clock_ready = XtalSelect_Init();
    GPIO_Init();
    Uart1_Init();
    Uart2_Init();
    Rs485_Init();
    TM0_Init();
    return clock_ready;
}
