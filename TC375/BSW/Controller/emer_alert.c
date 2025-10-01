#include "emer_alert.h"

#include "stm.h"

#include "buzzer.h"
#include "led.h"

static bool isAlertOn = false;

static void EmerAlert_On (void)
{
    if (!isAlertOn)
    {
        LED_On();
        Buzzer_On();
        isAlertOn = true;
    }
}

static void EmerAlert_Off (void)
{
    if (isAlertOn)
    {
        LED_Off();
        Buzzer_Off();
        isAlertOn = false;
    }
}

static void EmerAlert_Toggle (void)
{
    isAlertOn = !isAlertOn;

    if (isAlertOn)
    {
        LED_On();
        Buzzer_On();
    }
    else
    {
        LED_Off();
        Buzzer_Off();
    }
}

void EmerAlert_Update_Periodic (int64_t toggle_interval_ms)
{
    static uint64_t last_toggle_time_ms = 0;

    if (toggle_interval_ms > 0) // toggle_interval_ms > 0 -> toggle
    {
        uint64_t cur_time_ms = getTimeMs();

        if (cur_time_ms - last_toggle_time_ms < (uint64_t) toggle_interval_ms)
            return;

        EmerAlert_Toggle();
        last_toggle_time_ms = cur_time_ms;
    }
    else if (toggle_interval_ms < 0) // toggle_interval_ms < 0 -> off
    {
        EmerAlert_Off();
        last_toggle_time_ms = 0;
    }
    else // toggle_interval_ms == 0 -> on
    {
        EmerAlert_On();
        last_toggle_time_ms = 0;
    }
}
