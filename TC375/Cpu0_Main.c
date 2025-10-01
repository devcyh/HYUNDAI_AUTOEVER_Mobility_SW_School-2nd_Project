#include "Cpu0_Init.h"
#include "ccu.h"

void core0_main (void)
{
    if (!core0_init())
        return;

    run_ccu();
}
