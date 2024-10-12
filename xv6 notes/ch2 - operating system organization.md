- OS jobs/responsibilities:
	- **multiplexing:** time-sharing resources of the computer
	- **isolation:** if one process malfunctions, it shouldn't affect other processes
	- **interaction:** must be possible for processes to communicate
- how is OS organized to allow it to achieve these requirements?
- xv6 has a monolithic kernel
- a process is the unit of isolation in xv6
- xv6 runs on a multi-core RISC-V microprocessor
- xv6 is written in LP64 C
	- long and pointers $\rightarrow$ 64 bits (8 bytes)
	- int $\rightarrow$ 32 bits (4 bytes)
## 2.1 - abstracting physical resources
- why do we need an OS in the first place?
- why not implement sys calls as a library to be called from applications?
- ans: applications aren't always well-behaved, could choose not to give up control of CPU. OS enforces time-sharing, security, etc
- if any application has bugs or smth, would affect the whole computer, so stronger isolation is required
- **solution**: prevent apps from directly accessing hw(hardware) resources. instead, **abstract** the resources into services for the applications
- **e.g. 1**: xv6 apps need `open, read, write, close` system calls to interact w file system - can't write to disk directly
	- apps get the convenience of filepaths, because addressing etc is left to OS
- **e.g. 2**: OS transparently switches hardware CPUs among processes, so apps aren't even aware of time sharing
	- allows CPUs to get shared even if an app is in infinite loop
- **e.g. 3**: unix processes use `exec` to build up memory image, instead of directly interacting w physical memory
	- allows OS to decide where to place a process in memory. 
	- also gives users the convenience of a file system to store program images
- file descriptors are very convenient
	- abstract away many details (e.g. where data in a pipe or file is stored)
	- also defined in a way to simplify interaction
		- e.g. if one app in a pipeline fails, kernel generates end-of-file signal for the next process
- the system-call interface is designed for both programmer convenience and strong isolation
## 2.2 - user mode, supervisor mode, system calls
- strong isolation requires a hard boundary bw apps and OS
	- e.g. if an app fails, OS should be able to clean it up and continue running other apps w/o issue
- OS makes following arrangements to achieve strong isolation:
	- apps can't modify or read OS's data structures and instructions
	- apps can't access other processes' memory
- RISC-V has three modes CPU can be in: **machine, supervisor, user** $\rightarrow$ this is HARDWARE
	- **machine:** instructions have full privilege; CPU starts here; mostly for configuration
	- **supervisor:** CPU can exec privileged instructions, e.g. controlling interrupts, reading and writing to registers, etc
- applications run in user space - can only exec user-mode instructions
- kernel runs in supervisor mode - in kernel space (supervisor mode)
- an app that wants to invoke a kernel function has to transition to the kernel
	- special RISC-V instruction: `ecall` switches CPU from user mode to supervisor mode. 
	- enters kernel at certain entry point (specified by kernel)
## 2.3 - kernel organization
- what part of the OS should run in supervisor mode? 
- in **monolithic kernels:** 
	- entire OS runs w full hardware privilege
	- easier for diff parts of OS to cooperate
	- downside: interfaces bw diff parts of the OS are complex, high possibility for mistake, resulting in **fatal error**
	- fatal error $\rightarrow$ cause kernel to fail, computer stops working. has to reboot to start again
- in **microkernels:** 
	- bulk of OS is in user mode, minimal code runs in supervisor mode
	- reduces risk of mistakes in the kernel
	- **servers** are OS services (to interact w file system, device drivers, etc) running as processes in user mode
	- for apps to interact w file server, kernel provides IPC (inter-process communication) mechanism to send msgs from one user-mode process to another
		- e.g. the shell wanting to read a file $\rightarrow$ sends a msg to file server and waits for response
	- kernel interface consists of a few low-level functions
		- starting applications, sending msgs, accessing hw, etc
		- allows kernel to be relatively simple as most of OS is in user-level servers
- xv6 is **monolithic**. kernel interface corresponds to the OS interface, and implements the complete operating system
## 2.4 - code
- kernel source is in the `kernel/` sub-directory
- inter-module interfaces are defined in `kernel/defs.h`
- fig 2.2 - xv6 kernel source files
	- ![[Pasted image 20240921105354.png]]
## 2.5 - process overview
- a process is the unit of isolation in xv6 (and other Unix OSes)
- prevents one process from wrecking or spying on another's memory, CPU, file descriptors, etc
	- also from wrecking kernel
- mechanisms kernel uses to implement processes:
	- user/supervisor mode flag
	- address spaces
	- time-slicing of threads
- process abstraction provides programs with illusion of its own machine 
	- private memory system (address space)
	- own CPU to execute its instructions
- xv6 uses **page tables** (implemented by hw) to give each process its own addr space
	- a RISC-V page table maps a virtual address (that a risc-v instruction manipulates) to a physical address (CPU chips sends to main memory)
- separate page table for each process
- fig 2.3 - layout of process's virtual address space
	- ![[Pasted image 20240921110742.png]]
- address space includes process's user memory at virt. addr zero
	1. instructions
	2. global variables
	3. stack
	4. heap (process can expand)
	5. **trapframe** (ch4)
	6. **trampoline** (ch4)
- Max size of Virtual Address space: `MAXVA` = $2^{38} - 1 = \text{0x3fffffffff}$ 
	- pointers on riscv are 64 bits
	- hardware only uses low 39 bits for virt. addresses
	- xv6 only uses 38 of those 39
- `proc` $\rightarrow$ a struct the kernel uses to store state of each process (state has many pieces)
	- notation: `p->pagetable` $\rightarrow$ a pointer to the process's page table
- a process's **thread** of execution can be suspended and later resumed
- to switch bw processes, kernel suspends currently running thread and resumes another process's thread
- the state of a thread (local variables, function call ret addr) is stored on thread's stacks
	`proc.h`: 
	![[Pasted image 20241001131952.png]]
- two stacks: user and kernel (`p->kstack`) (user stack is in page table)
	- when exec-ing user instructions, user stack is in use, kernel stack empty
	- kernel code execs on kernel stack. user stack still contains data but isn't actively being used
	- process's thread alternates bw using user stack and kernel stack
	- kernel stack is separate so kernel can execute even if process has wrecked its user stack
- process makes system call by executing RISC-V's `ecall` instruction. 
	- `ecall` raises hw privilege level, changes `pc` to kernel-defined entry point. 
	- code at entry point switches to kernel stack and execute kernel instructions
	- kernel calls `sret` instruction to lower hw priv level
- process's thread can "block" in the kernel to wait for I/O, and resume when I/O has finished
- `p->state` indicates if process is **allocated**, **ready to run**, **running**, **waiting** for I/O, or **exiting**
- `p->pagetable` holds page table. hardware uses process's page table when exec-ing it in user space. also serves as record of the addresses of physical pages allocated to store the process's memory
## 2.6 - code: starting xv6 and the first process
- when riscv computer powers on, it initializes itself and runs a boot loader (read-only)
- boot loader loads xv6 kernel into memory at physical address `0x80000000` (because addr range from `0x0` to there contains I/O devices so not free)
- in machine mode, CPU execs xv6 starting at `_entry.S` (machine language code). riscv starts w paging hw disabled; virt. addresses map directly to phys addresses
- `_entry:`
	- sets up stack for C code. xv6 declares space for initial stack `stack0` in `kernel/start.c`
	- loads stack pointer register `sp` w the address `stack0+4096` (top of the stack, stacks grow downwards on riscv)
	- finally, calls into C code at `start`
- `start:`
	- performs some config stuff that's only allowed in machine mode
	- `mret` is usually used to return from a call from supervisor mode to machine mode. $\rightarrow$ used here to enter supervisor mode for the first time by simulating a return 
		- in register `mstatus`, sets previous privilege mode to supervisor
		- in register `mepc`, sets return address to address of `kernel/main.c`
		- in page-table register `satp`, writes `0` to disable virt addr translation in supervisor mode
		- delegates all interrupts and exceptions to supervisor mode
		- last task: programs clock chip to generate timer interrupts
	- "returns" to (enters) supervisor mode (at `main`) by calling `mret`
- `main:`
	- initializes devices and subsystems
	- creates first process by calling `userinit` in `kernel/proc.c` which executes `user/initcode.S`
- `initcode:`
		- re-enters kernel using `exec` system call
- `exec:` 
	- replaces memory and registers of current process with a new program: `/init`
	- kernel "returns" to user space in the `/init` process
- `/init:` 
	- creates new console device file
	- opens it as `fd`s 0, 1, and 2
	- starts shell on console
