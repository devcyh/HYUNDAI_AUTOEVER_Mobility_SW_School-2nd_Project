#include "PR.h"

#include "evadc.h"
#include "stm.h"

void PR_Init (void)
{
    Evadc_Init();
}

PRData_t PR_GetData (void)
{
    static const uint64_t interval_us = 100000; // 0.1sec
    static PRData_t ret = {0, 0};
    uint64_t now = STM0_getTimeUs();

    if (now >= ret.received_time_us + interval_us)
    {
        ret.val = Evadc_readPR();
        ret.received_time_us = now;
    }

    return ret;
}
