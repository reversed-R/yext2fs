#include "fake_disk.h"
#include "fake_kernel_api.h"
#include "yext2.h"
#include <stdio.h>
#include <string.h>

char *yext2_fuse_fake_disk;

void yext2_super_read(struct yext2_super_block *sb) {
  memcpy(sb, yext2_fuse_fake_disk + BOOT_SECTOR_SIZE,
         sizeof(struct yext2_super_block));
}

struct yext2_block_group_desc *yext2_get_group_desc(struct super_block *sb,
                                                    unsigned int block_group) {
  struct yext2_super_block *sbi = YEXT2_SB(sb);
  /* GDT is at the block immediately after the superblock */
  u32 gdt_block = le32_to_cpu(sbi->s_first_data_block) + 1;
  char *gdt = yext2_block_read(sb, gdt_block);
  return (struct yext2_block_group_desc *)gdt + block_group;
}

char *yext2_block_read(struct super_block *sb, sector_t block) {
  struct yext2_super_block *sbi = YEXT2_SB(sb);
  int block_size = YEXT2_BLOCK_BYTE_SIZE(sbi);
  return &yext2_fuse_fake_disk[block_size * block];
}

void yext2_group_desc_print(struct yext2_block_group_desc *bg) {
  printf("----block group---->\n");
  printf("bg_block_bitmap      : %u\n", bg->bg_block_bitmap);
  printf("bg_inode_bitmap      : %u\n", bg->bg_inode_bitmap);
  printf("bg_inode_table       : %u\n", bg->bg_inode_table);
  printf("bg_free_blocks_count : %d\n", bg->bg_free_blocks_count);
  printf("bg_free_inodes_count : %d\n", bg->bg_free_inodes_count);
  printf("bg_used_dirs_count   : %d\n", bg->bg_used_dirs_count);
  printf("<----block group----\n");
}
