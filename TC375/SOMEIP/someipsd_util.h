#ifndef SOMEIP_SOMEIPSD_UTIL_H_
#define SOMEIP_SOMEIPSD_UTIL_H_

#include <stdint.h>
#include "ip_addr.h"

uint16_t SOMEIPSD_AddHeader (uint8_t *txBuf, uint32_t length, uint16_t session_id);
uint16_t SOMEIPSD_AddSdHeader (uint8_t *txBuf, uint16_t txLen);
uint16_t SOMEIPSD_AddEntriesArrayLength (uint8_t *txBuf, uint16_t txLen, uint32_t length);
uint16_t SOMEIPSD_AddServiceEntry (uint8_t *txBuf, uint16_t txLen, uint8_t type, uint8_t idx_opt1, uint8_t idx_opt2,
        uint8_t num_opt1, uint8_t num_opt2, uint16_t service_id, uint16_t instance_id, uint8_t major_version,
        uint32_t ttl, uint32_t minor_version);
uint16_t SOMEIPSD_AddEventgroupEntry (uint8_t *txBuf, uint16_t txLen, uint8_t type, uint8_t idx_opt1, uint8_t idx_opt2,
        uint8_t num_opt1, uint8_t num_opt2, uint16_t service_id, uint16_t instance_id, uint8_t major_version,
        uint32_t ttl, uint8_t counter, uint16_t eventgroup_id);
uint16_t SOMEIPSD_AddOptionsArrayLength (uint8_t *txBuf, uint16_t txLen, uint32_t length);
uint16_t SOMEIPSD_Add0xX4Option (uint8_t *txBuf, uint16_t txLen, uint8_t type, ip_addr_t ipv4_addr, uint8_t l4_proto,
        uint16_t port);

#endif /* SOMEIP_SOMEIPSD_UTIL_H_ */
