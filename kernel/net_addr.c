// ------------------------------------------------------------------------------------------------
// net_addr.c
// ------------------------------------------------------------------------------------------------

#include "net_addr.h"
#include "format.h"

// ------------------------------------------------------------------------------------------------
Eth_Addr broadcast_eth_addr = { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };

// ------------------------------------------------------------------------------------------------
void eth_addr_to_str(char* str, size_t size, const Eth_Addr* addr)
{
    snprintf(str, size, "%02x:%02x:%02x:%02x:%02x:%02x",
            addr->n[0], addr->n[1], addr->n[2], addr->n[3], addr->n[4], addr->n[5]);
}

// ------------------------------------------------------------------------------------------------
void ipv4_addr_to_str(char* str, size_t size, const IPv4_Addr* addr)
{
    snprintf(str, size, "%d.%d.%d.%d", addr->u.n[0], addr->u.n[1], addr->u.n[2], addr->u.n[3]);
}