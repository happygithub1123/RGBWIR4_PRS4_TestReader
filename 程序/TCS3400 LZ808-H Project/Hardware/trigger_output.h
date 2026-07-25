#ifndef __TRIGGER_OUTPUT_H__
#define __TRIGGER_OUTPUT_H__

#include "Public_cfg.h"

void TriggerOutput_Init(void); /* 初始化Y1~Y4为不触发状态。 */
void TriggerOutput_Set(uint8 channel, uint8 active); /* 设置指定Y通道。 */

#endif
