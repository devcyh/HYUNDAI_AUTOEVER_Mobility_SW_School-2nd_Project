#include "someip.h"
#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/udp.h"
#include "Configuration.h"
#include "Ifx_Lwip.h"
#include <string.h>

#include "make_packet.h"
#include "my_stdio.h"
#include "ii_map.h"

#include "Buzzer.h"
#include "LED.h"
#include "my_stdio.h"
#include "PR.h"
#include "ToF.h"
#include "Ultrasonic.h"
#include "emer_alert.h"
#include "motor_controller.h"
#include "serialize_data.h"

#if LWIP_UDP

typedef struct
{
    ip_addr_t ip;           // 클라이언트 IP 주소
    uint16_t port;          // 클라이언트 포트
    uint16_t service_id;    // 구독한 서비스 ID
    uint16_t instance_id;   // 서비스 인스턴스 ID
    uint16_t eventgroup_id; // 구독한 이벤트 그룹 ID
    uint32_t ttl;           // TTL (초 단위)
    uint32_t expiry_time;   // 만료 시간 (시스템 틱)
    bool active;            // 활성 상태 플래그
} SOMEIPSD_Client_t;

#define MAX_OPTIONS 16
#define MAX_CLIENTS 10

static struct udp_pcb *g_SOMEIPSD_PCB;
static struct udp_pcb *g_SOMEIP_SERVICE1_PCB;
static struct udp_pcb *g_SOMEIP_SERVICE2_PCB;
static struct udp_pcb *g_SOMEIP_SERVICE3_PCB;

static SOMEIPSD_Client_t g_clients[MAX_CLIENTS];
//static IIMap g_sd_sessions;

static inline void SOMEIP_Update_Length (uint8_t *txBuf, uint16_t txLen)
{
    // SOME/IP Length 필드 업데이트 (Byte 4-7)
    // Length = Payload 크기 + 8 (Message ID부터 Payload까지)
    uint32_t length = txLen - 8;
    txBuf[4] = (length >> 24) & 0xFF;
    txBuf[5] = (length >> 16) & 0xFF;
    txBuf[6] = (length >> 8) & 0xFF;
    txBuf[7] = length & 0xFF;
}

static err_t SOMEIP_Send_Data (struct udp_pcb *upcb, uint8_t *txBuf, uint16_t txLen, const ip_addr_t *addr,
        uint16_t port)
{
    struct pbuf *txBuf2 = pbuf_alloc(PBUF_TRANSPORT, txLen, PBUF_RAM);
    if (txBuf2 == NULL)
    {
        my_printf("Failed to allocate memory for UDP packet buffer.\n");
        return ERR_MEM;
    }

    err_t err = pbuf_take(txBuf2, txBuf, txLen);
    if (err != ERR_OK)
    {
        return err;
    }

    // 응답 전송
    err = udp_sendto(upcb, txBuf2, addr, port);
    if (err == ERR_OK)
    {
        my_printf("[SOME/IP] Data transfer success!!\n");
    }
    else
    {
        my_printf("[SOME/IP] Data transfer failed!! \n");
    }

    pbuf_free(txBuf2);
    return err;
}

void SOMEIP_Service1_Callback (void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port)
{
    static PRData_t pr_latest_data;
    static ToFData_t tof_latest_data;
    static UltrasonicData_t ult_latest_data[ULTRASONIC_COUNT];

    if (p == NULL)
    {
        return;
    }

    // 수신 버퍼 크기 검증 (SOME/IP 최소 헤더: 16 bytes)
    if (p->len < 16 || p->len > 512)
    {
        my_printf("Invalid packet size: %d\n", p->len);
        pbuf_free(p);
        return;
    }

    uint8_t rxBuf[512];
    memcpy(rxBuf, p->payload, p->len);

    uint16_t ServiceID = (rxBuf[0] << 8) + rxBuf[1];
    uint16_t MethodID = (rxBuf[2] << 8) + rxBuf[3];
    uint8_t MessageType = rxBuf[14];

    // Service ID 및 Message Type 확인
    if (ServiceID == SERVICE_ID_SENSOR && MessageType == 0x00)
    {
        uint8_t txBuf[256];
        uint16_t txLen = 0;

        // SOME/IP 헤더 복사 (16 bytes)
        memcpy(txBuf, rxBuf, 16);
        // Message Type을 Response로 변경
        txBuf[14] = 0x80;

        // Method ID에 따라 처리
        if (MethodID == METHOD_ID_PR)
        {
            txLen = 16;
            pr_latest_data = PR_GetData();
            my_printf("PR: val=%lu, t=%llu\n", pr_latest_data.val, pr_latest_data.received_time_us);
            txLen = Serialize_PRData(txBuf, txLen, &pr_latest_data);
        }
        else if (MethodID == METHOD_ID_TOF)
        {
            txLen = 16;
            ToF_GetLatestData(&tof_latest_data);
            my_printf("ToF: dist=%f, t=%llu\n", tof_latest_data.distance_m, tof_latest_data.received_time_us);
            txLen = Serialize_ToFData(txBuf, txLen, &tof_latest_data);
        }
        else if (MethodID == METHOD_ID_ULT)
        {
            txLen = 16;
            txBuf[txLen++] = ULTRASONIC_COUNT;

            for (int i = 0; i < ULTRASONIC_COUNT; i++)
            {
                Ultrasonic_GetLatestData(i, &ult_latest_data[i]);
                my_printf("Ult%d: dist_raw=%d, dist_filt=%d, t=%llu\n", i, ult_latest_data[i].dist_raw_mm,
                        ult_latest_data[i].dist_filt_mm, ult_latest_data[i].received_time_us);
                txLen = Serialize_UltrasonicData(txBuf, txLen, &ult_latest_data[i]);
            }
        }

        if (txLen > 0)
        {
            // SOME/IP Length 필드 업데이트
            SOMEIP_Update_Length(txBuf, txLen);

            // 응답 전송
            SOMEIP_Send_Data(upcb, txBuf, txLen, addr, port);
        }
    }
    else if (ServiceID != METHOD_ID_PR)
    {
        my_printf("Requested Unknown Service ID\n");
    }

    pbuf_free(p);
}

void SOMEIP_Service2_Callback (void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port)
{
    static BuzzerData_t buzzer_latest_data;
    static LedData_t led_latest_data;

    if (p == NULL)
    {
        return;
    }

    // 수신 버퍼 크기 검증 (SOME/IP 최소 헤더: 16 bytes)
    if (p->len < 16 || p->len > 512)
    {
        my_printf("Invalid packet size: %d\n", p->len);
        pbuf_free(p);
        return;
    }

    uint8_t rxBuf[512];
    memcpy(rxBuf, p->payload, p->len);

    uint16_t ServiceID = (rxBuf[0] << 8) + rxBuf[1];
    uint16_t MethodID = (rxBuf[2] << 8) + rxBuf[3];
    uint8_t MessageType = rxBuf[14];

    // Service ID 및 Message Type 확인
    if (ServiceID == SERVICE_ID_CONTROL && MessageType == 0x00)
    {
        uint8_t txBuf[256];
        uint16_t txLen = 0;

        // SOME/IP 헤더 복사 (16 bytes)
        memcpy(txBuf, rxBuf, 16);
        // Message Type을 Response로 변경
        txBuf[14] = 0x80;

        bool isEmerAlertOn = EmerAlert_GetData().interval_ms >= 0 ? true : false;

        // Method ID에 따라 처리
        if (MethodID == METHOD_ID_BUZZER)
        {
            if (!isEmerAlertOn)
            {
                // Buzzer Control - Payload에서 값 추출
                uint8_t buzzer_command = rxBuf[16];  // 0: Off, 1: On
                int32_t frequency = (rxBuf[17] << 24) | (rxBuf[18] << 16) | (rxBuf[19] << 8) | rxBuf[20];

                // 부저 제어 수행
                Buzzer_SetFrequency(frequency);

                if (buzzer_command == 0x00)
                {
                    Buzzer_Off();
                }
                else if (buzzer_command == 0x01)
                {
                    Buzzer_On();
                }
            }

            buzzer_latest_data = Buzzer_GetData();
            my_printf("Buzzer Control: isOn=%d, frequency=%d\n", buzzer_latest_data.isOn, buzzer_latest_data.frequency);

            // Response payload 구성
            txLen = 16;
            txLen = Serialize_BuzzerData(txBuf, txLen, &buzzer_latest_data);
        }
        else if (MethodID == METHOD_ID_LED)
        {
            // LED Control - Payload에서 led_side 및 led_command 값 추출
            uint8_t led_side = rxBuf[16];       // LED_BACK, LED_FRONT_DOWN, LED_FRONT_UP
            uint8_t led_command = rxBuf[17];    // 0: Off, 1: On, 2: Toggle

            LedSide side = (LedSide) led_side;

            if (!(side == LED_BACK && isEmerAlertOn))
            {
                // LED 제어 수행
                if (led_command == 0x00)
                {
                    LED_Off(side);
                }
                else if (led_command == 0x01)
                {
                    LED_On(side);
                }
                else if (led_command == 0x02)
                {
                    LED_Toggle(side);
                }
            }

            led_latest_data = LED_GetData(side);
            my_printf("LED Control: side=%d, isOn=%d\n", led_latest_data.side, led_latest_data.isOn);

            // Response payload 구성
            txLen = 16;
            txLen = Serialize_LedData(txBuf, txLen, &led_latest_data);
        }

        if (txLen > 0)
        {
            // SOME/IP Length 필드 업데이트
            SOMEIP_Update_Length(txBuf, txLen);

            // 응답 전송
            SOMEIP_Send_Data(upcb, txBuf, txLen, addr, port);
        }
    }
    else if (ServiceID != SERVICE_ID_CONTROL)
    {
        my_printf("Requested Unknown Service ID\n");
    }

    pbuf_free(p);
}

void SOMEIP_Service3_Callback (void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port)
{
    static EmerAlertData_t emerAlert_latest_data;
    static MotorControllerData_t motor_controller_latest_data;

    if (p == NULL)
    {
        return;
    }

    // 수신 버퍼 크기 검증 (SOME/IP 최소 헤더: 16 bytes)
    if (p->len < 16 || p->len > 512)
    {
        my_printf("Invalid packet size: %d\n", p->len);
        pbuf_free(p);
        return;
    }

    uint8_t rxBuf[512];
    memcpy(rxBuf, p->payload, p->len);

    uint16_t ServiceID = (rxBuf[0] << 8) + rxBuf[1];
    uint16_t MethodID = (rxBuf[2] << 8) + rxBuf[3];
    uint8_t MessageType = rxBuf[14];

    // Service ID 및 Message Type 확인
    if (ServiceID == SERVICE_ID_SYSTEM && MessageType == 0x00)
    {
        uint8_t txBuf[256];
        uint16_t txLen = 0;

        // SOME/IP 헤더 복사 (16 bytes)
        memcpy(txBuf, rxBuf, 16);
        // Message Type을 Response로 변경
        txBuf[14] = 0x80;

        // Method ID에 따라 처리
        if (MethodID == METHOD_ID_ALERT)
        {
            // Emergency Alert Control - Payload에서 emerAlert_cycle_ms 값 추출
            int64_t emerAlert_cycle_ms = ((int64_t) rxBuf[16] << 56) | ((int64_t) rxBuf[17] << 48)
                    | ((int64_t) rxBuf[18] << 40) | ((int64_t) rxBuf[19] << 32) | ((int64_t) rxBuf[20] << 24)
                    | ((int64_t) rxBuf[21] << 16) | ((int64_t) rxBuf[22] << 8) | (int64_t) rxBuf[23];

            // Emergency Alert 업데이트 수행
            if (EmerAlert_Set_Interval(emerAlert_cycle_ms))
            {
                emerAlert_latest_data = EmerAlert_GetData();
            }
            my_printf("Emergency Alert Control: cycle=%lld ms\n", emerAlert_latest_data.interval_ms);

            // Response payload 구성
            txLen = 16;
            txLen = Serialize_EmerAlertData(txBuf, txLen, &emerAlert_latest_data);
        }
        else if (MethodID == METHOD_ID_MOTOR)
        {
            // Motor Control - Payload에서 motor_x, motor_y 값 추출
            int motor_x = (rxBuf[16] << 24) | (rxBuf[17] << 16) | (rxBuf[18] << 8) | rxBuf[19];
            int motor_y = (rxBuf[20] << 24) | (rxBuf[21] << 16) | (rxBuf[22] << 8) | rxBuf[23];

            // 모터 제어 수행
            if (MotorController_ProcessJoystickInput(motor_x, motor_y))
            {
                motor_controller_latest_data = MotorController_GetData();
            }
            my_printf("Motor Control: x=%d, y=%d, chA=%d, chB=%d\n", motor_controller_latest_data.x,
                    motor_controller_latest_data.y, motor_controller_latest_data.motorChA_speed,
                    motor_controller_latest_data.motorChB_speed);

            // Response payload 구성
            txLen = 16;
            txLen = Serialize_MotorControllerData(txBuf, txLen, &motor_controller_latest_data);
        }

        if (txLen > 0)
        {
            // SOME/IP Length 필드 업데이트
            SOMEIP_Update_Length(txBuf, txLen);

            // 응답 전송
            SOMEIP_Send_Data(upcb, txBuf, txLen, addr, port);
        }
    }
    else if (ServiceID != SERVICE_ID_SYSTEM)
    {
        my_printf("Requested Unknown Service ID\n");
    }

    pbuf_free(p);
}

void SOMEIP_SendNotification (void)
{

}

void SOMEIPSD_SendOfferService (ip_addr_t client_ip)
{
    static const int MSG_SIZE = 112;
    uint8_t MSG_OfferService[MSG_SIZE]; // 16 * 4 + 12 * 3 + 4 * 3 = 64 + 36 + 12 = 112

    uint16_t txLen = 0;
    uint8 *ip = Ifx_Lwip_getIpAddrPtr(); // 이놈 빅엔디안으로 TC375 ip 주소 내뱉는듯
    ip_addr_t ip_addr; // 얘는 리틀엔디안인듯
    IP4_ADDR(&ip_addr, ip[0], ip[1], ip[2], ip[3]);

    uint32_t session_id = 0;
//    if (!iiMap_find(&g_sd_sessions, client_ip.addr, &session_id))
//    {
//        session_id = 1;
//        iiMap_upsert_or_replace(&g_sd_sessions, client_ip.addr, session_id);
//    }

    txLen = SOMEIPSD_AddHeader(MSG_OfferService, MSG_SIZE - 8, session_id);
    txLen = SOMEIPSD_AddSdHeader(MSG_OfferService, txLen);
    txLen = SOMEIPSD_AddEntriesArrayLength(MSG_OfferService, txLen, 48); // 16 * 3
    txLen = SOMEIPSD_AddServiceEntry(MSG_OfferService, txLen, 0x01, 0, 0, 1, 0, SERVICE_ID_SENSOR, INSTANCE_ID, 1, 3,
            1); // Entry 1
    txLen = SOMEIPSD_AddServiceEntry(MSG_OfferService, txLen, 0x01, 1, 0, 1, 0, SERVICE_ID_CONTROL, INSTANCE_ID, 1, 3,
            1); // Entry 2
    txLen = SOMEIPSD_AddServiceEntry(MSG_OfferService, txLen, 0x01, 2, 0, 1, 0, SERVICE_ID_SYSTEM, INSTANCE_ID, 1, 3,
            1); // Entry 3
    txLen = SOMEIPSD_AddOptionsArrayLength(MSG_OfferService, txLen, 36); // 12 * 3
    txLen = SOMEIPSD_Add0xX4Option(MSG_OfferService, txLen, 0x04, ip_addr, 0x11, PN_SERVICE_1); // Option 1
    txLen = SOMEIPSD_Add0xX4Option(MSG_OfferService, txLen, 0x04, ip_addr, 0x11, PN_SERVICE_2); // Option 2
    txLen = SOMEIPSD_Add0xX4Option(MSG_OfferService, txLen, 0x04, ip_addr, 0x11, PN_SERVICE_3); // Option 3

    err_t err = SOMEIP_Send_Data(g_SOMEIPSD_PCB, MSG_OfferService, txLen, &client_ip, PN_SOMEIPSD);
    if (err == ERR_OK)
    {
//            session_id = (session_id % 0xFFFF) + 1;
//            iiMap_update(&g_sd_sessions, client_ip.addr, session_id);

        uint8_t ip_a = client_ip.addr & 0xFF;
        uint8_t ip_b = (client_ip.addr >> 8) & 0xFF;
        uint8_t ip_c = (client_ip.addr >> 16) & 0xFF;
        uint8_t ip_d = (client_ip.addr >> 24) & 0xFF;

        my_printf("Send Offer Service Success !!\n");
        my_printf("Offering 0x0100/0x0200/0x0300 from %d.%d.%d.%d to %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3], ip_a,
                ip_b, ip_c, ip_d);
    }
    else
    {
        my_printf("Send Offer Service Failed !! (Error: %d)\n", err);
    }
}

void SOMEIPSD_SendSubEvtGrpAck (ip_addr_t client_ip, uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
        uint32_t ttl)
{
    uint8_t MSG_SubEvtGrpAck[100];

    uint16_t txLen = 0;

    uint32_t session_id = 0;
//    if (!iiMap_find(&g_sd_sessions, client_ip.addr, &session_id))
//    {
//        session_id = 1;
//        iiMap_upsert_or_replace(&g_sd_sessions, client_ip.addr, session_id);
//    }

    txLen = SOMEIPSD_AddHeader(MSG_SubEvtGrpAck, 40, session_id);
    txLen = SOMEIPSD_AddSdHeader(MSG_SubEvtGrpAck, txLen);
    txLen = SOMEIPSD_AddEntriesArrayLength(MSG_SubEvtGrpAck, txLen, 16);
    txLen = SOMEIPSD_AddEventgroupEntry(MSG_SubEvtGrpAck, txLen, 0x07, 0, 0, 0, 0, service_id, instance_id, 1, ttl, 0,
            eventgroup_id);

    err_t err = SOMEIP_Send_Data(g_SOMEIPSD_PCB, MSG_SubEvtGrpAck, txLen, &client_ip, PN_SOMEIPSD);
    if (err == ERR_OK)
    {
//            session_id = (session_id % 0xFFFF) + 1;
//            iiMap_update(&g_sd_sessions, client_ip.addr, session_id);

        uint8_t ip_a = client_ip.addr & 0xFF;
        uint8_t ip_b = (client_ip.addr >> 8) & 0xFF;
        uint8_t ip_c = (client_ip.addr >> 16) & 0xFF;
        uint8_t ip_d = (client_ip.addr >> 24) & 0xFF;

        my_printf("Send SubscribeEventgroupAck to %d.%d.%d.%d:%d\n", ip_a, ip_b, ip_c, ip_d, PN_SOMEIPSD);
        my_printf("Service: 0x%04X, Instance: 0x%04X, EventGroup: 0x%04X, TTL: %d\n", service_id, instance_id,
                eventgroup_id, ttl);
    }
    else
    {
        my_printf("udp_sendto fail!! (Error: %d)\n", err);
    }
}

bool SOMEIPSD_AddClient (ip_addr_t client_ip, uint16_t client_port, uint16_t service_id, uint16_t instance_id,
        uint16_t eventgroup_id, uint32_t ttl)
{
    uint32_t current_time = sys_now(); // 현재 시스템 틱 (밀리초)
    uint32_t expiry_time = current_time + (ttl * 1000); // TTL을 밀리초로 변환

    // 기존 클라이언트 확인 (중복 방지 및 TTL 갱신)
    for (uint8_t i = 0; i < MAX_CLIENTS; i++)
    {
        if (g_clients[i].active && ip_addr_cmp(&g_clients[i].ip, &client_ip) && g_clients[i].port == client_port
                && g_clients[i].service_id == service_id && g_clients[i].instance_id == instance_id
                && g_clients[i].eventgroup_id == eventgroup_id)
        {
            my_printf("Client already subscribed - updating TTL\n");
            g_clients[i].ttl = ttl;
            g_clients[i].expiry_time = expiry_time;
            return true;
        }
    }

    // 빈 슬롯 찾기
    for (uint8_t i = 0; i < MAX_CLIENTS; i++)
    {
        if (!g_clients[i].active)
        {
            g_clients[i].ip = client_ip;
            g_clients[i].port = client_port;
            g_clients[i].service_id = service_id;
            g_clients[i].instance_id = instance_id;
            g_clients[i].eventgroup_id = eventgroup_id;
            g_clients[i].ttl = ttl;
            g_clients[i].expiry_time = expiry_time;
            g_clients[i].active = true;

            my_printf("Client added at slot %d\n", i);
            my_printf("Client Info - IP: %d.%d.%d.%d:%d, Service: 0x%04X, EventGroup: 0x%04X, TTL: %d sec\n",
                    client_ip.addr & 0xFF, (client_ip.addr >> 8) & 0xFF, (client_ip.addr >> 16) & 0xFF,
                    (client_ip.addr >> 24) & 0xFF, client_port, service_id, eventgroup_id, ttl);
            return true;
        }
    }

    return false; // 슬롯이 가득 찬 경우
}

bool SOMEIPSD_RemoveClient (ip_addr_t client_ip, uint16_t client_port, uint16_t service_id, uint16_t instance_id,
        uint16_t eventgroup_id)
{
    for (uint8_t i = 0; i < MAX_CLIENTS; i++)
    {
        if (g_clients[i].active && ip_addr_cmp(&g_clients[i].ip, &client_ip) && g_clients[i].port == client_port
                && g_clients[i].service_id == service_id && g_clients[i].instance_id == instance_id
                && g_clients[i].eventgroup_id == eventgroup_id)
        {
            my_printf("Removing client at slot %d\n", i);
            g_clients[i].active = false;
//            memset(&g_clients[i], 0, sizeof(SOMEIPSD_Client_t));
            return true;
        }
    }
    return false;
}

void SOMEIPSD_HandleSubscribe (ip_addr_t client_ip, uint16_t client_port, uint8_t *entry, uint8_t **option_pointers,
        uint8_t option_count)
{
    // Entry에서 Service ID, Instance ID, EventGroup ID 추출
    uint16_t service_id = (entry[4] << 8) | entry[5];
    uint16_t instance_id = (entry[6] << 8) | entry[7];
    uint8_t major_version = entry[8];
    uint32_t ttl = (entry[9] << 16) | (entry[10] << 8) | entry[11];
    uint8_t counter = entry[13] & 0x0F;
    uint16_t eventgroup_id = (entry[14] << 8) | entry[15];

    // Entry의 Option Index 정보 추출
    uint8_t index1 = entry[1];
    uint8_t index2 = entry[2];
    uint8_t num_opts1 = (entry[3] >> 4) & 0x0F;  // Number of Opts 1
    uint8_t num_opts2 = entry[3] & 0x0F;          // Number of Opts 2

    my_printf("\n=== Subscribe/StopSubscribe Request ===\n");
    my_printf("Service: 0x%04X, Instance: 0x%04X, EventGroup: 0x%04X\n", service_id, instance_id, eventgroup_id);
    my_printf("Major Version: %d, TTL: %d, Counter: %d\n", major_version, ttl, counter);
    my_printf("Option Index1: %d, NumOpts1: %d, Index2: %d, NumOpts2: %d\n", index1, num_opts1, index2, num_opts2);

    if (service_id != SERVICE_ID_SENSOR || instance_id != INSTANCE_ID
            || !(eventgroup_id == EVENTGROUP_ID_PR || eventgroup_id == EVENTGROUP_ID_TOF
                    || eventgroup_id == EVENTGROUP_ID_ULT))
    {
        // 잘못된 요청?
        // 여기서 SubscribeEventgroupNack를 보낼 수도 있음
        return;
    }

    uint8_t idx_opt[2] = {index1, index2};
    uint8_t num_opt[2] = {num_opts1, num_opts2};

    for (int i = 0; i < 2; i++)
    {
        for (int cur_opt = idx_opt[i]; cur_opt < idx_opt[i] + num_opt[i]; cur_opt++)
        {
            // Options Array에서 해당 index의 옵션 찾기
            ip_addr_t notification_ip;
            uint8_t protocol;
            uint16_t notification_port;
            bool endpoint_found = false;

            if (cur_opt < option_count)
            {
                uint8_t *option = option_pointers[cur_opt]; // index 위치의 옵션을 직접 접근

                uint16_t option_length = (option[0] << 8) | option[1];
                uint8_t option_type = option[2];
                bool discardable_flag = option[3] & 0x80;

                my_printf("Using Option[%d] - Length: %d, Type: 0x%02X, Disc Flag: %d\n", cur_opt, option_length,
                        option_type, discardable_flag);

                switch (option_type)
                {
                    case 0x04 : // IPv4 Endpoint Option
                        if (option_length >= 9)
                        {
                            // IPv4 주소 추출 (offset 4-7)
                            uint8_t ip_a = option[4];
                            uint8_t ip_b = option[5];
                            uint8_t ip_c = option[6];
                            uint8_t ip_d = option[7];

                            IP4_ADDR(&notification_ip, ip_a, ip_b, ip_c, ip_d);

                            // Protocol 확인 (offset 9)
                            protocol = option[9];  // 0x11 = UDP, 0x06 = TCP

                            // Port 추출 (offset 10-11)
                            notification_port = (option[10] << 8) | option[11];

                            if (protocol == 0x11 || protocol == 0x06)
                            {
                                endpoint_found = true;
                            }

                            my_printf("Notification Endpoint: %d.%d.%d.%d:%d (%s)\n", ip_a, ip_b, ip_c, ip_d,
                                    notification_port,
                                    protocol == 0x11 ? "UDP" : (protocol == 0x06 ? "TCP" : "Unknown"));
                        }
                        break;

                    case 0x06 : // IPv6 Endpoint Option
                        my_printf("IPv6 Endpoint not supported\n");
                        break;

                    case 0x14 : // IPv4 Multicast Option
                        my_printf("(Not implemented) IPv4 Multicast\n");
//                        if (option_length >= 9)
//                        {
//                            uint8_t ip_a = option[4];
//                            uint8_t ip_b = option[5];
//                            uint8_t ip_c = option[6];
//                            uint8_t ip_d = option[7];
//                            IP4_ADDR(&notification_ip, ip_a, ip_b, ip_c, ip_d);
//                            protocol = option[9];
//                            notification_port = (option[10] << 8) | option[11];
//
//                            if (protocol == 0x11) // UDP만 지원
//                            {
//                                endpoint_found = true;
//                            }
//
//                            my_printf("Multicast Endpoint: %d.%d.%d.%d:%d (%s)\n", ip_a, ip_b, ip_c, ip_d,
//                                    notification_port, protocol == 0x11 ? "UDP" : "Unknown");
//                        }
                        break;

                    case 0x16 : // IPv6 Multicast Option
                        my_printf("IPv6 Multicast not supported\n");
                        break;

                    default :
                        // 무시 option types: 0x01, 0x02, 0x24, 0x26
                        my_printf("Option type 0x%x not supported\n", option_type);
                        break;
                }
            }

            if (protocol == 0x06)
            {
                endpoint_found = false; // TCP 구현 안함
            }

            if (!endpoint_found)
            {
                my_printf("No valid endpoint option found\n");
                // 여기서 SubscribeEventgroupNack를 보낼 수도 있음
                return;
            }

            if (ttl > 0)
            {
                // 클라이언트 추가 (Notification을 보낼 주소 사용)
                if (SOMEIPSD_AddClient(notification_ip, notification_port, service_id, instance_id, eventgroup_id, ttl))
                {
                    // Subscribe Ack를 UDP 송신지로 전송
                    SOMEIPSD_SendSubEvtGrpAck(client_ip, service_id, instance_id, eventgroup_id, ttl);
                }
                else
                {
                    my_printf("Failed to add client - max clients reached\n");
                    // 여기서 SubscribeEventgroupNack를 보낼 수도 있음
                }
            }
            else
            {
                SOMEIPSD_RemoveClient(notification_ip, notification_port, service_id, instance_id, eventgroup_id);
            }
        }
    }
}

void SOMEIPSD_Recv_Callback (void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port)
{
    if (!p)
        return;

    uint8_t *buf = (uint8_t*) p->payload;
    uint16_t ServiceID = (buf[0] << 8) | buf[1];
    uint16_t MethodID = (buf[2] << 8) | buf[3];

    ip_addr_t client_ip = *addr;
    uint16_t client_port = port;

    if (ServiceID == 0xFFFFU && MethodID == 0x8100U)
    {
        // Entries Array 시작 위치 계산
        uint32_t entries_len = (buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23]; // Entries Array 길이 (바이트)
        uint8_t *entries = buf + 24; // Entries Array 시작

        // Options Array 시작 위치 계산
        uint8_t *opt_len_ptr = entries + entries_len;
        uint32_t options_len = (opt_len_ptr[0] << 24) | (opt_len_ptr[1] << 16) | (opt_len_ptr[2] << 8) | opt_len_ptr[3]; // Options Array 길이 (바이트)
        uint8_t *options = entries + entries_len + 4;

        // Options Array의 각 옵션 시작 위치를 미리 파싱
        uint8_t *option_pointers[MAX_OPTIONS];
        uint8_t option_count = 0;
        uint8_t *current_option = options;
        uint32_t parsed_len = 0;

        while (parsed_len < options_len && option_count < MAX_OPTIONS)
        {
            // 현재 옵션의 시작 위치 저장
            option_pointers[option_count] = current_option;

            // 옵션 길이 읽기 (첫 2바이트)
            uint16_t option_length = (current_option[0] << 8) | current_option[1];

            my_printf("Option[%d] - Length: %d, Offset: %d\n", option_count, option_length, parsed_len);

            // 다음 옵션으로 이동: Length(2) + Type(1) + Data(option_length)
            uint32_t total_option_size = 3 + option_length;
            current_option += total_option_size;
            parsed_len += total_option_size;
            option_count++;
        }

        my_printf("Total Options Parsed: %d\n", option_count);

        // Entries Array 처리
        static const uint32_t ENTRY_LEN = 16;
        uint8_t *entry = entries;
        uint32_t processed = 0;

        while (processed < entries_len) // entry 하나씩 처리 & 결과 send. 묶어서 보내면 트래픽면에서 좋지만 너무 복잡..
        {
            uint8_t entry_type = entry[0];

            switch (entry_type)
            {
                case 0x00 : // FindService
                    SOMEIPSD_SendOfferService(client_ip);
                    break;
                case 0x01 : // OfferService, StopOfferService
                    my_printf("Received OfferService\n");
                    // 이 ECU는 서비스 제공만 하고 제공받지는 않기 때문에 처리 로직 필요 없음
                    break;

                case 0x06 : // SubscribeEventgroup, StopSubscribeEventgroup
                    SOMEIPSD_HandleSubscribe(client_ip, client_port, entry, option_pointers, option_count);
                    break;

                case 0x07 : // SubscribeEventgroupAck, SubscribeEventgroupNack
                    my_printf("Received SubscribeEventgroupAck\n");
                    // 이 ECU는 서비스 제공만 하고 제공받지는 않기 때문에 처리 로직 필요 없음
                    break;

                default :
                    my_printf("Unknown SD Entry Type: 0x%02X\n", entry_type);
                    break;
            }

            processed += ENTRY_LEN;
            entry += ENTRY_LEN; // 다음 entry 위치로 이동
        }
    }

    pbuf_free(p);
}

static bool SOMEIP_UPCB_Init (struct udp_pcb **pcb_ptr, uint16_t port, udp_recv_fn recv)
{
    struct udp_pcb *SOMEIP_PCB = udp_new();
    if (SOMEIP_PCB)
    {
        /* bind pcb to the port */
        /* Using IP_ADDR_ANY allow the pcb to be used by any local interface */
        err_t err = udp_bind(SOMEIP_PCB, IP_ADDR_ANY, port);
        if (err == ERR_OK)
        {
            /* Set a receive callback for the pcb */
            udp_recv(SOMEIP_PCB, recv, NULL);
            my_printf("SOME/IP PCB Initialized! (Port: %d)\n", port);
        }
        else
        {
            udp_remove(SOMEIP_PCB);
            my_printf("SOME/IP PCB init failed!\n");
            return false;
        }
    }
    else
    {
        my_printf("Failed to create UDP PCB\n");
        return false;
    }

    *pcb_ptr = SOMEIP_PCB;
    return true;
}

bool SOMEIP_Init (void)
{
    if (!SOMEIP_UPCB_Init(&g_SOMEIPSD_PCB, PN_SOMEIPSD, (void*) SOMEIPSD_Recv_Callback))
        return false;
    if (!SOMEIP_UPCB_Init(&g_SOMEIP_SERVICE1_PCB, PN_SERVICE_1, (void*) SOMEIP_Service1_Callback))
        return false;
    if (!SOMEIP_UPCB_Init(&g_SOMEIP_SERVICE2_PCB, PN_SERVICE_2, (void*) SOMEIP_Service2_Callback))
        return false;
    if (!SOMEIP_UPCB_Init(&g_SOMEIP_SERVICE3_PCB, PN_SERVICE_3, (void*) SOMEIP_Service3_Callback))
        return false;

    memset(g_clients, 0, sizeof(g_clients));
//    iiMap_init(&g_sd_sessions);

    return true;
}

#endif /* LWIP_UDP */
