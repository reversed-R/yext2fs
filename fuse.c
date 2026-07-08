#include "fuse.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "fake_disk.h"
#include "fake_kernel_api.h"

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
    d_children = list_new();

    de_inode = YEXT2_INODE(dentry->d_inode);
    sbi = YEXT2_SB(sb);
    block_size = YEXT2_BLOCK_BYTE_SIZE(sbi);

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

  d_child_p = dentry->d_children;
  do {
    d_child = (struct dentry *)d_child_p->data;
    if (d_child == NULL)
      break;

    if ((de = (struct yext2_dentry *)d_child->d_fsdata) == NULL) {
      d_child_p = d_child_p->next;
      continue;
    }

    printf("de: %p\n", de);
    printf("de->name_len: %d\n", de->name_len);

    memcpy(de_name, de->name, de->name_len);
    de_name[de->name_len] = '\0';

    printf("de_name: %s\n", de_name);
    printf("current_de_name: %s\n", current_de_name);

    if (strcmp(current_de_name, de_name) == 0) {
      if (de->file_type == FT_DIR) {
        return yext2_search_dentry_of_path_fuse(sb, d_child, (path + i));
      } else {
        return NULL;
      }
    }

    d_child_p = d_child_p->next;
  } while (d_child_p != dentry->d_children);

  printf("return from yext2_search_dentry_of_path_fuse\n");

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
  memset(stbuf, 0, sizeof(struct stat));

  // FIXME: mock

  // handling of file system root
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }

  /* マウント箇所からみたパスが"/file"かを確認 */
  if (strcmp(path, "/file") == 0) {
    stbuf->st_mode = S_IFREG | 0777;  // 権限
    stbuf->st_nlink = 1;              // ハードリンクの数
    stbuf->st_size = strlen(content); // ファイルサイズ
    return 0;
  }

  return -ENOENT;
}

int yext2_open_fuse(const char *path, struct fuse_file_info *fi) { return 0; }

int yext2_read_fuse(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
  // FIXME: mock

  if (strcmp(path, "/file") == 0) {
    size_t len = strlen(content);
    if (offset >= len)
      return 0;

    /* データを返す */
    if ((size > len) || (offset + size > len)) {
      memcpy(buf, content + offset, len - offset);
      return len - offset;
    } else {
      memcpy(buf, content + offset, size);
      return size;
    }
  }

  return -ENOENT;
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

  dentry = dentry_alloc(NULL);
  memset(dentry->d_name, 0, 64);
  memcpy(dentry->d_name, path + last_slash + 1,
         last_slash == -1 ? i : i - last_slash);
  dentry->d_parent = parent;

  if ((res = yext2_mkdir(sb, parent, dentry)) < 0) {
    free(dentry);
    return res;
  }

  list_append(parent->d_children, dentry);
  return 0;
}
