#include "fake_disk.h"
#include "fake_kernel_api.h"
#include "yext2.h"
#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

void yext2_inode_init(struct yext2_super_block *sbi, struct yext2_inode *inode,
                      umode_t mode) {
  memset(inode, 0, sizeof(struct yext2_inode));

  if (S_ISDIR(mode))
    inode->i_size = cpu_to_le32(YEXT2_BLOCK_BYTE_SIZE(sbi));
  else
    inode->i_size = 0;

  if (S_ISDIR(mode))
    inode->i_blocks =
        cpu_to_le32(YEXT2_BLOCK_BYTE_SIZE(sbi) / 512); // sector count
  else
    inode->i_blocks = 0;

  inode->i_mode = mode;

  u32 time = yext2_get_time_sec();
  inode->i_atime = cpu_to_le32(time);
  inode->i_ctime = cpu_to_le32(time);
  inode->i_mtime = cpu_to_le32(time);
  // NOTE: uid, gid は現在の実装ではsyscallしない限り取れないので一旦実装しない
}

void yext2_inode_inc_link_count(struct yext2_inode *inode) {
  u16 count = le16_to_cpu(inode->i_links_count);
  inode->i_links_count = cpu_to_le16(count + 1);
}

void yext2_inode_dec_link_count(struct yext2_inode *inode) {
  u16 count = le16_to_cpu(inode->i_links_count);
  inode->i_links_count = cpu_to_le16(count - 1);
}

struct yext2_inode *yext2_get_inode(struct super_block *sb, ino_t ino) {
  struct yext2_super_block *sbi;
  unsigned long block_group, offset;
  struct yext2_block_group_desc *group_desc;
  char *inode_table;

  sbi = YEXT2_SB(sb);

  if (ino != YEXT2_ROOT_INO && ino < le32_to_cpu(sbi->s_first_ino) ||
      ino > le32_to_cpu(sbi->s_inodes_count)) {
    printf("invalid inode: ino: %ld\n", ino);
    return NULL;
  }

  block_group = YEXT2_INO_BLOCK_GROUP(sbi, ino);

  group_desc = yext2_get_group_desc(sb, block_group);

  inode_table = yext2_block_read(sb, le32_to_cpu(group_desc->bg_inode_table));

  offset = ((ino - 1) % le32_to_cpu(sbi->s_inodes_per_group)) *
           le16_to_cpu(sbi->s_inode_size);

  return (struct yext2_inode *)(inode_table + offset);
}

void yext2_inode_print(struct yext2_inode *inode) {
  printf("----inode---->\n");
  printf("i_mode          : %d\n", inode->i_mode);
  printf("i_uid           : %d\n", inode->i_uid);
  printf("i_size          : %u\n", inode->i_size);
  printf("i_atime         : %u\n", inode->i_atime);
  printf("i_ctime         : %u\n", inode->i_ctime);
  printf("i_mtime         : %u\n", inode->i_mtime);
  printf("i_dtime         : %u\n", inode->i_dtime);
  printf("i_gid           : %d\n", inode->i_gid);
  printf("i_links_count   : %d\n", inode->i_links_count);
  printf("i_blocks        : %u\n", inode->i_blocks);
  printf("i_flags         : %u\n", inode->i_flags);
  printf("<----inode----\n");
}

// alloc inode number
int yext2_alloc_ino(struct super_block *sb, ino_t parent) {
  struct yext2_super_block *sbi;
  struct yext2_block_group_desc *gd;
  char *inode_bitmap_block;
  unsigned long inodes_per_group, first_ino;
  int parent_byte_idx, start_byte, end_byte;

  sbi = YEXT2_SB(sb);

  gd = yext2_get_group_desc(sb, 0); // FIXME: 0
  inode_bitmap_block = yext2_block_read(sb, le32_to_cpu(gd->bg_inode_bitmap));

  first_ino = le32_to_cpu(sbi->s_first_ino);
  inodes_per_group = le32_to_cpu(sbi->s_inodes_per_group);
  end_byte = (inodes_per_group + 7) / 8;

  /* find empty inode number from the same block of parent inode */
  parent_byte_idx = (parent - 1) / 8;

  for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
    int ino = parent_byte_idx * 8 + bit_idx + 1;
    if (ino < (int)first_ino)
      continue;
    if (!YEXT2_BIT_AT_BYTE(inode_bitmap_block[parent_byte_idx], bit_idx)) {
      inode_bitmap_block[parent_byte_idx] |= (1 << bit_idx);
      gd->bg_free_inodes_count =
          cpu_to_le16(le16_to_cpu(gd->bg_free_inodes_count) - 1);
      sbi->s_free_inodes_count =
          cpu_to_le32(le32_to_cpu(sbi->s_free_inodes_count) - 1);
      return ino;
    }
  }

  /* fallback: find empty inode number from all blocks in the same block group
   */
  start_byte = (first_ino - 1) / 8;

  for (int byte_idx = start_byte; byte_idx < end_byte; byte_idx++) {
    if (byte_idx == parent_byte_idx)
      continue; /* 上で既に試した */
    for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
      int ino = byte_idx * 8 + bit_idx + 1;
      if (ino < (int)first_ino)
        continue;
      if (!YEXT2_BIT_AT_BYTE(inode_bitmap_block[byte_idx], bit_idx)) {
        inode_bitmap_block[byte_idx] |= (1 << bit_idx);
        gd->bg_free_inodes_count =
            cpu_to_le16(le16_to_cpu(gd->bg_free_inodes_count) - 1);
        sbi->s_free_inodes_count =
            cpu_to_le32(le32_to_cpu(sbi->s_free_inodes_count) - 1);
        return ino;
      }
    }
  }

  /* TODO: 全ブロックグループから探す */

  return -ENOSPC;
}

struct inode *yext2_new_inode(struct super_block *sb, struct inode *parent,
                              umode_t mode, const char *name) {
  struct yext2_super_block *sbi;
  struct yext2_inode *yinode;
  struct inode *inode;
  int ino;

  sbi = YEXT2_SB(sb);

  if ((ino = yext2_alloc_ino(sb, parent->ino)) < 0)
    return NULL;

  yinode = yext2_get_inode(sb, ino);
  yext2_inode_init(sbi, yinode, mode);

  inode = inode_get(sb, ino);
  inode->i_private = yinode;

  return inode;
}
