/**
 * @file modbus_uart2.c
 * @brief Modbus UART2 驱动实现
 *
 * @details
 * 本模块负责：
 *  - 485 方向控制
 *  - Modbus RTU 原始帧发送
 *  - 接收帧组包
 *  - 帧间超时判断
 *  - CRC16 计算与基础校验
 *
 * 不负责压力传感器业务寄存器语义。
 */

#include "modbus_uart2.h"
#include "Uart2.h"
#include "Timer0.h"

/*==============================================================================
 * 宏定义
 *============================================================================*/

#define MODBUS_UART2_FRAME_MAX_SIZE      64
#define MODBUS_UART2_FRAME_TIMEOUT_MS     5

/*==============================================================================
 * 模块内部变量
 *============================================================================*/

static uint8 g_modbus_rx_buf[MODBUS_UART2_FRAME_MAX_SIZE];
static uint8 g_modbus_frame_buf[MODBUS_UART2_FRAME_MAX_SIZE];
static uint16 g_modbus_rx_len = 0;
static uint16 g_modbus_frame_len = 0;
static bit g_modbus_rx_active = 0;
static bit g_modbus_frame_ready = 0;
static uint32 g_modbus_last_rx_ms = 0;

/*==============================================================================
 * 模块内部函数声明
 *============================================================================*/

static void ModbusUart2_SetTxMode(void);
static void ModbusUart2_SetRxMode(void);
static void ModbusUart2_ResetRxFrame(void);
static bit ModbusUart2_FrameHasValidCrc(const uint8 *buf, uint16 len);

/*==============================================================================
 * 模块内部函数实现
 *============================================================================*/

static void ModbusUart2_SetTxMode(void)
{
    P07 = 1;
}

static void ModbusUart2_SetRxMode(void)
{
    P07 = 0;
}

static void ModbusUart2_ResetRxFrame(void)
{
    g_modbus_rx_len = 0;
    g_modbus_rx_active = 0;
}

static bit ModbusUart2_FrameHasValidCrc(const uint8 *buf, uint16 len)
{
    uint16 calc_crc;
    uint16 frame_crc;

    if ((buf == 0) || (len < 4))
    {
        return 0;
    }

    calc_crc = ModbusUart2_CalcCrc(buf, len - 2);
    frame_crc = (uint16)buf[len - 2] | ((uint16)buf[len - 1] << 8);

    return (calc_crc == frame_crc) ? 1 : 0;
}

/*==============================================================================
 * 对外开放函数实现
 *============================================================================*/

void ModbusUart2_Init(void)
{
    /* P0.7 推挽输出，作为 485 方向控制 */
    P0 |= 0x80;
    P0M1 &= (uint8)(~0x80);
    P0M0 |= 0x80;

    ModbusUart2_ResetRxFrame();
    g_modbus_frame_len = 0;
    g_modbus_frame_ready = 0;

    Uart2_Init();
    ModbusUart2_SetRxMode();
}

void ModbusUart2_Poll(void)
{
    uint8 ch;
    uint32 now;
    uint16 i;

    while (Uart2_ReadByte(&ch))
    {
        if (g_modbus_rx_len < MODBUS_UART2_FRAME_MAX_SIZE)
        {
            g_modbus_rx_buf[g_modbus_rx_len++] = ch;
        }
        else
        {
            ModbusUart2_ResetRxFrame();
            g_modbus_frame_ready = 0;
            return;
        }

        g_modbus_rx_active = 1;
        g_modbus_last_rx_ms = Timer0_GetMs();
    }

    if (!g_modbus_rx_active)
    {
        return;
    }

    now = Timer0_GetMs();
    if ((uint32)(now - g_modbus_last_rx_ms) < MODBUS_UART2_FRAME_TIMEOUT_MS)
    {
        return;
    }

    if (ModbusUart2_FrameHasValidCrc(g_modbus_rx_buf, g_modbus_rx_len))
    {
        for (i = 0; i < g_modbus_rx_len; i++)
        {
            g_modbus_frame_buf[i] = g_modbus_rx_buf[i];
        }
        g_modbus_frame_len = g_modbus_rx_len;
        g_modbus_frame_ready = 1;
    }
    else
    {
        g_modbus_frame_len = 0;
        g_modbus_frame_ready = 0;
    }

    ModbusUart2_ResetRxFrame();
}

bit ModbusUart2_SendRequest(const uint8 *buf, uint16 len)
{
    bit send_ok;

    if ((buf == 0) || (len == 0))
    {
        return 0;
    }

    ModbusUart2_SetTxMode();
    send_ok = Uart2_SendBuffer(buf, len);
    ModbusUart2_SetRxMode();

    return send_ok;
}

bit ModbusUart2_GetFrame(uint8 *buf, uint16 *len)
{
    uint16 i;

    if ((buf == 0) || (len == 0))
    {
        return 0;
    }

    if (!g_modbus_frame_ready)
    {
        return 0;
    }

    for (i = 0; i < g_modbus_frame_len; i++)
    {
        buf[i] = g_modbus_frame_buf[i];
    }

    *len = g_modbus_frame_len;
    g_modbus_frame_len = 0;
    g_modbus_frame_ready = 0;

    return 1;
}

uint16 ModbusUart2_CalcCrc(const uint8 *buf, uint16 len)
{
    uint16 crc;
    uint16 i;
    uint8 j;

    if (buf == 0)
    {
        return 0;
    }

    crc = 0xFFFF;

    for (i = 0; i < len; i++)
    {
        crc ^= buf[i];
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}
