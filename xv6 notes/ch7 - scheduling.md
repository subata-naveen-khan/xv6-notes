## 7.1 - multiplexing
- xv6 multiplexes in two situations:
	- `sleep` and `wakeup` mechanism when a process  is waiting for smth
	- forced switch to cope w processes that computer for long periods
- challenges w multiplexing:
	- **how to switch** from one to another?
	- how to **force switches transparently** (to user processes)? (ans: timer interrupts)
	- locking plan to **avoid races**?
	- process's memory and other **resources need to be freed** when process exits, but it can't do that itself (e.g. can't free kernel stack while still using it)
	- each **core must remember which process** it's executing so syscalls affect the correct process's kernel state
	- need to avoid races which result in loss of **wakeup notifications**
## 7.2 - code: context switching
- fig 7.1: switching from one user process to another
	![[Pasted image 20241119135326.png]]
	steps:
	- **save:** user-kernel transition (syscall or interrupt) to old process's kernel thread
	- **swtch:** context switch to current CPU's scheduler thread
	- **swtch:** context switch to new process's kernel thread
	- **restore:** trap return to user-level process
- switching from one thread to another involves:
	- saving old thread's CPU registers
	- restoring previously-saved registers of new thread
	- stack pointer and program pointer are included too meaning CPU switches stacks and code it's running also
- the `swtch` function performs saves and restores for a kernel thread switch. doesn't know directly about threads, just saves and restores **contexts** (register sets)
- when time for process to give up CPU, it's kernel thread calls `swtch` to save its own context and return to the schedulers
- context is contained in `struct context` (kernel/proc.h)
	```
	struct context {
		uint64 ra;
		uint64 sp;
		
		// callee-saved
		uint64 s0;
		uint64 s1;
		.
		.
		.
		uint64 s10;
		uint64 s11;
	
	};
	```
- `context` is contained in a process's `struct proc`or a CPU's `struct cpu`
- `swtch` takes arguments: pointers of the old and new threads' contexts. saves current registers in `old`, loads registers from `new`, and returns.
	- **NOT:** only callee-saved registers are saved in the context. the caller-saved registers and the user-mode state are saved on the kernel stack (`&sp`) 
	- ![[Pasted image 20241229214356.png]]
## 7.3 - code: scheduling
- the scheduler is a special thread on each CPU that is running the `scheduler` function (proc.c:445). the function chooses which process to run next
- a process giving up the CPU must:
	1. acquire its own process lock `p->lock`
	2. release any other locks its holding
	3. update its state `p->state`
	4. call `sched`
	- e.g. `yield`, `sleep`, `exit` all follow this convention
- `sched()` double checks those conditions, and whether interrupts are disabled. then calls `swtch` to save context and switch to the scheduler context in `cpu->scheduler`
- `swtch` returns on scheduler's stack. scheduler continues `for` loop, finds process to run, switches to it
- **HELP???** idk what this means (page 3/69)
	![[Pasted image 20241229232553.png]]
