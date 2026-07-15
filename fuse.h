#ifndef YEXT2_FUSE_H
#define YEXT2_FUSE_H

#include <fuse.h>

int yext2_readdir_fuse(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi);

int yext2_mkdir_fuse(const char *path, mode_t mode);

int yext2_getattr_fuse(const char *path, struct stat *stbuf);

int yext2_create_fuse(const char *path, mode_t mode, struct fuse_file_info *fi);

int yext2_open_fuse(const char *path, struct fuse_file_info *fi);

int yext2_read_fuse(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi);

int yext2_write_fuse(const char *path, const char *data, size_t size,
                     off_t offset, struct fuse_file_info *fi);

#endif // !YEXT2_FUSE_H
