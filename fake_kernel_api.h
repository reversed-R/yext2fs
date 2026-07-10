#ifndef YEXT2_FAKE_KERNEL_API_H
#define YEXT2_FAKE_KERNEL_API_H

/* fake declaration of kernel primitive types */
#define bool _Bool
#define true 1
#define false 0

#define NULL 0

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef unsigned char le8;
typedef unsigned short le16;
typedef unsigned int le32;
typedef unsigned long long le64;

typedef unsigned long size_t;

typedef u64 sector_t;
typedef unsigned long ino_t;
typedef unsigned short umode_t;
typedef unsigned int mode_t;

// endian conversion API
// now I use little endian machine (x86_64),
// so that these macro pass through input to output.
#define le64_to_cpu(val) (val)
#define le32_to_cpu(val) (val)
#define le16_to_cpu(val) (val)

#define cpu_to_le64(val) (val)
#define cpu_to_le32(val) (val)
#define cpu_to_le16(val) (val)

// struct buffer_head {
//   size_t b_size; /* size of mapping */
//   char *b_data;  /* pointer to data within the page */
// };
//
// struct buffer_head *sb_bread(struct super_block *sb, sector_t block);

struct list {
  struct list *next, *prev;
  void *data;
};

struct inode {
  unsigned long ino;
  void *i_private;
};

struct super_block {
  void *s_fs_info;
  struct dentry *s_root;
  struct list *s_inodes; /* all inodes */
};

struct dentry {
  struct inode *d_inode;
  struct super_block *d_sb;
  struct list *d_children; /* our children */
  struct dentry *d_parent; /* parent dentry */
  void *d_fsdata;          /* fs-specific data */
  char d_name[64];
};

struct file {
  // fmode_t f_mode;
  const struct file_operations *f_op;
  void *private_data;
  struct inode *f_inode;
  unsigned int f_flags;
  // loff_t f_pos;
};

struct dir_context {};

struct inode *inode_get(struct super_block *sb, unsigned long ino);

struct dentry *d_make_root(struct inode *root_inode);

struct super_block *super_block_new();

struct dentry *dentry_alloc(struct inode *inode);

void d_instantiate(struct dentry *dentry, struct inode *inode);

struct list *list_new();

void list_append(const struct list *list, void *data);

// stat
#define S_IFMT 00170000
#define S_IFSOCK 0140000
#define S_IFLNK 0120000
#define S_IFREG 0100000
#define S_IFBLK 0060000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFIFO 0010000
#define S_ISUID 0004000
#define S_ISGID 0002000
#define S_ISVTX 0001000

/*
 * struct dirent file types
 * exposed to user via getdents(2), readdir(3)
 *
 * These match bits 12..15 of stat.st_mode
 * (ie "(i_mode >> 12) & 15").
 */
#define S_DT_SHIFT 12
#define S_DT(mode) (((mode) & S_IFMT) >> S_DT_SHIFT)
#define S_DT_MASK (S_IFMT >> S_DT_SHIFT)

/* these are defined by POSIX and also present in glibc's dirent.h */
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14

#define DT_MAX (S_DT_MASK + 1) /* 16 */

/*
 * fs on-disk file types.
 * Only the low 3 bits are used for the POSIX file types.
 * Other bits are reserved for fs private use.
 * These definitions are shared and used by multiple filesystems,
 * and MUST NOT change under any circumstances.
 *
 * Note that no fs currently stores the whiteout type on-disk,
 * so whiteout dirents are exposed to user as DT_CHR.
 */
#define FT_UNKNOWN 0
#define FT_REG_FILE 1
#define FT_DIR 2
#define FT_CHRDEV 3
#define FT_BLKDEV 4
#define FT_FIFO 5
#define FT_SOCK 6
#define FT_SYMLINK 7

#define FT_MAX 8

/*
 * declarations for helper functions, accompanying implementation
 * is in fs/fs_dirent.c
 */
unsigned char fs_ftype_to_dtype(unsigned int filetype);

unsigned char fs_umode_to_ftype(umode_t mode);

umode_t mode_to_umode(mode_t mode);

#endif // !YEXT2_FAKE_KERNEL_API_H
