#include "app_comm.h"
#include "mtf_protocol.h"
#include "sensor_control.h"
#include "Uart1.h"
#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

static uint8 idata g_app_rx_buffer[MTF_REQUEST_FRAME_LENGTH]; //缓存UART1接收的MTF固定长度请求帧
static uint8 g_app_rx_index;                                  //记录当前请求帧已接收字节数
static bit g_app_request_pending;                             //标记业务层异步请求正在等待唯一响应
static uint32 g_app_rx_start_ms;                              //记录当前请求帧首字节接收时间

/**
 * @brief  清除当前不完整请求帧
 * @param  无
 * @retval 无
 */
static void AppComm_ResetReceiver(void)
{
    g_app_rx_index = 0u;       // 丢弃当前未完成帧
    g_app_rx_start_ms = 0;     // 取消本帧接收超时计时
}

/**
 * @brief  处理一帧完整MTF请求
 * @param  buffer 请求帧缓冲区
 * @retval 无
 */
static void AppComm_ProcessFrame(const uint8 *buffer)
{
    MtfRequest_t request;

    // 格式错误时不进入业务层，也不访问任何传感器
    if (!MtfProtocol_ParseFrame(buffer, MTF_REQUEST_FRAME_LENGTH, &request))
    {
        MtfProtocol_SendError();
        return;
    }
    // 严格一问一答，上一请求未完成时拒绝新请求
    if (g_app_request_pending || SensorControl_IsBusy())
    {
        MtfProtocol_SendError();
        return;
    }
    // 业务层负责校验AB指令码和EFG取值范围
    if (!SensorControl_Start(request.command_code, request.parameter))
    {
        MtfProtocol_SendError();
        return;
    }

    g_app_request_pending = 1; // 等待异步操作产生唯一响应
}

/**
 * @brief  处理一个UART1接收字节
 * @param  dat 接收字节
 * @retval 无
 */
static void AppComm_HandleByte(uint8 dat)
{
    if (g_app_rx_index == 0u)
    {
        // 空闲时只接受帧头M，用于从连续字节流重新同步
        if (dat != 'M')
        {
            return;
        }

        g_app_rx_buffer[0] = dat;
        g_app_rx_index = 1u;
        g_app_rx_start_ms = Timer0_GetMs(); // 从首字节开始计算整帧超时
        return;
    }

    g_app_rx_buffer[g_app_rx_index++] = dat;
    // 提前收到换行表示帧长错误，立即清帧并返回异常
    if ((dat == '\n') && (g_app_rx_index != MTF_REQUEST_FRAME_LENGTH))
    {
        AppComm_ResetReceiver();
        MtfProtocol_SendError();
        return;
    }

    // 固定收满15字节后一次性校验，接收阶段不解析字段
    if (g_app_rx_index == MTF_REQUEST_FRAME_LENGTH)
    {
        AppComm_ProcessFrame(g_app_rx_buffer);
        AppComm_ResetReceiver();
    }
}

/**
 * @brief  轮询当前命令执行结果
 * @param  无
 * @retval 无
 */
static void AppComm_PollResult(void)
{
    float value;
    ProjectAsyncResult_t result;

    SensorControl_Poll();
    if (!g_app_request_pending)
    {
        return;
    }

    result = SensorControl_TakeResult(&value); // 结果只允许消费一次
    if (result == PROJECT_ASYNC_RESULT_SUCCESS)
    {
        MtfProtocol_SendValue(value);
        g_app_request_pending = 0;
    }
    else if (result == PROJECT_ASYNC_RESULT_ERROR)
    {
        MtfProtocol_SendError();
        g_app_request_pending = 0;
    }
}

/*==============================================================================
 对外函数
 *============================================================================*/

/**
 * @brief  初始化MTF应用通信
 * @param  无
 * @retval 无
 */
void AppComm_Init(void)
{
    AppComm_ResetReceiver();
    g_app_request_pending = 0; // 上电时没有待回复请求
    Uart1_ClearRx();
}

/**
 * @brief  轮询接收、命令执行和响应发送
 * @param  无
 * @retval 无
 */
void AppComm_Poll(void)
{
    uint8 dat;

    AppComm_PollResult();
    // 不完整帧超时后清除接收状态，避免后续字节拼入旧帧
    if ((g_app_rx_index > 0u) &&
        Timer0_HasElapsed(g_app_rx_start_ms, MTF_RECEIVE_TIMEOUT_MS))
    {
        AppComm_ResetReceiver();
        MtfProtocol_SendError();
    }

    // 环形缓冲溢出后帧边界已不可信，必须整体丢弃
    if (Uart1_TakeRxOverflow())
    {
        Uart1_ClearRx();
        AppComm_ResetReceiver();
        MtfProtocol_SendError();
        return;
    }

    // 单次轮询排空当前缓存，不在中断内执行协议解析
    while (Uart1_ReadByte(&dat))
    {
        AppComm_HandleByte(dat);
    }
}
