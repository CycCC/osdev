// ------------------------------------------------------------------------------------------------
// net/intf.c
// ------------------------------------------------------------------------------------------------

#include "net/intf.h"
#include "mem/vm.h"
#include "stdlib/string.h"

// ------------------------------------------------------------------------------------------------
// Globals

Link g_net_intf_list = { &g_net_intf_list, &g_net_intf_list };

// ------------------------------------------------------------------------------------------------
Net_Intf* net_intf_create()
{
    Net_Intf* intf = vm_alloc(sizeof(Net_Intf));
    memset(intf, 0, sizeof(Net_Intf));
    link_init(&intf->link);

    return intf;
}

// ------------------------------------------------------------------------------------------------
void net_intf_add(Net_Intf* intf)
{
    link_before(&g_net_intf_list, &intf->link);
}
