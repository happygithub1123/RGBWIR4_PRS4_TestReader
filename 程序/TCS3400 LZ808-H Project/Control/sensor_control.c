#include "sensor_control.h"
#include "tcs3400_control.h"
#include "pressure_control.h"

/*==============================================================================
 内部定义
 *============================================================================*/

typedef enum
{
    SENSOR_SOURCE_NONE = 0,    // 本项目中表示当前没有环境光或压力命令占用统一控制层
    SENSOR_SOURCE_TCS3400,     // 本项目中表示当前ICT命令交由TCS3400环境光控制器执行
    SENSOR_SOURCE_PRESSURE     // 本项目中表示当前ICT命令交由LZ808-H压力控制器执行
} SensorSource_t; // 本项目中用于选择统一轮询和结果获取应访问哪个传感器控制器

static SensorSource_t g_sensor_source; // 记录当前ICT命令由环境光还是压力控制器执行

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
    // 本项目中该函数建立环境光与压力控制器的统一空闲状态和底层初始配置
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
    // 本项目中该函数拆分Pin1并把工作值2分发到对应的环境光或压力操作
    uint8 target_code;
    uint8 function_code;
    uint8 pressure_channel;

    // 环境光或压力命令执行期间拒绝新命令，避免最终Float结果对应错请求
    if (SensorControl_IsBusy())
    {
        return 0;
    }

    target_code = command_code / 10u;     // 从Pin1十位取目标码A，决定传感器或压力限值组
    function_code = command_code % 10u;   // 从Pin1个位取功能码B，决定读取、设置或清零操作

    // 目标码1至4固定选择四路TCS3400，功能码选择该路环境光操作
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

    // Pin1为50或51时，工作值2直接作为LZ808-H实际Modbus地址
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

    // 目标码6至9依次选择四组压力限值，并固定对应Y1至Y4输出
    if ((target_code >= MTF_TARGET_CODE_PRESSURE_LIMIT_MIN) &&
        (target_code <= MTF_TARGET_CODE_PRESSURE_LIMIT_MAX))
    {
        pressure_channel =
            (uint8)(target_code - MTF_TARGET_CODE_PRESSURE_LIMIT_MIN + 1u);

        if (function_code == MTF_FUNCTION_CODE_LIMIT_LOWER)
        {
            if (!PressureControl_SetLowerLimit(pressure_channel, parameter))
            {
                return 0;
            }
        }
        else if (function_code == MTF_FUNCTION_CODE_LIMIT_UPPER)
        {
            if (!PressureControl_SetUpperLimit(pressure_channel, parameter))
            {
                return 0;
            }
        }
        else if ((function_code == MTF_FUNCTION_CODE_LIMIT_READ) &&
                 (parameter >= MODBUS_SLAVE_ADDRESS_MIN) &&
                 (parameter <= MODBUS_SLAVE_ADDRESS_MAX))
        {
            if (!PressureControl_StartCompareRead(pressure_channel,
                                                  (uint8)parameter))
            {
                return 0;
            }
        }
        else
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
    // 本项目中该函数只推进当前命令所属的子控制器，避免两个传感器链路同时运行
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
    // 本项目中该函数从正确的子控制器取回结果，并在消费后释放统一命令入口
    ProjectAsyncResult_t result;

    // 只从启动当前ICT命令的子控制器取结果，避免环境光和压力结果串用
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
        g_sensor_source = SENSOR_SOURCE_NONE; // Float结果取走后释放统一控制器，允许下一条ICT命令
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
    // 本项目中该函数用于MTF应用层判断上一条环境光或压力命令是否仍未结束
    return (g_sensor_source != SENSOR_SOURCE_NONE);
}
