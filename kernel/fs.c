// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb;

// 从磁盘中读取超级块
static void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// 初始化文件系统
// 即读入超级块
void fsinit(int dev)
{
  readsb(dev, &sb);
  if (sb.magic != FSMAGIC)
    panic("invalid file system");
  initlog(dev, &sb);
}

// 清零一个块
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Blocks.

// 分配一个全 0 的磁盘块
// 满了返回 0
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for (b = 0; b < sb.size; b += BPB)
  {
    bp = bread(dev, BBLOCK(b, sb));
    for (bi = 0; bi < BPB && b + bi < sb.size; bi++)
    {
      m = 1 << (bi % 8);
      if ((bp->data[bi / 8] & m) == 0)
      {                        // Is block free?
        bp->data[bi / 8] |= m; // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  printf("balloc: out of blocks\n");
  return 0;
}

// 释放一个磁盘块
// 这里仅仅修改了位图标记
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if ((bp->data[bi / 8] & m) == 0)
    panic("freeing free block");
  // 位图置 0 位
  bp->data[bi / 8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inode（索引节点）。
//
// 一个 inode 描述一个无名文件。
// inode 的磁盘结构包含元数据：文件的类型、大小、指向它的链接数，
// 以及保存文件内容的块列表。
//
// inode 在磁盘上按顺序排列，起始位置在 sb.inodestart 块。
// 每个 inode 都有一个编号，表示它在磁盘上的位置。
//
// 内核会在内存中维护一个正在使用的 inode 表，以便同步访问
// 被多个进程使用的 inode。内存中的 inode 包含一些在磁盘上不保存的
// 账务信息：ip->ref 和 ip->valid。
//
// 一个 inode 及其在内存中的表示需要经过一系列状态的转换，
// 才能被文件系统的其他代码使用。
//
// * 分配：当一个 inode 的类型（磁盘上的）不为零时，它就被分配。
// ialloc() 用于分配 inode，iput() 在引用计数和链接计数降为零时释放 inode。
//
// * 引用计数：inode 表中的条目当 ip->ref 为零时表示空闲。
// 否则，ip->ref 跟踪指向该条目的内存指针数（打开的文件和当前目录）。
// iget() 查找或创建一个表项并增加它的引用计数；iput() 则减少引用计数。
//
// * 有效性：只有当 ip->valid 为 1 时，inode 表项中的信息（类型、大小等）才是正确的。
// ilock() 从磁盘读取 inode 并设置 ip->valid，
// 当 ip->ref 降为零时，iput() 清除 ip->valid。
//
// * 锁定：文件系统代码只能在首先锁定 inode 后，才能查看和修改 inode 及其内容。
//
// 因此，典型的流程是：
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... 查看和修改 ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() 与 iget() 是分开的，以便系统调用可以长时间引用 inode（如对一个打开的文件），
// 只在短时间内锁定它（例如，在 read() 中）。这种分离有助于避免死锁和路径查找中的竞争。
// iget() 增加 ip->ref，确保 inode 保留在表中，并且指向它的指针仍然有效。
//
// 许多内部文件系统函数期望调用者已经锁定了相关的 inode；
// 这使得调用者可以创建多步的原子操作。
//
// itable.lock 自旋锁保护 itable 表项的分配。
// 由于 ip->ref 表示表项是否空闲，ip->dev 和 ip->inum 表示一个条目所持有的 inode，
// 因此在使用这些字段时，必须持有 itable.lock。
//
// ip->lock 睡眠锁保护除 ref、dev 和 inum 外的所有 ip-> 字段。
// 必须持有 ip->lock 才能读取或写入 inode 的 ip->valid、ip->size、ip->type 等。

struct
{
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

// 初始化 itable 和其上面的 inode
void iinit()
{
  int i = 0;

  initlock(&itable.lock, "itable");
  for (i = 0; i < NINODE; i++)
  {
    initsleeplock(&itable.inode[i].lock, "inode");
  }
}

static struct inode *iget(uint dev, uint inum);

// 在设备 dev 上分配一个 inode。
// 通过将其类型设置为 type 来标记该 inode 为已分配。
// 返回一个未加锁但已分配并被引用的 inode，
// 如果没有空闲的 inode，则返回 NULL。

struct inode *
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for (inum = 1; inum < sb.ninodes; inum++)
  {
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode *)bp->data + inum % IPB;
    if (dip->type == 0)
    { // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp); // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  printf("ialloc: no inodes\n");
  return 0;
}

// 将修改过的内存中的 inode 写回磁盘。
// 必须在每次更改 ip->xxx 字段（这些字段存储在磁盘上）后调用。
// 调用者必须持有 ip->lock 锁。
void iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode *)bp->data + ip->inum % IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// 在设备 dev 上查找编号为 inum 的 inode，
// 并返回其内存中的副本。此操作不锁定 inode，
// 也不从磁盘读取它。
static struct inode *
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&itable.lock);

  // Is the inode already in the table?
  empty = 0;
  for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++)
  {
    if (ip->ref > 0 && ip->dev == dev && ip->inum == inum)
    {
      ip->ref++;
      release(&itable.lock);
      return ip;
    }
    if (empty == 0 && ip->ref == 0) // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode entry.
  if (empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&itable.lock);

  return ip;
}

// 增加 ip 的引用计数。
// 返回 ip 以便支持 ip = idup(ip1) 的用法。
struct inode *
idup(struct inode *ip)
{
  acquire(&itable.lock);
  ip->ref++;
  release(&itable.lock);
  return ip;
}

// 锁定给定的 inode。
// 如果需要，从磁盘读取 inode。
void ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if (ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if (ip->valid == 0)
  {
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode *)bp->data + ip->inum % IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if (ip->type == 0)
      panic("ilock: no type");
  }
}

// 解锁给定的 inode。
void iunlock(struct inode *ip)
{
  if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// 释放对内存中 inode 的引用。
// 如果这是最后一次引用，inode 表项可以被回收。
// 如果这是最后一次引用且 inode 没有链接到它，
// 则在磁盘上释放 inode（及其内容）。
// 所有调用 iput() 的操作必须在事务中进行，以防需要释放 inode。
void iput(struct inode *ip)
{
  acquire(&itable.lock);

  if (ip->ref == 1 && ip->valid && ip->nlink == 0)
  {
    // inode 没有链接且没有其他引用：截断并释放。

    // ip->ref == 1 表示没有其他进程持有 ip 的锁，
    // 因此这个 acquire_sleep() 不会阻塞（或死锁）。

    acquiresleep(&ip->lock);

    release(&itable.lock);

    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    releasesleep(&ip->lock);

    acquire(&itable.lock);
  }

  ip->ref--;
  release(&itable.lock);
}

// 常见的惯用法：解锁，然后释放。
void iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

// Inode内容
//
// 每个inode关联的数据（内容）存储在磁盘上的块中。前NDIRECT个块号
// 列表保存在ip->addrs[]中。接下来的NINDIRECT个块
// 列表保存在块ip->addrs[NDIRECT]中.
//
// 返回inode ip中第n个块的磁盘块地址。
// 如果没有这样的块，bmap将为其分配一个。
// 如果磁盘空间不足，则返回0。

// 将文件的第 i 个数据块指针对应到磁盘号
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  // 如果是直接块
  if (bn < NDIRECT)
  {
    if ((addr = ip->addrs[bn]) == 0)
    {
      addr = balloc(ip->dev);
      if (addr == 0)
        return 0;
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  bn -= NDIRECT;

  // 间接
  if (bn < NINDIRECT)
  {
    // Load indirect block, allocating if necessary.
    if ((addr = ip->addrs[NDIRECT]) == 0)
    {
      addr = balloc(ip->dev);
      if (addr == 0)
        return 0;
      ip->addrs[NDIRECT] = addr;
    }
    
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn]) == 0)
    {
      addr = balloc(ip->dev);
      if (addr)
      {
        a[bn] = addr;
        log_write(bp);
      }
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// 截断inode（丢弃内容）。
// 调用者必须持有ip->lock。
// 删除了这个Inode指向的文件，但是并没有删除inode本身
void itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  // 释放直接索引块
  for (i = 0; i < NDIRECT; i++)
  {
    if (ip->addrs[i])
    {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  // 释放间接索引块
  if (ip->addrs[NDIRECT])
  {
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint *)bp->data;
    for (j = 0; j < NINDIRECT; j++)
    {
      if (a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  // 清空大小，写回 inode 
  ip->size = 0;
  iupdate(ip);
}

// 从inode复制stat信息。
// 调用者必须持有ip->lock。
void stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

// 从inode读取数据。
// 调用者必须持有ip->lock。
// 如果user_dst==1，则dst是用户虚拟地址；
// 否则，dst是内核地址。

// ip：目标文件的 inode，包含文件的元数据和文件内容的地址。
// user_dst：一个标志，指示目标地址是用户空间地址还是内核空间地址。
// dst：目标地址，数据将被写入该地址。
// off：读取的偏移量，从文件的哪个位置开始读取。
// n：需要读取的字节数。
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if (off > ip->size || off + n < off)
    return 0;
  if (off + n > ip->size)
    n = ip->size - off;

  for (tot = 0; tot < n; tot += m, off += m, dst += m)
  {
    uint addr = bmap(ip, off / BSIZE);
    if (addr == 0)
      break;
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off % BSIZE);
    if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1)
    {
      brelse(bp);
      tot = -1;
      break;
    }
    brelse(bp);
  }
  return tot;
}

// 将数据写入inode。
// 调用者必须持有ip->lock。
// 如果user_src==1，则src是用户虚拟地址；
// 否则，src是内核地址。
// 返回成功写入的字节数。
// 如果返回值小于请求的n，则表示发生了某种错误。
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if (off > ip->size || off + n < off)
    return -1;
  if (off + n > MAXFILE * BSIZE)
    return -1;

  for (tot = 0; tot < n; tot += m, off += m, src += m)
  {
    uint addr = bmap(ip, off / BSIZE);
    if (addr == 0)
      break;
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off % BSIZE);
    if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1)
    {
      brelse(bp);
      break;
    }
    log_write(bp);
    brelse(bp);
  }

  if (off > ip->size)
    ip->size = off;

  // write the i-node back to disk even if the size didn't change
  // because the loop above might have called bmap() and added a new
  // block to ip->addrs[].
  iupdate(ip);

  return tot;
}

// Directories        --------------------------------------

int namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// 在目录中查找目录项。
// 如果找到，将*poff设置为该目录项的字节偏移。
struct inode *
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if (dp->type != T_DIR)
    panic("dirlookup not DIR");

  for (off = 0; off < dp->size; off += sizeof(de))
  {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if (de.inum == 0)
      continue;
    if (namecmp(name, de.name) == 0)
    {
      // entry matches path element
      if (poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// 在目录dp中写入一个新的目录项（名称，inum）。
// 成功时返回0，失败时返回-1（例如，磁盘块不足）。
int dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if ((ip = dirlookup(dp, name, 0)) != 0)
  {
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for (off = 0; off < dp->size; off += sizeof(de))
  {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if (de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    return -1;

  return 0;
}

// 路径

// 将路径中的下一个路径元素复制到name中。
// 返回指向复制后的元素的指针。
// 返回的路径没有前导斜杠，
// 因此调用者可以通过检查*path=='\0'来判断是否是最后一个元素。
// 如果没有要移除的名称，返回0。

// 示例：
//   skipelem("a/bb/c", name) = "bb/c"，设置name = "a"
//   skipelem("///a//bb", name) = "bb"，设置name = "a"
//   skipelem("a", name) = ""，设置name = "a"
//   skipelem("", name) = skipelem("////", name) = 0

static char *
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while (*path == '/')
    path++;
  if (*path == 0)
    return 0;
  s = path;
  while (*path != '/' && *path != 0)
    path++;
  len = path - s;
  if (len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else
  {
    memmove(name, s, len);
    name[len] = 0;
  }
  while (*path == '/')
    path++;
  return path;
}

// 查找并返回路径名对应的inode。
// 如果parent != 0，返回父目录的inode，并将最后一个路径元素复制到name中，
// name必须有足够的空间来存放DIRSIZ字节。
// 必须在事务中调用，因为它调用了iput()。
static struct inode *
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if (*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while ((path = skipelem(path, name)) != 0)
  {
    ilock(ip);
    if (ip->type != T_DIR)
    {
      iunlockput(ip);
      return 0;
    }
    if (nameiparent && *path == '\0')
    {
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if ((next = dirlookup(ip, name, 0)) == 0)
    {
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if (nameiparent)
  {
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode *
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode *
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
