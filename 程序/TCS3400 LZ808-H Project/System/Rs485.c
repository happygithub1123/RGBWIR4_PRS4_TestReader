#include "Rs485.h"
#include "Uart2.h"
#include <intrins.h>

/*==============================================================================
 内部定义
 *============================================================================*/

sbit RS485_DIRECTION_PIN = P0 ^ 7;

/**
 * @brief  提供RS485方向切换保护时间
 * @param  无
 * @retval 无
 */
static void Rs485_TurnaroundDelay(void)
{
    uint8 count;

    for (count = 0u; count < 16u; count++)
    {
        _nop_();
    }
}

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化RS485方向控制
 * @param  无
 * @retval 无
 */
void Rs485_Init(void)
{
    RS485_DIRECTION_PIN = 0;
    Uart2_ClearRx();
}

/**
 * @brief  发送一帧RS485数据
 * @param  buffer 待发送缓冲区
 * @param  length 发送长度
 * @retval 1：发送成功；0：参数错误或UART2超时
 */
bit Rs485_SendBuffer(const uint8 *buffer, uint8 length)
{
    bit result;

    if ((buffer == 0) || (length == 0u))
    {
        return 0;
    }

    Uart2_ClearRx();
    RS485_DIRECTION_PIN = 1;
    Rs485_TurnaroundDelay();
    result = Uart2_SendBuffer(buffer, length);
    Rs485_TurnaroundDelay();
    RS485_DIRECTION_PIN = 0;
    return result;
}

/**
 * @brief  非阻塞读取RS485接收字节
 * @param  dat 接收字节存放地址
 * @retval 1：读取成功；0：无数据
 */
bit Rs485_ReadByte(uint8 *dat)
{
    return Uart2_ReadByte(dat);
}

/**
 * @brief  清空RS485接收缓冲
 * @param  无
 * @retval 无
 */
void Rs485_ClearRx(void)
{
    Uart2_ClearRx();
}

/**
 * @brief  获取并清除RS485接收溢出状态
 * @param  无
 * @retval 1：发生溢出；0：未溢出
 */
bit Rs485_TakeRxOverflow(void)
{
    return Uart2_TakeRxOverflow();
}
