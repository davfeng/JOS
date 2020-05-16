#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

static volatile uint32_t start;

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *chosen = NULL;
	uint32_t i;
	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.
	// LAB 4: Your code here.
	if (curenv == thiscpu->idle)
		start = 0;
	else
		start = (curenv - &envs[0]) + 1;
	i = start;

	for(; i < NENV; i++){
		if ((envs[i].env_status == ENV_RUNNABLE) && (envs[i].env_cpunum == thiscpu->cpu_id)){
			chosen = &envs[i];
			break;
		} 
	}

	//
	// check the envs before curenv
	//
	if (chosen == NULL) {
		assert(i == NENV);
		for (i = 0; i < start; i++) {
			if ((envs[i].env_status == ENV_RUNNABLE) && envs[i].env_cpunum == thiscpu->cpu_id){
				chosen = &envs[i];
				break;
			}
		}
	}

	// cprintf("cpu%d start=%d, i=%d, chosen=%08x \n", thiscpu->cpu_id, start, i, chosen? chosen->env_id:0);
	if (chosen) {
		
		//
		// run the selected env
		//
		
		//cprintf("cpu%d curenv=%08x chosen=%08x\n", thiscpu->cpu_id, curenv, chosen);
		env_run(chosen);
		return;
	}

	// no environment is selected, if previous running process on this cpu is runnable, run it
	if ((curenv->env_status == ENV_RUNNING) || (curenv->env_status == ENV_RUNNABLE)) {

		//
		// idle process is not in the envs list
		// if current running process is idle process
		// need to keep the next round start position
		//

		if (curenv != thiscpu->idle){
			//cprintf("cpu%d run the running process %08x\n", thiscpu->cpu_id, curenv->env_id);
		}
		return;
	}

	//
	// run the idle process on this cpu because no runnable processes
	//
	//cprintf("cpu%d curenv=%08x run idle process\n", thiscpu->cpu_id, curenv->env_id);
	chosen = thiscpu->idle;
	if (curenv != chosen)
		env_run(chosen);
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	//cprintf("%s: %d\n" ,__func__, thiscpu->cpu_id);
	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING ||
		     envs[i].env_status == ENV_INTERRUPTIBLE))
			break;
	}
	if (i == NENV) {
		//cprintf("No runnable environments in the system! run idle process\n");
		//while (1)
		//	monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		// Uncomment the following line after completing exercise 13
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}

