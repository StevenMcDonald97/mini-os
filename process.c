#include "process.h"
#include "trap.h"
#include "memory.h"
#include "print.h"
#include "lib.h"
#include "debug.h"

extern struct TSS Tss; 
static struct Process process_table[NUM_PROC];
static int pid_num = 1;
static struct ProcessControl pc;

static void set_tss(struct Process *proc){
    Tss.rsp0 = proc->stack + STACK_SIZE;    
}

// look through process table, and return an entry that is not 
// associated with a current process 
static struct Process* find_unused_process(void){
    struct Process *process = NULL;

    for (int i = 0; i < NUM_PROC; i++) {
        if (process_table[i].state == PROC_UNUSED) {
            process = &process_table[i];
            break;
        }
    }

    return process;
}

// set process structure given an unused process object
static void set_process_entry(struct Process *proc, uint64_t addr){
    uint64_t stack_top;

    proc->state = PROC_INIT;
    proc->pid = pid_num++;

	// allocate new page so each process has its own stack 
    proc->stack = (uint64_t)kalloc();
    ASSERT(proc->stack != 0);

    memset((void*)proc->stack, 0, PAGE_SIZE);     
	// stack grows downwards, so start at top
    stack_top = proc->stack + STACK_SIZE;

    // initialize process context 
    proc->context = stack_top - sizeof(struct TrapFrame) - 7*8;
    // 48 = size of 6 registers i.e. number of registers popped by swap
    // return to trap return (See trap) and jump to ring 3 
    *(uint64_t*)(proc->context + 6*8) = (uint64_t)TrapReturn;

	// trap frame for process is at top of kernel stack 
    proc->tf = (struct TrapFrame*)(stack_top - sizeof(struct TrapFrame)); 
	// load register values for process
    proc->tf->cs = 0x10|3;
    proc->tf->rip = 0x400000;
    proc->tf->ss = 0x18|3;
    proc->tf->rsp = 0x400000 + PAGE_SIZE;
    proc->tf->rflags = 0x202;
    
    proc->page_map = setup_kvm();
    ASSERT(proc->page_map != 0);
    ASSERT(setup_uvm(proc->page_map, (uint64_t)PHYSICAL_TO_VIRTUAL(0x20000), 5120));
    proc->state = PROC_READY;
}

static struct ProcessControl* get_pc(void){
    return &pc;
}

// find unsused process slot in process table and start process
void init_process(void){
    struct ProcessControl *process_control;
    struct Process *process;
    struct HeadList *list;
    uint64_t addr[2] = {0x20000, 0x30000};

    process_control = get_pc();
    list = &process_control->ready_list;

    for (int i = 0; i < 2; i++) {
        process = find_unused_process();
        set_process_entry(process, addr[i]);
        append_list_tail(list, (struct List*)process);
    }
}

void launch(void){
    struct ProcessControl *process_control;
    struct Process *process;

    process_control = get_pc();
    process = (struct Process*)remove_list_head(&process_control->ready_list);
    process->state = PROC_RUNNING;
    process_control->current_process = process;
    
    set_tss(process);
    switch_vm(process->page_map);
    pstart(process->tf);
}

static void switch_process(struct Process *prev, struct Process *current){
    set_tss(current);
    switch_vm(current->page_map);
    swap(&prev->context, current->context);
}

static void schedule(void){
    struct Process *prev_proc;
    struct Process *current_proc;
    struct ProcessControl *process_control;
    struct HeadList *list;

    process_control = get_pc();
    prev_proc = process_control->current_process;
    list = &process_control->ready_list;
    ASSERT(!is_list_empty(list));
    
    current_proc = (struct Process*)remove_list_head(list);
    current_proc->state = PROC_RUNNING;   
    process_control->current_process = current_proc;

    switch_process(prev_proc, current_proc);      
}

void yield(void){
    struct ProcessControl *process_control;
    struct Process *process;
    struct HeadList *list;
    
    process_control = get_pc();
    list = &process_control->ready_list;

    if (is_list_empty(list)) {
        return;
    }

    process = process_control->current_process;
    process->state = PROC_READY;
    append_list_tail(list, (struct List*)process);
    schedule();
}
