// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// inode 中直接块指针的最大数量（原为12，现改为11以腾出一个位置用于二级间接块）
#define NDIRECT 11

// 一个磁盘块中能存储的块地址数量（即一级间接块可索引的数据块数）
#define NINDIRECT (BSIZE / sizeof(uint))

// 二级间接结构所能索引的最大数据块数量：
// 即一个二级间接块包含 NINDIRECT 个一级间接块指针，
// 每个一级间接块又可索引 NINDIRECT 个数据块，故总共为 NINDIRECT × NINDIRECT
#define NDINDIRECT (NINDIRECT * NINDIRECT)

// 文件最大支持的总块数：
// = 直接块数 + 一级间接块可索引的块数 + 二级间接块可索引的块数
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT)

// 辅助宏：每个磁盘块可容纳的块地址数量（与 NINDIRECT 等价，用于提高代码可读性）
#define NADDR_PER_BLOCK (BSIZE / sizeof(uint))

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040


// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

