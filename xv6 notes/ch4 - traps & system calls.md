- three kinds of special events that force CPU to transfer control to special code
	- system call (user uses `ecall` to ask kernel to do smth for it)
	- exception (user or kernel does smth illegal)
	- device interrupt (a device signals that it needs attention)
- **trap:** a generic term for all three situations
- traps should be **transparent;** i.e. after trap is handled, previously running code should resume, without knowing that smth happened
- sequence:
	1. trap forces transfer of control into kernel
	2. kernel saves registers and other state so execution can be resumed later
	3. kernel executes appropriate handler code
	4. kernel restores saved state and returns from trap
	5. original code resumes
- xv6 trap handling 4 stages:
	1. hardware actions taken by the riscv CPU
	2. an assembly vector that prepares way for kernel C code
	3. a C trap handler that decides what to do w trap
	4. the system call or device-driver service routine
- three cases: **user space traps, kernel space traps, and timer interrupts**. separate assembly vectors and C trap handlers for each
## 4.1 - risc-v trap machinery
- each CPU has a set of control registers 
	- kernel writes to them to tell CPU how to handle traps
	- kernel can read to find out about a trap that has occurred
- important:
	- **`stvec`** - kernel writes **address of trap handler** here. riscv jumps here to handle a trap
	- **`sepc`** - riscv saves program counter here since `pc` gets overwritten (with `stvec`)
		- the **`sret`** (instruction) copies `sepc` back to `pc`. kernel can write to `sepc` to control where `sret` goes
	- **`scause`** - riscv puts a number here describing reason for trap
	- **`sscratch`** - kernel places value here that's useful for the start of a trap handler (transition from user to kernel mode)
	- **`sstatus`** - contains SIE bit that controls whether dev interrupts are enabled. if kernel clears SIE, riscv will defer dev ints until kernel sets it. SPP bit indicates if trap came from user or supervisor, and controls where `sret` returns
- more than one CPU can be handling a trap at a time
- ##### how riscv architecture forces a user or kernel space trap:
	1. if device interrupt, and SIE bit is clear, don't do any of the following
	2. disable interrupts by clearing SIE
	3. copy `pc` to **`sepc`**
	4. save current mode (user or supervisor) in SPP bit in `sstatus`
	5. set **`scause`** (cause of trap)
	6. set mode to supervisor
	7. copy **`stvec`** to **`pc`**
	8. start executing at new **`pc`**
- CPU doesn't:
	- switch to kernel page table
	- switch to a kernel stack
	- save any registers other than **`pc`**
- kernel software performs these tasks
	$\rightarrow$ flexibility - some OSs don't require page table switch in some situations
## 4.2 - traps from user space
- high-level path of trap from user space:
	- `uservec` (trampoline)
	- `usertrap`
	- on return: `usertrapret`
	- `userret`
- traps from user code are more challenging than from kernel since computer needs to be in supervisor mode $\rightarrow$ BUT riscv hardware doesn't switch page tables during trap
	- trap handler needs to explicitly switch the `satp` register to point to kernel page table
	- `satp`: Supervisor Address Translation and Protection (holds page table pointer)
	- `stvec` is a register that stores trap handler entry point. in user programs, `stvec` points to `uservec` (user mode trap handler)
	- to continue executing instructions after switch, `uservec` must be mapped at the same address in the kernel page table and every user page table
- solution: `TRAMPOLINE` page $\rightarrow$ allows for smooth transition from user to kernel mode. has the same virtual address in kernel page table and every user page table
- **sequence:**
	- kill me now