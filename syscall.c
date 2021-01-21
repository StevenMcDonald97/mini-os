#include "syscall.h"
#include "print.h"
#include "debug.h"
#include "stddef.h"

// number of function we can use is 10 for now
static SYSTEMCALL system_calls[10];

static int sys_write(int64_t *argptr){
    write_screen((char*)argptr[0], (int)argptr[1], 0xe);  
    return (int)argptr[1];
}

void init_system_call(void){
    system_calls[0] = sys_write;
}

void system_call(struct TrapFrame *tf){
	// rax, rdi, rsi are values pushed on stack by trap
	// rax= index number of syscall
	int64_t i = tf->rax;
	// rdi= params count
	int64_t param_count = tf->rdi;
	// rsi= argument passed to function 
	int64_t *argptr = (int64_t*)tf->rsi;
	// check that request is valid 
	// if checks fail, return -1 in rax register 
	if (param_count <0 || i != 0){
		tf->rax = -1;
		return;
	}

	ASSERT(system_calls[i] != NULL);
	tf->rax=system_calls[i](argptr);

}

