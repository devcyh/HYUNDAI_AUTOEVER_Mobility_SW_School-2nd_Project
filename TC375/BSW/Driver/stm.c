#include "stm.h"

#include "IfxStm.h"

#include "isr_priority.h"
#include "gpio.h"

#define STM_FREQUENCY_HZ    100000000ULL                    // 100 MHz
#define TICKS_PER_US        (STM_FREQUENCY_HZ / 1000000ULL) // 100 ticks per us
#define TICKS_PER_MS        (STM_FREQUENCY_HZ / 1000ULL)    // 100,000 ticks per ms

uint64_t STM0_getTime10ns (void)
{
    uint32_t upper1, lower, upper2;

    __dsync();
    do
    {
        upper1 = MODULE_STM0.TIM6.U;
        lower = MODULE_STM0.TIM0.U;
        upper2 = MODULE_STM0.TIM6.U;
    }while (upper1 != upper2);
    __isync();

    return (((uint64_t) upper1) << 32) | lower;
}

uint64_t STM0_getTimeUs (void)
{
    uint64_t ticks = STM0_getTime10ns();
    return ticks / TICKS_PER_US;
}

uint64_t STM0_getTimeMs (void)
{
    uint64_t ticks = STM0_getTime10ns();
    return ticks / TICKS_PER_MS;
}

IFX_INTERRUPT(Stm0IsrHandler, 0, ISR_PRIORITY_STM0);
void Stm0IsrHandler (void)
{
    GPIO_SetUltTrig(false);
}

void Stm0_Init (void)
{
    MODULE_STM0.CMCON.B.MSIZE0 = 31; // Compare 32 bit size
    MODULE_STM0.CMCON.B.MSTART0 = 0; // Compare start at 0 bit
    MODULE_STM0.ICR.B.CMP0OS = 0; // Interrupt Output 0

    MODULE_SRC.STM.STM[0].SR[0].B.TOS = 0; // CPU 0
    MODULE_SRC.STM.STM[0].SR[0].B.SRPN = ISR_PRIORITY_STM0;
    MODULE_SRC.STM.STM[0].SR[0].B.CLRR = 1; // Clear Interrupt Req.
    MODULE_SRC.STM.STM[0].SR[0].B.SRE = 1; // Enable Interrupt

    MODULE_STM0.ISCR.B.CMP0IRR = 1U; // Clear Interrupt Req.
    MODULE_STM0.ICR.B.CMP0EN = 1U; // Enable Interrupt
}

void Stm0_InterruptAfter (uint32_t delay_us)
{
    /* Set Compare register to current time + delay_us */
    uint64_t t = STM0_getTimeUs() + (uint64_t) delay_us;
    MODULE_STM0.CMP[0].U = (uint32_t) (t * 100ULL);
}
