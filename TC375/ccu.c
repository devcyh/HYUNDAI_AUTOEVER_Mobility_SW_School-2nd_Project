#include "ccu.h"

#include "Ifx_Lwip.h"

#include "my_stdio.h"
#include "tof.h"
#include "ultrasonic.h"

#include "emer_alert.h"

void run_ccu (void)
{
    while (true)
    {
        /* Process data queues */
        ToF_ProcessQueue();
        Ultrasonic_ProcessQueue();

        /* Check ethernet data & process */
        Ifx_Lwip_pollTimerFlags(); /* Poll LwIP timers and trigger protocols execution if required */
        Ifx_Lwip_pollReceiveFlags(); /* Receive data package through ETH */

        /* Run emer alert */
        EmerAlert_Update_Periodic();
    }
}
