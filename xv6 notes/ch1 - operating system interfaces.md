- job of an operating system:
	- share computer among mult programs + provide more useful services than hardware alone can support
	- manage and abstracts low-level hardware
	- share hardware among mult programs so they appear to run at the same time
	- provide controlled ways for progs to interact so they can share data and work together
- OS gives users interface to interact w computer. a good interface should be simple and narrow. but also have good features
- user space is limited. if it wants to do smth beyond what it's allowed, it requests the kernel
- **kernel**: a special program with a lot more software and hardware privileges than a regular user program. 
	- provides services to running programs (processes)
- each process memory contains instructions, data, and a stack
- kernel services:
	- processes
	- memory
	- file descriptors
	- pipes
	- file system
- when process needs a kernel service, it invokes a system call, which enters kernel, performs the service and returns.
- kernel uses hardware protection provided by CPU to ensure each process can only access its own memory
- kernel has hardware privileges. user programs execute without those privileges. when system call is invoked, hardware raises privilege level.
- **interface** is the collection of system calls a kernel provides
- **shell** is an ordinary program that reads commands from user and executes them
- CPU aka core aka processor aka hart $\rightarrow$ hardware thread
- xv6 system calls list (fig 1.2):
	![[Pasted image 20240825101426.png]]
## 1.1 - processes and memory
- virtual user address spaces (**VAS**) are created when a user program (.exe file) is run. OS allocates pages in physical memory for it
	- pages are scattered throughout the physical memory
	- CPU presents a virtual memory - that appears to be all together/in sequence - to the user program
- each process has user-space memory (instructions, data, stack) and per-process state private to the kernel
- xv6 time shares processes: switches CPUs among set of waiting processes. when not running, xv6 saves its CPU registers and restores them when running the program next. process is identified with a PID
	- uses round robin btw
- a process may create a new process using `fork` system call $\rightarrow$ creates a child process with the same memory as the parent process
-  some system calls: `fork`, `exit`, `wait`, `kill`, `getpid`
- `fork()` called $\rightarrow$ child process created, VAS of parent is copied into VAS of child - the two processes now run separately
	- VAS of child and parent is identical (except PID) initially, but they are different/separate. changing a variable in one doesn't change it in the other
	- in parent process, `fork` returns PID of child
			`int pid = fork();` $\rightarrow$ is run in parent, returns PID of child, gets saved in parent's `pid` variable
		in child process, `fork` returns zero
			`int pid = fork();` $\rightarrow$ child process is created at this point. so it doesn't make the `fork` system call but zero gets stored in its `pid` variable.
- `exec()` system call replaces child's memory with new memory image loaded from a file in the file system. format of file must specify which part holds instructions, which part is data, which instruction to start at, etc. ex6 uses the ELF format.
	- when exec succeeds, doesn't return to the calling prog. instructions loaded from file start executing.
- xv6 allocates memory implicitly. fork for child copy of parent memory, exec to hold the executable fiole.
## 1.2 - I/O and file descriptors
- **how is a command executed from the shell?** when shell receives the command, it forks. the child thread/process runs the command, then exits
- **file descriptor (fd):** small integer representing kernel-managed object that process can read from or write to
	- a process can obtain one by **opening a file**, directory, or **device**, **creating a pipe**, or duplicating an existing fd
	- **btw:** we will refer to the object that a fd refers to as a "file." the fd interface abstracts away differences bw files/pipes/devices - they all look like a stream of bytes.
- xv6 kernel uses file descriptor as an index to a per-process table. so every process has priv space starting at zero. by convention, fd 0-2 are:
	- fd 0 $\rightarrow$ standard input
	- fd 1 $\rightarrow$ standard output
	- fd 2 $\rightarrow$ standard error
- shell uses this convention to implement I/O redirection and pipelines. shell insures it always has three file descs open
- read and write system calls read bytes from and write bytes to open files. 
	- `read(fd, buf, n);` $\rightarrow$ reads `n` bytes from file descriptor `fd`, copies them into `buf`, and returns number of bytes read
	- `write(fd, bug, n);` $\rightarrow$ writes `n` bytes from `buf` to `fd` and returns num of bytes written
## 1.3 - pipes
- **pipe:** a small kernel buffer exposed to processes as a pair of `fd`s, one for reading, one for writing. writing data at one end of the pipe makes data available for reading from other end
- a way for inter-process (parent and child sharing pipe) and inter-file (diff processes sharing pipe) communication
- when `pipe(int p[2])` is called, it assigns `read` end of pipe to `p[0]` and `write` to `p[1]`
- code demonstration + explanation:
	```
	int p[2];
	char* argv[2];

	argv[0] = "wc";
	argv[1] = 0;

	pipe(p);
	if (fork() == 0) { // child
		close(0);
		dup(p[0]);
		close(p[0]);
		close(p[1]);
		exec("/bin/wc", argv);
	} else { // parent
		close(p[0]);
		write(p[1], "hello world\n", 12);
		close(p[1]);
	}
	```
	- empty array `p`. `pipe(p)` creates new pipe and records read/write `fd`s in `p`
	- after `fork` both parent and child have `fd`s referring to pipe
	- THE GOAL is to have the parent write `"hello world\n"` to the pipe and have the child receive it
	- **CHILD** (`if` block)
		- closes its read `fd` (which is 0)
		- duplicates `p[0]` (the pipe's read end)
			`int dup(int fd)` $\rightarrow$ Return a new file descriptor referring to the same file as fd
		- so `p[0]` is now the standard input of the child process 
			- **btw** how did xv6 know to store `p[0]` in standard input??? the operating system returns (uses) the lowest available `fd`. since we closed 0 (standard input) in the last step, it's now available
		- now original `p[0]` isn't needed and `p[1]` also isn't needed since child process won't be writing to pipe, so child closes both of those
		- runs the `exec` system call with `argv[]` as input
			- `int exec(char *file, char *argv[])` $\rightarrow$ Replaces calling program with a different file (path?) and execute it with arguments; only returns if error
			- essentially it's as if, instead of whatever process is running, we just went to `filepath` and entered args (what goes in the `argv[]` array) into the terminal
		- `argv[0] = "wc"` so the child process is replaced by the word count program
		- `argv[1] = 0`. in C, when a program is called, the operating system passes an array of strings (`argv[]`) as arguments. The last element of this array **must** be `NULL` (or `0`) to signal the end of the argument list. Without this, the program wouldn’t know where the argument list ends and might read invalid memory.
		- SO the word count program is run, and no arguments are given. so `wc` starts reading from stdin (which is `p[0]`, remember?)
		- `p[0]` contains `"hello world\n"`, so that's what the `wc` program receives as input
		- `wc` outputs: `1 2 12` (one line, two words, 12 characters)
			- **btw** how does `wc` know when to stop executing? we reach the **EOF** (end of file) so it stops automatically. 
			- in the terminal we are getting a continuous stream so we need to do ctrl+d to signal EOF
	- **PARENT** (`else` block)
		- closes `p[0]` since it doesn't need to read from pipe
		- writes "hello world\n" to `p[1]`, also tells it that it's 12 characters
		- then it's is done sending data through pipe so it closes `p[1]`
	- when no data is available, `read` end of pipe waits, either for data to be written, or for all `fd`s referring to `write` end are closed (read returns 0). 
	- this is why it's important to close `p[1]` from child before calling `wc`. otherwise `wc` would never see EOF
- if we enter something like `grep fork sh.c | wc - l`, the output of the first command (`grep fork sh.c`) will be the input to the second command (`wc - l`). xv6 does this by creating a pipe bw the two commands
- pipes' advantages over temporary files
	- pipes automatically clean themselves up
	- pipes can pass arbitrarily long data streams
	- pipes allow for parallel execution of pipeline stages
	- pipes' blocking reads and writes are more efficient
## 1.4 - file system
- files stored in a tree structure, w directories as branches
- **root:** starting directory
- `chdir(folder_path)` navigates to that folder
- `mkdir(folder_name)` makes folder w that name
- `open` returns fd
	- `open(file_name, O_RDONLY)` 
	- `open("a", O_CREATE)` creates a file, links with name "a"
	- `open(file_name, O_WRONLY)` 
- `mknod("/console", 1, 1)` creates a new device file $\rightarrow$ what is a device???
	- major and minor device numbers (the arguments in `mknod`), uniquely identify a kernel device
- a file's name is distinct from the actual file. the actual file is called an **inode,** which can have multiple names called **links**
	- each link consists of a file name and a reference to an inode
	- inode holds metadata about a file, including its **type (file or directory or device)**, length, location of its content on the disk, and no. of links
- `fstat(int fd, struct stat *sh)` retrieves information from the inode and places in a struct called stat: 
	- strat contains these attributes: `dev` (disk device), `ino` (inode number), `type`, `nlink` (no. of links), `size` (in bytes)
- `link` creates another file system name referring to same inode
	- `link("a", "b")` gives the file that "a" refers to another name, "b"
- `unlink("a")` removes that name from the file system. the file's inode and the disk space containing its content are only freed when all its links are removed
- `fd = open("x", O_CREATE|O_RDWR);`
	`unlink("x");` this creates and a file (inode) but removes its link. so when the process closes fd, it will be cleaned up automatically
- the shell commands `ln` and `rm` correspond to `link` and `unlink`
- **file utilities:** common commands that perform operations on files and directories. user-level programs that the shell invokes (aka not built into the shell). 
	- e.g. `mkdir, ln, rm`