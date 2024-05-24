#include "gdt.h"
#include "fb.h"

void gdt_set_entry(struct gdt_entry* gdt, int num, int base, int limit, char access, char granularity)
{
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= (granularity & 0xF0);
    gdt[num].access = access;
}

void gdt_init(struct gdt* gdt_descriptor, struct gdt_entry* gdt) {
    // Set up the GDT descriptor
    gdt_descriptor->size = (sizeof(struct gdt_entry) * 3) - 1;
    gdt_descriptor->address = (void*)gdt;

    // Set up the GDT entries
    // Null segment
    gdt_set_entry(gdt, 0, 0, 0, 0, 0);
    
    // Code segment
    gdt_set_entry(gdt, 1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    // Data segment
    gdt_set_entry(gdt, 2, 0, 0xFFFFFFFF, 0x92, 0xCF);
            
    // Load the GDT
    load_gdt(gdt_descriptor);
}