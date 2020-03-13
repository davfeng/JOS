struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct rtcdate;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;

// ide.c
void            ideinit(void);
void            ideintr(void);
void            iderw(struct buf*);

struct buf*     bread(uint32_t dev, uint32_t blockno);
void            bwrite(struct buf *b);
