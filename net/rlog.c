// ------------------------------------------------------------------------------------------------
// net/rlog.c
// ------------------------------------------------------------------------------------------------

#include "net/rlog.h"
#include "net/buf.h"
#include "net/intf.h"
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
    Net_Intf* intf;
    list_for_each(intf, g_net_intf_list, link)
    {
        if (!ipv4_addr_eq(&intf->broadcast_addr, &null_ipv4_addr))
        {
            NetBuf* buf = net_alloc_packet();
            u8* pkt = (u8*)(buf + 1);

            strcpy((char*)pkt, msg);
            u8* end = pkt + len;

            udp_tx(&intf->broadcast_addr, PORT_OSHELPER, PORT_OSHELPER, pkt, end);
        }
    }
}
