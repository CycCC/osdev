// ------------------------------------------------------------------------------------------------
// net_intf.h
// ------------------------------------------------------------------------------------------------

#pragma once

#include "link.h"
#include "net_addr.h"

// ------------------------------------------------------------------------------------------------
// Net Interface

typedef struct Net_Intf
{
    Link link;
    Eth_Addr eth_addr;
    IPv4_Addr ip_addr;
    IPv4_Addr subnet_mask;
    IPv4_Addr gateway_addr;
    const char* name;

    void (*init)(struct Net_Intf* intf);
    void (*poll)(struct Net_Intf* intf);
    void (*tx)(struct Net_Intf* intf, u8* pkt, uint len);
} Net_Intf;

// ------------------------------------------------------------------------------------------------
// Globals

extern Link g_net_intf_list;

// ------------------------------------------------------------------------------------------------
// Functions

Net_Intf* net_intf_create();
void net_intf_add(Net_Intf* intf);
