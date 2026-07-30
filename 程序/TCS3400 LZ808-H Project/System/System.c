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
 * @retval 无
 */
void Sys_Init(void)
{
    XtalSelect_Init();
    GPIO_Init();
    Uart1_Init();
    Uart2_Init();
    Rs485_Init();
    TM0_Init();
}
