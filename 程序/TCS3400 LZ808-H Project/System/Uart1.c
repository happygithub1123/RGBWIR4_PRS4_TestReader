#include "Uart1.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define UART1_RX_BUFFER_SIZE  32u    // 本项目缓存MTF上位机连续接收字节的UART1环形缓冲大小
#define UART1_RX_BUFFER_MASK  0x1Fu  // UART1读写索引在32字节MTF接收缓冲中快速回绕
#define UART1_TX_TIMEOUT      60000u // UART1发送中断异常时限制MTF响应等待循环次数

static volatile uint8 idata g_uart1_rx_buffer[UART1_RX_BUFFER_SIZE]; // 缓存UART1中断收到的ICT MTF请求字节
static volatile uint8 g_uart1_write_index;                           // 记录中断写入MTF接收环形缓冲的位置
static volatile uint8 g_uart1_read_index;                            // 记录主循环读取MTF接收环形缓冲的位置
static volatile bit g_uart1_tx_busy;                                 // 标记MTF Float响应的前一个字节仍在硬件发送
static volatile bit g_uart1_rx_overflow;                             // 标记ICT请求字节超过UART1环形缓冲容量

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
    // 本项目中该函数把UART1配置为ICT MTF通信使用的115200bps、8N1和中断接收
    SCON = 0x50u;  // UART1使用模式1、8N1并允许接收ICT发送的MTF请求
    AUXR |= 0x40u; // Timer1使用1T模式，匹配MTF通信115200bps重装值
    AUXR &= 0xFEu; // UART1固定选择Timer1作为MTF波特率发生器
    TMOD &= 0x0Fu; // Timer1使用16位自动重装，持续产生115200bps波特率
    TL1 = 0xD0u;   // 写入22.1184MHz下115200bps的Timer1低字节重装值
    TH1 = 0xFFu;   // 写入22.1184MHz下115200bps的Timer1高字节重装值
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
    // 本项目中该函数由主循环非阻塞取出一个ICT请求字节，缓冲为空时立即返回
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
    // 本项目中该函数在临界区清空MTF接收索引和溢出标志，重新建立帧边界
    bit es_state;

    es_state = ES;
    ES = 0; // 暂停UART1中断，防止清空MTF缓冲时索引被接收中断改写
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
    // 本项目中该函数原子取走UART1溢出状态，供应用层丢弃边界已损坏的MTF帧
    bit es_state;
    bit overflow;

    es_state = ES;
    ES = 0; // 暂停UART1中断，原子取走并清除MTF接收溢出标志
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
    // 本项目中该函数等待前一字节完成后把下一个MTF响应字节写入发送寄存器
    uint16 timeout;

    timeout = UART1_TX_TIMEOUT;
    // 等前一个MTF响应字节发送完成后再写SBUF，防止7字节Float帧被覆盖
    while (g_uart1_tx_busy)
    {
        if (--timeout == 0u)
        {
            g_uart1_tx_busy = 0;
            return 0;
        }
    }

    g_uart1_tx_busy = 1; // 标记MTF字节已交给硬件，TI中断完成后再允许下一字节
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
    // 本项目中该函数按顺序发送完整7字节Float响应，并等待最后一个字节真正移出
    uint8 index;
    uint16 timeout;

    if ((buffer == 0) || (length == 0u))
    {
        return 0;
    }

    // 逐字节等待UART1硬件完成，保证7字节MTF Float响应顺序不乱
    for (index = 0u; index < length; index++)
    {
        if (!Uart1_SendByte(buffer[index]))
        {
            return 0;
        }
    }

    timeout = UART1_TX_TIMEOUT;
    // 等MTF响应最后一个换行字节移出硬件后才报告整帧发送成功
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

/**
 * @brief  UART1中断服务函数
 * @param  无
 * @retval 无
 */
void Uart1_Isr(void) interrupt 4
{
    // 本项目中UART1中断只维护MTF收发字节和环形缓冲，不解析协议或访问传感器
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
        next_index = (uint8)((g_uart1_write_index + 1u) & UART1_RX_BUFFER_MASK); // 让MTF接收写索引在32字节缓冲内回绕

        // 保留一个空槽区分MTF缓冲空和满，最多保存31个未处理字节
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
