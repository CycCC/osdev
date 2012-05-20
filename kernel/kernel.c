// ------------------------------------------------------------------------------------------------
// kernel.c
// ------------------------------------------------------------------------------------------------

#include "console.h"
#include "acpi.h"
#include "idt.h"
#include "ioapic.h"
#include "local_apic.h"
#include "int.h"
#include "net.h"
#include "pci.h"
#include "pic.h"
#include "pit.h"
#include "smp.h"
#include "string.h"
#include "usb.h"
#include "vga.h"
#include "vm.h"

// ------------------------------------------------------------------------------------------------
extern char __bss_start, __bss_end;

extern void pit_interrupt();
extern void spurious_interrupt();

static void interrupt_init();

// ------------------------------------------------------------------------------------------------
// !! This function must be the first in the file.
int kmain()
{
    memset(&__bss_start, 0, &__bss_end - &__bss_start);

    vga_text_init();
    console_init();
    console_print("Welcome!\n");

    vm_init();
    acpi_init();
    interrupt_init();
    pci_init();
    net_init();
    smp_init();

    for (;;)
    {
        usb_poll();
        net_poll();
    }

    return 0;
}

// ------------------------------------------------------------------------------------------------
static void interrupt_init()
{
    // Build Interrupt Table
    idt_init();
    idt_set_handler(INT_TIMER, INTERRUPT_GATE, pit_interrupt);
    idt_set_handler(INT_SPURIOUS, INTERRUPT_GATE, spurious_interrupt);

    // Initialize subsystems
    pic_init();
    lapic_init();
    ioapic_init();
    pit_init();

    // Enable IO APIC entries
    ioapic_set_entry(ioapic_address, acpi_remap_irq(IRQ_TIMER), INT_TIMER);

    // Enable all interrupts
    __asm__ volatile("sti");
}
