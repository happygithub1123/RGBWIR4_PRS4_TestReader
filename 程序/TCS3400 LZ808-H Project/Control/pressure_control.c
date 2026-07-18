#include "pressure_control.h"
#include "pressure_meter.h"

/*==============================================================================
 内部定义
 *============================================================================*/

typedef enum
{
    PRESSURE_TASK_IDLE = 0,
    PRESSURE_TASK_WAIT_VALUE,
    PRESSURE_TASK_WAIT_DECIMAL,
    PRESSURE_TASK_WAIT_ZERO
} PressureTaskState_t;

static PressureTaskState_t g_pressure_task_state;
static ProjectAsyncResult_t g_pressure_result_state;
static uint8 g_pressure_slave;
static int16 g_pressure_raw_value;
static float g_pressure_result_value;

/**
 * @brief  完成当前压力命令
 * @param  result 命令结果
 * @param  value 正常返回值
 * @retval 无
 */
static void PressureControl_Finish(ProjectAsyncResult_t result, float value)
{
    g_pressure_task_state = PRESSURE_TASK_IDLE; // 停止后续状态轮询
    g_pressure_result_state = result;           // 保留结果等待上层消费
    g_pressure_result_value = value;
}

/**
 * @brief  按仪表小数位缩放压力值
 * @param  raw_value 有符号原始值
 * @param  decimal_places 小数位数0至4
 * @param  value 实际压力值存放地址
 * @retval 1：缩放成功；0：小数位非法
 */
static bit PressureControl_Scale(int16 raw_value, int16 decimal_places, float *value)
{
    if (value == 0)
    {
        return 0;
    }

    // 仪表仅定义0至4位小数，超出范围视为响应内容错误
    switch (decimal_places)
    {
        case 0: *value = (float)raw_value; return 1;
        case 1: *value = (float)raw_value / 10.0f; return 1;
        case 2: *value = (float)raw_value / 100.0f; return 1;
        case 3: *value = (float)raw_value / 1000.0f; return 1;
        case 4: *value = (float)raw_value / 10000.0f; return 1;
        default: return 0;
    }
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
    PressureMeter_Init();
    g_pressure_task_state = PRESSURE_TASK_IDLE;              // 当前无事务
    g_pressure_result_state = PROJECT_ASYNC_RESULT_NONE;      // 当前无待取结果
    g_pressure_slave = 0u;
    g_pressure_raw_value = 0;
    g_pressure_result_value = 0.0f;
}

/**
 * @brief  启动指定从机压力读取
 * @param  slave Modbus从机地址
 * @retval 1：命令已启动；0：参数、忙状态或发送错误
 */
bit PressureControl_StartRead(uint8 slave)
{
    // 地址越界或旧结果未消费时不访问RS485总线
    if ((slave < MODBUS_SLAVE_ADDRESS_MIN) ||
        (slave > MODBUS_SLAVE_ADDRESS_MAX) || PressureControl_IsBusy())
    {
        return 0;
    }
    if (!PressureMeter_Start(PRESSURE_METER_READ_VALUE, slave))
    {
        return 0;
    }

    g_pressure_slave = slave;
    g_pressure_task_state = PRESSURE_TASK_WAIT_VALUE; // 第一步读取原始压力
    return 1;
}

/**
 * @brief  启动指定从机压力清零
 * @param  slave Modbus从机地址
 * @retval 1：命令已启动；0：参数、忙状态或发送错误
 */
bit PressureControl_StartZero(uint8 slave)
{
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
    return ((g_pressure_task_state != PRESSURE_TASK_IDLE) ||
            (g_pressure_result_state != PROJECT_ASYNC_RESULT_NONE));
}
