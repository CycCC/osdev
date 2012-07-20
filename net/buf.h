// ------------------------------------------------------------------------------------------------
// net/buf.h
// ------------------------------------------------------------------------------------------------

#pragma once

#include "stdlib/link.h"

// ------------------------------------------------------------------------------------------------
// Net Buffer

#define NET_BUF_SIZE        2048
#define NET_BUF_START       256     // Room for various protocol headers + header below

typedef struct Net_Buf
{
    struct Net_Buf* next_buf;
    u8*             start;          // offset to data start
    u8*             end;            // offset to data end exclusive
} Net_Buf;

// ------------------------------------------------------------------------------------------------
// Functions

Net_Buf* net_alloc_buf();
void net_free_buf(Net_Buf* buf);

Net_Buf* net_create_send_buf();
