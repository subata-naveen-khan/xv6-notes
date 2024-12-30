- **driver**: code in an OS that manages a device 
	- configures device hardware
	- tells device to perform operations
	- handles interrupts
	- interacts w processes waiting for I/O from device
- kernel trap handler recognises when an interrupt is from a device, and calls its driver's trap handler (`devintr` in `kernel\trap.c`)
- driver executes concurrently with the device it's managing $\rightarrow$ code can be tricky
- drivers execute code in 2 contexts:
	- **top half** (runs in process's kernel thread); called via system calls; asks to start operation; waits for it to complete, then raises interrupt
	- **bottom half** (runs at interrupt time (interrupt handler)) figures out what operation was completed, wakes up waiting process, starts work on any waiting operation
## 5.1 - code: console input
- `console.c` $\rightarrow$ console driver; simple driver structure
	- accepts characters typed by human, via UART (Universal Asynchronous Receiver-Transmitter) serial-port hardware
	- driver accumulates a line of input at a time, processing special input chars
	- user processes use the `read` syscall to fetch lines of input from console
- the UART hardware appears to software as a set of **memory-mapped control registers** $\rightarrow$ there are some physical addresses connected to the UART device.
---
# AZKA LECTURE !!!
- UART manages I/O
- some phys addresses are connected to UART, so loads and stores interact w device hardware, no need for RAM
- address starts at 0x10000000, UART0
- registers: `lsr`, `rhr`, `thr`
- `lsr` (line status register) maintains buffer that stores input before it sends to the software
	- smth is in buffer, LSR bit is 1, otherwise 0
- FIFO hardware queue. RHR dequeues one character at a time
- THR transmitter holding register. for printing. UART reads from THR to see what char to print next
### console input
1. type into console
2. UART raises interrupt for each byte (char), stores chars in queue, LSR bit 1
3. kernel raises trap
4. trap.c $\rightarrow$ device interrupt
	check scause for cause (external device)
	PLIC is hardware that tells which device called the interrupt, calls its handler `uartintr`
5. read from RHR, call console interrupt
6. console interupt:
	- adds chars into consbuffer until it detects newline, then calls consoleread
	- treats backspace and some other characters specially
### console read
- shell is continuously reading user input (read syscall)
- route read bit to console read
- reads from cons buffer, returns to user space. if no characters, put process to sleep. when console is ready, wake up
- consoleint() is in main.c
### console output
- uartputc(). also has buffer uart_tx_buffer (tx=transmission)
- if buffer full, sleep until space in buffer
- uartstart()
- interrupt to say ke input write hogaya hai
### concurrency issues
- race condition $\rightarrow$ spinlock
- `consoleintr()` triggered while inside `consoleread()` $\rightarrow$ spinlock again
- consolebuffer allows one CPU at a time
### timer interrupts
- system needs a clock
- process switching needs clock
- each CPU has its own timer hardware (CLINT in xv6) independent but in sync coz same clock source
- timer interrupts start in machine mode and move to supervisor mode
- `mtime`, `mtimecmp` registers. (cmp = compare)
- system initialization in start.c
	- enables sup mode access to time control regs
	- programs timer hardware to deliver first timer interrupt
	- writes value in (mtimecp?)
- interrupt gen by CLINT, $\rightarrow$ trap handler $\rightarrow$ kernelvec.S rarrow
	- update scause register
	- pc <- mtvec
	- schedules next interrupt
	- software interrupt sent
	- whenever timer interrupt, next is scheduled



