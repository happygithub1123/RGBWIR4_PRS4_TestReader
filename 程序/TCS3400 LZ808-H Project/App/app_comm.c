#include "app_comm.h"
#include "mtf_protocol.h"
#include "sensor_control.h"
#include "Uart1.h"
#include "Timer0.h"

/*==============================================================================
 内部定义
 *============================================================================*/

static uint8 idata g_app_rx_buffer[MTF_REQUEST_FRAME_LENGTH];
static uint8 g_app_rx_index;
static bit g_app_request_pending;
static uint32 g_app_rx_start_ms;

/**
 * @brief  清除当前不完整请求帧
 * @param  无
 * @retval 无
 */
static void AppComm_ResetReceiver(void)
{
    g_app_rx_index = 0u;
    g_app_rx_start_ms = 0UL;
}

/**
 * @brief  处理一帧完整MTF请求
 * @param  buffer 请求帧缓冲区
 * @retval 无
 */
static void AppComm_ProcessFrame(const uint8 *buffer)
{
    MtfRequest_t request;

    if (!MtfProtocol_ParseFrame(buffer, MTF_REQUEST_FRAME_LENGTH, &request))
    {
        MtfProtocol_SendError();
        return;
    }
    if (g_app_request_pending || SensorControl_IsBusy())
    {
        MtfProtocol_SendError();
        return;
    }
    if (!SensorControl_Start(request.command, request.parameter))
    {
        MtfProtocol_SendError();
        return;
    }

    g_app_request_pending = 1;
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
        if (dat != 'M')
        {
            return;
        }

        g_app_rx_buffer[0] = dat;
        g_app_rx_index = 1u;
        g_app_rx_start_ms = Timer0_GetMs();
        return;
    }

    g_app_rx_buffer[g_app_rx_index++] = dat;
    if ((dat == '\n') && (g_app_rx_index != MTF_REQUEST_FRAME_LENGTH))
    {
        AppComm_ResetReceiver();
        MtfProtocol_SendError();
        return;
    }

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

    result = SensorControl_TakeResult(&value);
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
    g_app_request_pending = 0;
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
    if ((g_app_rx_index > 0u) &&
        Timer0_HasElapsed(g_app_rx_start_ms, MTF_RECEIVE_TIMEOUT_MS))
    {
        AppComm_ResetReceiver();
        MtfProtocol_SendError();
    }

    if (Uart1_TakeRxOverflow())
    {
        Uart1_ClearRx();
        AppComm_ResetReceiver();
        MtfProtocol_SendError();
        return;
    }

    while (Uart1_ReadByte(&dat))
    {
        AppComm_HandleByte(dat);
    }
}
