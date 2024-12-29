- interleaving: executing programs together
	- multiple CPUs (i.e. risc-v)
- concurrency: interleaving multiple instruction streams via multiprocessor parallelism, thread switching, or interrupts
- e.g. `kalloc` - could be called by 2 CPUs at the same time
- concurrency control techniques:
	- locks
	- others
- **locks** - provide mutual exclusion (only one CPU can hold a lock at a time, i.e. the item associated with the lock can only be used by one CPU at a time) - "protects" the data item
- problem with locks is they can kill performance, coz they serialize concurrent operations
## 6.1 - race conditions
- e.g. 2 processes calling `wait` on 2 diff CPUs. 
	- `wait` frees the child's memory. so the kernel on each CPU will call `kfree` to free children's pages. 
	- kernel allocator maintains linked list of free pages. `kalloc()` pops memory page from list, `kfree()` pushes page onto list
	- ideally, the 2 `kfree`s would execute in parallel. but that would fuck stuff up
	- linked list is in memory (shared by both CPUs). the `push` operation would work if executed in isolation, but not if both are executed at tthe same time. first update will be overwritten by the second. this is a race condition called **lost update**
- **race condition:** situation where a memory location is accessed concurrently, and at least one access is a write. 
	- lost update (two writes)
	- a read of an incompletely-updated data structure (one read, one write)
- **locks** provide **mutual exclusion**, so only one CPU at a time can exec sensitive lines of code.
- e.g.
	```
	struct lock listlock;
	
	acquire(&listlock);
	// critical section code 
	release(&listlock);
	```
- **critical section:** the code between the `acquire` and `release` . the lock **protects** the data being manipulated in the critical section
	- protects date $\rightarrow$ protects collection of **invariants** that apply to the data
	- invariants $\rightarrow$ properties of data structures that are maintained across operations
	- operation's correct behaviour depends on invariants being true when op starts
- lock **serializes** concurrent critical sections so they run one at a time $\rightarrow$ preserve invariants
- critical sections guarded by the same lock become **atomic** to each other. only see complete set of changes or none at all $\rightarrow$ no partially-completed updates
- locks limit performance
	- e.g. 2 processes calling `kfree` concurrently, the calls will be serialized, so no point in running them on diff CPUs
	- **conflict** or **contention:** when multiple processes want the same lock at the same time
	- avoiding lock contention is a major challenge in sophisticated OSes
	- placement of locks affects performance
## 6.2 - code: 
- 2 types of locks in xv6: spinlocks and sleep-locks
- `struct spinlock` (kernel/spinlock.h)
	```
	struct spinlock {
		uint locked;       // Is the lock held?
	
		char *name;        // Name of lock.
		struct cpu *cpu;   // The cpu holding the lock.
	};
	```
- naive approach:
	```
	void acquire (struct spinlock *lk) {
		for (;;) {                         // basically while(true)
			if lk -> locked == 0) {
				lk -> locked = 1;
				break;
			}
		}
	}
	```
	- ^ **doesn't work** coz 2 diff CPUs could see `lk->locked` is 0 at the same time, both grab the lock, violating mut exc property
	- need to execute lines as a single atomic step
- multi-core processors usually have instructions to implement atomic lock acquiring
	- on riscv: `amoswap r, a`
	- reads value at mem addr `a`and writes contents of register `r` at that addr, and puts the address into `r` 
		- $\rightarrow$ swaps contents of register and mem addr
	- happens atomically (in hardware mode ?)
- xv6's `acquire` uses `amoswap` 
	- return value is the old contents of `lk->locked`
	- `acquire` function wraps `amoswap` in a loop, retrying until it has acquired the lock
	- keeps putting `1` into lock and checking previous value. if `1`, lock is still held. if `0`, lock has been acquired
	- which CPU is holding the lock is recorded in `lk->cpu`. protected, can only be changed while holding lock
- `release` is the opposite 
	- clears `lk->cpu` field, then releases lock (assign `0` to `lk->locked`)
	- same `amoswap` instruction is used
## 6.2 - code: using locks
- a challenge with using locks: deciding how many locks to use, which data and invariants each should protect
- basic principles of when locks are necessary:
	1. any time a variable can be written by a CPU while another can read or write it, use a lock
	2. locks protect invariants; if an invariant involves mult. memory locations, all need to be protected by a single lock
- avoiding unnecessary locks is important for efficiency; they reduce parallelism
	- if parallelism isn't important, using a "big kernel lock" (only one CPU can enter the kernel at a time) is possible.
- `kalloc` is an example of **coarse-grained locking** $\rightarrow$ simpler but can have performance bottlenecks
	- single lock for multiple resources
	- problem w multiple CPUs: only one process can proceed at a time coz everything is locked
	- processes spinning in `acquire`, wasted CPU cycles
- **fine-grained locking** (e.g. file locks) $\rightarrow$ enhances concurrency but adds complexity
	- processes acting on different files can proceed w/o interference
## 6.4 - deadlock and lock ordering
- figure 6.3: locks in xv6
	![[Pasted image 20241229134519.png]]
- **global lock acquisition order:** if a code path thru kernel has to hold several locks at the same time, then all code paths acquiring those locks must acquire them in the **same order** otherwise risk of deadlock
	- e.g. T1 acquires lock A, T2 acquires lock B. T1 waiting for lock B indefinitely, T2 waiting for lock A indefinitely $\rightarrow$ **deadlock**
- locks are part of each function's specification: callers must invoke functions in a way that locks are acquired in the same order
- 