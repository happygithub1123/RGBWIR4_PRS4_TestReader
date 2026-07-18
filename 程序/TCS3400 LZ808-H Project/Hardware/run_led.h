#ifndef __RUN_LED_H__
#define __RUN_LED_H__

#include "Public_cfg.h"

void RunLed_Init(void); /* 初始化运行指示灯。 */
void RunLed_Poll(void); /* 每1秒翻转运行指示灯。 */

#endif
