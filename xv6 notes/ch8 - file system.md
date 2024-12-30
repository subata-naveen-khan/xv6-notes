- purpose: organize and store data
- features: 
	- sharing data bw users and apps
	- persistence so data is available after reboot
- xv6 file system addresses several challenges:
	- needs data structures to represent the tree of directories and files, record identities of blocks that hold each file's content, and record which disk areas are free
	- crash recovery support. must work correctly when restarted after a crash. the challenge is that a crash can interrupt a sequence of updates and leave inconsistent on-disk data structures
	- need to coordinate to maintain invariants to allow different processes to use file system
	- accessing disk is MUCH slower than accessing memory, so there must be an in-memory cache of popular blocks
## 8.1 - overview
- xv6 file system implementation is organized in 7 layers: (fig 8.1)
	1. **disk** layer reads and writes blocks on an virtio hard drive
	2. **buffer cache** layer caches disk blocks and syncs access to them, so only one kernel process at a time can modify data in a particular block 
	3. **logging** layer lets higher layers wrap updates to several blocks in a transaction, ensures atomicity in case of a crash
	4. **inode** layer provides each file when a unique i-number, and some blocks holding the file's data
	5. **directory** layer implements each folder as a special kind of inode whose content is a sequence of directory entries, each containing a file's name and i-number
	6. **pathname** layer provides hierarchical path names (e.g. `/a/b/c/img.png`) and resolves using recursive lookup 
	7. **file descriptor** layer abstracts Unix resources (pipes, devices, files, etc) using the file syst interface, simplifying app programmers' jobs
#### disk layer !!
- file system plan for where to store inodes and content blocks.
	- fig 8.2 - structure of xv6 file syst
		![[Pasted image 20241230035038.png]]
- disk is divided into several sections
	- block 0 $\rightarrow$ **boot sector**
	- block 1 $\rightarrow$ **superblock**, contains metadata about file system (size in blocks, no. of data blocks, no. of inodes, no. of blocks in log)
	- blocks starting at 2 hold the **log**
	- then **inodes,** mult inodes per block
	- **bitmap** tracking which data blocks are in use
	- remaining $\rightarrow$ **data** blocks
## 8.2 - buffer cache layer
- buffer cache is a ==doubly-linked list of `buf` structs== holding cached copies of disk blocks
- two jobs. (code is in `bio.c`)
	1. **synchronize access** to disk blocks, to ensure only one copy of a block is in memory, and only one kernel thread at a time can use it
	2. **cache popular blocks** so they don't need to be re-read from disk (slow)
- main exported functions: 
	- `bread` obtains buffer containing copy of block which can be read or modified in memory; returns locked buffer
	- `bwrite` writes modified buffer to appropriate block on disk; releases lock
	- a kernel thread must call `brelse` to release buffer when done w it (always needs to be executed after `bread`)
- per-buffer sleep-lock used to ensure only one thread at a time uses a buffer. buffer lock released with `brelse`
- buffer cache has a fixed no. of buffers to hold disk blocks, so cache (linked list) needs to recycle a buffer holding smth else to load new block. least recently used buffer gets replaced
## 8.3 - code: buffer cache
- `main()` (`kernel/main.c`) calls the `binit()` function
- `binit` initialises buffer cache during startup (`struct bcache`) 
	```
	struct bcache {
		struct spinlock lock;
		struct buf buf[NBUF];      // NUBF = size of cache
		struct buf head;
	};
	```
	- `struct buf`
		```
		{
			int valid;                 // if buffer contains copy of block
			int disk;                  // if content has been sent to disk 
			uint dev;
			uint blockno;
			struct sleeplock lock;
			uint refcnt;
			struct buf* pref;
			struct buf* next;
			uchar data[BSIZE];
		};
		```
- `bget` searches list for correct buffer. if exists, acquires sleep-lock for it, with `valid = 1`, then returns buf
	- if not, searches list again for looking for a buf with `refcnt = 0`, edits dev and blockno and acquires sleep-lock. returns buffer to be overwritten with `valid = 0`
- `bread` calls `bget(dev, blockno)` to get a buffer for the given sector. checks `b->valid`
	- if `1`, returns buf 
	- if `0`, call `virtio_disk_rw` to load correct data into buf, then return
- it's important for there to be MAX ONE buffer for any disk sector (`dev` + `blockno`), to ensure no inconsistent data. that's why all of `bget` happens while `bcache.lock` is held
- `bcache` (linked list) has a spinlock, `buf` has a sleeplock. the spinlock is only held while `bget` is searching through `bcache`, the buffer's sleeplock is held to protect specifically its content
- if all buffers busy (`refcnt=1` for all), `bget` panics. if it slept until a buffer became free, there would be a possibility of deadlock (why? idk)
- when `bread` returns buffer, caller has exclusive access to it, can read or write. 
- if writing, calls `bwrite` before releasing buf
	- `bwrite` writes the changed data to disk using `virtio_disk_rw`
- when caller is done w buf, calls `brelse` to release it 
	- `brelse` releases sleep-lock and moves buf to front of the list
	- moving buf means the list is ordered by how recently the buffers were used/released $\rightarrow$ makes searching in `bget` more efficient. searches for existing buffer, if doesn't exist, scans backward to find least recently used buf
## 8.4 - logging layer
- logging layer is for crash recovery. challenge:
	- many file-system operations involve multiple writes to the disk, a crash while only some of the writes have been done can leave file syst in an inconsistent state
	- e.g. crash during truncating a file (freeing content blocks)
		- may leave an inode pointing to a content block that's been marked free, or an allocated but unreferenced content block
		- after reboot, if the block is allocated to another file, 2 diff files are now pointing to the same block $\rightarrow$ security problem !!!
- solution: logging
	- instead of writing directly to file syst data structures, system call will place descriptions of all needed disk writes, in a log
	- once all writes have been logged, the sys call writes a commit record to log, indicating ke log contains a complete operation. 
	- then the system call copies the writes from log to on-disk file syst data structures. after completed, log gets erased
- possible cases after crash and reboot:
	1. log is empty $\rightarrow$ no unfinished/uncommitted operations
	2. log is marked as containing a complete operation $\rightarrow$ means copying from log to disk wasn't completed. the recovery code copies the writes to the file syst. log erased
	3. log doesn't contain complete operation $\rightarrow$ changes need to be ignored to ensure atomicity. log ignored and erased
- $\rightarrow$ $\rightarrow$ atomicity ensured !!!!
## 8.5 - log design
- log resides at a fixed location, address specified in superblock
- consists of header block followed by sequence of logged blocks (record of writes)
- header contains array of sector numbers for each logged block (why?), and count of log blocks
	- count = 0 $\rightarrow$ no transaction in log
	- count > 0 $\rightarrow$ log contains a complete transaction w/ that many no. of blocks
- xv6 only writes in header block when a transaction is committed, and sets to 0 when logged blocks have been copied. so a crash midway thru a transaction will result in count being 0 $\rightarrow$ logged blocks ignored
- each sys call's code has a part indicating the sequence of writes that must be atomic w respect to crashes:
	```
	begin_op();
	// sequence of writes
	end_op()
	```
- to allow concurrency of operations by diff. processes, the logging system can accumulate writes of mult. sys calls into one transaction 
	- so one commit can involve writes of mult. complete sys calls
	- logging system only commits when no file-system sys calls are happening
- **group commit** $\rightarrow$ committing several transactions together
	- reduces no. of disk operations because it amortizes the fixed cost of a commit (cost divided over mult. operations $\rightarrow$ lower avg per op)
	- group commit also hands disk system more concurrent writes at the same time, allows disk to write them during a single disk rotetion
- log has fixed amount of space on disk, so total no. of blocks in a transaction is limited. two consequences:
1. a single sys call can't write more blocks in one transaction than space in log
	- not a problem for most, but `write` and `unlink` may need to write many blocks
		- large file may write many data blocks and inode block 
		- unlinking might write many bitmap blocks and an inode
	- solution: break up large writes into mult. smaller writes that fit in log
2. logging system can't allow a sys call to start unless it's sure its no. of writes will fit in remaining space in log
## 8.6 - code: logging
- `begin_op()`
	```
	{
		acquire(&log.lock);
		while(1){
			if(log.committing) {
				sleep(&log, &log.lock);
			} else if ((log.lh.n + (log.outstanding+1)*MAXOPBLOCKS>LOGSIZE)){
				sleep(&log, &log.lock);
			} else {
				log.outstanding += 1;
				release(&log.lock);
				break
			}
		}
	}
	```
	- first `if` checks that log isn't currently committing
	- second `if` checks if there's enough log space left to hold the writes from this call
		- `log.outstanding`: no. of sys calls that have reserved log space
		- `MAXOPBLOCKS`: max no. of blocks any FS op writes (assumes worst case)
	- the `else` block increments `outstanding` to reserve space and prevent a commit occurring during this sys call
- `log_write`acts as proxy for `bwrite`
	- records block's sector no. in the array in header, reserves a slot in log, and pins buffer in block cache to prevent cache from evicting it
	- block has to stay in cache until committed $\rightarrow$ can't be written to disk until commit, but might be needed for other reads in the same transaction
	- notices when a block is written mult times during one transaction; allocates that block the same slot in the log to optimize space $\rightarrow$ **absorption**
		- absorbing several disk writes into one can save log space and achieve better performance because only one copy of disk block must be written to disk
- `end_op()` decrements `log.outstanding`. if it reaches zero, calls `commit()`
- `commit()` has 4 stages:
	1. **`write_log()`** $\rightarrow$ write modified blocks from `bcache` to log
	2. **`write_head()`** $\rightarrow$ write header to disk - commit point; commit is now in the log
	3. **`install_trans(0)`** $\rightarrow$ installs transaction to file system - proper location
	4. **`log.lh.n = 0; write_head()`** $\rightarrow$ count = 0, i.e. log erased
- `recover_from_log()` is called from `initlog` which is called from `fsinit` during boot, before the first user process runs
	- reads log header. if commit recorded, calls `install_trans(1)`. then clears log
## 8.7 - code: block allocator
- block allocator maintains a bitmap to track which disk blocks are free; one bit per block
	- zero $\rightarrow$ free, one $\rightarrow$ in use
- `balloc(dev)`
### this is unfinished !!!!!!! i wanted to move on
## 8.8 - inode layer
- can refer to:
	- the on-disk data structure containing a file's size and list of data block numbers
	- in-memory inode, containing a copy of the on-disk inode, + info needed for kernel
- on-disk inodes are packed together in the inode blocks. each is the same size, so it's easy to find the inode with a particular number $\rightarrow$  **i-number** is how they're IDed in the code
- 