// ------------------------------------------------------------------------------------------------
// net/tcp.c
// ------------------------------------------------------------------------------------------------

#include "net/tcp.h"
#include "net/buf.h"
#include "net/checksum.h"
#include "net/ipv4.h"
#include "net/net.h"
#include "net/port.h"
#include "net/route.h"
#include "net/swap.h"
#include "console/console.h"
#include "mem/vm.h"
#include "stdlib/string.h"
#include "time/pit.h"
#include "time/rtc.h"

// ------------------------------------------------------------------------------------------------
// Static Variables

static u32 tcp_base_isn;
static Link tcp_free_conns = { &tcp_free_conns, &tcp_free_conns };

// ------------------------------------------------------------------------------------------------
static bool tcp_parse_options(TCP_Options* opt, const u8* p, const u8* end)
{
    memset(opt, 0, sizeof(*opt));

    while (p < end)
    {
        u8 type = *p++;

        if (type == OPT_NOP)
        {
            continue;
        }
        else if (type == OPT_END)
        {
            break;
        }
        else
        {
            u8 opt_len = *p++;

            const u8* next = p + opt_len - 2;
            if (next > end)
            {
                return false;
            }

            switch (type)
            {
            case OPT_MSS:
                opt->mss = net_swap16(*(u16*)p);
                break;
            }

            p = next;
        }
    }

    return true;
}

// ------------------------------------------------------------------------------------------------
static void tcp_print(const u8* pkt, const u8* end)
{
    /*
    if (!net_trace)
    {
        return;
    }*/

    if (pkt + sizeof(TCP_Header) > end)
    {
        return;
    }

    const TCP_Header* hdr = (const TCP_Header*)pkt;

    u16 src_port = net_swap16(hdr->src_port);
    u16 dst_port = net_swap16(hdr->dst_port);
    u32 seq = net_swap32(hdr->seq);
    u32 ack = net_swap32(hdr->ack);
    u16 window_size = net_swap16(hdr->window_size);
    u16 checksum = net_swap16(hdr->checksum);
    u16 urgent = net_swap16(hdr->urgent);

    u16 checksum2 = net_checksum(pkt - sizeof(Checksum_Header), end);

    uint hdr_len = hdr->len >> 2;
    //const u8* data = (pkt + hdr_len);
    uint data_len = (end - pkt) - hdr_len;

    console_print("  TCP: src_port=%u dst_port=%u seq=%u ack=%u data_len=%u\n",
            src_port, dst_port, seq, ack, data_len);
    console_print("  TCP: flags=%02x window=%u urgent=%u checksum=%u%c\n",
            hdr->flags, window_size, urgent, checksum,
            checksum2 ? '!' : ' ');

    if (hdr_len > sizeof(TCP_Header))
    {
        const u8* p = pkt + sizeof(TCP_Header);
        const u8* end = p + hdr_len;

        TCP_Options opt;
        tcp_parse_options(&opt, p, end);

        if (opt.mss)
        {
            console_print("  TCP: mss=%u\n", opt.mss);
        }
    }
}

// ------------------------------------------------------------------------------------------------
static void tcp_tx_syn(TCP_Conn* conn, u32 seq)
{
    NetBuf* buf = net_alloc_packet();
    u8* pkt = (u8*)(buf + 1);

    // Header
    TCP_Header* hdr = (TCP_Header*)pkt;
    hdr->src_port = conn->local_port;
    hdr->dst_port = conn->remote_port;
    hdr->seq = seq;
    hdr->ack = 0;
    hdr->len = 0;
    hdr->flags = TCP_SYN;
    hdr->window_size = TCP_WINDOW_SIZE;
    hdr->checksum = 0;
    hdr->urgent = 0;
    tcp_swap(hdr);

    u8* p = pkt + sizeof(TCP_Header);

    // Maximum Segment Size
    p[0] = OPT_MSS;
    p[1] = 4;
    *(u16*)(p + 2) = net_swap16(1460);
    p += p[1];

    // Option End
    while ((p - pkt) & 3)
    {
        *p++ = 0;
    }

    hdr->len = (p - pkt) << 2;

    // Data
    u8* end = p;

    // Pseudo Header
    Checksum_Header* phdr = (Checksum_Header*)(pkt - sizeof(Checksum_Header));
    phdr->src = conn->intf->ip_addr;
    phdr->dst = conn->remote_addr;
    phdr->reserved = 0;
    phdr->protocol = IP_PROTOCOL_TCP;
    phdr->len = net_swap16(end - pkt);

    // Checksum
    u16 checksum = net_checksum(pkt - sizeof(Checksum_Header), end);
    hdr->checksum = net_swap16(checksum);

    // Transmit
    tcp_print(pkt, end);
    ipv4_tx_intf(conn->intf, &conn->next_addr, &conn->remote_addr, IP_PROTOCOL_TCP, pkt, end);
}

// ------------------------------------------------------------------------------------------------
static TCP_Conn* tcp_alloc()
{
    Link* p = tcp_free_conns.next;
    if (p != &tcp_free_conns)
    {
        link_remove(p);
        return link_data(p, TCP_Conn, link);
    }
    else
    {
        return vm_alloc(sizeof(TCP_Conn));
    }
}

// ------------------------------------------------------------------------------------------------
void tcp_init()
{
    // Compute base ISN from system clock and ticks since boot.  ISN is incremented every 4 us.
    DateTime dt;
    rtc_get_time(&dt);
    abs_time t = join_time(&dt);

    tcp_base_isn = (t * 1000 - pit_ticks) * 250;
}

// ------------------------------------------------------------------------------------------------
void tcp_rx(Net_Intf* intf, u8* pkt, u8* end)
{
    // Validate packet header
    const IPv4_Header* ip_hdr = (const IPv4_Header*)pkt;

    uint ihl = (ip_hdr->ver_ihl) & 0xf;
    const u8* tcp_pkt = pkt + (ihl << 2);

    if (pkt + sizeof(TCP_Header) > end)
    {
        return;
    }

    // Assemble Pseudo Header
    IPv4_Addr src_addr = ip_hdr->src;
    IPv4_Addr dst_addr = ip_hdr->dst;
    u8 protocol = ip_hdr->protocol;

    Checksum_Header* phdr = (Checksum_Header*)(tcp_pkt - sizeof(Checksum_Header));
    phdr->src = src_addr;
    phdr->dst = dst_addr;
    phdr->reserved = 0;
    phdr->protocol = protocol;
    phdr->len = net_swap16(end - tcp_pkt);

    tcp_print(tcp_pkt, end);

    // Process packet
    // const TCP_Header* tcp_hdr = (const TCP_Header*)tcp_pkt;
}

// ------------------------------------------------------------------------------------------------
void tcp_swap(TCP_Header* hdr)
{
    hdr->src_port = net_swap16(hdr->src_port);
    hdr->dst_port = net_swap16(hdr->dst_port);
    hdr->seq = net_swap32(hdr->seq);
    hdr->ack = net_swap32(hdr->ack);
    hdr->window_size = net_swap16(hdr->window_size);
    hdr->checksum = net_swap16(hdr->checksum);
    hdr->urgent = net_swap16(hdr->urgent);
}

// ------------------------------------------------------------------------------------------------
TCP_Conn* tcp_connect(const IPv4_Addr* addr, u16 port)
{
    // Find network interface through the routing table.
    const Net_Route* route = net_find_route(addr);
    if (!route)
    {
        return 0;
    }

    Net_Intf* intf = route->intf;

    // Determine ISN
    u32 isn = tcp_base_isn + pit_ticks * 250;

    // Create new connection object
    TCP_Conn* conn = tcp_alloc();

    memset(conn, 0, sizeof(TCP_Conn));
    conn->intf = intf;
    conn->local_addr = intf->ip_addr;
    conn->next_addr = *net_next_addr(route, addr);
    conn->remote_addr = *addr;
    conn->local_port = net_ephemeral_port();
    conn->remote_port = port;

    conn->snd_una = isn;
    conn->snd_nxt = isn + 1;
    conn->snd_wnd = TCP_WINDOW_SIZE;
    conn->snd_wl1 = 0;  // TODO?
    conn->snd_wl2 = 0;  // TODO?

    conn->rcv_nxt = 0;
    conn->rcv_wnd = TCP_WINDOW_SIZE;

    // Issue SYN segment
    tcp_tx_syn(conn, isn);
    conn->state = TCP_SYN_SENT;

    return conn;
}

// ------------------------------------------------------------------------------------------------
void tcp_close(TCP_Conn* conn)
{
    link_before(&tcp_free_conns, &conn->link);
}

// ------------------------------------------------------------------------------------------------
void tcp_send(TCP_Conn* conn, const void* data, uint count)
{
}

// ------------------------------------------------------------------------------------------------
uint tcp_recv(TCP_Conn* conn, void* data, uint count)
{
    return 0;
}
