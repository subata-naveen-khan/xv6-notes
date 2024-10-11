- page tables are the mechanism thru which the OS provides each process w its own private address space and memory
	- determine what memory addresses mean
	- + what parts of phys memory can be accessed
	- allow xv6 to isolate diff process's address spaces and multiplex them onto a single physical memory
- provide a level of indirection $\rightarrow$ enables xv6 to perform a few tricks
	- mapping the same memory (trampoline page) in several address spaces
	- guarding kernel and user stacks w an unmapped page
## 3.1 - paging hardware
- 