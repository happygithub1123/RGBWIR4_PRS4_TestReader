#include "pressure_meter.h"
#include "modbus_rtu.h"

/*==============================================================================
 内部定义
 *============================================================================*/

#define PRESSURE_VALUE_REGISTER    0x0000u
#define PRESSURE_DECIMAL_REGISTER  0x0012u
#define PRESSURE_ZERO_COIL         0x0000u
#define PRESSURE_ZERO_VALUE        0xFF00u

static PressureMeterResult_t g_pressure_meter_result; //保存压力仪表事务最终结果
static int16 g_pressure_meter_value;                  //保存压力仪表原始寄存器值

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

    // 旧结果未消费或Modbus事务未结束时禁止启动新请求
    if ((g_pressure_meter_result != PRESSURE_METER_RESULT_NONE) ||
        ModbusRtu_IsBusy())
    {
        return 0;
    }

    // 将业务操作转换为仪表固定寄存器或线圈请求
    if (operation == PRESSURE_METER_READ_VALUE)
    {
        started = ModbusRtu_StartReadHolding(slave, PRESSURE_VALUE_REGISTER);
    }
    else if (operation == PRESSURE_METER_READ_DECIMAL)
    {
        started = ModbusRtu_StartReadHolding(slave, PRESSURE_DECIMAL_REGISTER);
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

    return started;
}

/**
 * @brief  轮询并解析压力仪表响应
 * @param  无
 * @retval 无
 */
void PressureMeter_Poll(void)
{
    uint16 value;
    ModbusRtuResult_t result;

    ModbusRtu_Poll();
    result = ModbusRtu_TakeResult(&value);
    if (result == MODBUS_RTU_RESULT_NONE)
    {
        return;
    }
    if (result == MODBUS_RTU_RESULT_ERROR)
    {
        g_pressure_meter_result = PRESSURE_METER_RESULT_ERROR;
        return;
    }

    g_pressure_meter_value = (int16)value;
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

    g_pressure_meter_result = PRESSURE_METER_RESULT_NONE; // 结果只允许消费一次
    return result;
}
