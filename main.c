#define FUSE_USE_VERSION 29
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <malloc.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "fake_disk.h"
#include "fake_kernel_api.h"
#include "fuse.h"
#include "yext2.h"

extern char *yext2_fuse_fake_disk;

struct super_block *sb;
struct yext2_block_group_desc *bgd;

static int yext2_mount(struct super_block *sb) {
  struct yext2_super_block *yext2_sb;
  struct inode *root;
  struct yext2_inode *inode;
  int root_dentry_block_no;
  char *root_dentry_block;
  struct yext2_dentry *root_de;

  if ((yext2_sb = malloc(sizeof(struct yext2_super_block))) == NULL) {
    return -1;
  }

  printf("yext2_sb: %p\n", yext2_sb);

  // read fs from disk
  // block 0 is boot sector
  // block 1 is super block of first block group
  yext2_super_read(yext2_sb);

  printf("yext2_sb: %p\n", yext2_sb);
  printf("sb: %p\n", sb);
  printf("sb->s_fs_info: %p\n", sb->s_fs_info);

  sb->s_fs_info = (void *)yext2_sb;

  yext2_super_block_print(yext2_sb);

  root = inode_get(sb, YEXT2_ROOT_INO);
  printf("root: %p\n", root);

  bgd = yext2_get_group_desc(sb, 0);
  yext2_group_desc_print(bgd);

  inode = yext2_get_inode(sb, YEXT2_ROOT_INO);
  yext2_inode_print(inode);

  // TODO: temporaly implementation: pass pointer in the fake disk
  // in kernelland, use buffer_head
  root->i_private = inode;

  if ((sb->s_root = d_make_root(root)) == NULL) {
    return -1;
  }

  root_dentry_block_no = le32_to_cpu(inode->i_block[0]);
  root_dentry_block = yext2_block_read(sb, root_dentry_block_no);
  root_de = (struct yext2_dentry *)(root_dentry_block);
  sb->s_root->d_fsdata = root_de;

  printf("sb->s_root: %p\n", sb->s_root);

  return 0;
}

static struct fuse_operations yext2_fops = {
    .readdir = yext2_readdir_fuse,
    .mkdir = yext2_mkdir_fuse,
    .getattr = yext2_getattr_fuse,
    .open = yext2_open_fuse,
    .read = yext2_read_fuse,
};

int main(int argc, char *argv[]) {
  int dump_fd;
  char *mnt_path, *dump_path;
  struct fuse_args fuse_args = FUSE_ARGS_INIT(0, NULL);
  struct fuse_chan *fuse_chan;
  struct fuse *fuse;

  if (argc != 3) {
    printf("usage: <mount-path> <dump-file-path>\n");
    return -1;
  }

  mnt_path = argv[1];
  dump_path = argv[2];

  /* preparation of dumping file system binary to file */
  if ((dump_fd = open(dump_path, O_RDWR)) == -1) {
    printf("failed to open: %s, errno=%d\n", dump_path, errno);
    return -1;
  }

  if (ftruncate(dump_fd, YEXT2_FUSE_FAKE_DISK_SIZE) == -1) {
    printf("failed to ftruncate: errno=%d\n", errno);
    return -1;
  }

  if ((yext2_fuse_fake_disk =
           mmap(NULL, YEXT2_FUSE_FAKE_DISK_SIZE, PROT_READ | PROT_WRITE,
                MAP_SHARED, dump_fd, 0)) == MAP_FAILED) {
    printf("failed to mmap: errno=%d\n", errno);
    return -1;
  }
  /* end of preparation of dumping */

  if ((sb = super_block_new()) == NULL) {
    printf("failed to malloc for sb\n");
    return -1;
  }

  if (yext2_mount(sb)) {
    printf("failed to yext2_mount\n");
    return -1;
  }

  if (!(fuse_chan = fuse_mount(mnt_path, &fuse_args))) {
    printf("failed to fuse_mount\n");
    return -1;
  }

  if (!(fuse = fuse_new(fuse_chan, &fuse_args, &yext2_fops, sizeof(yext2_fops),
                        NULL))) {
    fuse_unmount(mnt_path, fuse_chan);
    printf("failed to fuse_new\n");
    return -1;
  }

  fuse_set_signal_handlers(fuse_get_session(fuse));
  fuse_loop_mt(fuse);

  fuse_unmount(mnt_path, fuse_chan);

  return 0;
}
