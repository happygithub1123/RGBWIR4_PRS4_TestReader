#include "sensor_control.h"
#include "tcs3400_control.h"
#include "pressure_control.h"

/*==============================================================================
 内部定义
 *============================================================================*/

typedef enum
{
    SENSOR_SOURCE_NONE = 0,
    SENSOR_SOURCE_TCS3400,
    SENSOR_SOURCE_PRESSURE
} SensorSource_t;

static SensorSource_t g_sensor_source;

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化统一传感器控制器
 * @param  无
 * @retval 无
 */
void SensorControl_Init(void)
{
    Tcs3400Control_Init();
    PressureControl_Init();
    g_sensor_source = SENSOR_SOURCE_NONE;
}

/**
 * @brief  按AB指令码启动传感器操作
 * @param  command_code AB十进制指令码
 * @param  parameter EFG参数
 * @retval 1：命令已接收；0：命令、参数或忙状态错误
 */
bit SensorControl_Start(uint8 command_code, uint16 parameter)
{
    uint8 target_code;
    uint8 function_code;

    // 任一子控制器占用期间都拒绝覆盖当前命令来源
    if (SensorControl_IsBusy())
    {
        return 0;
    }

    target_code = command_code / 10u;     // A：目标码
    function_code = command_code % 10u;   // B：功能码

    // 目标码1至4选择TCS3400，功能码选择该路具体操作
    if ((target_code >= MTF_TARGET_CODE_TCS3400_MIN) &&
        (target_code <= MTF_TARGET_CODE_TCS3400_MAX))
    {
        if (!Tcs3400Control_Start(target_code, function_code, parameter))
        {
            return 0;
        }

        g_sensor_source = SENSOR_SOURCE_TCS3400;
        return 1;
    }

    // 压力命令的EFG直接作为实际Modbus从机地址
    if ((target_code == MTF_TARGET_CODE_PRESSURE) &&
        (function_code == MTF_FUNCTION_CODE_PRESSURE_READ) &&
        (parameter >= MODBUS_SLAVE_ADDRESS_MIN) &&
        (parameter <= MODBUS_SLAVE_ADDRESS_MAX))
    {
        if (!PressureControl_StartRead((uint8)parameter))
        {
            return 0;
        }
        g_sensor_source = SENSOR_SOURCE_PRESSURE;
        return 1;
    }

    if ((target_code == MTF_TARGET_CODE_PRESSURE) &&
        (function_code == MTF_FUNCTION_CODE_PRESSURE_ZERO) &&
        (parameter >= MODBUS_SLAVE_ADDRESS_MIN) &&
        (parameter <= MODBUS_SLAVE_ADDRESS_MAX))
    {
        if (!PressureControl_StartZero((uint8)parameter))
        {
            return 0;
        }
        g_sensor_source = SENSOR_SOURCE_PRESSURE;
        return 1;
    }

    return 0;
}

/**
 * @brief  轮询当前传感器操作
 * @param  无
 * @retval 无
 */
void SensorControl_Poll(void)
{
    if (g_sensor_source == SENSOR_SOURCE_TCS3400)
    {
        Tcs3400Control_Poll();
    }
    else if (g_sensor_source == SENSOR_SOURCE_PRESSURE)
    {
        PressureControl_Poll();
    }
}

/**
 * @brief  获取并消费当前传感器操作结果
 * @param  value 正常结果存放地址
 * @retval 当前操作结果
 */
ProjectAsyncResult_t SensorControl_TakeResult(float *value)
{
    ProjectAsyncResult_t result;

    // 只向启动该命令的子控制器取结果，避免跨设备串响应
    if (g_sensor_source == SENSOR_SOURCE_TCS3400)
    {
        result = Tcs3400Control_TakeResult(value);
    }
    else if (g_sensor_source == SENSOR_SOURCE_PRESSURE)
    {
        result = PressureControl_TakeResult(value);
    }
    else
    {
        return PROJECT_ASYNC_RESULT_NONE;
    }

    if (result != PROJECT_ASYNC_RESULT_NONE)
    {
        g_sensor_source = SENSOR_SOURCE_NONE; // 结果消费后恢复统一空闲态
    }
    return result;
}

/**
 * @brief  查询统一控制器忙状态
 * @param  无
 * @retval 1：正在处理命令或结果待取；0：空闲
 */
bit SensorControl_IsBusy(void)
{
    return (g_sensor_source != SENSOR_SOURCE_NONE);
}
