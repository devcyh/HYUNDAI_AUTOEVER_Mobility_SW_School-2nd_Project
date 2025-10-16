#include "emer_alert.h"

#include "stm.h"

#include "buzzer.h"
#include "led.h"

static bool isAlertOn = false;
static int64_t g_toggle_interval_ms = -1;

static EmerAlertData_t latest_data;
static bool data_ready = false;

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

void EmerAlert_Update_Periodic (void)
{
    static uint64_t last_toggle_time_ms = 0;

    if (g_toggle_interval_ms > 0) // toggle_interval_ms > 0 -> toggle
    {
        uint64_t cur_time_ms = STM0_getTimeMs();

        if (cur_time_ms - last_toggle_time_ms >= (uint64_t) g_toggle_interval_ms)
        {
            EmerAlert_Toggle();
            last_toggle_time_ms = cur_time_ms;
        }
    }
    else if (g_toggle_interval_ms < 0) // toggle_interval_ms < 0 -> off
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

bool EmerAlert_Set_Interval (int64_t toggle_interval_ms)
{
    if (toggle_interval_ms == g_toggle_interval_ms)
        return false;

    g_toggle_interval_ms = toggle_interval_ms;

    latest_data.output_time_us = STM0_getTimeUs();
    latest_data.interval_ms = toggle_interval_ms;
    data_ready = true;

    return true;
}

bool EmerAlert_GetLatestData (EmerAlertData_t *out)
{
    if (!data_ready)
        return false;

    *out = latest_data;
    data_ready = false;
    return true;
}
