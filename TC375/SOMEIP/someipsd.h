#ifndef SOMEIP_SOMEIPSD_H_
#define SOMEIP_SOMEIPSD_H_

#include <stdbool.h>

#include "ip_addr.h"

bool SOMEIPSD_Init (void);
void SOMEIPSD_SendOfferService (ip_addr_t client_ip);

#endif /* SOMEIP_SOMEIPSD_H_ */

/*
 * SOME/IP-SD Entry Type vs TTL semantics
 *
 * ┌────────────┬─────────────────────────────┬──────────────────────────────┐
 * │ Entry Type │ TTL > 0                     │ TTL = 0                      │
 * ├────────────┼─────────────────────────────┼──────────────────────────────┤
 * │ 0x00       │ FindService                 │ —                            │
 * │ 0x01       │ OfferService                │ StopOfferService             │
 * │ 0x06       │ SubscribeEventgroup         │ StopSubscribeEventgroup      │
 * │ 0x07       │ SubscribeEventgroupAck      │ SubscribeEventgroupNack      │
 * └────────────┴─────────────────────────────┴──────────────────────────────┘
 */

/*
 * SOME/IP-SD Option Types (AUTOSAR R22-11) - 간략 정리
 *
 * 0x24: IPv4 SD Endpoint Option (SD Msg Sender의 IP, L4-Proto, Port 정보. SD Msg 별로 하나만 존재)
 * 0x26: IPv6 SD Endpoint Option
 *
 * 0x04: IPv4 Endpoint Option
 * 0x06: IPv6 Endpoint Option
 *
 * 0x14: IPv4 Multicast Option
 * 0x16: IPv6 Multicast Option
 *
 * 0x01: Configuration Option
 *
 * 0x02: Load Balancing Option
 */

/*
 * Usage of Options in Entries
 *
 * [PRS_SOMEIPSD_00583]
 * SOME/IP-SD Option types allowed per Entry Type
 * (from AUTOSAR_PRS_SOMEIPServiceDiscoveryProtocol, Table 5.1)
 *
 * ┌────────────────────────────┬─────────┬───────────┬───────────────┬───────────────┐
 * │         Entry Type         │ Endpoint│ Multicast │ Configuration │ Load Balancing│
 * ├────────────────────────────┼─────────┼───────────┼───────────────┼───────────────┤
 * │ FindService                │    0    │     0     │      0–1      │      0–1      │
 * │ OfferService               │   1–2   │     0     │      0–1      │      0–1      │
 * │ StopOfferService           │   1–2   │     0     │      0–1      │      0–1      │
 * │ SubscribeEventgroup        │   0–2   │    0–1    │      0–1      │      0–1      │
 * │ StopSubscribeEventgroup    │   0–2   │    0–1    │      0–1      │      0–1      │
 * │ SubscribeEventgroupAck     │    0    │    0–1    │      0–1      │      0–1      │
 * │ SubscribeEventgroupNack    │    0    │     0     │      0–1      │      0–1      │
 * └────────────────────────────┴─────────┴───────────┴───────────────┴───────────────┘
 *
 *  Legend:
 *   - "Endpoint Options": IPv4/IPv6 SD Endpoint (0x24/0x26) 또는 Entry 관련 Endpoint (0x04/0x06)
 *   - "Multicast": multicast-specific options for eventgroups (0x14/0x16)
 *   - "Configuration" : configuration or transport parameters (0x01)
 *   - "Load Balancing" : optional load-balancing info (0x02)
 *
 *  Example interpretations:
 *   - FindService: may include up to one Configuration and one LoadBalancing option.
 *   - OfferService: must include one or two Endpoint options.
 *   - SubscribeEventgroup: may include multiple Endpoint + optional Multicast + others.
 */
