// ------------------------------------------------------------------------------------------------
// net/rlog.c
// ------------------------------------------------------------------------------------------------

#include "net/rlog.h"
#include "net/intf.h"
#include "net/net.h"
#include "net/port.h"
#include "net/udp.h"
#include "stdlib/link.h"
#include "stdlib/format.h"
#include "stdlib/stdarg.h"
#include "stdlib/string.h"

// ------------------------------------------------------------------------------------------------
void rlog_print(const char* fmt, ...)
{
    char msg[1024];
    va_list args;

    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    uint len = strlen(msg) + 1;

    // For each interface, broadcast a packet
    Link* it = g_net_intf_list.next;
    Link* end = &g_net_intf_list;

    while (it != end)
    {
        Net_Intf* intf = link_data(it, Net_Intf, link);

        if (!ipv4_addr_eq(&intf->broadcast_addr, &null_ipv4_addr))
        {
            NetBuf* buf = net_alloc_packet();
            u8* pkt = (u8*)(buf + 1);

            strcpy((char*)pkt, msg);

            udp_tx(&intf->broadcast_addr, PORT_OSHELPER, PORT_OSHELPER, pkt, len);
        }

        it = it->next;
    }
}
