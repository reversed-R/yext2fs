#include "fake_kernel_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct list *list_new() {
  struct list *p;
  if ((p = malloc(sizeof(struct list))) == NULL) {
    printf("failed to malloc for list\n");
    exit(-1);
  }

  p->next = p;
  p->prev = p;
  p->data = NULL;

  return p;
}

void list_append(const struct list *list, void *data) {
  struct list *p, *new;
  p = (struct list *)list;
  do {
    if (p->data == NULL) {
      break;
    }
    p = p->next;
  } while (p->next != list);

  new = list_new();
  new->data = data;

  new->next = p->next;
  p->next = new;

  new->prev = p;
  new->next->prev = new;
}

struct inode *inode_get(struct super_block *sb, unsigned long ino) {
  struct inode *inode;
  struct list *p;
  p = sb->s_inodes;

  do {
    inode = (struct inode *)p->data;
    if (inode && inode->ino == ino) {
      return inode;
    }

    p = p->next;
  } while (p != sb->s_inodes);

  if ((inode = malloc(sizeof(struct inode))) == NULL) {
    printf("failed to malloc for inode\n");
    exit(-1);
  }
  inode->ino = ino;
  list_append(sb->s_inodes, inode);

  return inode;
}

struct dentry *dentry_alloc(struct inode *inode) {
  struct dentry *dentry;

  if ((dentry = malloc(sizeof(struct dentry))) == NULL) {
    printf("failed to malloc for dentry\n");
    exit(-1);
  }

  dentry->d_inode = inode;
  dentry->d_children = NULL;
  dentry->d_fsdata = NULL;

  return dentry;
}

struct dentry *d_make_root(struct inode *root_inode) {
  struct dentry *dentry;

  if ((dentry = malloc(sizeof(struct dentry))) == NULL) {
    printf("failed to malloc for dentry\n");
    exit(-1);
  }

  dentry->d_inode = root_inode;
  strncpy(dentry->d_name, "/\0", 2);
  dentry->d_children = NULL;
  dentry->d_fsdata = NULL;

  return dentry;
}

struct super_block *super_block_new() {
  struct super_block *sb;

  if ((sb = malloc(sizeof(struct super_block))) == NULL) {
    printf("failed to malloc for super_block\n");
    exit(-1);
  }

  sb->s_inodes = list_new();

  return sb;
}

/*
 * fs on-disk file type to dirent file type conversion
 */
static const unsigned char fs_dtype_by_ftype[FT_MAX] = {
    [FT_UNKNOWN] = DT_UNKNOWN, [FT_REG_FILE] = DT_REG, [FT_DIR] = DT_DIR,
    [FT_CHRDEV] = DT_CHR,      [FT_BLKDEV] = DT_BLK,   [FT_FIFO] = DT_FIFO,
    [FT_SOCK] = DT_SOCK,       [FT_SYMLINK] = DT_LNK};

unsigned char fs_ftype_to_dtype(unsigned int filetype) {
  if (filetype >= FT_MAX)
    return DT_UNKNOWN;

  return fs_dtype_by_ftype[filetype];
}

static const unsigned char fs_ftype_by_dtype[DT_MAX] = {
    [DT_REG] = FT_REG_FILE, [DT_DIR] = FT_DIR,    [DT_LNK] = FT_SYMLINK,
    [DT_CHR] = FT_CHRDEV,   [DT_BLK] = FT_BLKDEV, [DT_FIFO] = FT_FIFO,
    [DT_SOCK] = FT_SOCK,
};

unsigned char fs_umode_to_ftype(umode_t mode) {
  return fs_ftype_by_dtype[S_DT(mode)];
}

u32 yext2_get_time_sec() { return time(NULL); }
