#include "someipsd.h"
#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/udp.h"
#include "Configuration.h"
#include "Ifx_Lwip.h"
#include <string.h>
#include "etc.h"
#include "my_stdio.h"

#define MAX_OPTIONS 16
#define MAX_CLIENTS 10

struct udp_pcb* g_SOMEIPSD_PCB;

typedef struct
{
    ip_addr_t ip;           // 클라이언트 IP 주소
    uint16_t port;          // 클라이언트 포트
    uint16_t service_id;    // 구독한 서비스 ID
    uint16_t instance_id;   // 서비스 인스턴스 ID
    uint16_t eventgroup_id; // 구독한 이벤트 그룹 ID
    bool active;            // 활성 상태 플래그
} SOMEIPSD_Client_t;

static SOMEIPSD_Client_t g_clients[MAX_CLIENTS];
static uint8_t g_client_count = 0;

bool SOMEIPSD_Init(void)
{
    /* SOME/IP-SD Init */
    g_SOMEIPSD_PCB = udp_new();
    if (g_SOMEIPSD_PCB)
    {
        /* bind pcb to the 30490 port */
        /* Using IP_ADDR_ANY allow the pcb to be used by any local interface */
        err_t err = udp_bind(g_SOMEIPSD_PCB, IP_ADDR_ANY, PN_SOMEIPSD);
        delay_ms(100);
        if (err == ERR_OK)
        {
            /* Set a receive callback for the pcb */
            udp_recv(g_SOMEIPSD_PCB, (void*)SOMEIPSD_Recv_Callback, NULL);
            my_printf("SOME/IP-SD PCB Initialized! (Port: %d)\n", PN_SOMEIPSD);
        }
        else
        {
            udp_remove(g_SOMEIPSD_PCB);
            my_printf("SOME/IP-SD PCB init failed\n");
            return false;
        }
    }
    else
    {
        my_printf("Failed to create UDP PCB\n");
        return false;
    }

    // 클라이언트 배열 초기화
    memset(g_clients, 0, sizeof(g_clients));

    return true;
}

void SOMEIPSD_Recv_Callback(void* arg, struct udp_pcb* upcb, struct pbuf* p, const ip_addr_t* addr, uint16_t port)
{
    static const uint32_t ENTRY_LEN = 16;

    if (!p)
        return;

    uint16_t ServiceID = *(uint16_t*)p->payload;
    uint16_t MethodID = (*(((uint8_t*)p->payload) + 2) << 8) + *(((uint8_t*)p->payload) + 3);

    ip_addr_t client_ip = *addr;
    uint16_t client_port = port;

    if (ServiceID == 0xFFFFU && MethodID == 0x8100U)
    {
        // Entries Array 시작 위치 계산
        uint32_t entries_len = *((uint32_t*)(p->payload + 20)); // Entries Array 길이 (바이트)
        uint8_t* entries = (uint8_t*)p->payload + 24; // Entries Array 시작
        uint32_t processed = 0;

        // Options Array 시작 위치 계산
        uint32_t options_len = *((uint32_t*)(entries + entries_len)); // Options Array 길이 (바이트)
        uint8_t* options = entries + entries_len + 4;

        // Options Array의 각 옵션 시작 위치를 미리 파싱
        uint8_t* option_pointers[MAX_OPTIONS];
        uint8_t option_count = 0;
        uint8_t* current_option = options;
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
        while (processed < entries_len) // entry 하나씩 처리
        {
            uint8_t entry_type = entries[0];

            switch (entry_type)
            {
            case 0x00: // FindService
                SOMEIPSD_SendOfferService(client_ip.addr & 0xFF, (client_ip.addr >> 8) & 0xFF,
                    (client_ip.addr >> 16) & 0xFF, (client_ip.addr >> 24) & 0xFF);
                break;
            case 0x01: // OfferService
                my_printf("Received OfferService\n");
                break;

            case 0x06: // Subscribe EventGroup
                SOMEIPSD_HandleSubscribe(client_ip, client_port, entries, option_pointers, option_count);
                break;

            case 0x07: // SubscribeEventgroupAck
                my_printf("Received SubscribeEventgroupAck\n");
                break;

            default:
                my_printf("Unknown SD Entry Type: 0x%02X\n", entry_type);
                break;
            }

            processed += ENTRY_LEN; // 다음 entry 위치로 이동
            entries += ENTRY_LEN;
        }
    }

    pbuf_free(p);
}

void SOMEIPSD_HandleSubscribe(ip_addr_t client_ip, uint16_t client_port, uint8_t* entry,
    uint8_t** option_pointers, uint8_t option_count)
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

    my_printf("\n=== Subscribe Request ===\n");
    my_printf("Service: 0x%04X, Instance: 0x%04X, EventGroup: 0x%04X\n",
        service_id, instance_id, eventgroup_id);
    my_printf("TTL: %d, Major Version: %d, Counter: %d\n", ttl, major_version, counter);
    my_printf("Option Index1: %d, NumOpts1: %d, Index2: %d, NumOpts2: %d\n",
        index1, num_opts1, index2, num_opts2);

    // Options Array에서 해당 index의 옵션 찾기
    ip_addr_t notification_ip;
    uint16_t notification_port = 0;
    uint8_t protocol = 0;
    bool endpoint_found = false;

    if (num_opts1 > 0 && index1 < option_count)
    {
        // index1 위치의 옵션을 직접 접근
        uint8_t* option = option_pointers[index1];

        uint16_t option_length = (option[0] << 8) | option[1];
        uint8_t option_type = option[2];

        my_printf("Using Option[%d] - Type: 0x%02X, Length: %d\n", index1, option_type, option_length);

        if (option_type == 0x04)  // IPv4 Endpoint Option
        {
            if (option_length >= 9)
            {
                // IPv4 주소 추출 (offset 4-7)
                uint8_t ip_a = option[4];
                uint8_t ip_b = option[5];
                uint8_t ip_c = option[6];
                uint8_t ip_d = option[7];

                // Protocol 확인 (offset 9)
                protocol = option[9];  // 0x11 = UDP, 0x06 = TCP

                // Port 추출 (offset 10-11)
                notification_port = (option[10] << 8) | option[11];

                IP4_ADDR(&notification_ip, ip_a, ip_b, ip_c, ip_d);

                if (protocol == 0x11 || protocol == 0x06) {
                    endpoint_found = true;
                }

                my_printf("Notification Endpoint: %d.%d.%d.%d:%d (%s)\n",
                    ip_a, ip_b, ip_c, ip_d, notification_port,
                    protocol == 0x11 ? "UDP" : (protocol == 0x06 ? "TCP" : "Unknown"));
            }
        }
        else if (option_type == 0x06)  // IPv6 Endpoint Option
        {
            my_printf("IPv6 Endpoint not supported\n");
        }
        else if (option_type == 0x14)  // IPv4 Multicast Option
        {
            if (option_length >= 9)
            {
                uint8_t ip_a = option[4];
                uint8_t ip_b = option[5];
                uint8_t ip_c = option[6];
                uint8_t ip_d = option[7];

                protocol = option[9];
                notification_port = (option[10] << 8) | option[11];

                IP4_ADDR(&notification_ip, ip_a, ip_b, ip_c, ip_d);

                if (protocol == 0x11) {
                    endpoint_found = true;
                }

                my_printf("Multicast Endpoint: %d.%d.%d.%d:%d (%s)\n",
                    ip_a, ip_b, ip_c, ip_d, notification_port,
                    protocol == 0x11 ? "UDP" : "Unknown");
            }
        }
        else if (option_type == 0x16)  // IPv6 Multicast Option
        {
            my_printf("IPv6 Multicast not supported\n");
        }
        else if (option_type == 0x24) {
            my_printf("IPv4 SD Endpoint Option\n");
        }
        else if (option_type == 0x26) {
            my_printf("IPv6 SD Endpoint Option not supported\n");
        }
    }

    if (!endpoint_found)
    {
        my_printf("No valid endpoint option found\n");
        // 여기서 SubscribeEventgroupNack를 보낼 수도 있음
        return;
    }

    // 클라이언트 추가 (Notification을 보낼 주소 사용)
    if (SOMEIPSD_AddClient(notification_ip, notification_port, service_id, instance_id, eventgroup_id))
    {
        // Subscribe Ack를 UDP 송신지로 전송
        SOMEIPSD_SendSubEvtGrpAck(
            client_ip.addr & 0xFF,
            (client_ip.addr >> 8) & 0xFF,
            (client_ip.addr >> 16) & 0xFF,
            (client_ip.addr >> 24) & 0xFF,
            service_id,
            instance_id,
            eventgroup_id,
            ttl
        );
    }
    else
    {
        my_printf("Failed to add client - max clients reached\n");
    }
}

bool SOMEIPSD_AddClient(ip_addr_t client_ip, uint16_t client_port,
    uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id)
{
    // 기존 클라이언트 확인 (중복 방지)
    for (uint8_t i = 0; i < MAX_CLIENTS; i++)
    {
        if (g_clients[i].active &&
            ip_addr_cmp(&g_clients[i].ip, &client_ip) &&
            g_clients[i].port == client_port &&
            g_clients[i].service_id == service_id &&
            g_clients[i].eventgroup_id == eventgroup_id)
        {
            my_printf("Client already subscribed - updating entry\n");
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
            g_clients[i].active = true;
            g_client_count++;

            my_printf("Client added at slot %d (Total: %d/%d)\n", i, g_client_count, MAX_CLIENTS);
            my_printf("Client Info - IP: %d.%d.%d.%d:%d, Service: 0x%04X, EventGroup: 0x%04X\n",
                client_ip.addr & 0xFF,
                (client_ip.addr >> 8) & 0xFF,
                (client_ip.addr >> 16) & 0xFF,
                (client_ip.addr >> 24) & 0xFF,
                client_port, service_id, eventgroup_id);
            return true;
        }
    }

    return false; // 슬롯이 가득 찬 경우
}

void SOMEIPSD_SendOfferService(unsigned char ip_a, unsigned char ip_b, unsigned char ip_c, unsigned char ip_d)
{
    err_t err;
    uint8_t MSG_OfferService[] = {
        /* SOME/IP Header */
        0xFF, 0xFF, 0x81, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x00,
        /* SD HEADER */
        0x00, 0x00, 0x00, 0x00,
        /* ENTRIES ARRAY LENGTH */
        0x00, 0x00, 0x00, 0x30,
        /* Entry 1 */
        0x01, 0x00, 0x00, 0x10, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x01,
        /* Entry 2 */
        0x01, 0x01, 0x00, 0x10, 0x02, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x01,
        /* Entry 3 */
        0x01, 0x02, 0x00, 0x10, 0x03, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x01,
        /* OPTIONS ARRAY LENGTH */
        0x00, 0x00, 0x00, 0x24,
        /* Option 1 */
        0x00, 0x09, 0x04, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* Option 2 */
        0x00, 0x09, 0x04, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* Option 3 */
        0x00, 0x09, 0x04, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    uint8_t* ip = Ifx_Lwip_getIpAddrPtr();

    // Options Array 1
    MSG_OfferService[80] = ip[0];
    MSG_OfferService[81] = ip[1];
    MSG_OfferService[82] = ip[2];
    MSG_OfferService[83] = ip[3];

    MSG_OfferService[85] = 0x11; // UDP

    MSG_OfferService[86] = (uint8)(PN_SERVICE_1 >> 8) & 0xff;
    MSG_OfferService[87] = (uint8)PN_SERVICE_1 & 0xff;

    // Options Array 2
    MSG_OfferService[92] = ip[0];
    MSG_OfferService[93] = ip[1];
    MSG_OfferService[94] = ip[2];
    MSG_OfferService[95] = ip[3];

    MSG_OfferService[97] = 0x11;

    MSG_OfferService[98] = (uint8)(PN_SERVICE_2 >> 8) & 0xff;
    MSG_OfferService[99] = (uint8)PN_SERVICE_2 & 0xff;

    // Options Array 3
    MSG_OfferService[104] = ip[0];
    MSG_OfferService[105] = ip[1];
    MSG_OfferService[106] = ip[2];
    MSG_OfferService[107] = ip[3];

    MSG_OfferService[109] = 0x11;

    MSG_OfferService[110] = (uint8)(PN_SERVICE_3 >> 8) & 0xff;
    MSG_OfferService[111] = (uint8)PN_SERVICE_3 & 0xff;

    struct pbuf* txbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(MSG_OfferService), PBUF_RAM);
    if (txbuf != NULL)
    {
        udp_connect(g_SOMEIPSD_PCB, IP_ADDR_BROADCAST, PN_SOMEIPSD);
        pbuf_take(txbuf, MSG_OfferService, sizeof(MSG_OfferService));

        ip_addr_t destination_ip;
        IP4_ADDR(&destination_ip, ip_a, ip_b, ip_c, ip_d);
        u16_t destination_port = PN_SOMEIPSD;

        err = udp_sendto(g_SOMEIPSD_PCB, txbuf, &destination_ip, destination_port);
        if (err == ERR_OK)
        {
            my_printf("Send Offer Service Success !!\n");
            my_printf("Offering 0x0100/0x0200/0x0300 from %d.%d.%d.%d to %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3],
                ip_a, ip_b, ip_c, ip_d);
        }
        else
        {
            my_printf("Send Offer Service Failed !! (Error: %d)\n", err);
        }
        udp_disconnect(g_SOMEIPSD_PCB);

        pbuf_free(txbuf);
    }
    else
    {
        my_printf("Failed to allocate memory for UDP packet buffer.\n");
    }
}

void SOMEIPSD_SendSubEvtGrpAck(unsigned char ip_a, unsigned char ip_b, unsigned char ip_c, unsigned char ip_d,
    uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id, uint32_t ttl)
{
    err_t err;
    uint8_t MSG_SubEvtGrpAck[] = {
        /* SOMEIP Header */
        0xFF, 0xFF, 0x81, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x00,
        /* SOMEIP-SD Flags*/
        0xC0, 0x00, 0x00, 0x00,
        /* ENTRIES ARRAY LENGTH */
        0x00, 0x00, 0x00, 0x10,
        /* SubscribeEventgroupAck Entry */
        0x07,       // Type: SubscribeEventgroupAck
        0x00,       // Index 1st options
        0x00,       // Index 2nd options
        0x00,       // # of opt 1 = 0, # of opt 2 = 0
        0x00, 0x00, // Service ID (High, Low)
        0x00, 0x00, // Instance ID (High, Low)
        0x01,       // Major Version
        0x00, 0x00, 0x00, // TTL
        0x00,       // Reserved
        0x00, 0x00, // Eventgroup ID
        /* OPTIONS ARRAY LENGTH */
        0x00, 0x00, 0x00, 0x00
    };

    // Entry 정보 채우기
    MSG_SubEvtGrpAck[28] = (service_id >> 8) & 0xFF;
    MSG_SubEvtGrpAck[29] = service_id & 0xFF;
    MSG_SubEvtGrpAck[30] = (instance_id >> 8) & 0xFF;
    MSG_SubEvtGrpAck[31] = instance_id & 0xFF;
    MSG_SubEvtGrpAck[33] = (ttl >> 16) & 0xFF;
    MSG_SubEvtGrpAck[34] = (ttl >> 8) & 0xFF;
    MSG_SubEvtGrpAck[35] = ttl & 0xFF;
    MSG_SubEvtGrpAck[38] = (eventgroup_id >> 8) & 0xFF;
    MSG_SubEvtGrpAck[39] = eventgroup_id & 0xFF;

    struct pbuf* txbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(MSG_SubEvtGrpAck), PBUF_RAM);
    if (txbuf != NULL)
    {
        pbuf_take(txbuf, MSG_SubEvtGrpAck, sizeof(MSG_SubEvtGrpAck));

        ip_addr_t destination_ip;
        IP4_ADDR(&destination_ip, ip_a, ip_b, ip_c, ip_d);
        u16_t destination_port = PN_SOMEIPSD;

        err = udp_sendto(g_SOMEIPSD_PCB, txbuf, &destination_ip, destination_port);
        if (err == ERR_OK)
        {
            my_printf("Send SubscribeEventgroupAck to %d.%d.%d.%d:%d\n", ip_a, ip_b, ip_c, ip_d, destination_port);
            my_printf("Service: 0x%04X, Instance: 0x%04X, EventGroup: 0x%04X, TTL: %d\n",
                service_id, instance_id, eventgroup_id, ttl);
        }
        else
        {
            my_printf("udp_sendto fail!! (Error: %d)\n", err);
        }

        pbuf_free(txbuf);
    }
    else
    {
        my_printf("Failed to allocate memory for UDP packet buffer.\n");
    }
}

void SOMEIP_SendNotification(uint16_t service_id, uint16_t method_id,
    uint8_t* payload, uint16_t payload_len)
{
    err_t err;

    // SOME/IP 헤더 크기 (16바이트) + 페이로드
    uint16_t total_len = 16 + payload_len;

    struct pbuf* txbuf = pbuf_alloc(PBUF_TRANSPORT, total_len, PBUF_RAM);
    if (txbuf == NULL)
    {
        my_printf("Failed to allocate notification buffer\n");
        return;
    }

    uint8_t* data = (uint8_t*)txbuf->payload;

    // SOME/IP Header 구성
    data[0] = (service_id >> 8) & 0xFF;      // Service ID (High)
    data[1] = service_id & 0xFF;              // Service ID (Low)
    data[2] = (method_id >> 8) & 0xFF;        // Method ID (High)
    data[3] = method_id & 0xFF;               // Method ID (Low)

    uint32_t length = payload_len + 8;
    data[4] = (length >> 24) & 0xFF;          // Length
    data[5] = (length >> 16) & 0xFF;
    data[6] = (length >> 8) & 0xFF;
    data[7] = length & 0xFF;

    data[8] = 0x00;  data[9] = 0x00;          // Client ID
    data[10] = 0x00; data[11] = 0x00;         // Session ID
    data[12] = 0x01;                          // Protocol Version
    data[13] = 0x01;                          // Interface Version
    data[14] = 0x02;                          // Message Type (Notification)
    data[15] = 0x00;                          // Return Code

    // 페이로드 복사
    if (payload && payload_len > 0)
    {
        memcpy(&data[16], payload, payload_len);
    }

    // 구독한 모든 클라이언트에게 전송
    uint16_t sent_count = 0;
    for (uint8_t i = 0; i < MAX_CLIENTS; i++)
    {
        if (g_clients[i].active && g_clients[i].service_id == service_id)
        {
            err = udp_sendto(g_SOMEIPSD_PCB, txbuf, &g_clients[i].ip, g_clients[i].port);
            if (err == ERR_OK)
            {
                sent_count++;
            }
            else
            {
                my_printf("Failed to send notification to client %d (Error: %d)\n", i, err);
            }
        }
    }

    if (sent_count > 0)
    {
        my_printf("Notification sent to %d clients (Service: 0x%04X, Method: 0x%04X, Payload: %d bytes)\n",
            sent_count, service_id, method_id, payload_len);
    }

    pbuf_free(txbuf);
}
