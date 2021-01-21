#include "memory.h"
#include "print.h"
#include "debug.h"
#include "lib.h"
#include "stddef.h"
#include "stdbool.h"

static void free_region(uint64_t v, uint64_t e);

static struct FreeMemRegion free_mem_region[50];
static struct Page free_memory;
static uint64_t memory_end;
extern char end;

void init_memory(void)
{
    int32_t count = *(int32_t*)0x9000;
    uint64_t total_mem = 0;
	// E820 -> shorhand for how BIOS reports memory map
    struct E820 *mem_map = (struct E820*)0x9008;	
    int free_region_count = 0;

	ASSERT(count <= 50);

	for(int32_t i = 0; i<count;i++){
		// free memory region has type 1 
		// if free copy to free region structure
        if(mem_map[i].type == 1) {			
            free_mem_region[free_region_count].address = mem_map[i].address;
            free_mem_region[free_region_count].length = mem_map[i].length;
            total_mem += mem_map[i].length;
            free_region_count++;
        }
        printk("%x  %uKB  %u\n",mem_map[i].address,mem_map[i].length/1024,(uint64_t)mem_map[i].type);
	}
	// collect free memory pages
	for (int i=0; i<free_region_count; i++){
		uint64_t virtual_start = PHYSICAL_TO_VIRTUAL(free_mem_region[i].address);
		uint64_t virtual_end = virtual_start + free_mem_region[i].length;
		// if start of memory region is after the kernel starts (&end-> address of kernel, end is a symbol)
		if (virtual_start > (uint64_t)&end) {
            free_region(virtual_start, virtual_end);
		// else compare end of region with end of kernel. It larger, intialize from end of kernel to end of region
		} else if (virtual_end > (uint64_t)&end){
			free_region((uint64_t)&end, virtual_end);
		}

	}

    memory_end = (uint64_t)free_memory.next+PAGE_SIZE;   
    printk("Memory end: %x\n",memory_end);

}

static void free_region(uint64_t v, uint64_t e)
{
    for (uint64_t start = PA_UP(v); start+PAGE_SIZE <= e; start += PAGE_SIZE) {        
        if (start+PAGE_SIZE <= 0xffff800040000000) {            
           kfree(start);
        }
    }
}

void kfree(uint64_t vrtl){
	// check is valid page address (page aligned)
	ASSERT(vrtl % PAGE_SIZE == 0);
	// check the address is after the kernel space
    ASSERT(vrtl >= (uint64_t) & end);
	// check that the page size is not larger than 1 gb
    ASSERT(vrtl+PAGE_SIZE <= 0xffff800040000000);

	// copy head of lsit to first 8 bytes of page, then save to memory
    struct Page *page_address = (struct Page*)vrtl;
    page_address->next = free_memory.next;
    free_memory.next = page_address;
}

void* kalloc(void){
	struct Page *page_address=free_memory.next;
	// page_Address points to the next area of free memory
    if (page_address != NULL) {
        ASSERT((uint64_t)page_address % PAGE_SIZE == 0);
        ASSERT((uint64_t)page_address >= (uint64_t)&end);
        ASSERT((uint64_t)page_address+PAGE_SIZE <= 0xffff800040000000);

        free_memory.next = page_address->next;            
    }
    
    return page_address;
}

// find a sepcfic entry in the pml4t table for a virtual address 
// note: PDPTR refers to Page Directory Pointer Table Register
static PDPTR find_pml4t_entry(uint64_t map, uint64_t virtual, int alloc, uint32_t attribute){
    PDPTR *map_entry = (PDPTR*)map;
    PDPTR pdptr = NULL;
    unsigned int index = (virtual >> 39) & 0x1FF;
	// if page is present, set pdptr to virtual address of the entry
    if ((uint64_t)map_entry[index] & PTE_P) {
        pdptr = (PDPTR)PHYSICAL_TO_VIRTUAL(PDE_ADDR(map_entry[index]));       
    } 
    else if (alloc == 1) {
        pdptr = (PDPTR)kalloc();          
        if (pdptr != NULL) {     
            memset(pdptr, 0, PAGE_SIZE);     
            map_entry[index] = (PDPTR)(VIRTUAL_TO_PHYSICAL(pdptr) | attribute);           
        }
    }

	return pdptr;
}

// find endtry in page directory entry table which points to page directory table
// takes page map table, virtual address, alloc wether we should make non existing page or not 
static PD find_pdpt_entry(uint64_t map, uint64_t v, int alloc, uint32_t attribute){
	PDPTR pdptr = NULL;
    PD pd = NULL;
    unsigned int index = (v >> 30) & 0x1FF;
	// find pointer to page directory table
    pdptr = find_pml4t_entry(map, v, alloc, attribute);
    if (pdptr == NULL)
        return NULL;
	// if page exists, convert physical address to pd entry 
    if ((uint64_t)pdptr[index] & PTE_P) {      
        pd = (PD)PHYSICAL_TO_VIRTUAL(PDE_ADDR(pdptr[index]));      
    }
	// else if alloc is set allocate a new page and have entry point to this page
    else if (alloc == 1) {
        pd = (PD)kalloc();  
        if (pd != NULL) {    
            memset(pd, 0, PAGE_SIZE);       
            pdptr[index] = (PD)(VIRTUAL_TO_PHYSICAL(pd) | attribute);
        }
    } 

    return pd;
}

// map pages in page directory pointer table
bool map_pages(uint64_t map, uint64_t v, uint64_t e, uint64_t pa, uint32_t attribute){
	// align address with start and end of 2mb pages
    uint64_t vstart = PA_DOWN(v);
    uint64_t vend = PA_UP(e);
    PD pd = NULL;
    unsigned int index;

	// check if end address is after start of virtual address
    ASSERT(v < e);
	// make sure physical address is page aligned
    ASSERT(pa % PAGE_SIZE == 0);
	// make sure end of space is insize 1gb of physical memory
    ASSERT(pa+vend-vstart <= 1024*1024*1024);

	// find pages until end of virutal memory is reached 
	do {
		// find pointer to page entry table 
        pd = find_pdpt_entry(map, vstart, 1, attribute);    
        if (pd == NULL) {
            return false;
        }
		// find index of correct page entry. Page directory entry is at index 21
        index = (vstart >> 21) & 0x1FF;
        ASSERT(((uint64_t)pd[index] & PTE_P) == 0);

        pd[index] = (PDE)(pa | attribute | PTE_ENTRY);

        vstart += PAGE_SIZE;
        pa += PAGE_SIZE;
    } while (vstart + PAGE_SIZE <= vend);
  
    return true;
}

// load cr3 register with pml4t table 
void switch_vm(uint64_t map){
	printk("\n\n %u\n\n",VIRTUAL_TO_PHYSICAL(map));
	load_cr3(VIRTUAL_TO_PHYSICAL(map));
}

uint64_t setup_kvm(void){
	// new page memory level 4 table
    uint64_t page_map = (uint64_t)kalloc();
	if(page_map != 0){
        // initilalize page values to 0
        memset((void*)page_map, 0, PAGE_SIZE);        
        // pass in pml4t table, then start and end of memory, then start of physical addresses we want to map to
        if(!map_pages(page_map, KERNEL_BASE, memory_end, VIRTUAL_TO_PHYSICAL(KERNEL_BASE), PTE_P|PTE_W)){
            free_vm(page_map);
            page_map = 0;
        }
    }
    return page_map;

}

// load cr3 register with new translation table to allow mapping
void init_kvm(void)
{
    uint64_t page_map = setup_kvm();
    ASSERT(page_map != 0);
    switch_vm(page_map);
    printk("memory manager is working now");
}

// prepare virtual memory
bool setup_uvm(uint64_t map, uint64_t start, int data_size){
    bool status = false;
    void *page = kalloc();
    // if successfully allocated page
    if(page != NULL){
        // zero out memory
        memset(page, 0, PAGE_SIZE);
        status = map_pages(map, 0x400000, 0x400000+PAGE_SIZE, VIRTUAL_TO_PHYSICAL(page), PTE_P|PTE_W|PTE_U);
        if (status == true) {
            memcpy(page, (void*)start, data_size);
        }
        else {
            // if status is faalse, free pages and tables
            kfree((uint64_t)page);
            free_vm(map);
        }
    }
    
    return status;
}

// tell OS that a page(s) are free
void free_pages(uint64_t map, uint64_t vstart, uint64_t vend){
    unsigned int index;
    // check that vstart, vend are page aligned 
    ASSERT(vstart % PAGE_SIZE ==0);
    ASSERT(vend % PAGE_SIZE == 0);
    do {
        //  set alloc to 0 (third argument), only free if page exists 
        PD pd = find_pdpt_entry(map, vstart, 0, 0);

        if (pd != NULL){
            // index into page directory starts at 21st bit of the address
            index = (vstart >> 21) & 0x1FE;
            if(pd[index]& PTE_P){
                kfree(PHYSICAL_TO_VIRTUAL(PTE_ADDR(pd[index])));
                // indicate entry is unused
                pd[index] = 0;
            };
        }
        // go to next page, repeat
        vstart += PAGE_SIZE;
    } while (vstart+PAGE_SIZE <= vend);
}

// free all page directory table entries
static void free_pdt(uint64_t map){
    // address of table
    PDPTR *map_entry = (PDPTR*) map;

    for (int i=0; i<512; i++){
        if ((uint64_t)map_entry[i] & PTE_P){
            PD *pdptr = (PD*)PHYSICAL_TO_VIRTUAL(PDE_ADDR(map_entry[i]));

            for (int j=0; j<512; j++){
                if ((uint64_t)pdptr[j] & PTE_P){
                    if ((uint64_t)pdptr[j] & PTE_P){
                        kfree(PHYSICAL_TO_VIRTUAL(PDE_ADDR(pdptr[j])));
                        pdptr[j] = 0;
                    }
                }
            }
        }
    }
}

// free all page directory pointer table entries
static void free_pdpt(uint64_t map){
    // address of table
    PDPTR *map_entry = (PDPTR*) map;

    for (int i=0; i<512; i++){
        if ((uint64_t)map_entry[i] & PTE_P){
            kfree(PHYSICAL_TO_VIRTUAL(PDE_ADDR(map_entry[i])));
            map_entry[i] = 0;
        }
    }
}

// free the pml4t table
static void free_pml4t(uint64_t map){
    kfree(map);
}

// free virtual memory 
void free_vm(uint64_t map){
    free_pages(map,0x400000,0x400000+PAGE_SIZE);
    free_pdt(map);
    free_pdpt(map);
    free_pml4t(map);
}







