#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

static uint32_t start;
struct spinlock sched_lock = {
#ifdef DEBUG_SPINLOCK
	.name = "sched_lock"
#endif
};
// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle = NULL;
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
	i = start;
	spin_lock(&sched_lock);
	for(; i < NENV; i++){
		if ((envs[i].env_status == ENV_RUNNABLE)){
			idle = &envs[i];
			break;
		} 
	}
	//check the envs before the curr
	if(idle == NULL){
		assert(i == NENV);
		for(i = 0; i < start; i++){
			if ((envs[i].env_status == ENV_RUNNABLE)){
				idle = &envs[i];
				break;
			}
		}
	}
	//cprintf("%s: start=%d, i=%d, idle=0x%x, &envs[i]=0x%x\n", __func__, start,i,idle,&envs[i]);
	if(idle){
		if (i == NENV-1)
			start = 0;
		else
			start = i+1;

		//set the curenv as runnable
		if ((curenv && (curenv->env_status == ENV_RUNNING)))
			curenv->env_status = ENV_RUNNABLE;

		env_run(idle);
	}

	//no environment is selected, if previous running process on this cpu is runnable, run it
	if (curenv && curenv->env_status == ENV_RUNNING){
		start = thiscpu->cpu_env - &envs[0] + 1;

		env_run(thiscpu->cpu_env);
	}

	spin_unlock(&sched_lock);
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	spin_lock(&sched_lock);
	cprintf("%s: %d\n" ,__func__, thiscpu->cpu_id);
	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	spin_unlock(&sched_lock);
	// Release the big kernel lock as if we were "leaving" the kernel
	//cprintf("%s before unlock_kernel\n", __func__);
	//unlock_kernel();

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

