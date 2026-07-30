#include "tcs3400_control.h"
#include "tcs3400_driver.h"
#include "soft_i2c.h"
#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

typedef enum
{
    TCS_TASK_IDLE = 0,
    TCS_TASK_WAIT_DATA
} TcsTaskState_t;

typedef struct
{
    uint8 atime;
    uint8 gain_register;
    uint8 ready;
    uint16 integration_parameter;
    uint16 frame_wait_ms;
} TcsSensorState_t;

static TcsSensorState_t xdata g_tcs_sensor[TCS3400_SENSOR_COUNT]; // 保存四路传感器各自最后一次成功的配置
static TcsTaskState_t g_tcs_task_state;                           // 当前异步采样任务状态
static ProjectAsyncResult_t g_tcs_result_state;                   // 等待上层读取的命令结果
static uint8 g_tcs_task_sensor;                                   // 当前任务对应的传感器数组下标
static Tcs3400Channel_t g_tcs_task_channel;                       // 当前任务只读取的目标通道
static bit g_tcs_task_ir;                                         // 当前任务是否启用了IR到Clear的数据映射
static float g_tcs_result_value;                                  // 当前命令的正常浮点返回值
static uint32 g_tcs_task_start_ms;                                // 当前积分等待的起始时刻
static uint32 g_tcs_last_poll_ms;                                 // 上一次查询AVALID的时刻

/**
 * @brief  将传感器数组下标转换为软件I2C总线
 * @param  sensor_index 传感器数组下标
 * @retval 软件I2C总线编号
 */
static SoftI2C_Bus_t Tcs3400Control_GetBus(uint8 sensor_index)
{
    return (SoftI2C_Bus_t)sensor_index;
}

/**
 * @brief  查找六档积分时间配置
 * @param  parameter MTF积分时间参数
 * @param  atime ATIME寄存器值存放地址
 * @param  wait_ms 最大帧等待时间存放地址
 * @param  reply_ms 协议档位返回值存放地址
 * @retval 1：参数属于六档之一；0：非法参数
 */
static bit Tcs3400Control_GetIntegrationConfig(uint16 parameter, uint8 *atime,
                                                uint16 *wait_ms, float *reply_ms)
{
    if ((atime == 0) || (wait_ms == 0) || (reply_ms == 0))
    {
        return 0;
    }

    // 只接受协议定义的六档；等待上限包含积分时间和通信余量
    switch (parameter)
    {
        case 3u:
            *atime = 0xFFu; *wait_ms = 13u; *reply_ms = 2.78f;
            return 1;
        case 28u:
            *atime = 0xF6u; *wait_ms = 40u; *reply_ms = 27.8f;
            return 1;
        case 51u:
            *atime = 0xEEu; *wait_ms = 63u; *reply_ms = 51.0f;
            return 1;
        case 103u:
            *atime = 0xDBu; *wait_ms = 119u; *reply_ms = 103.0f;
            return 1;
        case 178u:
            *atime = 0xC0u; *wait_ms = 198u; *reply_ms = 178.0f;
            return 1;
        case 712u:
            *atime = 0x00u; *wait_ms = 761u; *reply_ms = 712.0f;
            return 1;
        default:
            return 0;
    }
}

/**
 * @brief  将增益参数转换为寄存器值
 * @param  parameter MTF增益参数
 * @param  gain_register 增益寄存器值存放地址
 * @retval 1：参数有效；0：非法参数
 */
static bit Tcs3400Control_GetGainRegister(uint16 parameter, uint8 *gain_register)
{
    if (gain_register == 0)
    {
        return 0;
    }

    // 协议增益值映射到CONTROL寄存器低两位
    switch (parameter)
    {
        case 1u:  *gain_register = 0x00u; return 1;
        case 4u:  *gain_register = 0x01u; return 1;
        case 16u: *gain_register = 0x02u; return 1;
        case 64u: *gain_register = 0x03u; return 1;
        default: return 0;
    }
}

/**
 * @brief  完成当前TCS命令
 * @param  result 命令结果
 * @param  value 正常返回值
 * @retval 无
 */
static void Tcs3400Control_Finish(ProjectAsyncResult_t result, float value)
{
    g_tcs_task_state = TCS_TASK_IDLE;
    g_tcs_task_ir = 0;
    g_tcs_result_value = value;
    g_tcs_result_state = result;
}

/**
 * @brief  处理TCS通信失败并使该路等待下次重新初始化
 * @param  sensor_index 传感器数组下标
 * @retval 无
 */
static void Tcs3400Control_Fail(uint8 sensor_index)
{
    SoftI2C_Bus_t bus;

    if (sensor_index < TCS3400_SENSOR_COUNT)
    {
        bus = Tcs3400Control_GetBus(sensor_index);

        // IR流程失败时尽力恢复普通Clear映射，下一条命令仍会完整重新初始化
        if (g_tcs_task_ir && (g_tcs_task_sensor == sensor_index))
        {
            TCS3400_SetIRMap(bus, 0);
            TCS3400_RestartIntegration(bus);
        }

        g_tcs_sensor[sensor_index].ready = 0u;
    }

    Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_ERROR, 0.0f);
}

/**
 * @brief  确保指定传感器已按最后一次成功配置完成初始化
 * @param  sensor_index 传感器数组下标
 * @retval 1：传感器可用；0：初始化或通信失败
 */
static bit Tcs3400Control_EnsureInitialized(uint8 sensor_index)
{
    TcsSensorState_t *sensor_state;

    if (sensor_index >= TCS3400_SENSOR_COUNT)
    {
        return 0;
    }

    sensor_state = &g_tcs_sensor[sensor_index];
    if (sensor_state->ready)
    {
        return 1;
    }

    // 初次上电或通信失败后，都按该路最后一次成功配置重新写入真实寄存器
    if (!TCS3400_Init(Tcs3400Control_GetBus(sensor_index),
                      sensor_state->atime,
                      sensor_state->gain_register))
    {
        sensor_state->ready = 0u;
        return 0;
    }

    sensor_state->ready = 1u;
    return 1;
}

/**
 * @brief  启动普通颜色通道的新一帧积分
 * @param  sensor_index 传感器数组下标
 * @param  channel 要读取的通道
 * @retval 1：命令已接收
 */
static bit Tcs3400Control_StartChannel(uint8 sensor_index,
                                       Tcs3400Channel_t channel)
{
    SoftI2C_Bus_t bus;

    bus = Tcs3400Control_GetBus(sensor_index);
    g_tcs_task_channel = channel;
    g_tcs_task_ir = 0;

    // 普通通道先确认IR映射关闭，再丢弃旧帧并启动一次新积分
    if (!TCS3400_SetIRMap(bus, 0) || !TCS3400_RestartIntegration(bus))
    {
        Tcs3400Control_Fail(sensor_index);
        return 1;
    }

    g_tcs_task_start_ms = Timer0_GetMs();
    g_tcs_last_poll_ms = g_tcs_task_start_ms;
    g_tcs_task_state = TCS_TASK_WAIT_DATA;
    return 1;
}

/**
 * @brief  启动IR映射后的新一帧积分
 * @param  sensor_index 传感器数组下标
 * @retval 1：命令已接收
 */
static bit Tcs3400Control_StartIr(uint8 sensor_index)
{
    SoftI2C_Bus_t bus;

    bus = Tcs3400Control_GetBus(sensor_index);
    g_tcs_task_channel = TCS3400_CHANNEL_CLEAR;
    g_tcs_task_ir = 1;

    // IR映射到Clear数据端口，回读确认后重新启动一次新积分
    if (!TCS3400_SetIRMap(bus, 1) || !TCS3400_RestartIntegration(bus))
    {
        Tcs3400Control_Fail(sensor_index);
        return 1;
    }

    g_tcs_task_start_ms = Timer0_GetMs();
    g_tcs_last_poll_ms = g_tcs_task_start_ms;
    g_tcs_task_state = TCS_TASK_WAIT_DATA;
    return 1;
}

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化四路TCS3400及独立配置状态
 * @param  无
 * @retval 无
 */
void Tcs3400Control_Init(void)
{
    uint8 index;

    SoftI2C_Init();
    g_tcs_task_state = TCS_TASK_IDLE;
    g_tcs_result_state = PROJECT_ASYNC_RESULT_NONE;
    g_tcs_task_sensor = TCS3400_SENSOR_COUNT;
    g_tcs_task_ir = 0;

    // 四路先装载27.8ms、4x默认值，再逐路写入真实寄存器
    for (index = 0u; index < TCS3400_SENSOR_COUNT; index++)
    {
        g_tcs_sensor[index].atime = TCS3400_DEFAULT_ATIME_REGISTER;
        g_tcs_sensor[index].gain_register = TCS3400_DEFAULT_GAIN_REGISTER;
        g_tcs_sensor[index].integration_parameter =
            TCS3400_DEFAULT_INTEGRATION_PARAMETER;
        g_tcs_sensor[index].frame_wait_ms = TCS3400_DEFAULT_FRAME_WAIT_MS;
        g_tcs_sensor[index].ready = 0u;

        // 某一路上电初始化失败不会阻塞其他路，收到该路命令时再自动重试
        Tcs3400Control_EnsureInitialized(index);
    }
}

/**
 * @brief  启动TCS3400命令
 * @param  target_code 目标码，传感器编号1至4
 * @param  function_code 功能码0至9
 * @param  parameter MTF参数
 * @retval 1：命令格式有效并已接收；0：命令或参数无效
 */
bit Tcs3400Control_Start(uint8 target_code, uint8 function_code, uint16 parameter)
{
    uint8 sensor_index;
    uint8 register_value;
    uint8 id;
    uint16 wait_ms;
    float actual_value;
    Tcs3400Channel_t channel;

    // 校验设备范围，并保护正在运行或尚未消费的结果
    if ((target_code < 1u) || (target_code > TCS3400_SENSOR_COUNT) ||
        (g_tcs_task_state != TCS_TASK_IDLE) ||
        (g_tcs_result_state != PROJECT_ASYNC_RESULT_NONE))
    {
        return 0;
    }

    sensor_index = (uint8)(target_code - 1u);
    g_tcs_task_sensor = sensor_index;
    g_tcs_task_ir = 0;

    // x0设置六档积分时间，只有写入成功才更新该路缓存
    if (function_code == 0u)
    {
        if (!Tcs3400Control_GetIntegrationConfig(parameter, &register_value,
                                                  &wait_ms, &actual_value))
        {
            return 0;
        }
        if (!Tcs3400Control_EnsureInitialized(sensor_index))
        {
            Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_ERROR, 0.0f);
            return 1;
        }
        if (!TCS3400_SetATIME(Tcs3400Control_GetBus(sensor_index),
                              register_value))
        {
            Tcs3400Control_Fail(sensor_index);
            return 1;
        }

        g_tcs_sensor[sensor_index].atime = register_value;
        g_tcs_sensor[sensor_index].integration_parameter = parameter;
        g_tcs_sensor[sensor_index].frame_wait_ms = wait_ms;
        Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_SUCCESS, actual_value);
        return 1;
    }

    // x1至x5每次只读取目标通道，工作值2必须固定为000
    if ((function_code >= 1u) && (function_code <= 5u))
    {
        if (parameter != 0u)
        {
            return 0;
        }
        if (!Tcs3400Control_EnsureInitialized(sensor_index))
        {
            Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_ERROR, 0.0f);
            return 1;
        }
        if (function_code == 5u)
        {
            return Tcs3400Control_StartIr(sensor_index);
        }

        switch (function_code)
        {
            case 1u: channel = TCS3400_CHANNEL_RED; break;
            case 2u: channel = TCS3400_CHANNEL_GREEN; break;
            case 3u: channel = TCS3400_CHANNEL_BLUE; break;
            default: channel = TCS3400_CHANNEL_CLEAR; break;
        }
        return Tcs3400Control_StartChannel(sensor_index, channel);
    }

    // x6设置四档增益，只有写入成功才更新该路缓存
    if (function_code == 6u)
    {
        if (!Tcs3400Control_GetGainRegister(parameter, &register_value))
        {
            return 0;
        }
        if (!Tcs3400Control_EnsureInitialized(sensor_index))
        {
            Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_ERROR, 0.0f);
            return 1;
        }
        if (!TCS3400_SetGain(Tcs3400Control_GetBus(sensor_index),
                             register_value))
        {
            Tcs3400Control_Fail(sensor_index);
            return 1;
        }

        g_tcs_sensor[sensor_index].gain_register = register_value;
        Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_SUCCESS, (float)parameter);
        return 1;
    }

    // x7和x8为保留操作，任何参数都不访问底层硬件
    if ((function_code == 7u) || (function_code == 8u))
    {
        return 0;
    }
    if ((function_code == 9u) && (parameter != 0u))
    {
        return 0;
    }

    // x9只允许工作值2为000，并返回真实ID寄存器值
    if (function_code == 9u)
    {
        if (!Tcs3400Control_EnsureInitialized(sensor_index))
        {
            Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_ERROR, 0.0f);
            return 1;
        }
        if (!TCS3400_ReadID(Tcs3400Control_GetBus(sensor_index), &id))
        {
            Tcs3400Control_Fail(sensor_index);
            return 1;
        }

        Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_SUCCESS, (float)id);
        return 1;
    }

    return 0;
}

/**
 * @brief  轮询目标通道采样状态
 * @param  无
 * @retval 无
 */
void Tcs3400Control_Poll(void)
{
    uint8 valid;
    uint16 value;
    TcsSensorState_t *sensor_state;
    SoftI2C_Bus_t bus;

    if (g_tcs_task_state == TCS_TASK_IDLE)
    {
        return;
    }

    sensor_state = &g_tcs_sensor[g_tcs_task_sensor];
    // 先等待设定积分时间，再按固定节拍查询AVALID并限制总等待时间
    if (!Timer0_HasElapsed(g_tcs_task_start_ms,
                           sensor_state->integration_parameter))
    {
        return;
    }
    if (Timer0_HasElapsed(g_tcs_task_start_ms, sensor_state->frame_wait_ms))
    {
        Tcs3400Control_Fail(g_tcs_task_sensor);
        return;
    }
    if (!Timer0_HasElapsed(g_tcs_last_poll_ms,
                           TCS3400_DATA_POLL_INTERVAL_MS))
    {
        return;
    }

    g_tcs_last_poll_ms = Timer0_GetMs();
    bus = Tcs3400Control_GetBus(g_tcs_task_sensor);
    if (!TCS3400_GetDataValid(bus, &valid))
    {
        Tcs3400Control_Fail(g_tcs_task_sensor);
        return;
    }
    if (!valid)
    {
        return;
    }
    if (!TCS3400_ReadChannel(bus, g_tcs_task_channel, &value))
    {
        Tcs3400Control_Fail(g_tcs_task_sensor);
        return;
    }

    // IR值保存在局部变量中；恢复普通Clear积分后立即返回，不再等待第二帧
    if (g_tcs_task_ir)
    {
        if (!TCS3400_SetIRMap(bus, 0) ||
            !TCS3400_RestartIntegration(bus))
        {
            Tcs3400Control_Fail(g_tcs_task_sensor);
            return;
        }
    }

    sensor_state->ready = 1u;
    Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_SUCCESS, (float)value);
}

/**
 * @brief  获取并消费TCS命令结果
 * @param  value 正常结果存放地址
 * @retval 当前命令结果
 */
ProjectAsyncResult_t Tcs3400Control_TakeResult(float *value)
{
    ProjectAsyncResult_t result;

    if (g_tcs_result_state == PROJECT_ASYNC_RESULT_NONE)
    {
        return PROJECT_ASYNC_RESULT_NONE;
    }

    result = g_tcs_result_state;
    if ((result == PROJECT_ASYNC_RESULT_SUCCESS) && (value != 0))
    {
        *value = g_tcs_result_value;
    }
    else if ((result == PROJECT_ASYNC_RESULT_SUCCESS) && (value == 0))
    {
        result = PROJECT_ASYNC_RESULT_ERROR;
    }

    g_tcs_result_state = PROJECT_ASYNC_RESULT_NONE;
    g_tcs_task_sensor = TCS3400_SENSOR_COUNT;
    return result;
}

/**
 * @brief  查询TCS控制器忙状态
 * @param  无
 * @retval 1：任务运行或结果待取；0：空闲
 */
bit Tcs3400Control_IsBusy(void)
{
    return ((g_tcs_task_state != TCS_TASK_IDLE) ||
            (g_tcs_result_state != PROJECT_ASYNC_RESULT_NONE));
}
