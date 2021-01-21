// A class to track and manage memory usage by the OS
// kernel virutal address is at 0xffff800000000000
#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

struct E820 {
    uint64_t address;
    uint64_t length;
    uint32_t type;
} __attribute__((packed));

struct FreeMemRegion {
    uint64_t address;
    uint64_t length;
};

struct Page {
    struct Page* next;
};

// used to find translation table
typedef uint64_t PDE;
// points to page directory entry 
typedef PDE* PD;
// points to page directory 
typedef PD* PDPTR;

// P,W, and U are attributes of table entries( i.e. first, second or third bit are 1)
#define PTE_P 1
#define PTE_W 2
#define PTE_U 4
#define PTE_ENTRY 0x80
#define KERNEL_BASE 0xffff800000000000
#define PAGE_SIZE (2*1024*1024)

#define PA_UP(v) ((((uint64_t)v + PAGE_SIZE-1) >> 21) << 21)
#define PA_DOWN(v) (((uint64_t)v >> 21) << 21)
#define PHYSICAL_TO_VIRTUAL(p) ((uint64_t)(p) + KERNEL_BASE)
#define VIRTUAL_TO_PHYSICAL(v) ((uint64_t)(v) - KERNEL_BASE)
#define PDE_ADDR(p) (((uint64_t)p >> 12) << 12)
// clear 21 bits to get physical address
#define PTE_ADDR(p) (((uint64_t)p >> 21) << 21)

void* kalloc(void);
void kfree(uint64_t v);
void init_memory(void);
void init_kvm(void);
bool map_pages(uint64_t map, uint64_t v, uint64_t e, uint64_t pa, uint32_t attribute);
void switch_vm(uint64_t map);
void load_cr3(uint64_t map);
void free_vm(uint64_t map);
void free_page(uint64_t map, uint64_t v, uint64_t e);
bool setup_uvm(uint64_t map, uint64_t start, int size);
uint64_t setup_kvm(void);

#endif