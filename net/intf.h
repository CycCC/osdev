// ------------------------------------------------------------------------------------------------
// net/intf.h
// ------------------------------------------------------------------------------------------------

#pragma once

#include "net/addr.h"
#include "stdlib/link.h"

// ------------------------------------------------------------------------------------------------
// Net Interface

typedef struct Net_Intf
{
    Link link;
    Eth_Addr eth_addr;
    IPv4_Addr ip_addr;
    IPv4_Addr broadcast_addr;
    const char* name;

    void (*poll)(struct Net_Intf* intf);
    void (*tx)(struct Net_Intf* intf, const void* dst_addr, u16 ether_type, u8* pkt, u8* end);
    void (*dev_tx)(u8* pkt, u8* end);
} Net_Intf;

// ------------------------------------------------------------------------------------------------
// Globals

extern Link g_net_intf_list;

// ------------------------------------------------------------------------------------------------
// Functions

Net_Intf* net_intf_create();
void net_intf_add(Net_Intf* intf);
