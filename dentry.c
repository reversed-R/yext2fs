#include "fake_disk.h"
#include "fake_kernel_api.h"
#include "yext2.h"
#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void yext2_dentry_print(struct yext2_dentry *dentry) {
  char de_name[YEXT2_NAME_LEN + 1];

  memcpy(de_name, dentry->name, dentry->name_len);
  de_name[dentry->name_len] = '\0';

  printf("dentry---->\n");
  printf("inode: %u\n", le32_to_cpu(dentry->inode));
  printf("file_type: %d\n", dentry->file_type);
  printf("name_len: %d\n", dentry->name_len);
  printf("rec_len: %d\n", le16_to_cpu(dentry->rec_len));
  printf("name: %s\n", de_name);
  printf("<----dentry\n");
}

static int yext2_make_empty_dir_block(struct super_block *sb, int block,
                                      ino_t ino, struct inode *parent) {
  char *dir_block;
  struct yext2_super_block *sbi;
  struct yext2_dentry *de;
  struct yext2_inode *parent_inode;

  sbi = YEXT2_SB(sb);
  dir_block = yext2_block_read(sb, block);
  parent_inode = YEXT2_INODE(parent);

  // '.'
  de = (struct yext2_dentry *)dir_block;
  de->name_len = 1;
  de->rec_len = cpu_to_le16(YEXT2_DIR_REC_LEN(1));
  memcpy(de->name, ".\0\0", 4);
  de->inode = cpu_to_le32(ino);
  de->file_type = FT_DIR;

  // '..'
  de = (struct yext2_dentry *)(dir_block + YEXT2_DIR_REC_LEN(1));
  de->name_len = 2;
  // ext2の仕様上、最終dentryはブロック末端まで長さがあるとする
  de->rec_len = cpu_to_le16(YEXT2_BLOCK_BYTE_SIZE(sbi) - YEXT2_DIR_REC_LEN(1));
  memcpy(de->name, "..\0", 4);
  de->inode = cpu_to_le32(parent->ino);
  de->file_type = fs_umode_to_ftype(parent_inode->i_mode);

  return 0;
}

// @dentry: 新たに作られるディレクトリエントリ
// @inode: 新たに作られるディレクトリエントリのために割り当てられたinode
int yext2_add_link(struct super_block *sb, struct dentry *dentry,
                   struct inode *inode) {
  struct yext2_super_block *sbi = YEXT2_SB(sb);
  struct dentry *parent = dentry->d_parent;
  struct yext2_inode *parent_yinode = YEXT2_INODE(parent->d_inode),
                     *yinode = YEXT2_INODE(inode);
  const char *name = dentry->d_name;
  int namelen = strlen(name);
  unsigned reclen = YEXT2_DIR_REC_LEN(namelen);
  unsigned block_size = YEXT2_BLOCK_BYTE_SIZE(sbi);
  int nblocks = le32_to_cpu(parent_yinode->i_size) / block_size;
  struct yext2_dentry *de;

  for (int b = 0; b < nblocks; b++) {
    char *block = yext2_block_read(sb, le32_to_cpu(parent_yinode->i_block[b]));
    de = (struct yext2_dentry *)block;
    unsigned int index = 0;

    while (index < block_size) {
      unsigned name_len = YEXT2_DIR_REC_LEN(de->name_len);
      unsigned rec_len = le16_to_cpu(de->rec_len);

      // TODO: block が埋まっているので新しいblockを追加するパターンを追加すべき

      if (!de->inode && rec_len >= reclen) {
        // 削除済みエントリなので再利用
        goto got_it;
      }

      if (rec_len >= name_len + reclen) {
        // 末尾のエントリ
        // 既存エントリを最小サイズにし、その後ろに新しいエントリを配置
        de->rec_len =
            cpu_to_le16(name_len); // 既存エントリは名前の長さ分の最小サイズに
        de = (struct yext2_dentry *)((char *)de +
                                     name_len); // 新しいエントリの位置
        de->rec_len = cpu_to_le16(
            rec_len - name_len); // 新しいエントリのサイズはブロックの残りすべて
        goto got_it;
      }

      index += rec_len;
      de = (struct yext2_dentry *)(block + index);
    }
  }

  return -EINVAL;

got_it:
  de->name_len = namelen;
  memcpy(de->name, name, namelen);
  de->inode = cpu_to_le32(dentry->d_inode->ino);
  de->file_type = fs_umode_to_ftype(yinode->i_mode);

  // FUSE: set pointer of disk dentry as fsdata
  dentry->d_fsdata = de;

  return 0;
}

// @parent: すべての値が設定されているべき
// @dentry: d_name のみ設定されていれば良い
int yext2_mkdir(struct super_block *sb, struct dentry *parent,
                struct dentry *dentry) {
  struct inode *inode;
  struct yext2_super_block *sbi;
  struct yext2_block_group_desc *gdt;
  struct yext2_inode *parent_yinode, *yinode;
  unsigned long dentry_block, goal;
  long tmp_dentry_block;

  sbi = YEXT2_SB(sb);
  parent_yinode = YEXT2_INODE(parent->d_inode);

  if ((inode = yext2_new_inode(sb, parent->d_inode, (S_IFDIR | 0755),
                               dentry->d_name)) == NULL)
    return -EFAULT;

  yinode = YEXT2_INODE(inode);

  /* allocate dentry block */
  goal = le32_to_cpu(sbi->s_first_data_block) +
         YEXT2_INO_BLOCK_GROUP(sbi, inode->ino) *
             le32_to_cpu(sbi->s_blocks_per_group);
  if ((tmp_dentry_block = yext2_alloc_block(sb, goal)) < 0)
    return -1;
  dentry_block = (unsigned long)tmp_dentry_block;

  if (yext2_make_empty_dir_block(sb, dentry_block, inode->ino,
                                 parent->d_inode) < 0)
    return -1;

  yinode->i_block[0] = cpu_to_le32(dentry_block);
  /* ---- allocate dentry block */

  d_instantiate(dentry, inode);
  dentry->d_parent = parent;
  if (yext2_add_link(sb, dentry, inode) < 0) {
    d_instantiate(dentry, NULL);
    free(inode);
  }

  // . and ..
  yext2_inode_inc_link_count(yinode);
  yext2_inode_inc_link_count(yinode);

  // . to ..
  yext2_inode_inc_link_count(parent_yinode);

  gdt = yext2_get_group_desc(sb, YEXT2_INO_BLOCK_GROUP(sbi, inode->ino));
  gdt->bg_used_dirs_count =
      cpu_to_le16(le16_to_cpu(gdt->bg_used_dirs_count) + 1);

  return 0;
}
