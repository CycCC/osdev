// ------------------------------------------------------------------------------------------------
// usb_dev.c
// ------------------------------------------------------------------------------------------------

#include "usb_dev.h"
#include "console.h"
#include "usb_driver.h"
#include "vm.h"

// ------------------------------------------------------------------------------------------------
static USB_Device* s_usb_dev_list;
static int s_next_addr = 1;

// ------------------------------------------------------------------------------------------------
USB_Device* usb_dev_create()
{
    // Initialize structure
    USB_Device* dev = vm_alloc(sizeof(USB_Device));
    if (dev)
    {
        dev->parent = 0;
        dev->next = s_usb_dev_list;
        dev->hc = 0;
        dev->drv = 0;

        dev->port = 0;
        dev->speed = 0;
        dev->addr = 0;
        dev->max_packet_size = 0;
        dev->endp_toggle = 0;

        dev->hc_reset = 0;
        dev->hc_transfer = 0;
        dev->hc_poll = 0;
        dev->drv_poll = 0;

        s_usb_dev_list = dev;
    }

    return dev;
}

// ------------------------------------------------------------------------------------------------
void usb_dev_init(USB_Device* dev)
{
    // Get first 8 bytes of device descriptor
    USB_DeviceDesc dev_desc;
    usb_dev_request(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_DEVICE << 8) | 0, 0,
        8, &dev_desc);

    dev->max_packet_size = dev_desc.max_packet_size;

    // Reset port following Windows behavior
    dev->hc_reset(dev);

    // Set address
    uint addr = s_next_addr++;

    usb_dev_request(dev,
        RT_HOST_TO_DEV | RT_STANDARD | RT_DEV,
        REQ_SET_ADDR, addr, 0,
        0, 0);

    dev->addr = addr;

    // Read entire descriptor
    usb_dev_request(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_DEVICE << 8) | 0, 0,
        sizeof(USB_DeviceDesc), &dev_desc);

    // Dump descriptor
    usb_print_device_desc(&dev_desc);

    // String Info
    u16 langs[USB_STRING_SIZE];
    usb_dev_get_langs(dev, langs);

    uint lang_id = langs[0];
    if (lang_id)
    {
        char product_str[USB_STRING_SIZE];
        char vendor_str[USB_STRING_SIZE];
        char serial_str[USB_STRING_SIZE];
        usb_dev_get_string(dev, product_str, lang_id, dev_desc.product_str);
        usb_dev_get_string(dev, vendor_str, lang_id, dev_desc.vendor_str);
        usb_dev_get_string(dev, serial_str, lang_id, dev_desc.serial_str);
        console_print("  Product='%s' Vendor='%s' Serial=%s\n", product_str, vendor_str, serial_str);
    }

    // Pick configuration and interface - grab first for now
    uint picked_conf_value = 0;
    USB_IntfDesc* picked_intf_desc = 0;
    USB_EndpDesc* picked_endp_desc = 0;

    for (uint conf_index = 0; conf_index < dev_desc.conf_count; ++conf_index)
    {
        u8 config_buf[256];

        // Get configuration total length
        usb_dev_request(dev,
            RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
            REQ_GET_DESC, (USB_DESC_CONF << 8) | conf_index, 0,
            4, config_buf);

        // Only static size supported for now
        USB_ConfDesc* conf_desc = (USB_ConfDesc*)config_buf;
        if (conf_desc->total_len > sizeof(config_buf))
        {
            console_print("  Configuration length %d greater than %d bytes",
                conf_desc->total_len, sizeof(config_buf));
            continue;
        }

        // Read all configuration data
        usb_dev_request(dev,
            RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
            REQ_GET_DESC, (USB_DESC_CONF << 8) | conf_index, 0,
            conf_desc->total_len, config_buf);

        usb_print_conf_desc(conf_desc);

        if (!picked_conf_value)
        {
            picked_conf_value = conf_desc->conf_value;
        }

        // Parse configuration data
        u8* data = config_buf + conf_desc->len;
        u8* end = config_buf + conf_desc->total_len;

        while (data < end)
        {
            u8 len = data[0];
            u8 type = data[1];

            switch (type)
            {
            case USB_DESC_INTF:
                {
                    USB_IntfDesc* intf_desc = (USB_IntfDesc*)data;
                    usb_print_intf_desc(intf_desc);

                    if (!picked_intf_desc)
                    {
                        picked_intf_desc = intf_desc;
                    }
                }
                break;

            case USB_DESC_ENDP:
                {
                    USB_EndpDesc* endp_desc = (USB_EndpDesc*)data;
                    usb_print_endp_desc(endp_desc);

                    if (!picked_endp_desc)
                    {
                        picked_endp_desc = endp_desc;
                    }
                }
                break;
            }

            data += len;
        }
    }

    // Configure device
    if (picked_conf_value && picked_intf_desc && picked_endp_desc)
    {
        usb_dev_request(dev,
            RT_HOST_TO_DEV | RT_STANDARD | RT_DEV,
            REQ_SET_CONF, picked_conf_value, 0,
            0, 0);

        dev->intf_desc = *picked_intf_desc;
        dev->endp_desc = *picked_endp_desc;

        // Initialize driver
        USB_Driver* driver = usb_driver_table;
        while (driver->init)
        {
            if (driver->init(dev))
            {
                break;
            }

            ++driver;
        }
    }
}

// ------------------------------------------------------------------------------------------------
void usb_dev_request(struct USB_Device* dev,
    uint type, uint request,
    uint value, uint index,
    uint len, void* data)
{
    USB_DevReq req;
    req.type = type;
    req.req = request;
    req.value = value;
    req.index = index;
    req.len = len;

    dev->hc_transfer(dev, &req, data);
}

// ------------------------------------------------------------------------------------------------
void usb_dev_get_langs(USB_Device* dev, u16* langs)
{
    u8 buf[256];
    USB_StringDesc* desc = (struct USB_StringDesc*)buf;

    // Get length
    usb_dev_request(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_STRING << 8) | 0, 0,
        1, desc);

    // Get lang data
    usb_dev_request(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_STRING << 8) | 0, 0,
        desc->len, desc);

    uint lang_len = (desc->len - 2) / 2;
    for (uint i = 0; i < lang_len; ++i)
    {
        langs[i] = desc->str[i];
    }

    langs[lang_len] = 0;
}

// ------------------------------------------------------------------------------------------------
void usb_dev_get_string(USB_Device* dev, char* str, uint lang_id, uint str_index)
{
    if (!str_index)
    {
        str[0] = '\0';
        return;
    }

    u8 buf[256];
    USB_StringDesc* desc = (struct USB_StringDesc*)buf;

    // Get string length
    usb_dev_request(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_STRING << 8) | str_index, lang_id,
        1, desc);

    // Get string data
    usb_dev_request(dev,
        RT_DEV_TO_HOST | RT_STANDARD | RT_DEV,
        REQ_GET_DESC, (USB_DESC_STRING << 8) | str_index, lang_id,
        desc->len, desc);

    // Dumb Unicode to ASCII conversion
    uint str_len = (desc->len - 2) / 2;
    for (uint i = 0; i < str_len; ++i)
    {
        str[i] = desc->str[i];
    }

    str[str_len] = '\0';
}

// ------------------------------------------------------------------------------------------------
void usb_poll()
{
    for (USB_Device* dev = s_usb_dev_list; dev; dev = dev->next)
    {
        if (dev->drv_poll)
        {
            dev->drv_poll(dev);
        }
    }
}
