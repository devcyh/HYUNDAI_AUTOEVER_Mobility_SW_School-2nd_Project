#ifndef SOMEIP_SOMEIP_H_
#define SOMEIP_SOMEIP_H_

/* Service IDs */
#define SERVICE_ID_SENSOR   0x0100
#define SERVICE_ID_CONTROL  0x0200
#define SERVICE_ID_SYSTEM   0x0300

/* Service1 (Sensor) Method IDs */
#define METHOD_ID_PR        0x0101
#define METHOD_ID_TOF       0x0102
#define METHOD_ID_ULT       0x0103

/* Service2 (Control) Method IDs */
#define METHOD_ID_BUZZER    0x0201
#define METHOD_ID_LED       0x0202

/* Service3 (System) Method IDs */
#define METHOD_ID_ALERT     0x0301
#define METHOD_ID_MOTOR     0x0302

/* Service1 (Sensor) EventGroup IDs */
#define EVENTGROUP_ID_PR    0x0010
#define EVENTGROUP_ID_TOF   0x0020
#define EVENTGROUP_ID_ULT   0x0030

/* Service1 (Sensor) Event IDs */
// PR Sensor
#define EVENT_ID_PR_DATA    0x8011

// ToF Sensor
#define EVENT_ID_TOF_DATA   0x8021

// ULT Sensors
#define EVENT_ID_ULT_1      0x8031
#define EVENT_ID_ULT_2      0x8032
#define EVENT_ID_ULT_3      0x8033

/* Other */
#define INSTANCE_ID         0x0001

#include <stdbool.h>
#include "ip_addr.h"

//bool SOMEIP_Init (void);
//void SOMEIPSD_SendOfferService (ip_addr_t client_ip);
//void SOMEIP_SendNotification (void);

#endif /* SOMEIP_SOMEIP_H_ */
