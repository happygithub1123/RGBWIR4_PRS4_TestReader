#ifndef __APP_COMM_H__
#define __APP_COMM_H__

#include "Public_cfg.h"

void AppComm_Init(void); /* 初始化MTF应用通信。 */
void AppComm_Poll(void); /* 轮询接收、命令执行和响应发送。 */

#endif
