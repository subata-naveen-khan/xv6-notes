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
- 