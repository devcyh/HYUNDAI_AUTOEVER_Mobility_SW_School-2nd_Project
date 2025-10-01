#include "someip.h"
#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/udp.h"
#include "Configuration.h"
#include "Ifx_Lwip.h"
#include <string.h>
#include "etc.h"
#include "my_stdio.h"

#if LWIP_UDP

struct udp_pcb *g_SOMEIPSERVICE_PCB;

void SOMEIP_Callback (void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, uint16 port);

bool SOMEIP_Init (void)
{
    /* SOME/IP Service1 Init */
    g_SOMEIPSERVICE_PCB = udp_new();
    if (g_SOMEIPSERVICE_PCB)
    {
        /* bind pcb to the 5000 port */
        /* Using IP_ADDR_ANY allow the pcb to be used by any local interface */
        err_t err = udp_bind(g_SOMEIPSERVICE_PCB, IP_ADDR_ANY, PN_SERVICE_1);
        if (err == ERR_OK)
        {
            /* Set a receive callback for the pcb */
            udp_recv(g_SOMEIPSERVICE_PCB, (void*) SOMEIP_Callback, NULL);
            my_printf("SOME/IP Service PCB Initialized!\n");
            return true;
        }
        else
        {
            udp_remove(g_SOMEIPSERVICE_PCB);
            my_printf("SOME/IP Service PCB init failed!\n");
            return false;
        }
    }
    return false;
}

void SOMEIP_Callback (void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, uint16 port)
{
    uint16 ServiceID = 0;
    uint16 MethodID = 0;
    uint8 MessageType = 0;
    if (p != NULL)
    {
        uint8 rxBuf[p->len];
        memcpy(rxBuf, p->payload, p->len);
        ServiceID = (rxBuf[0] << 8) + rxBuf[1];
        MethodID = (rxBuf[2] << 8) + rxBuf[3];

        if (ServiceID != 0x0100U)
        {
            my_printf("Requested Unknown Service ID\n");
        }
        else
        {
            /* Message Type: Request */
            MessageType = rxBuf[14];
            if (MessageType == 0x00)
            {
                /* Check Service ID & Method ID */
                if (ServiceID == 0x0100U)
                {
                    if (MethodID == 0x0101U || MethodID == 0x0102U)
                    {
                        if (MethodID == 0x0101U)
                        {
//                            SrvWelcomeLightInteriorControl(rxBuf[16]);
                        }
                        else
                        {
//                            SrvWelcomeLightExteriorControl(rxBuf[16]);
                        }

                        /* Send Response Message */
                        err_t err;
                        rxBuf[14] = 0x80;
                        struct pbuf *txbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(rxBuf), PBUF_RAM);
                        if (txbuf != NULL)
                        {
                            udp_connect(upcb, addr, port);
                            pbuf_take(txbuf, rxBuf, sizeof(rxBuf));
                            ip_addr_t destination_ip;
                            unsigned char a = (unsigned char) (addr->addr);
                            unsigned char b = (unsigned char) (addr->addr >> 8);
                            unsigned char c = (unsigned char) (addr->addr >> 16);
                            unsigned char d = (unsigned char) (addr->addr >> 24);

                            IP4_ADDR(&destination_ip, a, b, c, d);
                            u16_t destination_port = PN_SERVICE_1;
                            err = udp_sendto(upcb, txbuf, &destination_ip, destination_port);
                            if (err == ERR_OK)
                            {
                                my_printf("Send SOME/IP Service Response!! \n");
                            }
                            else
                            {
                                my_printf("Send SOME/IP Service Response Failed!! \n");
                            }
                            udp_disconnect(upcb);
                            pbuf_free(txbuf);
                        }
                        else
                        {
                            my_printf("Failed to allocate memory for UDP packet buffer.\n");
                        }
                    }
                }
            }
        }
        pbuf_free(p);
    }
}

#endif /* LWIP_UDP */
