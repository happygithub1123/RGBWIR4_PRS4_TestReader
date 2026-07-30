#include "app_comm.h"
#include "mtf_protocol.h"
#include "sensor_control.h"
#include "Uart1.h"
#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

static uint8 idata g_app_rx_buffer[MTF_REQUEST_FRAME_LENGTH]; // 缓存ICT通过UART1发送的15字节MTF请求
static uint8 g_app_rx_index;                                  // 记录当前MTF请求已经缓存的字节数
static bit g_app_request_pending;                             // 标记当前ICT请求正在等待唯一一帧传感器结果
static uint32 g_app_rx_start_ms;                              // 记录MTF首字节到达时刻，用于判断50ms收帧超时

/**
 * @brief  清除当前不完整请求帧
 * @param  无
 * @retval 无
 */
static void AppComm_ResetReceiver(void)
{
    // 本项目中该函数统一清除一条未完成MTF请求的缓存位置和超时起点
    g_app_rx_index = 0u;       // 丢弃当前未完成的MTF帧并从帧头重新同步
    g_app_rx_start_ms = 0;     // 清除当前MTF帧的50ms超时计时起点
}

/**
 * @brief  处理一帧完整MTF请求
 * @param  buffer 请求帧缓冲区
 * @retval 无
 */
static void AppComm_ProcessFrame(const uint8 *buffer)
{
    // 本项目中该函数把完整MTF帧转换成传感器任务，并保证格式错误只返回异常
    MtfRequest_t request;

    // MTF格式错误时不访问TCS3400或压力仪表，只向ICT返回统一异常
    if (!MtfProtocol_ParseFrame(buffer, MTF_REQUEST_FRAME_LENGTH, &request))
    {
        MtfProtocol_SendError();
        return;
    }
    // 本项目严格一问一答，上一条传感器命令未返回前拒绝覆盖当前任务
    if (g_app_request_pending || SensorControl_IsBusy())
    {
        MtfProtocol_SendError();
        return;
    }
    // 统一控制层继续校验Pin1命令含义和工作值2范围，CD保留码不参与判断
    if (!SensorControl_Start(request.command_code, request.parameter))
    {
        MtfProtocol_SendError();
        return;
    }

    g_app_request_pending = 1; // 记录该ICT请求已启动，后续只允许发送一帧最终响应
}

/**
 * @brief  处理一个UART1接收字节
 * @param  dat 接收字节
 * @retval 无
 */
static void AppComm_HandleByte(uint8 dat)
{
    // 本项目中该函数在主循环内逐字节拼接固定15字节MTF请求，不在中断中解析协议
    if (g_app_rx_index == 0u)
    {
        // UART1空闲时只接受MTF帧头M，避免旧字节或噪声被拼入新命令
        if (dat != 'M')
        {
            return;
        }

        g_app_rx_buffer[0] = dat;
        g_app_rx_index = 1u;
        g_app_rx_start_ms = Timer0_GetMs(); // 从字符M到达时开始计算MTF整帧50ms超时
        return;
    }

    g_app_rx_buffer[g_app_rx_index++] = dat;
    // 未收满15字节就收到换行说明ICT帧长错误，立即丢帧并返回异常
    if ((dat == '\n') && (g_app_rx_index != MTF_REQUEST_FRAME_LENGTH))
    {
        AppComm_ResetReceiver();
        MtfProtocol_SendError();
        return;
    }

    // 收满15字节后再统一校验MTF结构，UART1收字节阶段不执行传感器业务
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
    // 本项目中该函数检查异步传感器任务，并在完成后向ICT发送唯一一帧Float结果
    float value;
    ProjectAsyncResult_t result;

    SensorControl_Poll();
    if (!g_app_request_pending)
    {
        return;
    }

    result = SensorControl_TakeResult(&value); // 取走当前传感器结果并保证该ICT命令只响应一次
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
    // 本项目中该函数建立MTF应用层上电空闲状态，并清空UART1可能残留的请求字节
    AppComm_ResetReceiver();
    g_app_request_pending = 0; // 主板上电时没有正在等待回复的ICT请求
    Uart1_ClearRx();
}

/**
 * @brief  轮询接收、命令执行和响应发送
 * @param  无
 * @retval 无
 */
void AppComm_Poll(void)
{
    // 本项目中该函数每轮先处理MTF超时和UART1溢出，再排空字节并推进传感器结果
    uint8 dat;

    AppComm_PollResult();
    // MTF不完整帧超过50ms后清空状态，防止后续命令继续拼接旧数据
    if ((g_app_rx_index > 0u) &&
        Timer0_HasElapsed(g_app_rx_start_ms, MTF_RECEIVE_TIMEOUT_MS))
    {
        AppComm_ResetReceiver();
        MtfProtocol_SendError();
    }

    // UART1环形缓冲溢出后MTF帧边界已经丢失，整帧作废并返回异常
    if (Uart1_TakeRxOverflow())
    {
        Uart1_ClearRx();
        AppComm_ResetReceiver();
        MtfProtocol_SendError();
        return;
    }

    // 主循环一次排空UART1缓存，协议解析和传感器访问始终不放进中断
    while (Uart1_ReadByte(&dat))
    {
        AppComm_HandleByte(dat);
    }
}
