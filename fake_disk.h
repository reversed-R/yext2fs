#ifndef YEXT2_FAKE_DISK_H
#define YEXT2_FAKE_DISK_H

#include "fake_kernel_api.h"
#include "yext2.h"
#include <sys/types.h>

#define YEXT2_FUSE_FAKE_DISK_SIZE 1024 * 1024

// super block read
void yext2_super_read(struct yext2_super_block *sb);

// block read
char *yext2_block_read(struct super_block *sb, sector_t block);

// block write
void yext2_block_write(struct super_block *sb, sector_t block, loff_t offset,
                       const char *data, int size);

// block group descriptor read
struct yext2_block_group_desc *yext2_get_group_desc(struct super_block *sb,
                                                    unsigned int block_group);

// inode read
struct yext2_inode *yext2_get_inode(struct super_block *sb, ino_t ino);

#endif // !YEXT2_FAKE_DISK_H
