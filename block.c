#include "fake_disk.h"
#include "fake_kernel_api.h"

// グループ内の実ブロック数
static unsigned long blocks_count_in_group(struct yext2_super_block *sbi,
                                           unsigned long group) {
  unsigned long blocks_per_group = le32_to_cpu(sbi->s_blocks_per_group),
                blocks_count = le32_to_cpu(sbi->s_blocks_count),
                first_data_block = le32_to_cpu(sbi->s_first_data_block);

  unsigned long remain_blocks_count =
      blocks_count - first_data_block - group * blocks_per_group;
  if (remain_blocks_count > blocks_per_group)
    return blocks_per_group;

  return remain_blocks_count;
}

long yext2_alloc_block(struct super_block *sb, unsigned long goal) {
  struct yext2_super_block *sbi;
  struct yext2_block_group_desc *gd;
  char *block_bitmap_block;
  unsigned long goal_block_group, goal_in_group, blocks_per_group,
      first_data_block, byte_offset_in_block, bit_offset, groups_count,
      blocks_count_in_current_group, res_block_group;
  long res = -1;

  sbi = YEXT2_SB(sb);
  blocks_per_group = le32_to_cpu(sbi->s_blocks_per_group);
  first_data_block = le32_to_cpu(sbi->s_first_data_block);
  goal_block_group = (goal - first_data_block) / blocks_per_group;

  /* check whether `goal` block is free or not */
  gd = yext2_get_group_desc(sb, goal_block_group);
  block_bitmap_block = yext2_block_read(sb, le32_to_cpu(gd->bg_block_bitmap));
  goal_in_group = (goal - first_data_block) % blocks_per_group;
  byte_offset_in_block = goal_in_group / 8;
  bit_offset = goal_in_group % 8;

  if (!YEXT2_BIT_AT_BYTE(*(block_bitmap_block + byte_offset_in_block),
                         bit_offset)) {
    *(block_bitmap_block + byte_offset_in_block) |= (1 << bit_offset);
    res = goal;
    goto success;
  }

  /* find empty block in the same block group of `goal` */
  blocks_count_in_current_group = blocks_count_in_group(sbi, goal_block_group);
  for (int i = 0; i < blocks_count_in_current_group; i++) {
    byte_offset_in_block = i / 8;
    bit_offset = i % 8;
    if (!YEXT2_BIT_AT_BYTE(*(block_bitmap_block + byte_offset_in_block),
                           bit_offset)) {
      *(block_bitmap_block + byte_offset_in_block) |= (1 << bit_offset);
      res = first_data_block + goal_block_group * blocks_per_group + i;
      goto success;
    }
  };

  /* fallback: find epmty block in all blocks */
  groups_count = (le32_to_cpu(sbi->s_blocks_count) - first_data_block - 1) /
                     blocks_per_group +
                 1;
  for (int i = 0; i < groups_count; i++) {
    gd = yext2_get_group_desc(sb, i);
    block_bitmap_block = yext2_block_read(sb, le32_to_cpu(gd->bg_block_bitmap));
    blocks_count_in_current_group = blocks_count_in_group(sbi, i);

    for (int j = 0; j < blocks_count_in_current_group; j++) {
      byte_offset_in_block = j / 8;
      bit_offset = j % 8;
      if (!YEXT2_BIT_AT_BYTE(*(block_bitmap_block + byte_offset_in_block),
                             bit_offset)) {
        *(block_bitmap_block + byte_offset_in_block) |= (1 << bit_offset);
        res = first_data_block + (unsigned long)i * blocks_per_group + j;
        goto success;
      }
    };
  }

  // failed to find block
  return -1;

success:
  // TODO: decrement function
  res_block_group = (res - first_data_block) / blocks_per_group;
  gd = yext2_get_group_desc(sb, res_block_group);
  gd->bg_free_blocks_count--;

  sbi->s_free_blocks_count--;

  return res;
}
