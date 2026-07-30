#include "Rs485.h"
#include "Uart2.h"
#include <intrins.h>

/*==============================================================================
 内部定义
 *============================================================================*/

#define RS485_RECEIVE_LEVEL   0 // 本项目将P0.7拉低，使压力RS485收发器进入接收模式
#define RS485_TRANSMIT_LEVEL  1 // 本项目将P0.7拉高，使压力RS485收发器进入发送模式

// 本项目SN65HVD72的/RE与DE并接到P0.7，低电平接收压力响应，高电平发送请求
sbit RS485_RE_DE_PIN = P0 ^ 7;

/**
 * @brief  提供RS485方向切换保护时间
 * @param  无
 * @retval 无
 */
static void Rs485_TurnaroundDelay(void)
{
    // 本项目中该函数按稳定main保留16次空操作，保护SN65HVD72收发方向切换
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
 * @brief  初始化RS485方向控制并进入接收模式
 * @param  无
 * @retval 无
 */
void Rs485_Init(void)
{
    // 本项目中该函数让压力RS485总线上电进入接收状态并清空旧UART2数据
    RS485_RE_DE_PIN = RS485_RECEIVE_LEVEL; // 空闲时关闭驱动并开启接收
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
    // 本项目中该函数按稳定main完成接收转发送、整帧发送和发送转接收
    bit result;

    if ((buffer == 0) || (length == 0u))
    {
        return 0;
    }

    Uart2_ClearRx(); // 丢弃发送前可能残留的旧响应
    RS485_RE_DE_PIN = RS485_TRANSMIT_LEVEL; // 同时使能DE并关闭/RE
    Rs485_TurnaroundDelay(); // 等待收发器方向稳定后再发送首字节
    result = Uart2_SendBuffer(buffer, length);
    Rs485_TurnaroundDelay(); // 末字节移出后保留总线释放时间
    RS485_RE_DE_PIN = RS485_RECEIVE_LEVEL; // 无论发送结果如何都恢复接收
    return result;
}

/**
 * @brief  非阻塞读取RS485接收字节
 * @param  dat 接收字节存放地址
 * @retval 1：读取成功；0：无数据
 */
bit Rs485_ReadByte(uint8 *dat)
{
    // 本项目中该函数向Modbus层非阻塞提供一个LZ808-H响应字节
    return Uart2_ReadByte(dat);
}

/**
 * @brief  清空RS485接收缓冲
 * @param  无
 * @retval 无
 */
void Rs485_ClearRx(void)
{
    // 本项目中该函数清空UART2旧压力响应，重新建立下一帧接收边界
    Uart2_ClearRx();
}

/**
 * @brief  获取并清除RS485接收溢出状态
 * @param  无
 * @retval 1：发生溢出；0：未溢出
 */
bit Rs485_TakeRxOverflow(void)
{
    // 本项目中该函数向Modbus层报告并清除压力响应缓冲溢出状态
    return Uart2_TakeRxOverflow();
}
