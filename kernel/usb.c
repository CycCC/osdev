// ------------------------------------------------------------------------------------------------
// usb.c
// ------------------------------------------------------------------------------------------------

#include "usb.h"
#include "usb_controller.h"
#include "usb_dev.h"

// ------------------------------------------------------------------------------------------------
void usb_poll()
{
    for (USB_Controller* c = g_usb_controller_list; c; c = c->next)
    {
        if (c->poll)
        {
            c->poll(c);
        }
    }

    for (USB_Device* dev = g_usb_dev_list; dev; dev = dev->next)
    {
        if (dev->drv_poll)
        {
            dev->drv_poll(dev);
        }
    }
}
