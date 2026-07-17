/**
 * @file Uart2.c
 * @brief UART2 串口通信驱动实现
 *
 * @details
 * 本模块只负责 UART2 外设层能力：
 *  - UART2 初始化
 *  - UART2 接收中断
 *  - UART2 环形缓冲区
 *  - 原始字节/缓冲区发送
 *
 * 不负责 485 方向控制和 Modbus 协议处理。
 */

#include "Uart2.h"

/*==============================================================================
 * 宏定义
 *============================================================================*/

#define UART2_TX_TIMEOUT   60000u
#define UART2_RX_BUF_SIZE  64
#define UART2_RX_BUF_MASK  (UART2_RX_BUF_SIZE - 1)

#define UART2_PIN_SEL      UART2_S1
#define UART2_BRT_H        0xFF
#define UART2_BRT_L        0xD0

/*==============================================================================
 * 模块内部变量
 *============================================================================*/

static volatile uint8 idata g_uart2_rx_buf[UART2_RX_BUF_SIZE];
static volatile uint8 g_uart2_rx_wptr = 0;
static volatile uint8 g_uart2_rx_rptr = 0;

/*==============================================================================
 * 模块内部函数声明
 *============================================================================*/

static void Uart2_PinInit(void);

/*==============================================================================
 * 模块内部函数实现
 *============================================================================*/

static void Uart2_PinInit(void)
{
    /* UART2 选择 P1.0=RXD2, P1.1=TXD2 */
    P_SW2 &= (uint8)(~UART2_S2);
    P_SW2 |= UART2_PIN_SEL;

    /* P1.0 准双向输入，P1.1 推挽输出 */
    P1 |= 0x03;
    P1M1 &= (uint8)(~0x03);
    P1M0 = (uint8)((P1M0 & (uint8)(~0x03)) | 0x02);
}

/*==============================================================================
 * 对外开放函数实现
 *============================================================================*/

void Uart2_Init(void)
{
    Uart2_PinInit();

    g_uart2_rx_wptr = 0;
    g_uart2_rx_rptr = 0;

    /* UART2 模式1，8位数据，可变波特率，允许接收 */
    S2CON = S2SM1 | S2REN;

    /* Timer2 作为 UART2 波特率发生器，工作在 1T 模式 */
    AUXR &= (uint8)(~T2R);
    AUXR |= T2x12;
    T2H = UART2_BRT_H;
    T2L = UART2_BRT_L;
    AUXR |= T2R;

    /* 清 UART2 收发标志 */
    S2CON &= (uint8)(~(S2RI | S2TI));

    /* 开启 UART2 中断 */
    IE2 |= ES2;
}

bit Uart2_SendByte(uint8 dat)
{
    uint16 timeout;

    S2CON &= (uint8)(~S2TI);
    S2BUF = dat;

    timeout = UART2_TX_TIMEOUT;
    while ((S2CON & S2TI) == 0)
    {
        if (--timeout == 0)
        {
            return 0;
        }
    }

    S2CON &= (uint8)(~S2TI);
    return 1;
}

bit Uart2_SendBuffer(const uint8 *buf, uint16 len)
{
    uint16 i;

    if ((buf == 0) || (len == 0))
    {
        return 0;
    }

    for (i = 0; i < len; i++)
    {
        if (!Uart2_SendByte(buf[i]))
        {
            return 0;
        }
    }

    return 1;
}

bit Uart2_ReadByte(uint8 *dat)
{
    uint8 rptr;

    if (dat == 0)
    {
        return 0;
    }

    rptr = g_uart2_rx_rptr;
    if (rptr == g_uart2_rx_wptr)
    {
        return 0;
    }

    *dat = g_uart2_rx_buf[rptr];
    g_uart2_rx_rptr = (uint8)((rptr + 1) & UART2_RX_BUF_MASK);

    return 1;
}

/*==============================================================================
 * 中断服务函数实现
 *============================================================================*/

void Uart2_Isr(void) interrupt 8
{
    uint8 dat;
    uint8 next;

    if (S2CON & S2RI)
    {
        dat = S2BUF;
        S2CON &= (uint8)(~S2RI);

        next = (uint8)((g_uart2_rx_wptr + 1) & UART2_RX_BUF_MASK);
        if (next != g_uart2_rx_rptr)
        {
            g_uart2_rx_buf[g_uart2_rx_wptr] = dat;
            g_uart2_rx_wptr = next;
        }
    }

    if (S2CON & S2TI)
    {
        S2CON &= (uint8)(~S2TI);
    }
}
