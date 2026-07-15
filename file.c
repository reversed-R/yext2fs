
#include "fake_disk.h"
#include "fake_kernel_api.h"
#include "yext2.h"
#include <asm-generic/errno-base.h>
#include <stdlib.h>

int yext2_create(struct super_block *sb, struct inode *dir,
                 struct dentry *dentry, umode_t mode, bool excl) {
  struct inode *inode;
  struct yext2_super_block *sbi;
  struct yext2_inode *yinode;

  sbi = YEXT2_SB(sb);

  inode = yext2_new_inode(sb, dir, mode, dentry->d_name);
  if (inode == NULL)
    return -EFAULT;

  yinode = YEXT2_INODE(inode);

  d_instantiate(dentry, inode);

  if (yext2_add_link(sb, dentry, inode) < 0) {
    d_instantiate(dentry, NULL);
    free(inode);
  }

  // .. to this
  yext2_inode_inc_link_count(yinode);

  return 0;
}
