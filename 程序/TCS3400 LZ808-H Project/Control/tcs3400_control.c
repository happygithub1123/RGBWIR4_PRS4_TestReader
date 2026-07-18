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
    TCS_TASK_WAIT_RGBC,
    TCS_TASK_WAIT_IR,
    TCS_TASK_WAIT_CLEAR_RECOVERY
} TcsTaskState_t;

typedef struct
{
    uint8 atime;
    uint8 gain_register;
    uint8 initialized;
    uint8 communication_ok;
    uint8 data_valid;
    uint8 ir_map_enabled;
    uint8 operation_state;
    uint16 integration_parameter;
    uint16 frame_wait_ms;
} TcsSensorState_t;

static TcsSensorState_t xdata g_tcs_sensor[TCS3400_SENSOR_COUNT];
static TcsTaskState_t g_tcs_task_state;
static ProjectAsyncResult_t g_tcs_result_state;
static uint8 g_tcs_task_sensor;
static uint8 g_tcs_task_channel;
static uint16 g_tcs_saved_ir;
static float g_tcs_result_value;
static uint32 g_tcs_task_start_ms;
static uint32 g_tcs_last_poll_ms;

/**
 * @brief  将传感器编号转换为软件I2C总线
 * @param  sensor_index 传感器数组下标
 * @retval 软件I2C总线编号
 */
static SoftI2C_Bus_t Tcs3400Control_GetBus(uint8 sensor_index)
{
    return (SoftI2C_Bus_t)sensor_index;
}

/**
 * @brief  查找五档积分时间配置
 * @param  parameter MTF积分时间参数
 * @param  atime ATIME寄存器值存放地址
 * @param  wait_ms 最大帧等待时间存放地址
 * @param  actual_ms 典型积分时间存放地址
 * @retval 1：参数属于五档之一；0：非法参数
 */
static bit Tcs3400Control_GetIntegrationConfig(uint16 parameter, uint8 *atime,
                                                uint16 *wait_ms, float *actual_ms)
{
    if ((atime == 0) || (wait_ms == 0) || (actual_ms == 0))
    {
        return 0;
    }

    // 只接受协议定义的五档；等待上限包含积分时间和通信余量
    switch (parameter)
    {
        case 3u:
            *atime = 0xFFu; *wait_ms = 13u; *actual_ms = 2.78f;
            return 1;
        case 28u:
            *atime = 0xF6u; *wait_ms = 40u; *actual_ms = 27.8f;
            return 1;
        case 103u:
            *atime = 0xDBu; *wait_ms = 119u; *actual_ms = 103.0f;
            return 1;
        case 178u:
            *atime = 0xC0u; *wait_ms = 198u; *actual_ms = 178.0f;
            return 1;
        case 712u:
            *atime = 0x00u; *wait_ms = 761u; *actual_ms = 712.0f;
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
    if (g_tcs_task_sensor < TCS3400_SENSOR_COUNT)
    {
        g_tcs_sensor[g_tcs_task_sensor].operation_state = TCS_TASK_IDLE;
    }
    g_tcs_task_state = TCS_TASK_IDLE; // 停止采样状态机
    g_tcs_result_value = value;
    g_tcs_result_state = result;      // 保留结果等待上层消费
}

/**
 * @brief  处理TCS通信失败并尝试关闭IR映射
 * @param  sensor_index 传感器数组下标
 * @retval 无
 */
static void Tcs3400Control_Fail(uint8 sensor_index)
{
    SoftI2C_Bus_t bus;

    bus = Tcs3400Control_GetBus(sensor_index);
    g_tcs_sensor[sensor_index].communication_ok = 0u; // 标记真实硬件通信异常
    g_tcs_sensor[sensor_index].data_valid = 0u;       // 禁止复用旧采样状态

    // IR流程失败时尽力关闭映射，避免后续Clear通道保持IR数据
    if (g_tcs_sensor[sensor_index].ir_map_enabled)
    {
        if (TCS3400_SetIRMap(bus, 0))
        {
            g_tcs_sensor[sensor_index].ir_map_enabled = 0u;
            TCS3400_RestartIntegration(bus);
        }
    }

    Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_ERROR, 0.0f);
}

/**
 * @brief  启动普通RGBC通道读取
 * @param  sensor_index 传感器数组下标
 * @param  channel 通道编号1至4
 * @retval 1：命令已接收；0：启动失败
 */
static bit Tcs3400Control_StartRgbc(uint8 sensor_index, uint8 channel)
{
    SoftI2C_Bus_t bus;

    bus = Tcs3400Control_GetBus(sensor_index);
    // 普通通道先关闭IR映射，再强制启动一帧新积分
    if (!TCS3400_SetIRMap(bus, 0) || !TCS3400_RestartIntegration(bus))
    {
        Tcs3400Control_Fail(sensor_index);
        return 1;
    }

    g_tcs_sensor[sensor_index].ir_map_enabled = 0u;
    g_tcs_sensor[sensor_index].data_valid = 0u;
    g_tcs_sensor[sensor_index].operation_state = TCS_TASK_WAIT_RGBC;
    g_tcs_task_sensor = sensor_index;
    g_tcs_task_channel = channel;
    g_tcs_task_start_ms = Timer0_GetMs(); // 同时作为积分等待和超时基准
    g_tcs_last_poll_ms = g_tcs_task_start_ms;
    g_tcs_task_state = TCS_TASK_WAIT_RGBC;
    return 1;
}

/**
 * @brief  启动IR通道读取
 * @param  sensor_index 传感器数组下标
 * @retval 1：命令已接收；0：启动失败
 */
static bit Tcs3400Control_StartIr(uint8 sensor_index)
{
    SoftI2C_Bus_t bus;

    bus = Tcs3400Control_GetBus(sensor_index);
    // IR数据映射到Clear端口，必须重新积分后才能读取
    if (!TCS3400_SetIRMap(bus, 1))
    {
        Tcs3400Control_Fail(sensor_index);
        return 1;
    }

    g_tcs_sensor[sensor_index].ir_map_enabled = 1u;
    if (!TCS3400_RestartIntegration(bus))
    {
        Tcs3400Control_Fail(sensor_index);
        return 1;
    }

    g_tcs_sensor[sensor_index].data_valid = 0u;
    g_tcs_sensor[sensor_index].operation_state = TCS_TASK_WAIT_IR;
    g_tcs_task_sensor = sensor_index;
    g_tcs_task_channel = 5u;
    g_tcs_task_start_ms = Timer0_GetMs(); // 同时作为积分等待和超时基准
    g_tcs_last_poll_ms = g_tcs_task_start_ms;
    g_tcs_task_state = TCS_TASK_WAIT_IR;
    return 1;
}

/**
 * @brief  确保指定传感器已经完成真实硬件初始化
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
    if (sensor_state->initialized && sensor_state->communication_ok)
    {
        return 1;
    }

    // 断线重接后按该路缓存配置重新初始化，初次上电缓存即为178ms和16x
    if (!TCS3400_Init(Tcs3400Control_GetBus(sensor_index),
                      sensor_state->atime,
                      sensor_state->gain_register))
    {
        sensor_state->initialized = 0u;
        sensor_state->communication_ok = 0u;
        sensor_state->data_valid = 0u;
        sensor_state->ir_map_enabled = 0u;
        return 0;
    }

    sensor_state->initialized = 1u;
    sensor_state->communication_ok = 1u;
    sensor_state->data_valid = 0u;
    sensor_state->ir_map_enabled = 0u;
    sensor_state->operation_state = TCS_TASK_IDLE;
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

    // 四路先装载固件默认缓存，再逐路写入真实寄存器并检查I2C应答
    for (index = 0u; index < TCS3400_SENSOR_COUNT; index++)
    {
        g_tcs_sensor[index].atime = TCS3400_DEFAULT_ATIME_REGISTER;
        g_tcs_sensor[index].gain_register = TCS3400_DEFAULT_GAIN_REGISTER;
        g_tcs_sensor[index].integration_parameter = TCS3400_DEFAULT_INTEGRATION_MS;
        g_tcs_sensor[index].frame_wait_ms = 198u;
        g_tcs_sensor[index].initialized = 0u;
        g_tcs_sensor[index].communication_ok = 0u;
        g_tcs_sensor[index].data_valid = 0u;
        g_tcs_sensor[index].ir_map_enabled = 0u;
        g_tcs_sensor[index].operation_state = TCS_TASK_IDLE;

        // 初始化失败时保留异常状态，后续收到该路命令时再自动重试
        Tcs3400Control_EnsureInitialized(index);
    }
}

/**
 * @brief  启动TCS3400命令
 * @param  sensor 传感器编号1至4
 * @param  operation 操作编号0至9
 * @param  parameter MTF参数
 * @retval 1：命令格式有效并已接收；0：命令或参数无效
 */
bit Tcs3400Control_Start(uint8 sensor, uint8 operation, uint16 parameter)
{
    bit rollback_ok;
    uint8 sensor_index;
    uint8 register_value;
    uint8 id;
    uint16 wait_ms;
    float actual_value;

    // 校验设备范围并保护正在运行或尚未消费的结果
    if ((sensor < 1u) || (sensor > TCS3400_SENSOR_COUNT) ||
        (g_tcs_task_state != TCS_TASK_IDLE) ||
        (g_tcs_result_state != PROJECT_ASYNC_RESULT_NONE))
    {
        return 0;
    }

    sensor_index = (uint8)(sensor - 1u);
    g_tcs_task_sensor = sensor_index;

    // x0设置五档积分时间，写入成功后才更新该路缓存
    if (operation == 0u)
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
        if (!TCS3400_SetATIME(Tcs3400Control_GetBus(sensor_index), register_value))
        {
            // 新值写入失败时回写旧值；回滚失败则保持通信异常
            rollback_ok = TCS3400_SetATIME(
                Tcs3400Control_GetBus(sensor_index),
                g_tcs_sensor[sensor_index].atime);
            Tcs3400Control_Fail(sensor_index);
            g_tcs_sensor[sensor_index].communication_ok = rollback_ok ? 1u : 0u;
            return 1;
        }

        g_tcs_sensor[sensor_index].atime = register_value;
        g_tcs_sensor[sensor_index].integration_parameter = parameter;
        g_tcs_sensor[sensor_index].frame_wait_ms = wait_ms;
        g_tcs_sensor[sensor_index].communication_ok = 1u;
        Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_SUCCESS, actual_value);
        return 1;
    }

    // x1至x5只读取当前配置下的新数据，EFG必须固定为000
    if ((operation >= 1u) && (operation <= 5u))
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
        if (operation == 5u)
        {
            return Tcs3400Control_StartIr(sensor_index);
        }
        return Tcs3400Control_StartRgbc(sensor_index, operation);
    }

    // x6设置四档增益，写入成功后才更新该路缓存
    if (operation == 6u)
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
        if (!TCS3400_SetGain(Tcs3400Control_GetBus(sensor_index), register_value))
        {
            // 新值写入失败时回写旧值；回滚失败则保持通信异常
            rollback_ok = TCS3400_SetGain(
                Tcs3400Control_GetBus(sensor_index),
                g_tcs_sensor[sensor_index].gain_register);
            Tcs3400Control_Fail(sensor_index);
            g_tcs_sensor[sensor_index].communication_ok = rollback_ok ? 1u : 0u;
            return 1;
        }

        g_tcs_sensor[sensor_index].gain_register = register_value;
        g_tcs_sensor[sensor_index].communication_ok = 1u;
        Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_SUCCESS, (float)parameter);
        return 1;
    }

    // x7和x8为保留操作，任何参数都不访问底层硬件
    if ((operation == 7u) || (operation == 8u))
    {
        return 0;
    }
    if ((operation == 9u) && (parameter != 0u))
    {
        return 0;
    }
    // x9只允许EFG=000，并直接返回真实ID寄存器值
    if (operation == 9u)
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
        g_tcs_sensor[sensor_index].communication_ok = 1u;
        Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_SUCCESS, (float)id);
        return 1;
    }

    return 0;
}

/**
 * @brief  轮询TCS采样和IR恢复状态
 * @param  无
 * @retval 无
 */
void Tcs3400Control_Poll(void)
{
    uint8 valid;
    uint16 value;
    Tcs3400RgbcData_t rgbc;
    TcsSensorState_t *sensor_state;
    SoftI2C_Bus_t bus;

    if (g_tcs_task_state == TCS_TASK_IDLE)
    {
        return;
    }

    sensor_state = &g_tcs_sensor[g_tcs_task_sensor];
    // 先等待最短积分时间，再按轮询间隔检查AVALID并限制总等待时间
    if (!Timer0_HasElapsed(g_tcs_task_start_ms, sensor_state->integration_parameter))
    {
        return;
    }
    if (Timer0_HasElapsed(g_tcs_task_start_ms, sensor_state->frame_wait_ms))
    {
        Tcs3400Control_Fail(g_tcs_task_sensor);
        return;
    }
    if (!Timer0_HasElapsed(g_tcs_last_poll_ms, TCS3400_DATA_POLL_INTERVAL_MS))
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
    sensor_state->data_valid = valid; // 记录本次从状态寄存器读取的真实结果
    if (!valid)
    {
        return;
    }

    // RGBC在同一有效帧中读取，再按命令选择一个通道返回
    if (g_tcs_task_state == TCS_TASK_WAIT_RGBC)
    {
        if (!TCS3400_ReadRGBC(bus, &rgbc))
        {
            Tcs3400Control_Fail(g_tcs_task_sensor);
            return;
        }

        switch (g_tcs_task_channel)
        {
            case 1u: value = rgbc.red; break;
            case 2u: value = rgbc.green; break;
            case 3u: value = rgbc.blue; break;
            default: value = rgbc.clear; break;
        }

        sensor_state->communication_ok = 1u;
        Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_SUCCESS, (float)value);
        return;
    }

    // 保存映射后的IR值，再关闭映射并启动Clear恢复积分
    if (g_tcs_task_state == TCS_TASK_WAIT_IR)
    {
        if (!TCS3400_ReadClearPort(bus, &g_tcs_saved_ir) ||
            !TCS3400_SetIRMap(bus, 0))
        {
            Tcs3400Control_Fail(g_tcs_task_sensor);
            return;
        }

        sensor_state->ir_map_enabled = 0u;
        if (!TCS3400_RestartIntegration(bus))
        {
            Tcs3400Control_Fail(g_tcs_task_sensor);
            return;
        }

        sensor_state->data_valid = 0u;
        sensor_state->operation_state = TCS_TASK_WAIT_CLEAR_RECOVERY;
        g_tcs_task_state = TCS_TASK_WAIT_CLEAR_RECOVERY;
        g_tcs_task_start_ms = Timer0_GetMs();
        g_tcs_last_poll_ms = g_tcs_task_start_ms;
        return;
    }

    // Clear恢复帧有效后才返回已保存IR，确保下一条Clear读取不受映射影响
    sensor_state->communication_ok = 1u;
    Tcs3400Control_Finish(PROJECT_ASYNC_RESULT_SUCCESS, (float)g_tcs_saved_ir);
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

    g_tcs_result_state = PROJECT_ASYNC_RESULT_NONE; // 结果只允许消费一次
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
