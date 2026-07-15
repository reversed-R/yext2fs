#include "fuse.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "fake_disk.h"
#include "fake_kernel_api.h"
#include "yext2.h"

#define max(x, y) ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) > (y) ? (y) : (x))

static const char *content = "Hello, World!\n";

extern struct super_block *sb;

static void yext2_load_dir_children(struct super_block *sb,
                                    struct dentry *dentry) {
  struct yext2_inode *de_inode, *child_inode;
  struct yext2_super_block *sbi;
  unsigned long block, index, block_size;
  char *dentry_block;
  struct yext2_dentry *de;
  char de_name[YEXT2_NAME_LEN + 1];
  struct list *d_children = NULL;
  struct inode *inode;
  struct dentry *d_child;
  int truncated_name_len;

  // if not cached yet in fake kernel dentry
  if (dentry->d_children == NULL) {
    de_inode = YEXT2_INODE(dentry->d_inode);
    sbi = YEXT2_SB(sb);
    block_size = YEXT2_BLOCK_BYTE_SIZE(sbi);

    if (!S_ISDIR(de_inode->i_mode))
      return;

    d_children = list_new();

    for (int i = 0; i < le32_to_cpu(de_inode->i_size) / block_size; i++) {
      block = le32_to_cpu(de_inode->i_block[i]);
      dentry_block = yext2_block_read(sb, block);

      index = 0;
      while (index < block_size) {
        de = (struct yext2_dentry *)(dentry_block + index);

        if (le16_to_cpu(de->rec_len) == 0)
          break;

        yext2_dentry_print(de);

        child_inode = yext2_get_inode(sb, le32_to_cpu(de->inode));
        printf("child_inode: %p\n", child_inode);
        inode = inode_get(sb, le32_to_cpu(de->inode));
        printf("inode: %p\n", inode);
        inode->i_private = child_inode;

        d_child = dentry_alloc(inode);
        printf("d_child: %p\n", d_child);
        d_child->d_parent = dentry;
        truncated_name_len = de->name_len > sizeof(d_child->d_name) - 1
                                 ? sizeof(d_child->d_name) - 1
                                 : de->name_len;
        memcpy(d_child->d_name, de->name, truncated_name_len);
        printf("truncated_name_len: %d\n", truncated_name_len);
        d_child->d_name[truncated_name_len] = '\0';
        printf("d_child->d_name: %s\n", d_child->d_name);
        d_child->d_fsdata = de;

        list_append(d_children, d_child);
        printf("list_append\n");

        index += le16_to_cpu(de->rec_len);
      }
    }

    dentry->d_children = d_children;
  }
}

// search path-matched dentry child recursively.
// dentry will be updated as path-matched dentry.
//
// # Example:
//  struct dentry *dentry = root; // filesystem root dentry ('/')
//  struct dentry *target_dentry
//    = yext2_search_dentry_of_path_fuse(sb, dentry, '/foo/bar');
//
//  // now, target_dentry is of '/foo/bar'
//  // and target_dentry.d_children are children entries of '/foo/bar'
//    ('/foo/bar/*')
static struct dentry *yext2_search_dentry_of_path_fuse(struct super_block *sb,
                                                       struct dentry *dentry,
                                                       const char *path) {
  char current_de_name[YEXT2_NAME_LEN + 1], de_name[YEXT2_NAME_LEN + 1];
  int i;
  struct list *d_child_p;
  struct dentry *d_child;
  struct yext2_dentry *de;

  yext2_load_dir_children(sb, dentry);
  printf("yext2_load_dir_children: dentry: %p\n", dentry);

  i = 0;
  while (path[i] != '\0' && path[i] != '/' && i < YEXT2_NAME_LEN)
    i++;

  memcpy(current_de_name, path, i);
  current_de_name[i] = '\0';

  printf("current_de_name: %s, i: %d\n", current_de_name, i);

  // if path is ''
  if (i == 0 && path[0] == '\0')
    return dentry;

  // if starts with '/'
  if (i == 0 && path[0] == '/')
    return yext2_search_dentry_of_path_fuse(sb, dentry, (path + 1));

  // if d_children is NULL after load_dir_children, dentry is not directory.
  if (dentry->d_children == NULL)
    return NULL;

  d_child_p = dentry->d_children;
  do {
    d_child = (struct dentry *)d_child_p->data;

    if (d_child == NULL ||
        (de = (struct yext2_dentry *)d_child->d_fsdata) == NULL) {
      d_child_p = d_child_p->next;
      continue;
    }

    memcpy(de_name, de->name, de->name_len);
    de_name[de->name_len] = '\0';

    printf("de_name: %s\n", de_name);

    if (strcmp(current_de_name, de_name) == 0) {
      if (de->file_type == FT_DIR) {
        return yext2_search_dentry_of_path_fuse(sb, d_child, (path + i));
      } else if (path[i] == '\0') {
        // path の末尾において、ディレクトリ以外を発見したらそれを返す
        return d_child;
      } else {
        return NULL;
      }
    }

    d_child_p = d_child_p->next;
  } while (d_child_p != dentry->d_children);

  return NULL;
}

int yext2_readdir_fuse(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
  struct dentry *dentry, *d_child;
  struct list *d_child_p;

  printf("path: %s\n", path);

  dentry = yext2_search_dentry_of_path_fuse(sb, sb->s_root, path);
  if (dentry == NULL)
    return -ENOENT;

  if (dentry->d_children == NULL) {
    printf("failed\n");
    return -ENOENT;
  }

  d_child_p = dentry->d_children;
  do {
    if (d_child_p->data != NULL) {
      d_child = (struct dentry *)d_child_p->data;
      printf("d_name: %s\n", d_child->d_name);
      filler(buf, d_child->d_name, NULL, 0);
    }

    d_child_p = d_child_p->next;
  } while (d_child_p != dentry->d_children);

  return 0;
}

int yext2_getattr_fuse(const char *path, struct stat *stbuf) {
  struct dentry *dentry;
  struct yext2_dentry *de;
  struct yext2_inode *yinode;
  int ino;

  printf("path: %s\n", path);

  memset(stbuf, 0, sizeof(struct stat));

  dentry = yext2_search_dentry_of_path_fuse(sb, sb->s_root, path);
  printf("getattr: dentry: %p\n", dentry);
  if (dentry == NULL)
    return -ENOENT;

  de = YEXT2_DENTRY(dentry);
  printf("de: %p\n", de);
  ino = le32_to_cpu(de->inode);
  yinode = yext2_get_inode(sb, ino);
  if (yinode == NULL)
    return -ENOENT;

  stbuf->st_mode = le16_to_cpu(yinode->i_mode);
  stbuf->st_nlink = le16_to_cpu(yinode->i_links_count);
  stbuf->st_size = le32_to_cpu(yinode->i_size);
  stbuf->st_blocks = le32_to_cpu(yinode->i_blocks);

  printf("st_mode: %x\n", le16_to_cpu(yinode->i_mode));
  printf("st_nlink: %d\n", le16_to_cpu(yinode->i_links_count));
  printf("st_size: %d\n", le32_to_cpu(yinode->i_size));
  printf("st_blocks: %d\n", le32_to_cpu(yinode->i_blocks));

  return 0;
}

int yext2_create_fuse(const char *path, mode_t mode,
                      struct fuse_file_info *fi) {
  struct dentry *parent, *dentry;
  struct list *d_child_p;
  int last_slash = -1;
  int i = 0;
  char *tmp_path; // temporaly path buffer which the deepest dir is cut
  int res;

  printf("create: path: %s\n", path);

  // cut the deepest dir
  // foo/bar/baz
  //         ^^^ 今作る部分
  //        ^
  //        | last_slash
  // ^^^^^^^
  // | 現在存在している部分
  while (path[i]) {
    if (path[i] == '/')
      last_slash = i;

    i++;
  }
  if ((tmp_path = malloc(last_slash + 1)) == NULL)
    return -ENOMEM;
  memcpy(tmp_path, path, last_slash);
  tmp_path[last_slash] = '\0';

  printf("create: parent path: %s\n", tmp_path);

  parent = yext2_search_dentry_of_path_fuse(sb, sb->s_root, tmp_path);
  printf("create: parent: %p\n", parent);
  free(tmp_path);
  if (parent == NULL)
    return -ENOENT;

  // check parent directory already has dentry of `path`
  d_child_p = parent->d_children;
  printf("create: d_child_p: %p\n", d_child_p);
  do {
    dentry = (struct dentry *)d_child_p->data;

    if (dentry == NULL) {
      d_child_p = d_child_p->next;
      continue;
    }

    printf("dentry->d_name: %s\n", dentry->d_name);

    if (strcmp(dentry->d_name, (path + last_slash)) == 0) {
      return -EEXIST;
    }

    d_child_p = d_child_p->next;
  } while (d_child_p != parent->d_children);

  dentry = dentry_alloc(NULL);
  memset(dentry->d_name, 0, 64);
  memcpy(dentry->d_name, path + last_slash + 1,
         last_slash == -1 ? (i < 64 ? i : 63)
                          : (i - last_slash < 64 ? i - last_slash : 63));
  dentry->d_parent = parent;

  if ((res = yext2_create(sb, parent->d_inode, dentry, mode, false)) < 0) {
    free(dentry);
    return res;
  }

  list_append(parent->d_children, dentry);

  return 0;
}

int yext2_open_fuse(const char *path, struct fuse_file_info *fi) { return 0; }

int yext2_read_fuse(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
  struct dentry *dentry;
  struct yext2_super_block *sbi;
  struct yext2_inode *yinode;
  int i_block, offset_in_block;
  u32 block, file_size, block_size, readable_size;
  char *data_block;

  sbi = YEXT2_SB(sb);
  block_size = YEXT2_BLOCK_BYTE_SIZE(sbi);
  i_block = offset / block_size;
  offset_in_block = offset % block_size;

  dentry = yext2_search_dentry_of_path_fuse(sb, sb->s_root, path);
  if (dentry == NULL)
    return -ENOENT;

  yinode = YEXT2_INODE(dentry->d_inode);
  file_size = le32_to_cpu(yinode->i_size);
  if (offset > file_size)
    return 0;

  block = yext2_get_block_from_i_block(sb, yinode, i_block);
  if (block == 0)
    return 0; // nothing to read

  readable_size =
      min((file_size - offset), min(size, (block_size - offset_in_block)));
  data_block = yext2_block_read(sb, block);
  memcpy(buf, data_block + offset_in_block, readable_size);

  return readable_size;
}

int yext2_write_fuse(const char *path, const char *data, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
  struct dentry *dentry;
  struct inode *inode;
  struct yext2_super_block *sbi;
  struct yext2_inode *yinode;
  int i_block, offset_in_block, block_size;
  u32 block, goal;
  long tmp_block;

  sbi = YEXT2_SB(sb);
  block_size = YEXT2_BLOCK_BYTE_SIZE(sbi);
  i_block = offset / block_size;
  offset_in_block = offset % block_size;

  dentry = yext2_search_dentry_of_path_fuse(sb, sb->s_root, path);
  printf("write: dentry: %p\n", dentry);
  if (dentry == NULL)
    return -ENOENT;
  inode = dentry->d_inode;
  yinode = YEXT2_INODE(inode);

  printf("write: i_block: %d\n", i_block);

  block = yext2_get_block_from_i_block(sb, yinode, i_block);
  printf("write: block: %d\n", block);
  if (block == 0) {
    // block not yet allcated, so need to allocate.
    goal = yext2_find_near(sbi, inode, i_block);
    tmp_block = yext2_alloc_block(sb, goal);
    if (tmp_block < 0)
      return -ENOSPC;
    block = (u32)tmp_block;
    yinode->i_block[i_block] = cpu_to_le32(block);
    yinode->i_blocks = cpu_to_le32(le32_to_cpu(yinode->i_blocks) +
                                   YEXT2_BLOCK_BYTE_SIZE(sbi) / SECTOR_SIZE);
  }

  printf("write: (2) block: %d\n", block);

  yext2_block_write(sb, block, offset_in_block, data, size);

  yinode->i_size = cpu_to_le32(max(le32_to_cpu(yinode->i_size), offset + size));

  return size;
}

int yext2_mkdir_fuse(const char *path, mode_t mode) {
  struct dentry *parent, *dentry;
  int last_slash = -1;
  int i = 0;
  char *tmp_path; // temporaly path buffer which the deepest dir is cut
  int res;

  printf("path: %s\n", path);

  // cut the deepest dir
  // foo/bar/baz
  //         ^^^ 今作る部分
  //        ^
  //        | last_slash
  // ^^^^^^^
  // | 現在存在している部分
  while (path[i]) {
    if (path[i] == '/')
      last_slash = i;

    i++;
  }
  if ((tmp_path = malloc(last_slash + 1)) == NULL)
    return -ENOMEM;
  memcpy(tmp_path, path, last_slash);
  tmp_path[last_slash] = '\0';

  parent = yext2_search_dentry_of_path_fuse(sb, sb->s_root, tmp_path);
  free(tmp_path);
  if (parent == NULL)
    return -ENOENT;

  dentry = dentry_alloc(NULL);
  memset(dentry->d_name, 0, 64);
  memcpy(dentry->d_name, path + last_slash + 1,
         last_slash == -1 ? (i < 64 ? i : 63)
                          : (i - last_slash < 64 ? i - last_slash : 63));
  dentry->d_parent = parent;

  if ((res = yext2_mkdir(sb, parent, dentry)) < 0) {
    free(dentry);
    return res;
  }

  list_append(parent->d_children, dentry);
  return 0;
}
