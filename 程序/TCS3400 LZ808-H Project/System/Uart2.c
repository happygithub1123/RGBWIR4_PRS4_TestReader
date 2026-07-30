#include "Uart2.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define UART2_RX_BUFFER_SIZE  32u    // 本项目缓存压力仪表Modbus响应字节的UART2环形缓冲大小
#define UART2_RX_BUFFER_MASK  0x1Fu  // UART2读写索引在32字节Modbus接收缓冲中快速回绕
#define UART2_TX_TIMEOUT      60000u // UART2发送中断异常时限制RS485请求等待循环次数

static volatile uint8 idata g_uart2_rx_buffer[UART2_RX_BUFFER_SIZE]; // 缓存UART2中断收到的LZ808-H Modbus响应字节
static volatile uint8 g_uart2_write_index;                           // 记录中断写入压力响应环形缓冲的位置
static volatile uint8 g_uart2_read_index;                            // 记录主循环读取压力响应环形缓冲的位置
static volatile bit g_uart2_tx_busy;                                 // 标记压力Modbus请求的前一个字节仍在硬件发送
static volatile bit g_uart2_rx_overflow;                             // 标记压力响应字节超过UART2环形缓冲容量

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化UART2
 * @param  无
 * @retval 无
 */
void Uart2_Init(void)
{
    // 本项目中该函数把UART2配置为LZ808-H RS485通信使用的115200bps、8N1
    P_SW2 &= (uint8)(~UART2_S2);
    P_SW2 |= UART2_S1; // 将压力UART2固定映射到P1.0 RXD2和P1.1 TXD2

    S2CON = S2SM1 | S2REN; // UART2使用模式1、8N1并允许接收LZ808-H响应
    AUXR &= (uint8)(~T2R); // 写入压力串口重装值前停止Timer2，避免产生错误波特率
    AUXR |= T2x12;         // Timer2使用1T模式，匹配压力通信115200bps重装值
    T2H = 0xFFu;           // 写入22.1184MHz下115200bps的Timer2高字节重装值
    T2L = 0xD0u;           // 写入22.1184MHz下115200bps的Timer2低字节重装值
    AUXR |= T2R;           // 启动Timer2，为压力UART2持续提供115200bps波特率

    g_uart2_write_index = 0u;
    g_uart2_read_index = 0u;
    g_uart2_tx_busy = 0;
    g_uart2_rx_overflow = 0;
    S2CON &= (uint8)(~(S2RI | S2TI));
    IE2 |= ES2;
}

/**
 * @brief  非阻塞读取UART2接收字节
 * @param  dat 接收字节存放地址
 * @retval 1：读取成功；0：缓冲为空或参数无效
 */
bit Uart2_ReadByte(uint8 *dat)
{
    // 本项目中该函数由Modbus状态机非阻塞取出一个压力仪表响应字节
    if ((dat == 0) || (g_uart2_read_index == g_uart2_write_index))
    {
        return 0;
    }

    *dat = g_uart2_rx_buffer[g_uart2_read_index];
    g_uart2_read_index = (uint8)((g_uart2_read_index + 1u) & UART2_RX_BUFFER_MASK);
    return 1;
}

/**
 * @brief  清空UART2接收缓冲
 * @param  无
 * @retval 无
 */
void Uart2_ClearRx(void)
{
    // 本项目中该函数在临界区清空压力响应索引和溢出标志，重新建立Modbus帧边界
    uint8 interrupt_state;

    interrupt_state = IE2 & ES2;
    IE2 &= (uint8)(~ES2); // 暂停UART2中断，防止清空压力缓冲时索引被接收中断改写
    g_uart2_write_index = 0u;
    g_uart2_read_index = 0u;
    g_uart2_rx_overflow = 0;
    IE2 |= interrupt_state;
}

/**
 * @brief  获取并清除UART2接收溢出状态
 * @param  无
 * @retval 1：发生溢出；0：未溢出
 */
bit Uart2_TakeRxOverflow(void)
{
    // 本项目中该函数原子取走UART2溢出状态，供Modbus层判定当前单次压力请求失败
    uint8 interrupt_state;
    bit overflow;

    interrupt_state = IE2 & ES2;
    IE2 &= (uint8)(~ES2); // 暂停UART2中断，原子取走并清除压力响应溢出标志
    overflow = g_uart2_rx_overflow;
    g_uart2_rx_overflow = 0;
    IE2 |= interrupt_state;
    return overflow;
}

/**
 * @brief  发送UART2字节
 * @param  dat 待发送字节
 * @retval 1：开始发送；0：等待前一字节超时
 */
bit Uart2_SendByte(uint8 dat)
{
    // 本项目中该函数等待前一字节完成后把下一个压力Modbus请求字节写入S2BUF
    uint16 timeout;

    timeout = UART2_TX_TIMEOUT;
    // 等前一个Modbus请求字节发送完成后再写S2BUF，防止8字节帧被覆盖
    while (g_uart2_tx_busy)
    {
        if (--timeout == 0u)
        {
            g_uart2_tx_busy = 0;
            return 0;
        }
    }

    g_uart2_tx_busy = 1; // 标记压力请求字节已交给硬件，S2TI完成后再允许下一字节
    S2BUF = dat;
    return 1;
}

/**
 * @brief  发送UART2缓冲区并等待完成
 * @param  buffer 待发送缓冲区
 * @param  length 发送长度
 * @retval 1：发送成功；0：参数错误或发送超时
 */
bit Uart2_SendBuffer(const uint8 *buffer, uint8 length)
{
    // 本项目中该函数按顺序发送完整压力请求，并等待CRC末字节真正移出硬件
    uint8 index;
    uint16 timeout;

    if ((buffer == 0) || (length == 0u))
    {
        return 0;
    }

    // 逐字节等待UART2硬件完成，保证8字节压力Modbus请求顺序不乱
    for (index = 0u; index < length; index++)
    {
        if (!Uart2_SendByte(buffer[index]))
        {
            return 0;
        }
    }

    timeout = UART2_TX_TIMEOUT;
    // 等压力请求最后一个CRC字节移出硬件后再切换RS485为接收
    while (g_uart2_tx_busy)
    {
        if (--timeout == 0u)
        {
            g_uart2_tx_busy = 0;
            return 0;
        }
    }

    return 1;
}

/**
 * @brief  UART2中断服务函数
 * @param  无
 * @retval 无
 */
void Uart2_Isr(void) interrupt 8
{
    // 本项目中UART2中断只维护压力收发字节和环形缓冲，不校验Modbus业务
    uint8 dat;
    uint8 next_index;

    if (S2CON & S2RI)
    {
        S2CON &= (uint8)(~S2RI);
        dat = S2BUF;
        next_index = (uint8)((g_uart2_write_index + 1u) & UART2_RX_BUFFER_MASK); // 让压力响应写索引在32字节缓冲内回绕

        // 保留一个空槽区分压力缓冲空和满，最多保存31个未处理字节
        if (next_index != g_uart2_read_index)
        {
            g_uart2_rx_buffer[g_uart2_write_index] = dat;
            g_uart2_write_index = next_index;
        }
        else
        {
            g_uart2_rx_overflow = 1;
        }
    }

    if (S2CON & S2TI)
    {
        S2CON &= (uint8)(~S2TI);
        g_uart2_tx_busy = 0;
    }
}
