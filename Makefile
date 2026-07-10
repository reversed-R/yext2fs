build: main.c super.c fake_disk.c fake_kernel_api.c block.c inode.c fuse.c dentry.c file.c
	gcc main.c super.c fake_disk.c fake_kernel_api.c block.c inode.c fuse.c dentry.c file.c -o yext2 -D_FILE_OFFSET_BITS=64 -lfuse

.PHONY: build
