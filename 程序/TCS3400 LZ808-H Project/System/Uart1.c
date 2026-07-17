#include "Uart1.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define UART1_RX_BUFFER_SIZE  32u
#define UART1_RX_BUFFER_MASK  0x1Fu
#define UART1_TX_TIMEOUT      60000u

static volatile uint8 idata g_uart1_rx_buffer[UART1_RX_BUFFER_SIZE];
static volatile uint8 g_uart1_write_index;
static volatile uint8 g_uart1_read_index;
static volatile bit g_uart1_tx_busy;
static volatile bit g_uart1_rx_overflow;

/**
 * @brief  等待UART1发送完成
 * @param  无
 * @retval 1：发送完成；0：等待超时
 */
static bit Uart1_WaitTxComplete(void)
{
    uint16 timeout;

    timeout = UART1_TX_TIMEOUT;
    while (g_uart1_tx_busy)
    {
        if (--timeout == 0u)
        {
            g_uart1_tx_busy = 0;
            return 0;
        }
    }

    return 1;
}

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化UART1
 * @param  无
 * @retval 无
 */
void Uart1_Init(void)
{
    SCON = 0x50u;
    AUXR |= 0x40u;
    AUXR &= 0xFEu;
    TMOD &= 0x0Fu;
    TL1 = 0xD0u;
    TH1 = 0xFFu;
    ET1 = 0;
    TR1 = 1;

    g_uart1_write_index = 0u;
    g_uart1_read_index = 0u;
    g_uart1_tx_busy = 0;
    g_uart1_rx_overflow = 0;
    RI = 0;
    TI = 0;
    ES = 1;
}

/**
 * @brief  非阻塞读取UART1接收字节
 * @param  dat 接收字节存放地址
 * @retval 1：读取成功；0：缓冲为空或参数无效
 */
bit Uart1_ReadByte(uint8 *dat)
{
    if ((dat == 0) || (g_uart1_read_index == g_uart1_write_index))
    {
        return 0;
    }

    *dat = g_uart1_rx_buffer[g_uart1_read_index];
    g_uart1_read_index = (uint8)((g_uart1_read_index + 1u) & UART1_RX_BUFFER_MASK);
    return 1;
}

/**
 * @brief  清空UART1接收缓冲
 * @param  无
 * @retval 无
 */
void Uart1_ClearRx(void)
{
    bit es_state;

    es_state = ES;
    ES = 0;
    g_uart1_write_index = 0u;
    g_uart1_read_index = 0u;
    g_uart1_rx_overflow = 0;
    ES = es_state;
}

/**
 * @brief  获取并清除UART1接收溢出状态
 * @param  无
 * @retval 1：发生溢出；0：未溢出
 */
bit Uart1_TakeRxOverflow(void)
{
    bit es_state;
    bit overflow;

    es_state = ES;
    ES = 0;
    overflow = g_uart1_rx_overflow;
    g_uart1_rx_overflow = 0;
    ES = es_state;
    return overflow;
}

/**
 * @brief  发送UART1字节
 * @param  dat 待发送字节
 * @retval 1：开始发送；0：等待前一字节超时
 */
bit Uart1_SendByte(uint8 dat)
{
    if (!Uart1_WaitTxComplete())
    {
        return 0;
    }

    g_uart1_tx_busy = 1;
    SBUF = dat;
    return 1;
}

/**
 * @brief  发送UART1缓冲区
 * @param  buffer 待发送缓冲区
 * @param  length 发送长度
 * @retval 1：发送成功；0：参数错误或发送超时
 */
bit Uart1_SendBuffer(const uint8 *buffer, uint8 length)
{
    uint8 index;

    if ((buffer == 0) || (length == 0u))
    {
        return 0;
    }

    for (index = 0u; index < length; index++)
    {
        if (!Uart1_SendByte(buffer[index]))
        {
            return 0;
        }
    }

    return Uart1_WaitTxComplete();
}

/**
 * @brief  UART1中断服务函数
 * @param  无
 * @retval 无
 */
void Uart1_Isr(void) interrupt 4
{
    uint8 dat;
    uint8 next_index;

    if (TI)
    {
        TI = 0;
        g_uart1_tx_busy = 0;
    }

    if (RI)
    {
        RI = 0;
        dat = SBUF;
        next_index = (uint8)((g_uart1_write_index + 1u) & UART1_RX_BUFFER_MASK);

        if (next_index != g_uart1_read_index)
        {
            g_uart1_rx_buffer[g_uart1_write_index] = dat;
            g_uart1_write_index = next_index;
        }
        else
        {
            g_uart1_rx_overflow = 1;
        }
    }
}
