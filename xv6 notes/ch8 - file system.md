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
- `bget` searches list for correct buffer. if exists, acquires sleep-lock for it, sets `valid = 1`, then returns buf
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
