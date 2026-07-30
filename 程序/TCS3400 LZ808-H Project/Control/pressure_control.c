#include "pressure_control.h"
#include "pressure_meter.h"
#include "trigger_output.h"

/*==============================================================================
 内部定义
 *============================================================================*/

typedef enum
{
    PRESSURE_TASK_IDLE = 0,      // 本项目中表示当前没有压力仪表命令
    PRESSURE_TASK_WAIT_VALUE,    // 本项目中表示正在等待0x0000压力原始值
    PRESSURE_TASK_WAIT_DECIMAL,  // 本项目中表示正在等待0x0012小数位
    PRESSURE_TASK_WAIT_ZERO      // 本项目中表示正在等待清零线圈回显
} PressureTaskState_t; // 本项目中记录稳定main的压力读取、换算和清零阶段

typedef struct
{
    uint16 lower_limit;  // 本项目中保存该组Y输出使用的整数压力下限
    uint16 upper_limit;  // 本项目中保存该组Y输出使用的整数压力上限
    uint8 lower_enabled; // 本项目中标记该组下限是否已经设置
    uint8 upper_enabled; // 本项目中标记该组上限是否已经设置
} PressureLimitConfig_t; // 本项目中保存Y1至Y4各自的RAM压力限值

static PressureTaskState_t g_pressure_task_state; // 记录当前压力命令执行阶段
static ProjectAsyncResult_t g_pressure_result_state; // 保存等待统一控制层取走的压力结果
static uint8 g_pressure_slave; // 记录当前工作值2指定的Modbus地址
static int16 g_pressure_raw_value; // 暂存LZ808-H返回的有符号压力原始值
static float g_pressure_result_value; // 保存换算后返回ICT的压力Float值
static PressureLimitConfig_t xdata g_pressure_limit[PRESSURE_LIMIT_CHANNEL_COUNT]; // 保存Y1至Y4压力限值
static uint8 g_pressure_compare_channel; // 0表示普通读取，1至4表示读取后更新对应Y通道

/**
 * @brief  完成当前压力命令
 * @param  result 命令结果
 * @param  value 正常返回值
 * @retval 无
 */
static void PressureControl_Finish(ProjectAsyncResult_t result, float value)
{
    // 本项目中该函数结束稳定压力状态机并保存等待上层取走的结果
    g_pressure_task_state = PRESSURE_TASK_IDLE; // 停止后续状态轮询
    g_pressure_result_state = result;           // 保留结果等待上层消费
    g_pressure_result_value = value;
    g_pressure_compare_channel = 0u;
}

/**
 * @brief  按仪表小数位缩放压力值
 * @param  raw_value 有符号原始值
 * @param  decimal_places 小数位数0至3
 * @param  value 实际压力值存放地址
 * @retval 1：缩放成功；0：小数位非法
 */
static bit PressureControl_Scale(int16 raw_value, int16 decimal_places, float *value)
{
    // 本项目中该函数按仪表小数位把有符号原始值换算成实际压力Float
    if (value == 0)
    {
        return 0;
    }

    // 仪表手册仅定义0至3位小数，超出范围时不把错误配置换算成压力值
    switch (decimal_places)
    {
        case 0: *value = (float)raw_value; return 1;
        case 1: *value = (float)raw_value / 10.0f; return 1;
        case 2: *value = (float)raw_value / 100.0f; return 1;
        case 3: *value = (float)raw_value / 1000.0f; return 1;
        default: return 0;
    }
}

/**
 * @brief  按当前限值更新指定Y输出
 * @param  channel 压力逻辑通道1~4
 * @param  actual_value 当前带符号实际压力值
 * @retval 无
 */
static void PressureControl_UpdateTrigger(uint8 channel, float actual_value)
{
    // 本项目中该函数按压力绝对值和对应限值更新Y1至Y4中的指定一路
    uint8 index;
    uint8 active;
    float comparison_value;

    index = channel - 1u;
    active = 0u;
    comparison_value = actual_value;
    if (comparison_value < 0.0f)
    {
        comparison_value = -comparison_value; // 限值只比较压力大小，不判断正负方向
    }

    if ((g_pressure_limit[index].lower_enabled != 0u) &&
        (comparison_value < (float)g_pressure_limit[index].lower_limit))
    {
        active = 1u;
    }
    if ((g_pressure_limit[index].upper_enabled != 0u) &&
        (comparison_value > (float)g_pressure_limit[index].upper_limit))
    {
        active = 1u;
    }

    TriggerOutput_Set(channel, active);
}

/**
 * @brief  启动普通压力读取或带比较的压力读取
 * @param  slave Modbus从机地址
 * @param  compare_channel 0表示普通读取，1~4表示比较后更新对应Y
 * @retval 1：命令已启动；0：参数、忙状态或发送错误
 */
static bit PressureControl_StartReadTask(uint8 slave, uint8 compare_channel)
{
    // 本项目中该函数保存压力地址和比较通道，并启动读取0x0000的第一帧事务
    if ((slave < MODBUS_SLAVE_ADDRESS_MIN) ||
        (slave > MODBUS_SLAVE_ADDRESS_MAX) || PressureControl_IsBusy())
    {
        return 0;
    }
    if ((compare_channel > PRESSURE_LIMIT_CHANNEL_COUNT) ||
        !PressureMeter_Start(PRESSURE_METER_READ_VALUE, slave))
    {
        return 0;
    }

    g_pressure_slave = slave;
    g_pressure_compare_channel = compare_channel;
    g_pressure_task_state = PRESSURE_TASK_WAIT_VALUE; // 第一帧读取原始压力
    return 1;
}

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化压力控制器
 * @param  无
 * @retval 无
 */
void PressureControl_Init(void)
{
    // 本项目中该函数初始化稳定压力链路、四组RAM限值和Y1至Y4输出
    uint8 index;

    PressureMeter_Init();
    TriggerOutput_Init();
    g_pressure_task_state = PRESSURE_TASK_IDLE;              // 当前无事务
    g_pressure_result_state = PROJECT_ASYNC_RESULT_NONE;      // 当前无待取结果
    g_pressure_slave = 0u;
    g_pressure_raw_value = 0;
    g_pressure_result_value = 0.0f;
    g_pressure_compare_channel = 0u;

    // 上电只清除RAM限值和Y输出，不向任何压力仪表写配置
    for (index = 0u; index < PRESSURE_LIMIT_CHANNEL_COUNT; index++)
    {
        g_pressure_limit[index].lower_limit = 0u;
        g_pressure_limit[index].upper_limit = 0u;
        g_pressure_limit[index].lower_enabled = 0u;
        g_pressure_limit[index].upper_enabled = 0u;
    }
}

/**
 * @brief  设置指定压力逻辑通道的下限
 * @param  channel 压力逻辑通道1~4
 * @param  limit_value 下限整数值0~999
 * @retval 1：设置成功；0：参数、上下限关系或忙状态错误
 */
bit PressureControl_SetLowerLimit(uint8 channel, uint16 limit_value)
{
    // 本项目中该函数校验并保存指定Y通道下限，不向压力仪表写入配置
    uint8 index;

    if ((channel < 1u) || (channel > PRESSURE_LIMIT_CHANNEL_COUNT) ||
        (limit_value > PRESSURE_LIMIT_VALUE_MAX) || PressureControl_IsBusy())
    {
        return 0;
    }

    index = channel - 1u;
    if ((g_pressure_limit[index].upper_enabled != 0u) &&
        (limit_value >= g_pressure_limit[index].upper_limit))
    {
        return 0;
    }

    g_pressure_limit[index].lower_limit = limit_value;
    g_pressure_limit[index].lower_enabled = 1u;
    TriggerOutput_Set(channel, 0u); // 新限值等待下一次成功读取后再判断
    PressureControl_Finish(PROJECT_ASYNC_RESULT_SUCCESS, (float)limit_value);
    return 1;
}

/**
 * @brief  设置指定压力逻辑通道的上限
 * @param  channel 压力逻辑通道1~4
 * @param  limit_value 上限整数值0~999
 * @retval 1：设置成功；0：参数、上下限关系或忙状态错误
 */
bit PressureControl_SetUpperLimit(uint8 channel, uint16 limit_value)
{
    // 本项目中该函数校验并保存指定Y通道上限，不覆盖有效下限关系
    uint8 index;

    if ((channel < 1u) || (channel > PRESSURE_LIMIT_CHANNEL_COUNT) ||
        (limit_value > PRESSURE_LIMIT_VALUE_MAX) || PressureControl_IsBusy())
    {
        return 0;
    }

    index = channel - 1u;
    if ((g_pressure_limit[index].lower_enabled != 0u) &&
        (limit_value <= g_pressure_limit[index].lower_limit))
    {
        return 0;
    }

    g_pressure_limit[index].upper_limit = limit_value;
    g_pressure_limit[index].upper_enabled = 1u;
    TriggerOutput_Set(channel, 0u); // 新限值等待下一次成功读取后再判断
    PressureControl_Finish(PROJECT_ASYNC_RESULT_SUCCESS, (float)limit_value);
    return 1;
}

/**
 * @brief  启动指定从机压力读取
 * @param  slave Modbus从机地址
 * @retval 1：命令已启动；0：参数、忙状态或发送错误
 */
bit PressureControl_StartRead(uint8 slave)
{
    // 本项目中该函数启动普通压力读取，完成后不改变任何Y输出
    return PressureControl_StartReadTask(slave, 0u);
}

/**
 * @brief  启动指定逻辑通道的压力读取和限值比较
 * @param  channel 压力逻辑通道1~4，对应Y1~Y4
 * @param  slave 本次读取的实际Modbus从机地址
 * @retval 1：命令已启动；0：参数、忙状态或发送错误
 */
bit PressureControl_StartCompareRead(uint8 channel, uint8 slave)
{
    // 本项目中该函数启动压力读取，并在成功后更新指定Y输出
    if ((channel < 1u) || (channel > PRESSURE_LIMIT_CHANNEL_COUNT))
    {
        return 0;
    }

    return PressureControl_StartReadTask(slave, channel);
}

/**
 * @brief  启动指定从机压力清零
 * @param  slave Modbus从机地址
 * @retval 1：命令已启动；0：参数、忙状态或发送错误
 */
bit PressureControl_StartZero(uint8 slave)
{
    // 本项目中该函数向指定LZ808-H发送稳定main的清零线圈请求
    // 地址越界或旧结果未消费时不访问RS485总线
    if ((slave < MODBUS_SLAVE_ADDRESS_MIN) ||
        (slave > MODBUS_SLAVE_ADDRESS_MAX) || PressureControl_IsBusy())
    {
        return 0;
    }
    if (!PressureMeter_Start(PRESSURE_METER_ZERO, slave))
    {
        return 0;
    }

    g_pressure_slave = slave;
    g_pressure_compare_channel = 0u;
    g_pressure_task_state = PRESSURE_TASK_WAIT_ZERO; // 等待写线圈回显
    return 1;
}

/**
 * @brief  轮询压力读取和清零事务
 * @param  无
 * @retval 无
 */
void PressureControl_Poll(void)
{
    // 本项目中该函数分两帧读取压力值和小数位，或等待清零回显完成
    int16 meter_value;
    float scaled_value;
    PressureMeterResult_t result;

    if (g_pressure_task_state == PRESSURE_TASK_IDLE)
    {
        return;
    }

    PressureMeter_Poll();
    result = PressureMeter_TakeResult(&meter_value);
    if (result == PRESSURE_METER_RESULT_NONE)
    {
        return;
    }
    if (result == PRESSURE_METER_RESULT_ERROR)
    {
        PressureControl_Finish(PROJECT_ASYNC_RESULT_ERROR, 0.0f);
        return;
    }

    // 压力值和小数位分两帧读取，避免阻塞主循环等待连续事务
    if (g_pressure_task_state == PRESSURE_TASK_WAIT_VALUE)
    {
        g_pressure_raw_value = meter_value;
        if (!PressureMeter_Start(PRESSURE_METER_READ_DECIMAL, g_pressure_slave))
        {
            PressureControl_Finish(PROJECT_ASYNC_RESULT_ERROR, 0.0f);
            return;
        }

        g_pressure_task_state = PRESSURE_TASK_WAIT_DECIMAL; // 下一帧读取缩放位数
        return;
    }

    if (g_pressure_task_state == PRESSURE_TASK_WAIT_DECIMAL)
    {
        if (!PressureControl_Scale(g_pressure_raw_value, meter_value, &scaled_value))
        {
            PressureControl_Finish(PROJECT_ASYNC_RESULT_ERROR, 0.0f);
            return;
        }

        if (g_pressure_compare_channel != 0u)
        {
            PressureControl_UpdateTrigger(g_pressure_compare_channel, scaled_value);
        }

        PressureControl_Finish(PROJECT_ASYNC_RESULT_SUCCESS, scaled_value);
        return;
    }

    // 剩余运行态只可能是清零等待，成功响应统一返回0.0f
    PressureControl_Finish(PROJECT_ASYNC_RESULT_SUCCESS, 0.0f);
}

/**
 * @brief  获取并消费压力命令结果
 * @param  value 正常结果存放地址
 * @retval 当前命令结果
 */
ProjectAsyncResult_t PressureControl_TakeResult(float *value)
{
    // 本项目中该函数向统一控制层交付一次压力结果并清除待取状态
    ProjectAsyncResult_t result;

    if (g_pressure_result_state == PROJECT_ASYNC_RESULT_NONE)
    {
        return PROJECT_ASYNC_RESULT_NONE;
    }

    result = g_pressure_result_state;
    if ((result == PROJECT_ASYNC_RESULT_SUCCESS) && (value != 0))
    {
        *value = g_pressure_result_value;
    }
    else if ((result == PROJECT_ASYNC_RESULT_SUCCESS) && (value == 0))
    {
        result = PROJECT_ASYNC_RESULT_ERROR;
    }

    g_pressure_result_state = PROJECT_ASYNC_RESULT_NONE; // 结果只允许消费一次
    return result;
}

/**
 * @brief  查询压力控制器忙状态
 * @param  无
 * @retval 1：任务运行或结果待取；0：空闲
 */
bit PressureControl_IsBusy(void)
{
    // 本项目中该函数判断压力任务是否仍在运行或有结果等待取走
    return ((g_pressure_task_state != PRESSURE_TASK_IDLE) ||
            (g_pressure_result_state != PROJECT_ASYNC_RESULT_NONE));
}
