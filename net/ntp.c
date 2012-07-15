// ------------------------------------------------------------------------------------------------
// net/ntp.c
// ------------------------------------------------------------------------------------------------

#include "net/ntp.h"
#include "net/buf.h"
#include "net/net.h"
#include "net/port.h"
#include "net/swap.h"
#include "net/udp.h"
#include "console/console.h"
#include "time/rtc.h"
#include "time/time.h"

// ------------------------------------------------------------------------------------------------
// NTP Constants

#define NTP_VERSION                     4
#define UNIX_EPOCH                      0x83aa7e80

// ------------------------------------------------------------------------------------------------
// NTP Packet

typedef struct NTP_Header
{
    u8 mode;
    u8 stratum;
    u8 poll;
    i8 precision;
    u32 root_delay;
    u32 root_dispersion;
    u32 ref_id;
    u64 ref_timestamp;
    u64 orig_timestamp;
    u64 rx_timestamp;
    u64 tx_timestamp;
} PACKED NTP_Header;

// ------------------------------------------------------------------------------------------------
// NTP Modes

#define MODE_CLIENT                     3
#define MODE_SERVER                     4

// ------------------------------------------------------------------------------------------------
void ntp_rx(Net_Intf* intf, const u8* pkt, const u8* end)
{
    ntp_print(pkt, end);

    if (pkt + sizeof(NTP_Header) > end)
    {
        return;
    }

    NTP_Header* hdr = (NTP_Header*)pkt;

    abs_time t = (net_swap64(hdr->tx_timestamp) >> 32) - UNIX_EPOCH;
    DateTime dt;
    split_time(&dt, t, tz_local);

    char str[TIME_STRING_SIZE];
    format_time(str, sizeof(str), &dt);
    console_print("Setting time to %s\n", str);

    rtc_set_time(&dt);
}

// ------------------------------------------------------------------------------------------------
void ntp_tx(const IPv4_Addr* dst_addr)
{
    NetBuf* buf = net_alloc_packet();
    u8* pkt = (u8*)(buf + 1);

    NTP_Header* hdr = (NTP_Header*)pkt;
    hdr->mode = (NTP_VERSION << 3) | MODE_CLIENT;
    hdr->stratum = 0;
    hdr->poll = 4;
    hdr->precision = -6;
    hdr->root_delay = net_swap32(1 << 16);
    hdr->root_dispersion = net_swap32(1 << 16);
    hdr->ref_id = net_swap32(0);
    hdr->ref_timestamp = net_swap64(0);
    hdr->orig_timestamp = net_swap64(0);
    hdr->rx_timestamp = net_swap64(0);
    hdr->tx_timestamp = net_swap64(0);

    u8* end = pkt + sizeof(NTP_Header);
    uint src_port = net_ephemeral_port();

    ntp_print(pkt, end);
    udp_tx(dst_addr, PORT_NTP, src_port, pkt, end);
}

// ------------------------------------------------------------------------------------------------
void ntp_print(const u8* pkt, const u8* end)
{
    if (~net_trace & (1 << 3))
    {
        return;
    }

    if (pkt + sizeof(NTP_Header) > end)
    {
        return;
    }

    NTP_Header* hdr = (NTP_Header*)pkt;

    abs_time t = (net_swap64(hdr->tx_timestamp) >> 32) - UNIX_EPOCH;

    console_print("   NTP: mode=%02x stratum=%d poll=%d precision=%d\n",
        hdr->mode, hdr->stratum, hdr->poll, hdr->precision);
    console_print("   NTP: root_delay=%08x root_dispersion=%08x ref_id=%08x\n",
        net_swap32(hdr->root_delay), net_swap32(hdr->root_dispersion), net_swap32(hdr->ref_id));
    console_print("   NTP: ref_timestamp=%016x orig_timestamp=%016x\n",
        net_swap64(hdr->ref_timestamp), net_swap64(hdr->orig_timestamp));
    console_print("   NTP: rx_timestamp=%016x tx_timestamp=%016x\n",
        net_swap64(hdr->rx_timestamp), net_swap64(hdr->tx_timestamp));
    console_print("   NTP: unix_epoch=%u\n",
        t);
}

