/**
 * @file Uart.c
 * @brief UART1 串口通信驱动实现
 *
 * @details
 * 本模块负责 UART1 引脚配置、波特率配置、字节和字符串发送、
 * 中断接收、环形缓冲区管理，以及常用调试数据打印。
 *
 * UART1 当前配置：
 *  - 通信模式：模式1，8位数据，可变波特率
 *  - 波特率发生器：Timer1
 *  - 系统时钟：内部 22.1184 MHz
 *  - Timer1 模式：1T
 *  - 波特率：115200 bps
 */

#include "Uart.h"

/*==============================================================================
 * 宏定义
 *============================================================================*/

#define UART1_TX_TIMEOUT  60000u
#define UART_RX_BUF_SIZE  64
#define UART_RX_BUF_MASK  (UART_RX_BUF_SIZE - 1)

/*==============================================================================
 * 模块内部变量
 *============================================================================*/

static volatile uint8 idata g_uart_rx_buf[UART_RX_BUF_SIZE];
static volatile uint8 g_uart_rx_wptr = 0;
static volatile uint8 g_uart_rx_rptr = 0;

/*==============================================================================
 * 对外开放函数实现
 *============================================================================*/

void Uart1_Init(void)
{
    SCON = 0x50;        // 8位数据, 可变波特率
    AUXR |= 0x40;       // Timer1 1T模式
    AUXR &= 0xFE;       // 串口1选择Timer1为波特率发生器
    TMOD &= 0x0F;       // 设置Timer1模式
    TL1 = 0xD0;         // 115200bps @ 22.1184MHz
    TH1 = 0xFF;
    ET1 = 0;            // 禁止Timer1中断
    TR1 = 1;            // 启动Timer1

    RI = 0;
    TI = 0;

    g_uart_rx_wptr = 0;
    g_uart_rx_rptr = 0;

    ES = 1;             // 仅开启UART1中断接收
}

void Uart_SendByte(unsigned char dat)
{
    uint16 timeout;
    bit old_es;

    old_es = ES;
    ES = 0;

    TI = 0;
    SBUF = dat;

    timeout = UART1_TX_TIMEOUT;
    while (!TI)
    {
        if (--timeout == 0)
        {
            break;
        }
    }

    TI = 0;
    ES = old_es;
}

void Uart_SendStr(const unsigned char *Str)
{
    while (*Str)
    {
        Uart_SendByte(*Str++);
    }
}

bit Uart_ReadByte(uint8 *dat)
{
    uint8 rptr;

    if (dat == 0)
    {
        return 0;
    }

    rptr = g_uart_rx_rptr;
    if (rptr == g_uart_rx_wptr)
    {
        return 0;
    }

    *dat = g_uart_rx_buf[rptr];
    g_uart_rx_rptr = (uint8)((rptr + 1) & UART_RX_BUF_MASK);

    return 1;
}

char putchar(char c)
{
    Uart_SendByte((unsigned char)c);
    return c;
}

void Uart_PrintUint16(unsigned int num)
{
    char buf[6];
    char i;

    if (num == 0)
    {
        Uart_SendByte('0');
        return;
    }

    i = 0;
    while (num)
    {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }

    while (i--)
    {
        Uart_SendByte(buf[i]);
    }
}

void Uart_PrintHex8(unsigned char dat)
{
    unsigned char hi;
    unsigned char lo;
    char c;

    hi = (dat >> 4) & 0x0F;
    lo = dat & 0x0F;

    c = (hi < 10) ? ('0' + hi) : ('A' + hi - 10);
    Uart_SendByte(c);

    c = (lo < 10) ? ('0' + lo) : ('A' + lo - 10);
    Uart_SendByte(c);
}

/*==============================================================================
 * 中断服务函数实现
 *============================================================================*/

void Uart1_Isr(void) interrupt 4
{
    uint8 dat;
    uint8 next;

    if (RI)
    {
        dat = SBUF;
        RI = 0;

        next = (uint8)((g_uart_rx_wptr + 1) & UART_RX_BUF_MASK);
        if (next != g_uart_rx_rptr)
        {
            g_uart_rx_buf[g_uart_rx_wptr] = dat;
            g_uart_rx_wptr = next;
        }
    }

    if (TI)
    {
        TI = 0;
    }
}
