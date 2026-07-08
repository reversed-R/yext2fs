#include "yext2.h"

#include "fake_kernel_api.h"
#include <stdio.h>

void yext2_super_block_print(struct yext2_super_block *sb) {
  printf("----super block---->\n");
  printf("s_inodes_count     : %u\n", le32_to_cpu(sb->s_inodes_count));
  printf("s_blocks_count     : %u\n", le32_to_cpu(sb->s_blocks_count));
  printf("s_r_blocks_count   : %u\n", le32_to_cpu(sb->s_r_blocks_count));
  printf("s_free_blocks_count: %u\n", le32_to_cpu(sb->s_free_blocks_count));
  printf("s_free_inodes_count: %u\n", le32_to_cpu(sb->s_free_inodes_count));
  printf("s_first_data_block : %u\n", le32_to_cpu(sb->s_first_data_block));
  printf("s_log_block_size   : %u\n", le32_to_cpu(sb->s_log_block_size));
  printf("s_log_frag_size    : %u\n", le32_to_cpu(sb->s_log_frag_size));
  printf("s_blocks_per_group : %u\n", le32_to_cpu(sb->s_blocks_per_group));
  printf("s_frags_per_group  : %u\n", le32_to_cpu(sb->s_frags_per_group));
  printf("s_inodes_per_group : %u\n", le32_to_cpu(sb->s_inodes_per_group));
  printf("s_mtime            : %u\n", le32_to_cpu(sb->s_mtime));
  printf("s_wtime            : %u\n", le32_to_cpu(sb->s_wtime));
  printf("s_mnt_count        : %d\n", le16_to_cpu(sb->s_mnt_count));
  printf("s_max_mnt_count    : %d\n", le16_to_cpu(sb->s_max_mnt_count));
  printf("s_magic            : %d\n", le16_to_cpu(sb->s_magic));
  printf("s_state            : %d\n", le16_to_cpu(sb->s_state));
  printf("s_errors           : %d\n", le16_to_cpu(sb->s_errors));
  printf("s_minor_rev_level  : %d\n", le16_to_cpu(sb->s_minor_rev_level));
  printf("s_lastcheck        : %u\n", le32_to_cpu(sb->s_lastcheck));
  printf("s_checkinterval    : %u\n", le32_to_cpu(sb->s_checkinterval));
  printf("s_creator_os       : %u\n", le32_to_cpu(sb->s_creator_os));
  printf("s_rev_level        : %u\n", le32_to_cpu(sb->s_rev_level));
  printf("s_def_resuid       : %d\n", le16_to_cpu(sb->s_def_resuid));
  printf("s_def_resgid       : %d\n", le16_to_cpu(sb->s_def_resgid));
  printf("s_first_ino        : %u\n", le32_to_cpu(sb->s_first_ino));
  printf("s_inode_size       : %d\n", le16_to_cpu(sb->s_inode_size));
  printf("s_block_group_nr   : %d\n", le16_to_cpu(sb->s_block_group_nr));
  printf("<----super block----\n");
}
