#include "modbus_rtu.h"
#include "Rs485.h"
#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define MODBUS_FUNCTION_READ_HOLDING       0x03u
#define MODBUS_FUNCTION_WRITE_SINGLE_COIL  0x05u
#define MODBUS_REQUEST_LENGTH              8u
#define MODBUS_READ_RESPONSE_LENGTH        7u
#define MODBUS_WRITE_RESPONSE_LENGTH       8u
#define MODBUS_EXCEPTION_RESPONSE_LENGTH   5u
#define MODBUS_RESPONSE_MAX_LENGTH         8u

typedef enum
{
    MODBUS_STATE_IDLE = 0,
    MODBUS_STATE_WAIT_RETRY,
    MODBUS_STATE_WAIT_RESPONSE,
    MODBUS_STATE_SUCCESS,
    MODBUS_STATE_ERROR
} ModbusState_t;

static ModbusState_t g_modbus_state;                              //保存当前Modbus事务阶段
static uint8 xdata g_modbus_request[MODBUS_REQUEST_LENGTH];       //保存请求帧供通信失败后原样重发
static uint8 xdata g_modbus_response[MODBUS_RESPONSE_MAX_LENGTH]; //缓存当前Modbus响应帧
static uint8 g_modbus_expected_function;                          //保存本次请求期望的功能码
static uint8 g_modbus_normal_length;                              //保存正常响应帧固定长度
static uint8 g_modbus_expected_length;                            //保存当前响应应接收的完整长度
static uint8 g_modbus_response_length;                            //记录已接收的响应字节数
static uint8 g_modbus_attempt_count;                              //记录本次事务已经发送的次数
static uint16 g_modbus_result_value;                              //保存校验完成后的16位结果
static uint32 g_modbus_start_ms;                                  //记录响应或重发等待起点

/**
 * @brief  处理一次Modbus通信失败
 * @param  无
 * @retval 无
 */
static void ModbusRtu_CommunicationFailed(void)
{
    g_modbus_response_length = 0u;
    if (g_modbus_attempt_count >= MODBUS_MAX_ATTEMPTS)
    {
        g_modbus_state = MODBUS_STATE_ERROR;
        return;
    }

    g_modbus_start_ms = Timer0_GetMs();
    g_modbus_state = MODBUS_STATE_WAIT_RETRY; // 等待帧间隔后重发同一请求
}

/**
 * @brief  发送当前保存的Modbus请求
 * @param  无
 * @retval 无
 */
static void ModbusRtu_SendAttempt(void)
{
    g_modbus_attempt_count++;
    g_modbus_response_length = 0u;
    g_modbus_expected_length = g_modbus_normal_length;

    if (!Rs485_SendBuffer(g_modbus_request, MODBUS_REQUEST_LENGTH))
    {
        ModbusRtu_CommunicationFailed();
        return;
    }

    g_modbus_start_ms = Timer0_GetMs();
    g_modbus_state = MODBUS_STATE_WAIT_RESPONSE;
}

/**
 * @brief  启动一项Modbus请求
 * @param  request 请求前6字节
 * @param  function 目标功能码
 * @param  response_length 正常响应长度
 * @retval 1：请求已接收；0：总线忙
 */
static bit ModbusRtu_StartRequest(const uint8 *request, uint8 function,
                                  uint8 response_length)
{
    uint8 index;
    uint16 crc;

    if (g_modbus_state != MODBUS_STATE_IDLE)
    {
        return 0;
    }

    for (index = 0u; index < 6u; index++)
    {
        g_modbus_request[index] = request[index];
    }

    crc = ModbusRtu_CalcCrc(g_modbus_request, 6u);
    g_modbus_request[6] = (uint8)(crc & 0xFFu); // Modbus CRC低字节先发送
    g_modbus_request[7] = (uint8)(crc >> 8);
    g_modbus_expected_function = function;
    g_modbus_normal_length = response_length;
    g_modbus_result_value = 0u;
    g_modbus_attempt_count = 0u;
    ModbusRtu_SendAttempt();
    return 1;
}

/**
 * @brief  检查当前响应帧CRC
 * @param  无
 * @retval 1：CRC正确；0：CRC错误
 */
static bit ModbusRtu_IsCrcValid(void)
{
    uint16 received_crc;
    uint16 calculated_crc;

    received_crc = (uint16)g_modbus_response[g_modbus_response_length - 2u] |
                   ((uint16)g_modbus_response[g_modbus_response_length - 1u] << 8);
    calculated_crc = ModbusRtu_CalcCrc(
        g_modbus_response, (uint8)(g_modbus_response_length - 2u));
    return (received_crc == calculated_crc);
}

/**
 * @brief  校验并解析已经收齐的Modbus响应
 * @param  无
 * @retval 无
 */
static void ModbusRtu_ValidateResponse(void)
{
    uint8 index;

    // 地址、功能码或CRC不符时按通信错帧重发
    if (!ModbusRtu_IsCrcValid() ||
        (g_modbus_response[0] != g_modbus_request[0]))
    {
        ModbusRtu_CommunicationFailed();
        return;
    }

    // 从机明确返回异常功能码时不重复发送同一条业务请求
    if (g_modbus_response[1] ==
        (uint8)(g_modbus_expected_function | 0x80u))
    {
        g_modbus_state = MODBUS_STATE_ERROR;
        return;
    }
    if (g_modbus_response[1] != g_modbus_expected_function)
    {
        ModbusRtu_CommunicationFailed();
        return;
    }

    if (g_modbus_expected_function == MODBUS_FUNCTION_READ_HOLDING)
    {
        if ((g_modbus_response_length != MODBUS_READ_RESPONSE_LENGTH) ||
            (g_modbus_response[2] != 2u))
        {
            ModbusRtu_CommunicationFailed();
            return;
        }

        g_modbus_result_value =
            ((uint16)g_modbus_response[3] << 8) | g_modbus_response[4];
    }
    else
    {
        if (g_modbus_response_length != MODBUS_WRITE_RESPONSE_LENGTH)
        {
            ModbusRtu_CommunicationFailed();
            return;
        }

        // 写单线圈成功响应必须完整回显请求的地址、功能、线圈和值
        for (index = 0u; index < 6u; index++)
        {
            if (g_modbus_response[index] != g_modbus_request[index])
            {
                ModbusRtu_CommunicationFailed();
                return;
            }
        }
        g_modbus_result_value = 0u;
    }

    g_modbus_state = MODBUS_STATE_SUCCESS;
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
    g_modbus_attempt_count = 0u;
}

/**
 * @brief  启动单个保持寄存器读取
 * @param  slave 从机地址
 * @param  address 寄存器地址
 * @retval 1：请求已接收；0：参数或忙状态错误
 */
bit ModbusRtu_StartReadHolding(uint8 slave, uint16 address)
{
    uint8 request[6];

    if ((slave < MODBUS_SLAVE_ADDRESS_MIN) ||
        (slave > MODBUS_SLAVE_ADDRESS_MAX))
    {
        return 0;
    }

    request[0] = slave;
    request[1] = MODBUS_FUNCTION_READ_HOLDING;
    request[2] = (uint8)(address >> 8);
    request[3] = (uint8)(address & 0xFFu);
    request[4] = 0x00u;
    request[5] = 0x01u; // 本项目固定读取一个16位寄存器
    return ModbusRtu_StartRequest(
        request, MODBUS_FUNCTION_READ_HOLDING, MODBUS_READ_RESPONSE_LENGTH);
}

/**
 * @brief  启动写单线圈请求
 * @param  slave 从机地址
 * @param  address 线圈地址
 * @param  value 写入值
 * @retval 1：请求已接收；0：参数或忙状态错误
 */
bit ModbusRtu_StartWriteSingleCoil(uint8 slave, uint16 address, uint16 value)
{
    uint8 request[6];

    if ((slave < MODBUS_SLAVE_ADDRESS_MIN) ||
        (slave > MODBUS_SLAVE_ADDRESS_MAX) ||
        ((value != 0x0000u) && (value != 0xFF00u)))
    {
        return 0;
    }

    request[0] = slave;
    request[1] = MODBUS_FUNCTION_WRITE_SINGLE_COIL;
    request[2] = (uint8)(address >> 8);
    request[3] = (uint8)(address & 0xFFu);
    request[4] = (uint8)(value >> 8);
    request[5] = (uint8)(value & 0xFFu);
    return ModbusRtu_StartRequest(
        request, MODBUS_FUNCTION_WRITE_SINGLE_COIL,
        MODBUS_WRITE_RESPONSE_LENGTH);
}

/**
 * @brief  轮询Modbus响应、重发间隔和事务超时
 * @param  无
 * @retval 无
 */
void ModbusRtu_Poll(void)
{
    uint8 dat;

    if (g_modbus_state == MODBUS_STATE_WAIT_RETRY)
    {
        if (Timer0_HasElapsed(g_modbus_start_ms, MODBUS_RETRY_DELAY_MS))
        {
            ModbusRtu_SendAttempt();
        }
        return;
    }
    if (g_modbus_state != MODBUS_STATE_WAIT_RESPONSE)
    {
        return;
    }

    if (Rs485_TakeRxOverflow())
    {
        ModbusRtu_CommunicationFailed();
        return;
    }

    while (Rs485_ReadByte(&dat))
    {
        if (g_modbus_response_length >= MODBUS_RESPONSE_MAX_LENGTH)
        {
            ModbusRtu_CommunicationFailed();
            return;
        }

        g_modbus_response[g_modbus_response_length++] = dat;
        if ((g_modbus_response_length == 2u) &&
            (g_modbus_response[1] ==
             (uint8)(g_modbus_expected_function | 0x80u)))
        {
            g_modbus_expected_length = MODBUS_EXCEPTION_RESPONSE_LENGTH;
        }

        if (g_modbus_response_length == g_modbus_expected_length)
        {
            ModbusRtu_ValidateResponse();
            return;
        }
        if (g_modbus_response_length > g_modbus_expected_length)
        {
            ModbusRtu_CommunicationFailed();
            return;
        }
    }

    if (Timer0_HasElapsed(g_modbus_start_ms, MODBUS_RESPONSE_TIMEOUT_MS))
    {
        ModbusRtu_CommunicationFailed();
    }
}

/**
 * @brief  查询Modbus事务是否仍被占用
 * @param  无
 * @retval 1：事务或结果未结束；0：空闲
 */
bit ModbusRtu_IsBusy(void)
{
    return (g_modbus_state != MODBUS_STATE_IDLE);
}

/**
 * @brief  获取并消费已经校验的Modbus结果
 * @param  value 16位结果存放地址
 * @retval 当前事务结果
 */
ModbusRtuResult_t ModbusRtu_TakeResult(uint16 *value)
{
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
    if (value == 0)
    {
        g_modbus_state = MODBUS_STATE_IDLE;
        return MODBUS_RTU_RESULT_ERROR;
    }

    *value = g_modbus_result_value;
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
