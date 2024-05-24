#ifndef INCLUDE_K_H
#define INCLUDE_K_H

struct gdt {
    unsigned short size;
    void* address;
} __attribute__((packed));

struct gdt_entry {
    short limit_low;
    short base_low;
    char base_middle;
    char access;
    char granularity;
    char base_high;
} __attribute__((packed));

void load_gdt(struct gdt* gdt);

void gdt_init(struct gdt* gdt_descriptor, struct gdt_entry* gdt);

void gdt_set_entry(struct gdt_entry* gdt, int num, int base, int limit, char access, char granularity);

#endif /* INCLUDE_K_H */