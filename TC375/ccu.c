#include "ccu.h"

#include "my_stdio.h"
#include "tof.h"
#include "ultrasonic.h"

#include "emer_alert.h"
#include "motor_controller.h"

#include "output_status.h"

static const uint64_t CYCLE_INTERVAL_US = 50000; // 50000us = 50ms

void run_ccu (void)
{
    static ToFData_t tof_latest_data;
    static UltrasonicData_t ult_latest_data[ULTRASONIC_COUNT];
    static MotorControllerData_t motor_controller_latest_data;

    static int pre_motor_x = MOTOR_STOP;
    static int pre_motor_y = MOTOR_STOP;
    static int64_t pre_emerAlert_cycle_ms = EMER_ALERT_OFF;

    while (true)
    {
        /* Process data queues */
        ToF_ProcessQueue();
        Ultrasonic_ProcessQueue();

        /* Get ToF data */
        ToF_GetLatestData(&tof_latest_data);

        /* Get ultrasonic data */
        for (int i = 0; i < ULTRASONIC_COUNT; i++)
        {
            Ultrasonic_GetLatestData(i, &ult_latest_data[i]);
        }

        /* Keep previous output value */
        int motor_x = pre_motor_x;
        int motor_y = pre_motor_y;
        int64_t emerAlert_cycle_ms = pre_emerAlert_cycle_ms;

        /* Check ethernet data & process */
        Ifx_Lwip_pollTimerFlags(); /* Poll LwIP timers and trigger protocols execution if required */
        Ifx_Lwip_pollReceiveFlags(); /* Receive data package through ETH */

        /* Set motor control */
        if (MotorController_ProcessJoystickInput(motor_x, motor_y)) // Controll motor
        {
            MotorController_GetLatestData(&motor_controller_latest_data);
            pre_motor_x = motor_x;
            pre_motor_y = motor_y;
        }

        /* Update emergency alert state */
        EmerAlert_Update_Periodic(emerAlert_cycle_ms);
        pre_emerAlert_cycle_ms = emerAlert_cycle_ms;

        /* Print sensor data */
//        my_printf("ToF: %lf %d %llu\n", tof_latest_data.distance_m, tof_latest_data.distance_status,
//                tof_latest_data.received_time_us);
//        for (int i = 0; i < ULTRASONIC_COUNT; i++)
//        {
//            my_printf("Ult%d: %d %d %llu\n", i, ult_latest_data[i].dist_raw_mm, ult_latest_data[i].dist_filt_mm,
//                    ult_latest_data[i].received_time_us);
//        }
    }
}
