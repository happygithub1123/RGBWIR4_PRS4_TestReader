#include "modbus_rtu.h"
#include "Rs485.h"
#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define MODBUS_FUNCTION_READ_HOLDING       0x03u
#define MODBUS_FUNCTION_WRITE_SINGLE_COIL  0x05u
#define MODBUS_REQUEST_LENGTH              8u
#define MODBUS_RESPONSE_MAX_LENGTH         16u

typedef enum
{
    MODBUS_STATE_IDLE = 0,
    MODBUS_STATE_WAITING,
    MODBUS_STATE_SUCCESS,
    MODBUS_STATE_ERROR
} ModbusState_t;

static ModbusState_t g_modbus_state;
static uint8 g_modbus_expected_slave;
static uint8 g_modbus_expected_function;
static uint8 g_modbus_expected_length;
static uint8 g_modbus_response_length;
static uint8 xdata g_modbus_response[MODBUS_RESPONSE_MAX_LENGTH];
static uint32 g_modbus_start_ms;

/**
 * @brief  启动一帧Modbus请求
 * @param  request 请求帧
 * @param  slave 目标从机地址
 * @param  function 目标功能码
 * @retval 1：请求已发送；0：总线忙或发送失败
 */
static bit ModbusRtu_StartRequest(uint8 *request, uint8 slave, uint8 function)
{
    uint16 crc;

    if (g_modbus_state != MODBUS_STATE_IDLE)
    {
        return 0;
    }

    crc = ModbusRtu_CalcCrc(request, 6u);
    request[6] = (uint8)(crc & 0xFFu); // Modbus CRC低字节先发送
    request[7] = (uint8)(crc >> 8);

    // 保存本次请求身份，后续响应必须同时匹配地址和功能码
    g_modbus_expected_slave = slave;
    g_modbus_expected_function = function;
    g_modbus_expected_length = 0u;
    g_modbus_response_length = 0u;
    Rs485_ClearRx(); // 发送前丢弃上一事务残留字节

    if (!Rs485_SendBuffer(request, MODBUS_REQUEST_LENGTH))
    {
        return 0;
    }

    g_modbus_start_ms = Timer0_GetMs(); // 从请求发送完成后开始计算响应超时
    g_modbus_state = MODBUS_STATE_WAITING;
    return 1;
}

/**
 * @brief  校验已经收齐的Modbus响应
 * @param  无
 * @retval 无
 */
static void ModbusRtu_ValidateResponse(void)
{
    uint16 received_crc;
    uint16 calculated_crc;

    // 响应末尾CRC同样按低字节在前还原
    received_crc = (uint16)g_modbus_response[g_modbus_response_length - 2u] |
                   ((uint16)g_modbus_response[g_modbus_response_length - 1u] << 8);
    calculated_crc = ModbusRtu_CalcCrc(g_modbus_response,
                                       (uint8)(g_modbus_response_length - 2u));

    // CRC、从机地址或功能码任一不符都禁止向业务层交付数据
    if ((received_crc != calculated_crc) ||
        (g_modbus_response[0] != g_modbus_expected_slave) ||
        (g_modbus_response[1] != g_modbus_expected_function))
    {
        g_modbus_state = MODBUS_STATE_ERROR;
        return;
    }

    g_modbus_state = MODBUS_STATE_SUCCESS;
}

/**
 * @brief  根据响应头确定完整帧长度
 * @param  无
 * @retval 无
 */
static void ModbusRtu_UpdateExpectedLength(void)
{
    uint8 function;

    if (g_modbus_response_length < 2u)
    {
        return;
    }

    function = g_modbus_response[1];
    // Modbus异常响应固定为地址、异常功能码、异常码和CRC共5字节
    if (function == (uint8)(g_modbus_expected_function | 0x80u))
    {
        g_modbus_expected_length = 5u;
        return;
    }
    if (function != g_modbus_expected_function)
    {
        g_modbus_state = MODBUS_STATE_ERROR;
        return;
    }

    if (function == MODBUS_FUNCTION_WRITE_SINGLE_COIL)
    {
        g_modbus_expected_length = 8u; // 写单线圈正常响应完整回显请求
    }
    else if ((function == MODBUS_FUNCTION_READ_HOLDING) &&
             (g_modbus_response_length >= 3u))
    {
        g_modbus_expected_length = (uint8)(g_modbus_response[2] + 5u); // 地址、功能、字节数、数据、CRC
        if (g_modbus_expected_length > MODBUS_RESPONSE_MAX_LENGTH)
        {
            g_modbus_state = MODBUS_STATE_ERROR;
        }
    }
}

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化Modbus RTU主机状态
 * @param  无
 * @retval 无
 */
void ModbusRtu_Init(void)
{
    g_modbus_state = MODBUS_STATE_IDLE;
    g_modbus_response_length = 0u;
    g_modbus_expected_length = 0u;
}

/**
 * @brief  启动保持寄存器读取请求
 * @param  slave 从机地址
 * @param  address 起始寄存器地址
 * @param  count 寄存器数量
 * @retval 1：请求已发送；0：参数、忙状态或发送错误
 */
bit ModbusRtu_StartReadHolding(uint8 slave, uint16 address, uint16 count)
{
    uint8 request[MODBUS_REQUEST_LENGTH];

    if ((slave < MODBUS_SLAVE_ADDRESS_MIN) ||
        (slave > MODBUS_SLAVE_ADDRESS_MAX) || (count == 0u) || (count > 4u))
    {
        return 0;
    }

    // 多字节地址和数量按Modbus大端顺序装入请求
    request[0] = slave;
    request[1] = MODBUS_FUNCTION_READ_HOLDING;
    request[2] = (uint8)(address >> 8);
    request[3] = (uint8)(address & 0xFFu);
    request[4] = (uint8)(count >> 8);
    request[5] = (uint8)(count & 0xFFu);
    return ModbusRtu_StartRequest(request, slave, MODBUS_FUNCTION_READ_HOLDING);
}

/**
 * @brief  启动写单线圈请求
 * @param  slave 从机地址
 * @param  address 线圈地址
 * @param  value 写入值
 * @retval 1：请求已发送；0：参数、忙状态或发送错误
 */
bit ModbusRtu_StartWriteSingleCoil(uint8 slave, uint16 address, uint16 value)
{
    uint8 request[MODBUS_REQUEST_LENGTH];

    if ((slave < MODBUS_SLAVE_ADDRESS_MIN) ||
        (slave > MODBUS_SLAVE_ADDRESS_MAX) ||
        ((value != 0x0000u) && (value != 0xFF00u)))
    {
        return 0;
    }

    // 多字节线圈地址和值按Modbus大端顺序装入请求
    request[0] = slave;
    request[1] = MODBUS_FUNCTION_WRITE_SINGLE_COIL;
    request[2] = (uint8)(address >> 8);
    request[3] = (uint8)(address & 0xFFu);
    request[4] = (uint8)(value >> 8);
    request[5] = (uint8)(value & 0xFFu);
    return ModbusRtu_StartRequest(request, slave, MODBUS_FUNCTION_WRITE_SINGLE_COIL);
}

/**
 * @brief  轮询Modbus响应和事务超时
 * @param  无
 * @retval 无
 */
void ModbusRtu_Poll(void)
{
    uint8 dat;

    if (g_modbus_state != MODBUS_STATE_WAITING)
    {
        return;
    }
    // 缓冲溢出后帧内容不完整，直接终止当前事务
    if (Rs485_TakeRxOverflow())
    {
        g_modbus_state = MODBUS_STATE_ERROR;
        return;
    }

    // 按响应头动态确定长度，收满后立即校验而不等待静默间隔
    while (Rs485_ReadByte(&dat))
    {
        if (g_modbus_response_length >= MODBUS_RESPONSE_MAX_LENGTH)
        {
            g_modbus_state = MODBUS_STATE_ERROR;
            return;
        }

        g_modbus_response[g_modbus_response_length++] = dat;
        ModbusRtu_UpdateExpectedLength();
        if (g_modbus_state == MODBUS_STATE_ERROR)
        {
            return;
        }
        if ((g_modbus_expected_length > 0u) &&
            (g_modbus_response_length == g_modbus_expected_length))
        {
            // 从机异常响应不作为正常业务数据返回
            if (g_modbus_response[1] ==
                (uint8)(g_modbus_expected_function | 0x80u))
            {
                g_modbus_state = MODBUS_STATE_ERROR;
            }
            else
            {
                ModbusRtu_ValidateResponse();
            }
            return;
        }
        if ((g_modbus_expected_length > 0u) &&
            (g_modbus_response_length > g_modbus_expected_length))
        {
            g_modbus_state = MODBUS_STATE_ERROR;
            return;
        }
    }

    // 使用时间差判断超时，允许系统毫秒计数自然回绕
    if (Timer0_HasElapsed(g_modbus_start_ms, MODBUS_RESPONSE_TIMEOUT_MS))
    {
        g_modbus_state = MODBUS_STATE_ERROR;
    }
}

/**
 * @brief  查询Modbus事务是否正在等待响应
 * @param  无
 * @retval 1：忙；0：空闲或结果待取
 */
bit ModbusRtu_IsBusy(void)
{
    return (g_modbus_state == MODBUS_STATE_WAITING);
}

/**
 * @brief  获取并消费Modbus响应结果
 * @param  buffer 响应缓冲区
 * @param  length 响应长度存放地址
 * @retval 当前响应结果
 */
ModbusRtuResult_t ModbusRtu_TakeResponse(uint8 *buffer, uint8 *length)
{
    uint8 index;

    // 错误状态消费后立即回到空闲，下一请求可以重新开始
    if (g_modbus_state == MODBUS_STATE_ERROR)
    {
        g_modbus_state = MODBUS_STATE_IDLE;
        g_modbus_response_length = 0u;
        return MODBUS_RTU_RESULT_ERROR;
    }
    if (g_modbus_state != MODBUS_STATE_SUCCESS)
    {
        return MODBUS_RTU_RESULT_NONE;
    }
    if ((buffer == 0) || (length == 0))
    {
        g_modbus_state = MODBUS_STATE_IDLE;
        return MODBUS_RTU_RESULT_ERROR;
    }

    *length = g_modbus_response_length;
    // 复制到调用方缓冲后清除内部事务状态，响应只交付一次
    for (index = 0u; index < g_modbus_response_length; index++)
    {
        buffer[index] = g_modbus_response[index];
    }

    g_modbus_state = MODBUS_STATE_IDLE;
    g_modbus_response_length = 0u;
    return MODBUS_RTU_RESULT_SUCCESS;
}

/**
 * @brief  计算Modbus CRC16
 * @param  buffer 输入缓冲区
 * @param  length 输入长度
 * @retval CRC16结果
 */
uint16 ModbusRtu_CalcCrc(const uint8 *buffer, uint8 length)
{
    uint8 byte_index;
    uint8 bit_index;
    uint16 crc;

    if (buffer == 0)
    {
        return 0u;
    }

    crc = 0xFFFFu;
    // 使用Modbus多项式0xA001，逐字节按最低位优先计算
    for (byte_index = 0u; byte_index < length; byte_index++)
    {
        crc ^= buffer[byte_index];
        for (bit_index = 0u; bit_index < 8u; bit_index++)
        {
            if (crc & 0x0001u)
            {
                crc = (crc >> 1) ^ 0xA001u;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}
