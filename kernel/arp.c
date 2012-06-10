// ------------------------------------------------------------------------------------------------
// arp.c
// ------------------------------------------------------------------------------------------------

#include "arp.h"
#include "console.h"
#include "eth.h"
#include "ipv4.h"
#include "net.h"
#include "string.h"

// ------------------------------------------------------------------------------------------------
// ARP Protocol
#define ARP_HTYPE_ETH       0x01

#define ARP_OP_REQUEST      0x01
#define ARP_OP_REPLY        0x02

// ------------------------------------------------------------------------------------------------
// ARP Cache
#define ARP_CACHE_SIZE      16

typedef struct ARP_Entry
{
    Eth_Addr ha;
    IPv4_Addr pa;
} ARP_Entry;

static ARP_Entry arp_cache[ARP_CACHE_SIZE];

// ------------------------------------------------------------------------------------------------
static void arp_print(const u8* pkt, uint len)
{
    if (!net_trace)
    {
        return;
    }

    if (len < sizeof(ARP_Header))
    {
        return;
    }

    const ARP_Header* hdr = (const ARP_Header*)pkt;

    u16 htype = net_swap16(hdr->htype);
    u16 ptype = net_swap16(hdr->ptype);
    u8 hlen = hdr->hlen;
    u8 plen = hdr->plen;
    u16 op = net_swap16(hdr->op);
    console_print(" ARP: htype=0x%x ptype=0x%x hlen=%d plen=%d op=%d\n",
            htype, ptype, hlen, plen, op);

    if (htype == ARP_HTYPE_ETH && ptype == ET_IPV4 && len >= 28)
    {
        const Eth_Addr* sha = (const Eth_Addr*)(pkt + 8);
        const IPv4_Addr* spa = (const IPv4_Addr*)(pkt + 14);
        const Eth_Addr* tha = (const Eth_Addr*)(pkt + 18);
        const IPv4_Addr* tpa = (const IPv4_Addr*)(pkt + 24);

        char sha_str[ETH_ADDR_STRING_SIZE];
        char spa_str[IPV4_ADDR_STRING_SIZE];
        char tha_str[ETH_ADDR_STRING_SIZE];
        char tpa_str[IPV4_ADDR_STRING_SIZE];

        eth_addr_to_str(sha_str, sizeof(sha_str), sha);
        ipv4_addr_to_str(spa_str, sizeof(spa_str), spa);
        eth_addr_to_str(tha_str, sizeof(tha_str), tha);
        ipv4_addr_to_str(tpa_str, sizeof(tpa_str), tpa);

        console_print(" ARP: %s spa=%s\n", sha_str, spa_str);
        console_print(" ARP: %s tpa=%s\n", tha_str, tpa_str);
    }
}

// ------------------------------------------------------------------------------------------------
static void arp_snd(Net_Intf* intf, uint op, const Eth_Addr* tha, const IPv4_Addr* tpa)
{
    u8 buf[256];

    u8* pkt = buf + MAX_PACKET_HEADER;

    // HTYPE
    pkt[0] = (ARP_HTYPE_ETH >> 8) & 0xff;
    pkt[1] = (ARP_HTYPE_ETH) & 0xff;

    // PTYPE
    pkt[2] = (ET_IPV4 >> 8) & 0xff;
    pkt[3] = (ET_IPV4) & 0xff;

    // HLEN
    pkt[4] = sizeof(Eth_Addr);

    // PLEN
    pkt[5] = sizeof(IPv4_Addr);

    // Operation
    pkt[6] = (op >> 8) & 0xff;
    pkt[7] = (op) & 0xff;

    // SHA
    *(Eth_Addr*)(pkt + 8) = intf->eth_addr;

    // SPA
    *(IPv4_Addr*)(pkt + 14) = intf->ip_addr;

    // THA
    if (op == ARP_OP_REQUEST)
    {
        *(Eth_Addr*)(pkt + 18) = null_eth_addr;
    }
    else
    {
        *(Eth_Addr*)(pkt + 18) = *tha;
    }

    // TPA
    *(IPv4_Addr*)(pkt + 24) = *tpa;

    // Print
    arp_print(pkt, 28);

    // Transmit packet
    intf->tx(intf, tha, ET_ARP, pkt, 28);
}

// ------------------------------------------------------------------------------------------------
void arp_request(Net_Intf* intf, const IPv4_Addr* tpa)
{
    arp_snd(intf, ARP_OP_REQUEST, &broadcast_eth_addr, tpa);
}

// ------------------------------------------------------------------------------------------------
void arp_reply(Net_Intf* intf, const Eth_Addr* tha, const IPv4_Addr* tpa)
{
    arp_snd(intf, ARP_OP_REPLY, tha, tpa);
}

// ------------------------------------------------------------------------------------------------
static void arp_add(const Eth_Addr* ha, const IPv4_Addr* pa)
{
    // TODO - handle overflow
    ARP_Entry* entry = arp_cache;
    ARP_Entry* end = entry + ARP_CACHE_SIZE;
    for (; entry != end; ++entry)
    {
        if (!entry->pa.u.bits)
        {
            break;
        }
    }

    if (entry != end)
    {
        entry->ha = *ha;
        entry->pa = *pa;
    }
}

// ------------------------------------------------------------------------------------------------
void arp_init()
{
    // Clear cache of all entries
    ARP_Entry* entry = arp_cache;
    ARP_Entry* end = entry + ARP_CACHE_SIZE;
    for (; entry != end; ++entry)
    {
        memset(&entry->ha, 0, sizeof(Eth_Addr));
        memset(&entry->pa, 0, sizeof(IPv4_Addr));
    }
}

// ------------------------------------------------------------------------------------------------
static ARP_Entry* arp_lookup(const IPv4_Addr* pa)
{
    ARP_Entry* entry = arp_cache;
    ARP_Entry* end = entry + ARP_CACHE_SIZE;
    for (; entry != end; ++entry)
    {
        if (entry->pa.u.bits == pa->u.bits)
        {
            return entry;
        }
    }

    return 0;
}

// ------------------------------------------------------------------------------------------------
const Eth_Addr* arp_lookup_mac(const IPv4_Addr* pa)
{
    ARP_Entry* entry = arp_lookup(pa);
    if (entry)
    {
        return &entry->ha;
    }

    return 0;
}

// ------------------------------------------------------------------------------------------------
void arp_rx(Net_Intf* intf, const u8* pkt, uint len)
{
    arp_print(pkt, len);

    // Decode Header
    if (len < sizeof(ARP_Header))
    {
        return;
    }

    const ARP_Header* hdr = (const ARP_Header*)pkt;

    u16 htype = net_swap16(hdr->htype);
    u16 ptype = net_swap16(hdr->ptype);
    u16 op = net_swap16(hdr->op);

    // Skip packets that are not Ethernet, IPv4, or well-formed
    if (htype != ARP_HTYPE_ETH || ptype != ET_IPV4 || len < 28)
    {
        return;
    }

    // Decode addresses
    const Eth_Addr* sha = (const Eth_Addr*)(pkt + 8);
    const IPv4_Addr* spa = (const IPv4_Addr*)(pkt + 14);
    //const Eth_Addr* tha = (const Eth_Addr*)(pkt + 18);
    const IPv4_Addr* tpa = (const IPv4_Addr*)(pkt + 24);

    // Update existing entry if we know about this source IP address
    bool merge = false;
    ARP_Entry* entry = arp_lookup(spa);
    if (entry)
    {
        if (net_trace)
        {
            console_print("ARP Merging entry\n");
        }

        entry->ha = *sha;
        merge = true;
    }

    // Check if this ARP packet is targeting our IP
    if (tpa->u.bits == intf->ip_addr.u.bits)
    {
        // Add a new entry if we didn't update earlier.
        if (!merge)
        {
            if (net_trace)
            {
                console_print("ARP Adding entry\n");
            }

            arp_add(sha, spa);
        }

        // Respond to requests.
        if (op == ARP_OP_REQUEST)
        {
            arp_reply(intf, sha, spa);
        }
    }
}
