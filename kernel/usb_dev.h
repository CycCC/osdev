// ------------------------------------------------------------------------------------------------
// usb_dev.h
// ------------------------------------------------------------------------------------------------

#pragma once

#include "usb_desc.h"
#include "usb_req.h"

// ------------------------------------------------------------------------------------------------
// USB Limits

#define USB_STRING_SIZE                 127

// ------------------------------------------------------------------------------------------------
// USB Speeds

#define USB_FULL_SPEED                  0x00
#define USB_LOW_SPEED                   0x01
#define USB_HIGH_SPEED                  0x02

// ------------------------------------------------------------------------------------------------
// USB Device

typedef struct USB_Device
{
    struct USB_Device* parent;
    struct USB_Device* next;
    void* hc;
    void* drv;

    uint port;
    uint speed;
    uint addr;
    uint max_packet_size;
    uint endp_toggle;

    USB_IntfDesc intf_desc;
    USB_EndpDesc endp_desc;

    bool (*hc_reset)(struct USB_Device* dev);
    bool (*hc_transfer)(struct USB_Device* dev, USB_DevReq* req, void* data);
    bool (*hc_poll)(struct USB_Device* dev, uint len, void* data);

    void (*drv_poll)(struct USB_Device* dev);
} USB_Device;

// ------------------------------------------------------------------------------------------------
// Functions

USB_Device* usb_dev_create();
void usb_dev_init(USB_Device* dev);
void usb_dev_request(USB_Device* dev,
    uint type, uint request,
    uint value, uint index,
    uint len, void* data);
void usb_dev_get_langs(USB_Device* dev, u16* langs);
void usb_dev_get_string(USB_Device* dev, char* str, uint lang_id, uint str_index);
