#include "tcs3400_control.h"
#include "tcs3400_driver.h"
#include "soft_i2c.h"
#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

typedef enum
{
    TCS_TASK_IDLE = 0,          // 本项目中表示当前没有TCS3400采样任务
    TCS_TASK_WAIT_RGBC,         // 本项目中表示正在等待普通R、G、B或Clear积分完成
    TCS_TASK_WAIT_IR,           // 本项目中表示正在等待IR映射后的积分完成
    TCS_TASK_WAIT_CLEAR_RECOVERY // 本项目中表示IR读取后正在等待普通Clear恢复完成
} TcsTaskState_t; // 本项目中记录稳定main的RGBC、IR及Clear恢复采样阶段

typedef struct
{
    uint8 atime;                  // 本项目中保存该路最后成功的ATIME寄存器值
    uint8 gain_register;          // 本项目中保存该路最后成功的增益寄存器值
    uint8 initialized;            // 本项目中标记该路是否已经完成真实寄存器初始化
    uint8 communication_ok;       // 本项目中标记该路最近一次I2C通信是否正常
    uint8 data_valid;             // 本项目中记录当前积分数据是否已经通过AVALID确认
    uint8 ir_map_enabled;         // 本项目中记录Clear寄存器当前是否映射为IR
    uint8 operation_state;        // 本项目中保存该路当前执行的稳定采样阶段
    uint16 integration_parameter; // 本项目中保存工作值2对应的积分时间档位
    uint16 frame_wait_ms;         // 本项目中保存该积分档允许等待新数据的最长时间
} TcsSensorState_t; // 本项目中保存四路TCS3400彼此独立的配置和通信状态

static TcsSensorState_t xdata g_tcs_sensor[TCS3400_SENSOR_COUNT]; // 保存四路TCS3400的稳定配置和状态
static TcsTaskState_t g_tcs_task_state;                           // 记录当前环境光任务所处采样阶段
static ProjectAsyncResult_t g_tcs_result_state;                   // 保存等待上层取走的环境光结果状态
static uint8 g_tcs_task_sensor;                                   // 记录当前Pin1选择的TCS3400数组下标
static uint8 g_tcs_task_channel;                                  // 记录当前读取R、G、B、Clear或IR通道
static uint16 g_tcs_saved_ir;                                     // 暂存IR值直到普通Clear恢复完成
static float g_tcs_result_value;                                  // 保存最终返回ICT的环境光Float值
static uint32 g_tcs_task_start_ms;                                // 记录当前积分或恢复阶段的开始时刻
static uint32 g_tcs_last_poll_ms;                                 // 限制AVALID查询频率的上次轮询时刻

/**
 * @brief  将传感器编号转换为软件I2C总线
 * @param  sensor_index 传感器数组下标
 * @retval 软件I2C总线编号
 */
static SoftI2C_Bus_t Tcs3400Control_GetBus(uint8 sensor_index)
{
    // 本项目中传感器数组下标与四路软件I2C总线编号一一对应
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
    // 本项目中该函数把六档工作值2转换成ATIME、等待时间和Float返回值
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
    // 本项目中该函数把1x、4x、16x和64x工作值2转换成CONTROL寄存器值
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
    // 本项目中该函数结束稳定TCS状态机并保存等待统一控制层取走的结果
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
    // 本项目中该函数标记I2C通信异常，并尽力关闭可能残留的IR映射
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
    // 本项目中该函数按稳定main流程关闭IR映射并启动一帧完整RGBC积分
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
    // 本项目中该函数开启IR映射并启动IR读取所需的新积分
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
    // 本项目中该函数在命令执行前按最后成功配置确认目标传感器真实可用
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

    // 断线重接后按该路缓存配置重新初始化，初次上电缓存即为27.8ms和4x
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
    // 本项目中该函数装载27.8ms和4x默认值，并按稳定main逐路初始化四个传感器
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
        g_tcs_sensor[index].integration_parameter = TCS3400_DEFAULT_INTEGRATION_PARAMETER;
        g_tcs_sensor[index].frame_wait_ms = TCS3400_DEFAULT_FRAME_WAIT_MS;
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
 * @param  target_code 目标码，传感器编号1至4
 * @param  function_code 功能码0至9
 * @param  parameter MTF参数
 * @retval 1：命令格式有效并已接收；0：命令或参数无效
 */
bit Tcs3400Control_Start(uint8 target_code, uint8 function_code, uint16 parameter)
{
    // 本项目中该函数校验Pin1和工作值2，并启动设置、RGBC、IR或ID操作
    bit rollback_ok;
    uint8 sensor_index;
    uint8 register_value;
    uint8 id;
    uint16 wait_ms;
    float actual_value;

    // 校验设备范围并保护正在运行或尚未消费的结果
    if ((target_code < 1u) || (target_code > TCS3400_SENSOR_COUNT) ||
        (g_tcs_task_state != TCS_TASK_IDLE) ||
        (g_tcs_result_state != PROJECT_ASYNC_RESULT_NONE))
    {
        return 0;
    }

    sensor_index = (uint8)(target_code - 1u);
    g_tcs_task_sensor = sensor_index;

    // x0设置六档积分时间，写入成功后才更新该路缓存
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
        return Tcs3400Control_StartRgbc(sensor_index, function_code);
    }

    // x6设置四档增益，写入成功后才更新该路缓存
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
    if ((function_code == 7u) || (function_code == 8u))
    {
        return 0;
    }
    if ((function_code == 9u) && (parameter != 0u))
    {
        return 0;
    }
    // x9只允许EFG=000，并直接返回真实ID寄存器值
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
    // 本项目中该函数等待AVALID、只读取指定普通通道并完成IR后的Clear恢复
    uint8 valid;
    uint16 value;
    Tcs3400Channel_t channel;
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

    // 普通命令只读取Pin1指定的一个通道，积分和AVALID流程保持稳定main不变
    if (g_tcs_task_state == TCS_TASK_WAIT_RGBC)
    {
        switch (g_tcs_task_channel)
        {
            case 1u: channel = TCS3400_CHANNEL_RED; break;
            case 2u: channel = TCS3400_CHANNEL_GREEN; break;
            case 3u: channel = TCS3400_CHANNEL_BLUE; break;
            default: channel = TCS3400_CHANNEL_CLEAR; break;
        }

        if (!TCS3400_ReadChannel(bus, channel, &value))
        {
            Tcs3400Control_Fail(g_tcs_task_sensor);
            return;
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
    // 本项目中该函数向统一控制层交付一次环境光结果并清除待取状态
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
    // 本项目中该函数判断TCS3400任务是否仍在采样或有结果等待取走
    return ((g_tcs_task_state != TCS_TASK_IDLE) ||
            (g_tcs_result_state != PROJECT_ASYNC_RESULT_NONE));
}
