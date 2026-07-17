#include "pressure_meter.h"
#include "modbus_rtu.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define PRESSURE_VALUE_REGISTER    0x0000u
#define PRESSURE_DECIMAL_REGISTER  0x0012u
#define PRESSURE_ZERO_COIL         0x0000u
#define PRESSURE_ZERO_VALUE        0xFF00u

static PressureMeterOperation_t g_pressure_meter_operation;
static PressureMeterResult_t g_pressure_meter_result;
static int16 g_pressure_meter_value;

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化压力仪表通信层
 * @param  无
 * @retval 无
 */
void PressureMeter_Init(void)
{
    ModbusRtu_Init();
    g_pressure_meter_result = PRESSURE_METER_RESULT_NONE;
    g_pressure_meter_value = 0;
}

/**
 * @brief  启动压力仪表操作
 * @param  operation 操作类型
 * @param  slave Modbus从机地址
 * @retval 1：请求已启动；0：参数、忙状态或发送错误
 */
bit PressureMeter_Start(PressureMeterOperation_t operation, uint8 slave)
{
    bit started;

    if ((g_pressure_meter_result != PRESSURE_METER_RESULT_NONE) ||
        ModbusRtu_IsBusy())
    {
        return 0;
    }

    if (operation == PRESSURE_METER_READ_VALUE)
    {
        started = ModbusRtu_StartReadHolding(slave, PRESSURE_VALUE_REGISTER, 1u);
    }
    else if (operation == PRESSURE_METER_READ_DECIMAL)
    {
        started = ModbusRtu_StartReadHolding(slave, PRESSURE_DECIMAL_REGISTER, 1u);
    }
    else if (operation == PRESSURE_METER_ZERO)
    {
        started = ModbusRtu_StartWriteSingleCoil(slave, PRESSURE_ZERO_COIL,
                                                  PRESSURE_ZERO_VALUE);
    }
    else
    {
        return 0;
    }

    if (started)
    {
        g_pressure_meter_operation = operation;
    }
    return started;
}

/**
 * @brief  轮询并解析压力仪表响应
 * @param  无
 * @retval 无
 */
void PressureMeter_Poll(void)
{
    uint8 frame[16];
    uint8 length;
    ModbusRtuResult_t result;

    ModbusRtu_Poll();
    result = ModbusRtu_TakeResponse(frame, &length);
    if (result == MODBUS_RTU_RESULT_NONE)
    {
        return;
    }
    if (result == MODBUS_RTU_RESULT_ERROR)
    {
        g_pressure_meter_result = PRESSURE_METER_RESULT_ERROR;
        return;
    }

    if ((g_pressure_meter_operation == PRESSURE_METER_READ_VALUE) ||
        (g_pressure_meter_operation == PRESSURE_METER_READ_DECIMAL))
    {
        if ((length != 7u) || (frame[1] != 0x03u) || (frame[2] != 2u))
        {
            g_pressure_meter_result = PRESSURE_METER_RESULT_ERROR;
            return;
        }

        g_pressure_meter_value = (int16)(((uint16)frame[3] << 8) | frame[4]);
    }
    else
    {
        if ((length != 8u) || (frame[1] != 0x05u) ||
            (frame[2] != 0x00u) || (frame[3] != 0x00u) ||
            (frame[4] != 0xFFu) || (frame[5] != 0x00u))
        {
            g_pressure_meter_result = PRESSURE_METER_RESULT_ERROR;
            return;
        }

        g_pressure_meter_value = 0;
    }

    g_pressure_meter_result = PRESSURE_METER_RESULT_SUCCESS;
}

/**
 * @brief  获取并消费压力仪表操作结果
 * @param  value 结果存放地址
 * @retval 当前操作结果
 */
PressureMeterResult_t PressureMeter_TakeResult(int16 *value)
{
    PressureMeterResult_t result;

    if (g_pressure_meter_result == PRESSURE_METER_RESULT_NONE)
    {
        return PRESSURE_METER_RESULT_NONE;
    }

    result = g_pressure_meter_result;
    if ((result == PRESSURE_METER_RESULT_SUCCESS) && (value != 0))
    {
        *value = g_pressure_meter_value;
    }
    else if ((result == PRESSURE_METER_RESULT_SUCCESS) && (value == 0))
    {
        result = PRESSURE_METER_RESULT_ERROR;
    }

    g_pressure_meter_result = PRESSURE_METER_RESULT_NONE;
    return result;
}
