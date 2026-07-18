#include "Uart2.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define UART2_RX_BUFFER_SIZE  32u
#define UART2_RX_BUFFER_MASK  0x1Fu
#define UART2_TX_TIMEOUT      60000u

static volatile uint8 idata g_uart2_rx_buffer[UART2_RX_BUFFER_SIZE];
static volatile uint8 g_uart2_write_index;
static volatile uint8 g_uart2_read_index;
static volatile bit g_uart2_tx_busy;
static volatile bit g_uart2_rx_overflow;

/**
 * @brief  等待UART2发送完成
 * @param  无
 * @retval 1：发送完成；0：等待超时
 */
static bit Uart2_WaitTxComplete(void)
{
    uint16 timeout;

    timeout = UART2_TX_TIMEOUT;
    // 有限等待防止发送中断异常时永久阻塞主循环
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
    P_SW2 &= (uint8)(~UART2_S2);
    P_SW2 |= UART2_S1; // UART2切换到P1.0 RXD2和P1.1 TXD2

    S2CON = S2SM1 | S2REN; // UART2模式1，8N1并允许接收
    AUXR &= (uint8)(~T2R); // 配置重装值前停止Timer2
    AUXR |= T2x12;         // Timer2使用1T模式
    T2H = 0xFFu;           // 22.1184 MHz下115200 bps重装高字节
    T2L = 0xD0u;           // 22.1184 MHz下115200 bps重装低字节
    AUXR |= T2R;           // 启动UART2波特率发生器

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
    uint8 interrupt_state;

    interrupt_state = IE2 & ES2;
    IE2 &= (uint8)(~ES2); // 临界区防止中断同时修改环形缓冲索引
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
    uint8 interrupt_state;
    bit overflow;

    interrupt_state = IE2 & ES2;
    IE2 &= (uint8)(~ES2); // 原子读取并清除中断更新的溢出标志
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
    if (!Uart2_WaitTxComplete())
    {
        return 0;
    }

    g_uart2_tx_busy = 1; // S2TI中断在硬件发送完成后清除此标志
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
    uint8 index;

    if ((buffer == 0) || (length == 0u))
    {
        return 0;
    }

    // 逐字节等待前一字节完成，保证整帧发送顺序
    for (index = 0u; index < length; index++)
    {
        if (!Uart2_SendByte(buffer[index]))
        {
            return 0;
        }
    }

    return Uart2_WaitTxComplete();
}

/**
 * @brief  UART2中断服务函数
 * @param  无
 * @retval 无
 */
void Uart2_Isr(void) interrupt 8
{
    uint8 dat;
    uint8 next_index;

    if (S2CON & S2RI)
    {
        S2CON &= (uint8)(~S2RI);
        dat = S2BUF;
        next_index = (uint8)((g_uart2_write_index + 1u) & UART2_RX_BUFFER_MASK); // 32字节环形回绕

        // 保留一个空槽区分空与满，实际最多缓存31字节
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
