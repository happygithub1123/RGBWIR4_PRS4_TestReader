#include "Xtalselect.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define XTAL_STABLE_TIMEOUT  60000u

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  启动并选择外部晶振
 * @param  无
 * @retval 1：切换成功；0：晶振稳定超时
 */
bit XtalSelect_Init(void)
{
    uint16 timeout;

    P_SW2 |= 0x80u;
    XOSCCR = 0xC0u;
    timeout = XTAL_STABLE_TIMEOUT;

    while (((XOSCCR & 0x01u) == 0u) && (timeout > 0u))
    {
        timeout--;
    }

    if (timeout == 0u)
    {
        P_SW2 &= 0x7Fu;
        return 0;
    }

    CLKDIV = 0x00u;
    CLKSEL = 0x01u;
    P_SW2 &= 0x7Fu;
    return 1;
}
