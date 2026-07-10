#ifndef YEXT2_H
#define YEXT2_H

#include "fake_kernel_api.h"

#define BOOT_SECTOR_SIZE 1024

#define YEXT2_BAD_INO 1 /* bad blocks inode */
#define YEXT2_ROOT_INO 2
#define YEXT2_BL_INO 5        /* boot loader inode */
#define YEXT2_UNDEL_DIR_INO 6 /* undelete directory inode */

#define YEXT2_NDIR_BLOCKS 12
#define YEXT2_IND_BLOCK YEXT2_NDIR_BLOCKS
#define YEXT2_DIND_BLOCK (YEXT2_IND_BLOCK + 1)
#define YEXT2_TIND_BLOCK (YEXT2_DIND_BLOCK + 1)
#define YEXT2_N_BLOCKS (YEXT2_TIND_BLOCK + 1)

#define YEXT2_NAME_LEN 255

/*
 * YEXT2_DIR_PAD defines the directory entries boundaries
 *
 * NOTE: It must be a multiple of 4
 */
#define YEXT2_DIR_PAD 4
#define YEXT2_DIR_ROUND (YEXT2_DIR_PAD - 1)
#define YEXT2_DIR_REC_LEN(name_len)                                            \
  (((name_len) + 8 + YEXT2_DIR_ROUND) & ~YEXT2_DIR_ROUND)
#define YEXT2_MAX_REC_LEN ((1 << 16) - 1)

struct yext2_super_block {
  le32 s_inodes_count;      /* Inodes count */
  le32 s_blocks_count;      /* Blocks count */
  le32 s_r_blocks_count;    /* Reserved blocks count */
  le32 s_free_blocks_count; /* Free blocks count */
  le32 s_free_inodes_count; /* Free inodes count */
  le32 s_first_data_block;  /* First Data Block */
  le32 s_log_block_size;    /* Block size */
  le32 s_log_frag_size;     /* Fragment size */
  le32 s_blocks_per_group;  /* # Blocks per group */
  le32 s_frags_per_group;   /* # Fragments per group */
  le32 s_inodes_per_group;  /* # Inodes per group */
  le32 s_mtime;             /* Mount time */
  le32 s_wtime;             /* Write time */
  le16 s_mnt_count;         /* Mount count */
  le16 s_max_mnt_count;     /* Maximal mount count */
  le16 s_magic;             /* Magic signature */
  le16 s_state;             /* File system state */
  le16 s_errors;            /* Behaviour when detecting errors */
  le16 s_minor_rev_level;   /* minor revision level */
  le32 s_lastcheck;         /* time of last check */
  le32 s_checkinterval;     /* max. time between checks */
  le32 s_creator_os;        /* OS */
  le32 s_rev_level;         /* Revision level */
  le16 s_def_resuid;        /* Default uid for reserved blocks */
  le16 s_def_resgid;        /* Default gid for reserved blocks */
  /*
   * These fields are for EXT2_DYNAMIC_REV superblocks only.
   *
   * Note: the difference between the compatible feature set and
   * the incompatible feature set is that if there is a bit set
   * in the incompatible feature set that the kernel doesn't
   * know about, it should refuse to mount the filesystem.
   *
   * e2fsck's requirements are more strict; if it doesn't know
   * about a feature in either the compatible or incompatible
   * feature set, it must abort and not try to meddle with
   * things it doesn't understand...
   */
  le32 s_first_ino;              /* First non-reserved inode */
  le16 s_inode_size;             /* size of inode structure */
  le16 s_block_group_nr;         /* block group # of this superblock */
  le32 s_feature_compat;         /* compatible feature set */
  le32 s_feature_incompat;       /* incompatible feature set */
  le32 s_feature_ro_compat;      /* readonly-compatible feature set */
  u8 s_uuid[16];                 /* 128-bit uuid for volume */
  char s_volume_name[16];        /* volume name */
  char s_last_mounted[64];       /* directory where last mounted */
  le32 s_algorithm_usage_bitmap; /* For compression */
  /*
   * Performance hints.  Directory preallocation should only
   * happen if the EXT2_COMPAT_PREALLOC flag is on.
   */
  u8 s_prealloc_blocks;     /* Nr of blocks to try to preallocate*/
  u8 s_prealloc_dir_blocks; /* Nr to preallocate for dirs */
  u16 s_padding1;
  /*
   * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
   */
  u8 s_journal_uuid[16]; /* uuid of journal superblock */
  u32 s_journal_inum;    /* inode number of journal file */
  u32 s_journal_dev;     /* device number of journal file */
  u32 s_last_orphan;     /* start of list of inodes to delete */
  u32 s_hash_seed[4];    /* HTREE hash seed */
  u8 s_def_hash_version; /* Default hash version to use */
  u8 s_reserved_char_pad;
  u16 s_reserved_word_pad;
  le32 s_default_mount_opts;
  le32 s_first_meta_bg; /* First metablock block group */
  u32 s_reserved[190];  /* Padding to the end of the block */
};

struct yext2_block_group_desc {
  le32 bg_block_bitmap;      /* Blocks bitmap block */
  le32 bg_inode_bitmap;      /* Inodes bitmap block */
  le32 bg_inode_table;       /* Inodes table block */
  le16 bg_free_blocks_count; /* Free blocks count */
  le16 bg_free_inodes_count; /* Free inodes count */
  le16 bg_used_dirs_count;   /* Directories count */
  le16 bg_pad;
  le32 bg_reserved[3];
};

struct yext2_inode {
  le16 i_mode;        /* File mode */
  le16 i_uid;         /* Low 16 bits of Owner Uid */
  le32 i_size;        /* Size in bytes */
  le32 i_atime;       /* Access time */
  le32 i_ctime;       /* Creation time */
  le32 i_mtime;       /* Modification time */
  le32 i_dtime;       /* Deletion Time */
  le16 i_gid;         /* Low 16 bits of Group Id */
  le16 i_links_count; /* Links count */
  le32 i_blocks;      /* Blocks count */
  le32 i_flags;       /* File flags */

  struct { /* OS dependent 1 */
    le32 l_i_reserved1;
  } linux1;

  le32 i_block[YEXT2_N_BLOCKS]; /* Pointers to blocks */
  le32 i_generation;            /* File version (for NFS) */
  le32 i_file_acl;              /* File ACL */
  le32 i_dir_acl;               /* Directory ACL */
  le32 i_faddr;                 /* Fragment address */

  struct {        /* OS dependent 2 */
    u8 l_i_frag;  /* Fragment number */
    u8 l_i_fsize; /* Fragment size */
    u16 i_pad1;
    le16 l_i_uid_high; /* these 2 fields    */
    le16 l_i_gid_high; /* were reserved2[0] */
    u32 l_i_reserved2;
  } linux2;
};

// same as version 2 structure (fs/ext2/ext2.h :: struct ext2_dir_entry_2)
struct yext2_dentry {
  le32 inode;   /* Inode number */
  le16 rec_len; /* Directory entry length */
  u8 name_len;  /* Name length */
  u8 file_type;
  char name[]; /* File name, up to YEXT2_NAME_LEN */
};

#define YEXT2_SB(sb) ((struct yext2_super_block *)sb->s_fs_info)

#define YEXT2_INODE(inode) ((struct yext2_inode *)inode->i_private)

#define YEXT2_DENTRY(dentry) ((struct yext2_dentry *)dentry->d_fsdata)

// NOTE: block size is calculated from:
// 1024 (ext2 minimal block size) * 2 ^ sbi->s_log_block_size
#define YEXT2_BLOCK_BYTE_SIZE(sbi) (1024 << le32_to_cpu(sbi->s_log_block_size))

void yext2_super_block_print(struct yext2_super_block *sb);

void yext2_group_desc_print(struct yext2_block_group_desc *bg);

void yext2_inode_print(struct yext2_inode *inode);

void yext2_dentry_print(struct yext2_dentry *dentry);

int yext2_create(struct super_block *sb, struct inode *dir,
                 struct dentry *dentry, umode_t mode, bool excl);

int yext2_readdir(struct file *file, struct dir_context *ctx);

int yext2_mkdir(struct super_block *sb, struct dentry *parent,
                struct dentry *dentry);

// alloc inode number
int yext2_alloc_ino(struct super_block *sb, ino_t parent);

long yext2_alloc_block(struct super_block *sb, unsigned long goal);

void yext2_inode_init(struct yext2_super_block *sbi, struct yext2_inode *inode,
                      umode_t mode);

void yext2_inode_inc_link_count(struct yext2_inode *inode);

void yext2_inode_dec_link_count(struct yext2_inode *inode);

int yext2_add_link(struct super_block *sb, struct dentry *dentry,
                   struct inode *inode);

struct inode *yext2_new_inode(struct super_block *sb, struct inode *parent,
                              umode_t mode, const char *name);

//  <------------ byte ------------>
// | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// <-->
// bit
#define YEXT2_BIT_AT_BYTE(byte, bit) ((byte) & (1 << (bit)))

#define YEXT2_INO_BLOCK_GROUP(sbi, ino)                                        \
  ((ino - 1) / le32_to_cpu(sbi->s_inodes_per_group))

u32 yext2_get_time_sec();

#endif // !YEXT2_H
#define YEXT2_H
