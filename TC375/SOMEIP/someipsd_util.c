#include "someipsd_util.h"

// 1. SOME/IP Header (16 bytes)
uint16_t SOMEIPSD_AddHeader (uint8_t *txBuf, uint32_t length, uint16_t session_id)
{
    static const uint16_t service_id = 0xFFFF;
    static const uint16_t method_id = 0x8100;
    static const uint16_t client_id = 0x0001;
    static const uint8_t protocol_ver = 0x01;
    static const uint8_t interface_ver = 0x01;
    static const uint8_t message_type = 0x02;
    static const uint8_t return_code = 0x00;

    uint8_t *p = txBuf;

    p[0] = (uint8_t) ((service_id >> 8) & 0xFF);
    p[1] = (uint8_t) (service_id & 0xFF);

    p[2] = (uint8_t) ((method_id >> 8) & 0xFF);
    p[3] = (uint8_t) (method_id & 0xFF);

    p[4] = (uint8_t) ((length >> 24) & 0xFF);
    p[5] = (uint8_t) ((length >> 16) & 0xFF);
    p[6] = (uint8_t) ((length >> 8) & 0xFF);
    p[7] = (uint8_t) (length & 0xFF);

    p[8] = (uint8_t) ((client_id >> 8) & 0xFF);
    p[9] = (uint8_t) (client_id & 0xFF);

    p[10] = (uint8_t) ((session_id >> 8) & 0xFF);
    p[11] = (uint8_t) (session_id & 0xFF);

    p[12] = protocol_ver;
    p[13] = interface_ver;
    p[14] = message_type;
    p[15] = return_code;

    return 16;
}

// 2. SD Header (4 bytes)
uint16_t SOMEIPSD_AddSdHeader (uint8_t *txBuf, uint16_t txLen)
{
    uint8_t *p = txBuf + txLen;
    p[16] = 0x80; // 공식 문서를 보면 조건에 따라 Flags [8 bit] 가 바뀔 수도 있음. 근데 귀찮아서 안함.
    p[17] = p[18] = p[19] = 0x00;
    return txLen + 4;
}

// 3. Entries Array Length (4 bytes)
uint16_t SOMEIPSD_AddEntriesArrayLength (uint8_t *txBuf, uint16_t txLen, uint32_t length)
{
    uint8_t *p = txBuf + txLen;

    p[0] = (length >> 24) & 0xFF;
    p[1] = (length >> 16) & 0xFF;
    p[2] = (length >> 8) & 0xFF;
    p[3] = length & 0xFF;

    return txLen + 4;
}

// 4-1. Service Entry (16 bytes)
uint16_t SOMEIPSD_AddServiceEntry (uint8_t *txBuf, uint16_t txLen, uint8_t type, uint8_t idx_opt1, uint8_t idx_opt2,
        uint8_t num_opt1, uint8_t num_opt2, uint16_t service_id, uint16_t instance_id, uint8_t major_version,
        uint32_t ttl, uint32_t minor_version)
{
    uint8_t *p = txBuf + txLen;

    /* Type */
    p[0] = type;

    /* Index of first and second option run */
    p[1] = idx_opt1;
    p[2] = idx_opt2;

    /* Number of options (4 bits + 4 bits) */
    p[3] = ((num_opt1 & 0x0F) << 4) | (num_opt2 & 0x0F);

    /* Service ID */
    p[4] = (uint8_t) ((service_id >> 8) & 0xFF);
    p[5] = (uint8_t) (service_id & 0xFF);

    /* Instance ID */
    p[6] = (uint8_t) ((instance_id >> 8) & 0xFF);
    p[7] = (uint8_t) (instance_id & 0xFF);

    /* Major Version */
    p[8] = major_version;

    /* TTL (24 bits) */
    p[9] = (uint8_t) ((ttl >> 16) & 0xFF);
    p[10] = (uint8_t) ((ttl >> 8) & 0xFF);
    p[11] = (uint8_t) (ttl & 0xFF);

    /* Minor Version (32 bits) */
    p[12] = (uint8_t) ((minor_version >> 24) & 0xFF);
    p[13] = (uint8_t) ((minor_version >> 16) & 0xFF);
    p[14] = (uint8_t) ((minor_version >> 8) & 0xFF);
    p[15] = (uint8_t) (minor_version & 0xFF);

    return txLen + 16;
}

// 4-2. Eventgroup Entry (16 bytes)
uint16_t SOMEIPSD_AddEventgroupEntry (uint8_t *txBuf, uint16_t txLen, uint8_t type, uint8_t idx_opt1, uint8_t idx_opt2,
        uint8_t num_opt1, uint8_t num_opt2, uint16_t service_id, uint16_t instance_id, uint8_t major_version,
        uint32_t ttl, uint8_t counter, uint16_t eventgroup_id)
{
    uint8_t *p = txBuf + txLen;

    /* Type */
    p[0] = type;

    /* Option indices */
    p[1] = idx_opt1;
    p[2] = idx_opt2;

    /* Number of options (4 + 4 bits) */
    p[3] = ((num_opt1 & 0x0F) << 4) | (num_opt2 & 0x0F);

    /* Service ID */
    p[4] = (uint8_t) ((service_id >> 8) & 0xFF);
    p[5] = (uint8_t) (service_id & 0xFF);

    /* Instance ID */
    p[6] = (uint8_t) ((instance_id >> 8) & 0xFF);
    p[7] = (uint8_t) (instance_id & 0xFF);

    /* Major Version */
    p[8] = major_version;

    /* TTL (24 bits) */
    p[9] = (uint8_t) ((ttl >> 16) & 0xFF);
    p[10] = (uint8_t) ((ttl >> 8) & 0xFF);
    p[11] = (uint8_t) (ttl & 0xFF);

    /* Reserved(12 bits=0) + Counter(4 bits) */
    p[12] = 0x00;
    p[13] = (uint8_t) (counter & 0x0F);

    /* Eventgroup ID */
    p[14] = (uint8_t) ((eventgroup_id >> 8) & 0xFF);
    p[15] = (uint8_t) (eventgroup_id & 0xFF);

    return txLen + 16;
}

// 5. Options Array Length (4 bytes)
uint16_t SOMEIPSD_AddOptionsArrayLength (uint8_t *txBuf, uint16_t txLen, uint32_t length)
{
    uint8_t *p = txBuf + txLen;

    p[0] = (length >> 24) & 0xFF;
    p[1] = (length >> 16) & 0xFF;
    p[2] = (length >> 8) & 0xFF;
    p[3] = length & 0xFF;

    return txLen + 4;
}

// 6-1. IPv4 Endpoint / Multicast / SD Endpoint Option 작성 (공용)
uint16_t SOMEIPSD_Add0xX4Option (uint8_t *txBuf, uint16_t txLen, uint8_t type, ip_addr_t ipv4_addr, uint8_t l4_proto,
        uint16_t port)
{
    static const uint16_t option_len = 0x0009;
    uint8_t *p = txBuf + txLen;

    p[0] = (uint8_t) ((option_len >> 8) & 0xff);
    p[1] = (uint8_t) (option_len & 0xff);
    p[2] = type;
    p[3] = 0x00; // Discardable Flag [1 bit]: Shall be set to 0. Bit 1 to bit 7 are reserved.

    p[4] = ipv4_addr.addr & 0xFF;
    p[5] = (ipv4_addr.addr >> 8) & 0xFF;
    p[6] = (ipv4_addr.addr >> 16) & 0xFF;
    p[7] = (ipv4_addr.addr >> 24) & 0xFF;

    p[8] = 0x00; // reserved
    p[9] = l4_proto;
    p[10] = (uint8_t) ((port >> 8) & 0xff);
    p[11] = (uint8_t) (port & 0xff);

    return txLen + 3 + option_len;
}
