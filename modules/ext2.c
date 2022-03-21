/**
 * @file modules/ext2.c
 * @brief Implementation of the Ext2 filesystem.
 * @package x86_64
 *
 * @warning There are many known bugs in this implementation.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2014-2021 K. Lange <klange@toaruos.org>
 */
#include <errno.h>
#include <kernel/types.h>
#include <kernel/vfs.h>
#include <kernel/printf.h>
#include <kernel/time.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <kernel/tokenize.h>
#include <kernel/module.h>
#include <kernel/mutex.h>

#include <sys/ioctl.h>

#define debug_print(lvl, str, ...) do { if (this->flags & EXT2_FLAG_LOUD) { printf("ext2: %s: " str "\n", #lvl __VA_OPT__(,) __VA_ARGS__); } } while (0)

#define EXT2_SUPER_MAGIC 0xEF53
#define EXT2_DIRECT_BLOCKS 12

/* Super block struct. */
struct ext2_superblock {
	uint32_t inodes_count;
	uint32_t blocks_count;
	uint32_t r_blocks_count;
	uint32_t free_blocks_count;
	uint32_t free_inodes_count;
	uint32_t first_data_block;
	uint32_t log_block_size;
	uint32_t log_frag_size;
	uint32_t blocks_per_group;
	uint32_t frags_per_group;
	uint32_t inodes_per_group;
	uint32_t mtime;
	uint32_t wtime;

	uint16_t mnt_count;
	uint16_t max_mnt_count;
	uint16_t magic;
	uint16_t state;
	uint16_t errors;
	uint16_t minor_rev_level;

	uint32_t lastcheck;
	uint32_t checkinterval;
	uint32_t creator_os;
	uint32_t rev_level;

	uint16_t def_resuid;
	uint16_t def_resgid;

	/* EXT2_DYNAMIC_REV */
	uint32_t first_ino;
	uint16_t inode_size;
	uint16_t block_group_nr;
	uint32_t feature_compat;
	uint32_t feature_incompat;
	uint32_t feature_ro_compat;

	uint8_t uuid[16];
	uint8_t volume_name[16];

	uint8_t last_mounted[64];

	uint32_t algo_bitmap;

	/* Performance Hints */
	uint8_t prealloc_blocks;
	uint8_t prealloc_dir_blocks;
	uint16_t _padding;

	/* Journaling Support */
	uint8_t journal_uuid[16];
	uint32_t journal_inum;
	uint32_t jounral_dev;
	uint32_t last_orphan;

	/* Directory Indexing Support */
	uint32_t hash_seed[4];
	uint8_t def_hash_version;
	uint16_t _padding_a;
	uint8_t _padding_b;

	/* Other Options */
	uint32_t default_mount_options;
	uint32_t first_meta_bg;
	uint8_t _unused[760];

} __attribute__ ((packed));

typedef struct ext2_superblock ext2_superblock_t;

/* Block group descriptor. */
struct ext2_bgdescriptor {
	uint32_t block_bitmap;
	uint32_t inode_bitmap;		// block no. of inode bitmap
	uint32_t inode_table;
	uint16_t free_blocks_count;
	uint16_t free_inodes_count;
	uint16_t used_dirs_count;
	uint16_t pad;
	uint8_t reserved[12];
} __attribute__ ((packed));

typedef struct ext2_bgdescriptor ext2_bgdescriptor_t;

/* File Types */
#define EXT2_S_IFSOCK	0xC000
#define EXT2_S_IFLNK	0xA000
#define EXT2_S_IFREG	0x8000
#define EXT2_S_IFBLK	0x6000
#define EXT2_S_IFDIR	0x4000
#define EXT2_S_IFCHR	0x2000
#define EXT2_S_IFIFO	0x1000

/* setuid, etc. */
#define EXT2_S_ISUID	0x0800
#define EXT2_S_ISGID	0x0400
#define EXT2_S_ISVTX	0x0200

/* rights */
#define EXT2_S_IRUSR	0x0100
#define EXT2_S_IWUSR	0x0080
#define EXT2_S_IXUSR	0x0040
#define EXT2_S_IRGRP	0x0020
#define EXT2_S_IWGRP	0x0010
#define EXT2_S_IXGRP	0x0008
#define EXT2_S_IROTH	0x0004
#define EXT2_S_IWOTH	0x0002
#define EXT2_S_IXOTH	0x0001

/* This is not actually the inode table.
 * It represents an inode in an inode table on disk. */
struct ext2_inodetable {
	uint16_t mode;
	uint16_t uid;
	uint32_t size;			// file length in byte.
	uint32_t atime;
	uint32_t ctime;
	uint32_t mtime;
	uint32_t dtime;
	uint16_t gid;
	uint16_t links_count;
	uint32_t blocks;
	uint32_t flags;
	uint32_t osd1;
	uint32_t block[15];
	uint32_t generation;
	uint32_t file_acl;
	uint32_t dir_acl;
	uint32_t faddr;
	uint8_t osd2[12];
} __attribute__ ((packed));

typedef struct ext2_inodetable ext2_inodetable_t;

/* Represents directory entry on disk. */
struct ext2_dir {
	uint32_t inode;
	uint16_t rec_len;
	uint8_t name_len;
	uint8_t file_type;
	char name[];		/* Actually a set of characters, at most 255 bytes */
} __attribute__ ((packed));

typedef struct ext2_dir ext2_dir_t;

#define EXT2_BGD_BLOCK 2

#define E_SUCCESS   0
#define E_BADBLOCK  1
#define E_NOSPACE   2
#define E_BADPARENT 3

#undef _symlink
#define _symlink(inode) ((char *)(inode)->block)

/*
 * EXT2 filesystem object
 */
typedef struct {
	ext2_superblock_t       * superblock;          /* Device superblock, contains important information */
	ext2_bgdescriptor_t     * block_groups;        /* Block Group Descriptor / Block groups */
	fs_node_t               * root_node;           /* Root FS node (attached to mountpoint) */

	fs_node_t               * block_device;        /* Block device node XXX unused */

	unsigned int              block_size;          /* Size of one block */
	unsigned int              pointers_per_block;  /* Number of pointers that fit in a block */
	unsigned int              inodes_per_group;    /* Number of inodes in a "group" */
	unsigned int              block_group_count;   /* Number of blocks groups */

	uint8_t                   bgd_block_span;
	uint8_t                   bgd_offset;
	unsigned int              inode_size;

	uint8_t *                 cache_data;

	int flags;

	sched_mutex_t *           mutex;
} ext2_fs_t;

#define EXT2_FLAG_READWRITE 0x0002
#define EXT2_FLAG_LOUD      0x0004

/*
 * These macros were used in the original toaru ext2 driver.
 * They make referring to some of the core parts of the drive a bit easier.
 */
#define BGDS (this->block_group_count)
#define SB   (this->superblock)
#define BGD  (this->block_groups)
#define RN   (this->root_node)

/*
 * These macros deal with the block group descriptor bitmap
 */
#define BLOCKBIT(n)  (bg_buffer[((n) >> 3)] & (1 << (((n) % 8))))
#define BLOCKBYTE(n) (bg_buffer[((n) >> 3)])
#define SETBIT(n)    (1 << (((n) % 8)))

static int node_from_file(ext2_fs_t * this, ext2_inodetable_t *inode, ext2_dir_t *direntry,  fs_node_t *fnode);
static int ext2_root(ext2_fs_t * this, ext2_inodetable_t *inode, fs_node_t *fnode);
static ext2_inodetable_t * read_inode(ext2_fs_t * this, size_t inode);
static void refresh_inode(ext2_fs_t * this, ext2_inodetable_t * inodet,  size_t inode);
static int write_inode(ext2_fs_t * this, ext2_inodetable_t *inode, size_t index);
static fs_node_t * finddir_ext2(fs_node_t *node, char *name);
static size_t allocate_block(ext2_fs_t * this);

/**
 * ext2->rewrite_superblock Rewrite the superblock.
 *
 * Superblocks are a bit different from other blocks, as they are always in the same place,
 * regardless of what the filesystem block size is. This doesn't work well with our setup,
 * so we need to special-case it.
 */
static int rewrite_superblock(ext2_fs_t * this) {
	write_fs(this->block_device, 1024, sizeof(ext2_superblock_t), (uint8_t *)SB);
	return E_SUCCESS;
}

/**
 * ext2->read_block Read a block from the block device associated with this filesystem.
 *
 * The read block will be copied into the buffer pointed to by `buf`.
 *
 * @param block_no Number of block to read.
 * @param buf      Where to put the data read.
 * @returns Error code or E_SUCCESS
 */
static int read_block(ext2_fs_t * this, unsigned int block_no, uint8_t * buf) {
	/* 0 is an invalid block number. So is anything beyond the total block count, but we can't check that. */
	if (!block_no) {
		return E_BADBLOCK;
	}

	/* In such cases, we read directly from the block device */
	read_fs(this->block_device, block_no * this->block_size, this->block_size, (uint8_t *)buf);

	/* And return SUCCESS */
	return E_SUCCESS;
}

/**
 * ext2->write_block Write a block to the block device.
 *
 * @param block_no Block to write
 * @param buf      Data in the block
 * @returns Error code or E_SUCCESSS
 */
static int write_block(ext2_fs_t * this, unsigned int block_no, uint8_t *buf) {
	if (!block_no) {
		debug_print(ERROR, "Attempted to write to block #0. Enable tracing and retry this operation.");
		debug_print(ERROR, "Your file system is most likely corrupted now.");
		return E_BADBLOCK;
	}

	/* This operation requires the filesystem lock */
	write_fs(this->block_device, block_no * this->block_size, this->block_size, buf);

	/* We're done. */
	return E_SUCCESS;
}

/**
 * ext2->set_block_number Set the "real" block number for a given "inode" block number.
 *
 * @param inode   Inode to operate on
 * @param iblock  Block offset within the inode
 * @param rblock  Real block number
 * @returns Error code or E_SUCCESS
 */
static unsigned int set_block_number(ext2_fs_t * this, ext2_inodetable_t * inode, unsigned int inode_no, unsigned int iblock, unsigned int rblock) {

	unsigned int p = this->pointers_per_block;

	/* We're going to do some crazy math in a bit... */
	unsigned int a, b, c, d, e, f, g;

	uint8_t * tmp;

	if (iblock < EXT2_DIRECT_BLOCKS) {
		inode->block[iblock] = rblock;
		return E_SUCCESS;
	} else if (iblock < EXT2_DIRECT_BLOCKS + p) {
		/* XXX what if inode->block[EXT2_DIRECT_BLOCKS] isn't set? */
		if (!inode->block[EXT2_DIRECT_BLOCKS]) {
			unsigned int block_no = allocate_block(this);
			if (!block_no) return E_NOSPACE;
			inode->block[EXT2_DIRECT_BLOCKS] = block_no;
			write_inode(this, inode, inode_no);
		}
		tmp = malloc(this->block_size);
		read_block(this, inode->block[EXT2_DIRECT_BLOCKS], (uint8_t *)tmp);

		((uint32_t *)tmp)[iblock - EXT2_DIRECT_BLOCKS] = rblock;
		write_block(this, inode->block[EXT2_DIRECT_BLOCKS], (uint8_t *)tmp);

		free(tmp);
		return E_SUCCESS;
	} else if (iblock < EXT2_DIRECT_BLOCKS + p + p * p) {
		a = iblock - EXT2_DIRECT_BLOCKS;
		b = a - p;
		c = b / p;
		d = b - c * p;

		if (!inode->block[EXT2_DIRECT_BLOCKS+1]) {
			unsigned int block_no = allocate_block(this);
			if (!block_no) return E_NOSPACE;
			inode->block[EXT2_DIRECT_BLOCKS+1] = block_no;
			write_inode(this, inode, inode_no);
		}

		tmp = malloc(this->block_size);
		read_block(this, inode->block[EXT2_DIRECT_BLOCKS + 1], (uint8_t *)tmp);

		if (!((uint32_t *)tmp)[c]) {
			unsigned int block_no = allocate_block(this);
			if (!block_no) goto no_space_free;
			((uint32_t *)tmp)[c] = block_no;
			write_block(this, inode->block[EXT2_DIRECT_BLOCKS + 1], (uint8_t *)tmp);
		}

		uint32_t nblock = ((uint32_t *)tmp)[c];
		read_block(this, nblock, (uint8_t *)tmp);

		((uint32_t  *)tmp)[d] = rblock;
		write_block(this, nblock, (uint8_t *)tmp);

		free(tmp);
		return E_SUCCESS;
	} else if (iblock < EXT2_DIRECT_BLOCKS + p + p * p + p) {
		a = iblock - EXT2_DIRECT_BLOCKS;
		b = a - p;
		c = b - p * p;
		d = c / (p * p);
		e = c - d * p * p;
		f = e / p;
		g = e - f * p;

		if (!inode->block[EXT2_DIRECT_BLOCKS+2]) {
			unsigned int block_no = allocate_block(this);
			if (!block_no) return E_NOSPACE;
			inode->block[EXT2_DIRECT_BLOCKS+2] = block_no;
			write_inode(this, inode, inode_no);
		}

		tmp = malloc(this->block_size);
		read_block(this, inode->block[EXT2_DIRECT_BLOCKS + 2], (uint8_t *)tmp);

		if (!((uint32_t *)tmp)[d]) {
			unsigned int block_no = allocate_block(this);
			if (!block_no) goto no_space_free;
			((uint32_t *)tmp)[d] = block_no;
			write_block(this, inode->block[EXT2_DIRECT_BLOCKS + 2], (uint8_t *)tmp);
		}

		uint32_t nblock = ((uint32_t *)tmp)[d];
		read_block(this, nblock, (uint8_t *)tmp);

		if (!((uint32_t *)tmp)[f]) {
			unsigned int block_no = allocate_block(this);
			if (!block_no) goto no_space_free;
			((uint32_t *)tmp)[f] = block_no;
			write_block(this, nblock, (uint8_t *)tmp);
		}

		nblock = ((uint32_t *)tmp)[f];
		read_block(this, nblock, (uint8_t *)tmp);

		((uint32_t *)tmp)[g] = nblock;
		write_block(this, nblock, (uint8_t *)tmp);

		free(tmp);
		return E_SUCCESS;
	}

	debug_print(CRITICAL, "EXT2 driver tried to write to a block number that was too high (%d)", rblock);
	return E_BADBLOCK;
no_space_free:
	free(tmp);
	return E_NOSPACE;
}

/**
 * ext2->get_block_number Given an inode block number, get the real block number.
 *
 * @param inode   Inode to operate on
 * @param iblock  Block offset within the inode
 * @returns Real block number
 */
static unsigned int get_block_number(ext2_fs_t * this, ext2_inodetable_t * inode, unsigned int iblock) {

	unsigned int p = this->pointers_per_block;

	/* We're going to do some crazy math in a bit... */
	unsigned int a, b, c, d, e, f, g;

	uint8_t * tmp;

	if (iblock < EXT2_DIRECT_BLOCKS) {
		return inode->block[iblock];
	} else if (iblock < EXT2_DIRECT_BLOCKS + p) {
		/* XXX what if inode->block[EXT2_DIRECT_BLOCKS] isn't set? */
		tmp = malloc(this->block_size);
		read_block(this, inode->block[EXT2_DIRECT_BLOCKS], (uint8_t *)tmp);

		unsigned int out = ((uint32_t *)tmp)[iblock - EXT2_DIRECT_BLOCKS];
		free(tmp);
		return out;
	} else if (iblock < EXT2_DIRECT_BLOCKS + p + p * p) {
		a = iblock - EXT2_DIRECT_BLOCKS;
		b = a - p;
		c = b / p;
		d = b - c * p;

		tmp = malloc(this->block_size);
		read_block(this, inode->block[EXT2_DIRECT_BLOCKS + 1], (uint8_t *)tmp);

		uint32_t nblock = ((uint32_t *)tmp)[c];
		read_block(this, nblock, (uint8_t *)tmp);

		unsigned int out = ((uint32_t  *)tmp)[d];
		free(tmp);
		return out;
	} else if (iblock < EXT2_DIRECT_BLOCKS + p + p * p + p) {
		a = iblock - EXT2_DIRECT_BLOCKS;
		b = a - p;
		c = b - p * p;
		d = c / (p * p);
		e = c - d * p * p;
		f = e / p;
		g = e - f * p;

		tmp = malloc(this->block_size);
		read_block(this, inode->block[EXT2_DIRECT_BLOCKS + 2], (uint8_t *)tmp);

		uint32_t nblock = ((uint32_t *)tmp)[d];
		read_block(this, nblock, (uint8_t *)tmp);

		nblock = ((uint32_t *)tmp)[f];
		read_block(this, nblock, (uint8_t *)tmp);

		unsigned int out = ((uint32_t  *)tmp)[g];
		free(tmp);
		return out;
	}

	debug_print(CRITICAL, "EXT2 driver tried to read to a block number that was too high (%d)", iblock);

	return 0;
}

static int write_inode(ext2_fs_t * this, ext2_inodetable_t *inode, size_t index) {
	if (!index) {
		dprintf("ext2: Attempt to write inode 0\n");
		return E_BADBLOCK;
	}
	index--;

	size_t group = index / this->inodes_per_group;
	if (group > BGDS) {
		return E_BADBLOCK;
	}

	size_t inode_table_block = BGD[group].inode_table;
	index -= group * this->inodes_per_group;
	size_t block_offset = (index * this->inode_size) / this->block_size;
	size_t offset_in_block = index - block_offset * (this->block_size / this->inode_size);

	ext2_inodetable_t *inodet = malloc(this->block_size);
	/* Read the current table block */
	read_block(this, inode_table_block + block_offset, (uint8_t *)inodet);
	memcpy((uint8_t *)((uintptr_t)inodet + offset_in_block * this->inode_size), inode, this->inode_size);
	write_block(this, inode_table_block + block_offset, (uint8_t *)inodet);
	free(inodet);

	return E_SUCCESS;
}

static size_t allocate_block(ext2_fs_t * this) {
	size_t block_no     = 0;
	size_t block_offset = 0;
	size_t group        = 0;
	uint8_t * bg_buffer = malloc(this->block_size);

	mutex_acquire(this->mutex);

	for (unsigned int i = 0; i < BGDS; ++i) {
		if (BGD[i].free_blocks_count > 0) {
			read_block(this, BGD[i].block_bitmap, (uint8_t *)bg_buffer);
			while (BLOCKBIT(block_offset)) {
				++block_offset;
			}
			block_no = block_offset + SB->blocks_per_group * i;
			group = i;
			break;
		}
	}

	if (!block_no) {
		mutex_release(this->mutex);
		debug_print(CRITICAL, "No available blocks, disk is out of space!");
		free(bg_buffer);
		return 0;
	}

	debug_print(WARNING, "allocating block #%zu (group %zu)", block_no, group);

	BLOCKBYTE(block_offset) |= SETBIT(block_offset);
	write_block(this, BGD[group].block_bitmap, (uint8_t *)bg_buffer);

	BGD[group].free_blocks_count--;
	for (int i = 0; i < this->bgd_block_span; ++i) {
		write_block(this, this->bgd_offset + i, (uint8_t *)((uintptr_t)BGD + this->block_size * i));
	}

	SB->free_blocks_count--;
	rewrite_superblock(this);

	memset(bg_buffer, 0x00, this->block_size);
	write_block(this, block_no, bg_buffer);

	mutex_release(this->mutex);

	free(bg_buffer);

	return block_no;

}


/**
 * ext2->allocate_inode_block Allocate a block in an inode.
 *
 * @param inode Inode to operate on
 * @param inode_no Number of the inode (this is not part of the struct)
 * @param block Block within inode to allocate
 * @returns Error code or E_SUCCESS
 */
static int allocate_inode_block(ext2_fs_t * this, ext2_inodetable_t * inode, unsigned int inode_no, unsigned int block) {
	debug_print(NOTICE, "Allocating block #%d for inode #%d", block, inode_no);
	unsigned int block_no = allocate_block(this);

	if (!block_no) return E_NOSPACE;

	set_block_number(this, inode, inode_no, block, block_no);

	unsigned int t = (block + 1) * (this->block_size / 512);
	if (inode->blocks < t) {
		debug_print(NOTICE, "Setting inode->blocks to %d = (%d fs blocks)", t, t / (this->block_size / 512));
		inode->blocks = t;
	}
	write_inode(this, inode, inode_no);

	return E_SUCCESS;
}

/**
 * ext2->inode_read_block
 *
 * @param inode
 * @param no
 * @param block
 * @parma buf
 * @returns Real block number for reference.
 */
static unsigned int inode_read_block(ext2_fs_t * this, ext2_inodetable_t * inode, unsigned int block, uint8_t * buf) {

	if (block >= inode->blocks / (this->block_size / 512)) {
		memset(buf, 0x00, this->block_size);
		debug_print(WARNING, "Tried to read an invalid block. Asked for %d (0-indexed), but inode only has %d!", block, inode->blocks / (this->block_size / 512));
		return 0;
	}

	unsigned int real_block = get_block_number(this, inode, block);
	read_block(this, real_block, buf);

	return real_block;
}

/**
 * ext2->inode_write_block
 */
static unsigned int inode_write_block(ext2_fs_t * this, ext2_inodetable_t * inode, unsigned int inode_no, unsigned int block, uint8_t * buf) {
	if (block >= inode->blocks / (this->block_size / 512)) {
		debug_print(WARNING, "Attempting to write beyond the existing allocated blocks for this inode.");
		debug_print(WARNING, "Inode %d, Block %d", inode_no, block);
	}

	debug_print(WARNING, "clearing and allocating up to required blocks (block=%d, %d)", block, inode->blocks);
	char * empty = NULL;

	while (block >= inode->blocks / (this->block_size / 512)) {
		allocate_inode_block(this, inode, inode_no, inode->blocks / (this->block_size / 512));
		refresh_inode(this, inode, inode_no);
	}
	if (empty) free(empty);
	debug_print(WARNING, "... done");

	unsigned int real_block = get_block_number(this, inode, block);
	debug_print(WARNING, "Writing virtual block %d for inode %d maps to real block %d", block, inode_no, real_block);

	write_block(this, real_block, buf);
	return real_block;
}

/**
 * ext2->create_entry
 *
 * @returns Error code or E_SUCCESS
 */
static int create_entry(fs_node_t * parent, char * name, uint32_t inode) {
	ext2_fs_t * this = (ext2_fs_t *)parent->device;

	ext2_inodetable_t * pinode = read_inode(this,parent->inode);
	if (((pinode->mode & EXT2_S_IFDIR) == 0) || (name == NULL)) {
		debug_print(WARNING, "Attempted to allocate an inode in a parent that was not a directory.");
		return E_BADPARENT;
	}

	debug_print(WARNING, "Creating a directory entry for %s pointing to inode %d.", name, inode);

	/* okay, how big is it... */

	debug_print(WARNING, "We need to append %zd bytes to the direcotry.", sizeof(ext2_dir_t) + strlen(name));

	unsigned int rec_len = sizeof(ext2_dir_t) + strlen(name);
	rec_len += (rec_len % 4) ? (4 - (rec_len % 4)) : 0;

	debug_print(WARNING, "Our directory entry looks like this:");
	debug_print(WARNING, "  inode     = %d", inode);
	debug_print(WARNING, "  rec_len   = %d", rec_len);
	debug_print(WARNING, "  name_len  = %zd", strlen(name));
	debug_print(WARNING, "  file_type = %d", 0);
	debug_print(WARNING, "  name      = %s", name);

	debug_print(WARNING, "The inode size is marked as: %d", pinode->size);
	debug_print(WARNING, "Block size is %d", this->block_size);

	uint8_t * block = malloc(this->block_size);
	uint8_t block_nr = 0;
	uint32_t dir_offset = 0;
	uint32_t total_offset = 0;
	int modify_or_replace = 0;
	ext2_dir_t *previous;

	inode_read_block(this, pinode, block_nr, block);
	while (total_offset < pinode->size) {
		if (dir_offset >= this->block_size) {
			block_nr++;
			dir_offset -= this->block_size;
			inode_read_block(this, pinode, block_nr, block);
		}
		ext2_dir_t *d_ent = (ext2_dir_t *)((uintptr_t)block + dir_offset);

		unsigned int sreclen = d_ent->name_len + sizeof(ext2_dir_t);
		sreclen += (sreclen % 4) ? (4 - (sreclen % 4)) : 0;

		{
			char f[d_ent->name_len+1];
			memcpy(f, d_ent->name, d_ent->name_len);
			f[d_ent->name_len] = 0;
			debug_print(WARNING, " * file: %s", f);
		}
		debug_print(WARNING, "   rec_len: %d", d_ent->rec_len);
		debug_print(WARNING, "   type: %d", d_ent->file_type);
		debug_print(WARNING, "   namel: %d", d_ent->name_len);
		debug_print(WARNING, "   inode: %d", d_ent->inode);

		if (d_ent->rec_len != sreclen && total_offset + d_ent->rec_len == pinode->size) {
			debug_print(WARNING, "  - should be %d, but instead points to end of block", sreclen);
			debug_print(WARNING, "  - we've hit the end, should change this pointer");

			dir_offset += sreclen;
			total_offset += sreclen;

			modify_or_replace = 1; /* Modify */
			previous = d_ent;

			break;
		}

		if (d_ent->inode == 0) {
			modify_or_replace = 2; /* Replace */
		}

		dir_offset += d_ent->rec_len;
		total_offset += d_ent->rec_len;
	}

	if (!modify_or_replace) {
		debug_print(WARNING, "That's odd, this shouldn't have happened, we made it all the way here without hitting our two end conditions?");
	}

	if (modify_or_replace == 1) {
		debug_print(WARNING, "The last node in the list is a real node, we need to modify it.");

		if (dir_offset + rec_len >= this->block_size) {
			block_nr++;
			allocate_inode_block(this, pinode, parent->inode, block_nr);
			memset(block, 0, this->block_size);
			dir_offset = 0;
			pinode->size += this->block_size;
			write_inode(this, pinode, parent->inode);
		} else {
			unsigned int sreclen = previous->name_len + sizeof(ext2_dir_t);
			sreclen += (sreclen % 4) ? (4 - (sreclen % 4)) : 0;
			previous->rec_len = sreclen;
			debug_print(WARNING, "Set previous node rec_len to %d", sreclen);
		}

	} else if (modify_or_replace == 2) {
		debug_print(WARNING, "The last node in the list is a fake node, we'll replace it.");
	}

	debug_print(WARNING, " total_offset = 0x%x", total_offset);
	debug_print(WARNING, "   dir_offset = 0x%x", dir_offset);
	ext2_dir_t *d_ent = (ext2_dir_t *)((uintptr_t)block + dir_offset);

	d_ent->inode     = inode;
	d_ent->rec_len   = this->block_size - dir_offset;
	d_ent->name_len  = strlen(name);
	d_ent->file_type = 0; /* This is unused */
	memcpy(d_ent->name, name, strlen(name));

	inode_write_block(this, pinode, parent->inode, block_nr, block);

	free(block);
	free(pinode);


	return E_NOSPACE;
}

static unsigned int allocate_inode(ext2_fs_t * this) {
	uint32_t node_no     = 0;
	uint32_t node_offset = 0;
	uint32_t group       = 0;
	uint8_t * bg_buffer  = malloc(this->block_size);

	mutex_acquire(this->mutex);

	for (unsigned int i = 0; i < BGDS; ++i) {
		if (BGD[i].free_inodes_count > 0) {
			debug_print(NOTICE, "Group %d has %d free inodes.", i, BGD[i].free_inodes_count);
			read_block(this, BGD[i].inode_bitmap, (uint8_t *)bg_buffer);

			/* Sorry for the weird loops */
			while (1) {
				while (BLOCKBIT(node_offset)) {
					node_offset++;
					if (node_offset == this->inodes_per_group) {
						goto _next_block;
					}
				}
				node_no = node_offset + i * this->inodes_per_group + 1;
				/* Is this a reserved inode? */
				if (node_no <= 10) {
					node_offset++;
					if (node_offset == this->inodes_per_group) {
						goto _next_block;
					}
					continue;
				}
				break;
			}
			if (node_offset == this->inodes_per_group) {
				_next_block:
				node_offset = 0;
				continue;
			}
			group = i;
			break;
		}
	}
	if (!node_no) {
		mutex_release(this->mutex);
		dprintf("ext2: Out of inodes? node_no = 0\n");
		return 0;
	}

	BLOCKBYTE(node_offset) |= SETBIT(node_offset);

	write_block(this, BGD[group].inode_bitmap, (uint8_t *)bg_buffer);
	free(bg_buffer);

	BGD[group].free_inodes_count--;
	for (int i = 0; i < this->bgd_block_span; ++i) {
		write_block(this, this->bgd_offset + i, (uint8_t *)((uintptr_t)BGD + this->block_size * i));
	}

	SB->free_inodes_count--;
	rewrite_superblock(this);

	mutex_release(this->mutex);

	return node_no;
}

static int mkdir_ext2(fs_node_t * parent, char * name, mode_t permission) {
	if (!name) return -EINVAL;

	ext2_fs_t * this = parent->device;
	if (!(this->flags & EXT2_FLAG_READWRITE)) return -EROFS;

	/* first off, check if it exists */
	fs_node_t * check = finddir_ext2(parent, name);
	if (check) {
		debug_print(WARNING, "A file by this name already exists: %s", name);
		free(check);
		return -EEXIST;
	}

	/* Allocate an inode for it */
	unsigned int inode_no = allocate_inode(this);
	ext2_inodetable_t * inode = read_inode(this,inode_no);

	/* Set the access and creation times to now */
	inode->atime = now();
	inode->ctime = inode->atime;
	inode->mtime = inode->atime;
	inode->dtime = 0; /* This inode was never deleted */

	/* Empty the file */
	memset(inode->block, 0x00, sizeof(inode->block));
	inode->blocks = 0;
	inode->size = 0; /* empty */

	/* Assign it to root */
	inode->uid = this_core->current_process->user;
	inode->gid = this_core->current_process->user_group;

	/* misc */
	inode->faddr = 0;
	inode->links_count = 2; /* There's the parent's pointer to us, and our pointer to us. */
	inode->flags = 0;
	inode->osd1 = 0;
	inode->generation = 0;
	inode->file_acl = 0;
	inode->dir_acl = 0;

	/* File mode */
	inode->mode = EXT2_S_IFDIR;
	inode->mode |= 0xFFF & permission;

	/* Write the osd blocks to 0 */
	memset(inode->osd2, 0x00, sizeof(inode->osd2));

	/* Write out inode changes */
	write_inode(this, inode, inode_no);

	/* Now append the entry to the parent */
	create_entry(parent, name, inode_no);

	inode->size = this->block_size;
	write_inode(this, inode, inode_no);

	uint8_t * tmp = malloc(this->block_size);
	ext2_dir_t * t = malloc(12);
	memset(t, 0, 12);
	t->inode = inode_no;
	t->rec_len = 12;
	t->name_len = 1;
	t->name[0] = '.';
	memcpy(&tmp[0], t, 12);
	t->inode = parent->inode;
	t->name_len = 2;
	t->name[1] = '.';
	t->rec_len = this->block_size - 12;
	memcpy(&tmp[12], t, 12);
	free(t);

	inode_write_block(this, inode, inode_no, 0, tmp);

	free(inode);
	free(tmp);

	/* Update parent link count */
	ext2_inodetable_t * pinode = read_inode(this, parent->inode);
	pinode->links_count++;
	write_inode(this, pinode, parent->inode);
	free(pinode);

	/* Update directory count in block group descriptor */
	uint32_t group = inode_no / this->inodes_per_group;
	BGD[group].used_dirs_count++;
	for (int i = 0; i < this->bgd_block_span; ++i) {
		write_block(this, this->bgd_offset + i, (uint8_t *)((uintptr_t)BGD + this->block_size * i));
	}

	return 0;
}

static int create_ext2(fs_node_t * parent, char * name, mode_t permission) {
	if (!name) return -EINVAL;

	ext2_fs_t * this = parent->device;
	if (!(this->flags & EXT2_FLAG_READWRITE)) return -EROFS;

	/* first off, check if it exists */
	fs_node_t * check = finddir_ext2(parent, name);
	if (check) {
		debug_print(WARNING, "A file by this name already exists: %s", name);
		free(check);
		return -EEXIST;
	}

	/* Allocate an inode for it */
	unsigned int inode_no = allocate_inode(this);
	ext2_inodetable_t * inode = read_inode(this,inode_no);

	/* Set the access and creation times to now */
	inode->atime = now();
	inode->ctime = inode->atime;
	inode->mtime = inode->atime;
	inode->dtime = 0; /* This inode was never deleted */

	/* Empty the file */
	memset(inode->block, 0x00, sizeof(inode->block));
	inode->blocks = 0;
	inode->size = 0; /* empty */

	inode->uid = this_core->current_process->user;
	inode->gid = this_core->current_process->user_group;

	/* misc */
	inode->faddr = 0;
	inode->links_count = 1; /* The one we're about to create. */
	inode->flags = 0;
	inode->osd1 = 0;
	inode->generation = 0;
	inode->file_acl = 0;
	inode->dir_acl = 0;

	/* File mode */
	/* TODO: Use the mask from `permission` */
	inode->mode = EXT2_S_IFREG;
	inode->mode |= 0xFFF & permission;

	/* Write the osd blocks to 0 */
	memset(inode->osd2, 0x00, sizeof(inode->osd2));

	/* Write out inode changes */
	write_inode(this, inode, inode_no);

	/* Now append the entry to the parent */
	create_entry(parent, name, inode_no);

	free(inode);

	return 0;
}

static int chmod_ext2(fs_node_t * node, mode_t mode) {
	ext2_fs_t * this = node->device;
	if (!(this->flags & EXT2_FLAG_READWRITE)) return -EROFS;

	ext2_inodetable_t * inode = read_inode(this,node->inode);

	inode->mode = (inode->mode & 0xFFFFF000) | mode;

	write_inode(this, inode, node->inode);

	return 0;
}

/**
 * direntry_ext2
 */
static ext2_dir_t * direntry_ext2(ext2_fs_t * this, ext2_inodetable_t * inode, uint32_t no, uint32_t index) {
	uint8_t *block = malloc(this->block_size);
	uint8_t block_nr = 0;
	inode_read_block(this, inode, block_nr, block);
	uint32_t dir_offset = 0;
	uint32_t total_offset = 0;
	uint32_t dir_index = 0;

	while (total_offset < inode->size && dir_index <= index) {
		ext2_dir_t *d_ent = (ext2_dir_t *)((uintptr_t)block + dir_offset);

		if (d_ent->inode != 0 && dir_index == index) {
			ext2_dir_t *out = malloc(d_ent->rec_len);
			memcpy(out, d_ent, d_ent->rec_len);
			free(block);
			return out;
		}

		dir_offset += d_ent->rec_len;
		total_offset += d_ent->rec_len;

		if (d_ent->inode) {
			dir_index++;
		}

		if (dir_offset >= this->block_size) {
			block_nr++;
			dir_offset -= this->block_size;
			inode_read_block(this, inode, block_nr, block);
		}
	}

	free(block);
	return NULL;
}

/**
 * finddir_ext2
 */
static fs_node_t * finddir_ext2(fs_node_t *node, char *name) {

	ext2_fs_t * this = (ext2_fs_t *)node->device;

	ext2_inodetable_t *inode = read_inode(this,node->inode);
	//assert(inode->mode & EXT2_S_IFDIR);
	uint8_t * block = malloc(this->block_size);
	ext2_dir_t *direntry = NULL;
	uint8_t block_nr = 0;
	inode_read_block(this, inode, block_nr, block);
	uint32_t dir_offset = 0;
	uint32_t total_offset = 0;

	while (total_offset < inode->size) {
		if (dir_offset >= this->block_size) {
			block_nr++;
			dir_offset -= this->block_size;
			inode_read_block(this, inode, block_nr, block);
		}
		ext2_dir_t *d_ent = (ext2_dir_t *)((uintptr_t)block + dir_offset);

		if (d_ent->inode == 0 || strlen(name) != d_ent->name_len) {
			dir_offset += d_ent->rec_len;
			total_offset += d_ent->rec_len;

			continue;
		}

		char *dname = malloc(sizeof(char) * (d_ent->name_len + 1));
		memcpy(dname, &(d_ent->name), d_ent->name_len);
		dname[d_ent->name_len] = '\0';
		if (!strcmp(dname, name)) {
			free(dname);
			direntry = malloc(d_ent->rec_len);
			memcpy(direntry, d_ent, d_ent->rec_len);
			break;
		}
		free(dname);

		dir_offset += d_ent->rec_len;
		total_offset += d_ent->rec_len;
	}
	free(inode);
	if (!direntry) {
		free(block);
		return NULL;
	}
	fs_node_t *outnode = malloc(sizeof(fs_node_t));
	memset(outnode, 0, sizeof(fs_node_t));

	inode = read_inode(this, direntry->inode);

	if (!node_from_file(this, inode, direntry, outnode)) {
		debug_print(CRITICAL, "Oh dear. Couldn't allocate the outnode?");
	}

	free(direntry);
	free(inode);
	free(block);
	return outnode;
}

static int unlink_ext2(fs_node_t * node, char * name) {
	/* XXX this is a very bad implementation */
	ext2_fs_t * this = (ext2_fs_t *)node->device;
	if (!(this->flags & EXT2_FLAG_READWRITE)) return -EROFS;

	ext2_inodetable_t *inode = read_inode(this,node->inode);
	//assert(inode->mode & EXT2_S_IFDIR);
	uint8_t * block = malloc(this->block_size);
	ext2_dir_t *direntry = NULL;
	uint8_t block_nr = 0;
	inode_read_block(this, inode, block_nr, block);
	uint32_t dir_offset = 0;
	uint32_t total_offset = 0;

	while (total_offset < inode->size) {
		if (dir_offset >= this->block_size) {
			block_nr++;
			dir_offset -= this->block_size;
			inode_read_block(this, inode, block_nr, block);
		}
		ext2_dir_t *d_ent = (ext2_dir_t *)((uintptr_t)block + dir_offset);

		if (d_ent->inode == 0 || strlen(name) != d_ent->name_len) {
			dir_offset += d_ent->rec_len;
			total_offset += d_ent->rec_len;

			continue;
		}

		char *dname = malloc(sizeof(char) * (d_ent->name_len + 1));
		memcpy(dname, &(d_ent->name), d_ent->name_len);
		dname[d_ent->name_len] = '\0';
		if (!strcmp(dname, name)) {
			free(dname);
			direntry = d_ent;
			break;
		}
		free(dname);

		dir_offset += d_ent->rec_len;
		total_offset += d_ent->rec_len;
	}
	if (!direntry) {
		free(inode);
		free(block);
		return -ENOENT;
	}

	unsigned int new_inode = direntry->inode;
	direntry->inode = 0;
	inode_write_block(this, inode, node->inode, block_nr, block);
	free(inode);
	free(block);

	inode = read_inode(this, new_inode);

	if (inode->links_count == 1) {
		dprintf("ext2: TODO: unlinking '%s' (inode=%u) which now has no links; should delete\n",
			name, new_inode);
	}

	if (inode->links_count > 0) {
		inode->links_count--;
		write_inode(this, inode, new_inode);
	}

	free(inode);

	return 0;
}


static void refresh_inode(ext2_fs_t * this, ext2_inodetable_t * inodet,  size_t inode) {
	if (!inode) {
		dprintf("ext2: Attempt to read inode 0\n");
		return;
	}
	inode--;

	uint32_t group = inode / this->inodes_per_group;
	if (group > BGDS) {
		return;
	}
	uint32_t inode_table_block = BGD[group].inode_table;
	inode -= group * this->inodes_per_group;	// adjust index within group
	uint32_t block_offset		= (inode * this->inode_size) / this->block_size;
	uint32_t offset_in_block    = inode - block_offset * (this->block_size / this->inode_size);

	uint8_t * buf = malloc(this->block_size);

	read_block(this, inode_table_block + block_offset, buf);

	ext2_inodetable_t *inodes = (ext2_inodetable_t *)buf;

	memcpy(inodet, (uint8_t *)((uintptr_t)inodes + offset_in_block * this->inode_size), this->inode_size);

	free(buf);
}

/**
 * read_inode
 */
static ext2_inodetable_t * read_inode(ext2_fs_t * this, size_t inode) {
	ext2_inodetable_t *inodet   = malloc(this->inode_size);
	refresh_inode(this, inodet, inode);
	return inodet;
}

static ssize_t read_ext2(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
	ext2_fs_t * this = (ext2_fs_t *)node->device;
	ext2_inodetable_t * inode = read_inode(this, node->inode);
	uint32_t end;
	if (inode->size == 0) return 0;
	if (offset + size > inode->size) {
		end = inode->size;
	} else {
		end = offset + size;
	}
	uint32_t start_block  = offset / this->block_size;
	uint32_t end_block    = end / this->block_size;
	uint32_t end_size     = end - end_block * this->block_size;
	uint32_t size_to_read = end - offset;

	uint8_t * buf = malloc(this->block_size);
	if (start_block == end_block) {
		inode_read_block(this, inode, start_block, buf);
		memcpy(buffer, (uint8_t *)(((uintptr_t)buf) + ((uintptr_t)offset % this->block_size)), size_to_read);
	} else {
		uint32_t block_offset;
		uint32_t blocks_read = 0;
		for (block_offset = start_block; block_offset < end_block; block_offset++, blocks_read++) {
			if (block_offset == start_block) {
				inode_read_block(this, inode, block_offset, buf);
				memcpy(buffer, (uint8_t *)(((uintptr_t)buf) + ((uintptr_t)offset % this->block_size)), this->block_size - (offset % this->block_size));
			} else {
				inode_read_block(this, inode, block_offset, buf);
				memcpy(buffer + this->block_size * blocks_read - (offset % this->block_size), buf, this->block_size);
			}
		}
		if (end_size) {
			inode_read_block(this, inode, end_block, buf);
			memcpy(buffer + this->block_size * blocks_read - (offset % this->block_size), buf, end_size);
		}
	}
	free(inode);
	free(buf);
	return size_to_read;
}

static ssize_t write_inode_buffer(ext2_fs_t * this, ext2_inodetable_t * inode, uint32_t inode_number, off_t offset, size_t size, uint8_t *buffer) {
	uint32_t end = offset + size;
	if (end > inode->size) {
		inode->size = end;
		write_inode(this, inode, inode_number);
	}

	uint32_t start_block  = offset / this->block_size;
	uint32_t end_block    = end / this->block_size;
	uint32_t end_size     = end - end_block * this->block_size;
	uint32_t size_to_read = end - offset;
	uint8_t * buf = malloc(this->block_size);
	if (start_block == end_block) {
		inode_read_block(this, inode, start_block, buf);
		memcpy((uint8_t *)(((uintptr_t)buf) + ((uintptr_t)offset % this->block_size)), buffer, size_to_read);
		inode_write_block(this, inode, inode_number, start_block, buf);
	} else {
		uint32_t block_offset;
		uint32_t blocks_read = 0;
		for (block_offset = start_block; block_offset < end_block; block_offset++, blocks_read++) {
			if (block_offset == start_block) {
				int b = inode_read_block(this, inode, block_offset, buf);
				memcpy((uint8_t *)(((uintptr_t)buf) + ((uintptr_t)offset % this->block_size)), buffer, this->block_size - (offset % this->block_size));
				inode_write_block(this, inode, inode_number, block_offset, buf);
				if (!b) {
					refresh_inode(this, inode, inode_number);
				}
			} else {
				int b = inode_read_block(this, inode, block_offset, buf);
				memcpy(buf, buffer + this->block_size * blocks_read - (offset % this->block_size), this->block_size);
				inode_write_block(this, inode, inode_number, block_offset, buf);
				if (!b) {
					refresh_inode(this, inode, inode_number);
				}
			}
		}
		if (end_size) {
			inode_read_block(this, inode, end_block, buf);
			memcpy(buf, buffer + this->block_size * blocks_read - (offset % this->block_size), end_size);
			inode_write_block(this, inode, inode_number, end_block, buf);
		}
	}
	free(buf);
	return size_to_read;
}

static ssize_t write_ext2(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
	ext2_fs_t * this = (ext2_fs_t *)node->device;
	if (!(this->flags & EXT2_FLAG_READWRITE)) return -EROFS;

	ext2_inodetable_t * inode = read_inode(this, node->inode);

	ssize_t rv = write_inode_buffer(this, inode, node->inode, offset, size, buffer);
	free(inode);
	return rv;
}

static int truncate_ext2(fs_node_t * node) {
	ext2_fs_t * this = node->device;
	if (!(this->flags & EXT2_FLAG_READWRITE)) return -EROFS;

	ext2_inodetable_t * inode = read_inode(this,node->inode);
	inode->size = 0;
	write_inode(this, inode, node->inode);
	return 0;
}

static void open_ext2(fs_node_t *node, unsigned int flags) {
	/* Nothing to do here */
}

static void close_ext2(fs_node_t *node) {
	/* Nothing to do here */
}


/**
 * readdir_ext2
 */
static struct dirent * readdir_ext2(fs_node_t *node, unsigned long index) {

	ext2_fs_t * this = (ext2_fs_t *)node->device;

	ext2_inodetable_t *inode = read_inode(this, node->inode);
	//assert(inode->mode & EXT2_S_IFDIR);
	ext2_dir_t *direntry = direntry_ext2(this, inode, node->inode, index);
	if (!direntry) {
		free(inode);
		return NULL;
	}
	struct dirent *dirent = malloc(sizeof(struct dirent));
	memcpy(&dirent->d_name, &direntry->name, direntry->name_len);
	dirent->d_name[direntry->name_len] = '\0';
	dirent->d_ino = direntry->inode;
	free(direntry);
	free(inode);
	return dirent;
}

static int symlink_ext2(fs_node_t * parent, char * target, char * name) {
	if (!name) return -EINVAL;

	ext2_fs_t * this = parent->device;
	if (!(this->flags & EXT2_FLAG_READWRITE)) return -EROFS;

	/* first off, check if it exists */
	fs_node_t * check = finddir_ext2(parent, name);
	if (check) {
		debug_print(WARNING, "A file by this name already exists: %s", name);
		free(check);
		return -EEXIST; /* this should probably have a return value... */
	}

	/* Allocate an inode for it */
	unsigned int inode_no = allocate_inode(this);
	ext2_inodetable_t * inode = read_inode(this,inode_no);

	/* Set the access and creation times to now */
	inode->atime = now();
	inode->ctime = inode->atime;
	inode->mtime = inode->atime;
	inode->dtime = 0; /* This inode was never deleted */

	/* Empty the file */
	memset(inode->block, 0x00, sizeof(inode->block));
	inode->blocks = 0;
	inode->size = 0; /* empty */

	/* Assign it to current user */
	inode->uid = this_core->current_process->user;
	inode->gid = this_core->current_process->user_group;

	/* misc */
	inode->faddr = 0;
	inode->links_count = 1; /* The one we're about to create. */
	inode->flags = 0;
	inode->osd1 = 0;
	inode->generation = 0;
	inode->file_acl = 0;
	inode->dir_acl = 0;

	inode->mode = EXT2_S_IFLNK;

	/* I *think* this is what you're supposed to do with symlinks */
	inode->mode |= 0777;

	/* Write the osd blocks to 0 */
	memset(inode->osd2, 0x00, sizeof(inode->osd2));

	size_t target_len = strlen(target);
	int embedded = target_len <= 60; // sizeof(_symlink(inode));
	if (embedded) {
		memcpy(_symlink(inode), target, target_len);
		inode->size = target_len;
	}

	/* Write out inode changes */
	write_inode(this, inode, inode_no);

	/* Now append the entry to the parent */
	create_entry(parent, name, inode_no);


	/* If we didn't embed it in the inode just use write_inode_buffer to finish the job */
	if (!embedded) {
		write_inode_buffer(parent->device, inode, inode_no, 0, target_len, (uint8_t *)target);
	}
	free(inode);

	return 0;
}

static ssize_t readlink_ext2(fs_node_t * node, char * buf, size_t size) {
	ext2_fs_t * this = (ext2_fs_t *)node->device;
	ext2_inodetable_t * inode = read_inode(this, node->inode);
	size_t read_size = inode->size < size ? inode->size : size;
	if (inode->size > 60) { //sizeof(_symlink(inode))) {
		read_ext2(node, 0, read_size, (uint8_t *)buf);
	} else {
		memcpy(buf, _symlink(inode), read_size);
	}

	/* Believe it or not, we actually aren't supposed to include the nul in the length. */
	if (read_size < size) {
		buf[read_size] = '\0';
	}

	free(inode);
	return read_size;
}

static int ioctl_ext2(fs_node_t * node, unsigned long request, void * argp) {
	ext2_fs_t * this = (ext2_fs_t *)node->device;

	switch (request) {
		case IOCTLSYNC:
			return ioctl_fs(this->block_device, IOCTLSYNC, NULL);

		default:
			return -EINVAL;
	}
}

static int node_from_file(ext2_fs_t * this, ext2_inodetable_t *inode, ext2_dir_t *direntry,  fs_node_t *fnode) {
	if (!fnode) {
		/* You didn't give me a node to write into, go **** yourself */
		return 0;
	}
	/* Information from the direntry */
	fnode->device = (void *)this;
	fnode->inode = direntry->inode;
	memcpy(&fnode->name, &direntry->name, direntry->name_len);
	fnode->name[direntry->name_len] = '\0';
	/* Information from the inode */
	fnode->uid = inode->uid;
	fnode->gid = inode->gid;
	fnode->length = inode->size;
	fnode->mask = inode->mode & 0xFFF;
	fnode->nlink = inode->links_count;
	/* File Flags */
	fnode->flags = 0;
	if ((inode->mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
		fnode->flags   |= FS_FILE;
		fnode->read     = read_ext2;
		fnode->write    = write_ext2;
		fnode->truncate = truncate_ext2;
		fnode->create   = NULL;
		fnode->mkdir    = NULL;
		fnode->readdir  = NULL;
		fnode->finddir  = NULL;
		fnode->symlink  = NULL;
		fnode->readlink = NULL;
	}
	if ((inode->mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
		fnode->flags   |= FS_DIRECTORY;
		fnode->create   = create_ext2;
		fnode->mkdir    = mkdir_ext2;
		fnode->unlink   = unlink_ext2;
		fnode->symlink  = symlink_ext2;
		fnode->readdir  = readdir_ext2;
		fnode->finddir  = finddir_ext2;
		fnode->write    = NULL;
		fnode->readlink = NULL;
	}
	if ((inode->mode & EXT2_S_IFBLK) == EXT2_S_IFBLK) {
		fnode->flags |= FS_BLOCKDEVICE;
	}
	if ((inode->mode & EXT2_S_IFCHR) == EXT2_S_IFCHR) {
		fnode->flags |= FS_CHARDEVICE;
	}
	if ((inode->mode & EXT2_S_IFIFO) == EXT2_S_IFIFO) {
		fnode->flags |= FS_PIPE;
	}
	if ((inode->mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
		fnode->flags   |= FS_SYMLINK;
		fnode->read     = NULL;
		fnode->write    = NULL;
		fnode->create   = NULL;
		fnode->mkdir    = NULL;
		fnode->readdir  = NULL;
		fnode->finddir  = NULL;
		fnode->readlink = readlink_ext2;
	}

	fnode->atime   = inode->atime;
	fnode->mtime   = inode->mtime;
	fnode->ctime   = inode->ctime;

	fnode->chmod   = chmod_ext2;
	fnode->open    = open_ext2;
	fnode->close   = close_ext2;
	fnode->ioctl = ioctl_ext2;
	return 1;
}

static int ext2_root(ext2_fs_t * this, ext2_inodetable_t *inode, fs_node_t *fnode) {
	if (!fnode) {
		return 0;
	}
	/* Information for root dir */
	fnode->device = (void *)this;
	fnode->inode = 2;
	fnode->name[0] = '/';
	fnode->name[1] = '\0';
	/* Information from the inode */
	fnode->uid = inode->uid;
	fnode->gid = inode->gid;
	fnode->length = inode->size;
	fnode->mask = inode->mode & 0xFFF;
	fnode->nlink = inode->links_count;
	/* File Flags */
	fnode->flags = 0;
	if ((inode->mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
		debug_print(CRITICAL, "Root appears to be a regular file.");
		debug_print(CRITICAL, "This is probably very, very wrong.");
		return 0;
	}
	if ((inode->mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
	} else {
		debug_print(CRITICAL, "Root doesn't appear to be a directory.");
		debug_print(CRITICAL, "This is probably very, very wrong.");

		debug_print(ERROR, "Other useful information:");
		debug_print(ERROR, "%d", inode->uid);
		debug_print(ERROR, "%d", inode->gid);
		debug_print(ERROR, "%d", inode->size);
		debug_print(ERROR, "%d", inode->mode);
		debug_print(ERROR, "%d", inode->links_count);

		return 0;
	}
	if ((inode->mode & EXT2_S_IFBLK) == EXT2_S_IFBLK) {
		fnode->flags |= FS_BLOCKDEVICE;
	}
	if ((inode->mode & EXT2_S_IFCHR) == EXT2_S_IFCHR) {
		fnode->flags |= FS_CHARDEVICE;
	}
	if ((inode->mode & EXT2_S_IFIFO) == EXT2_S_IFIFO) {
		fnode->flags |= FS_PIPE;
	}
	if ((inode->mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
		fnode->flags |= FS_SYMLINK;
	}

	fnode->atime   = inode->atime;
	fnode->mtime   = inode->mtime;
	fnode->ctime   = inode->ctime;

	fnode->flags |= FS_DIRECTORY;
	fnode->read    = NULL;
	fnode->write   = NULL;
	fnode->chmod   = chmod_ext2;
	fnode->open    = open_ext2;
	fnode->close   = close_ext2;
	fnode->readdir = readdir_ext2;
	fnode->finddir = finddir_ext2;
	fnode->ioctl   = NULL;
	fnode->create  = create_ext2;
	fnode->mkdir   = mkdir_ext2;
	fnode->unlink  = unlink_ext2;
	return 1;
}

static fs_node_t * mount_ext2(fs_node_t * block_device, int flags) {

	ext2_fs_t * this = malloc(sizeof(ext2_fs_t));

	memset(this, 0x00, sizeof(ext2_fs_t));

	this->flags = flags;

	this->block_device = block_device;
	this->block_size = 1024;
	/* We need to keep an owned refcount to this device if it was something we opened... */
	//vfs_lock(this->block_device);

	this->mutex = mutex_init("ext2 fs");

	SB = malloc(this->block_size);

	debug_print(INFO, "Reading superblock...");
	read_block(this, 1, (uint8_t *)SB);
	if (SB->magic != EXT2_SUPER_MAGIC) {
		debug_print(ERROR, "... not an EXT2 filesystem? (magic didn't match, got 0x%x)", SB->magic);
		return NULL;
	}
	this->inode_size = SB->inode_size;
	if (SB->inode_size == 0) {
		this->inode_size = 128;
	}
	this->block_size = 1024 << SB->log_block_size;
	this->pointers_per_block = this->block_size / 4;
	debug_print(INFO, "Log block size = %d -> %d", SB->log_block_size, this->block_size);
	BGDS = SB->blocks_count / SB->blocks_per_group;
	if (SB->blocks_per_group * BGDS < SB->blocks_count) {
		BGDS += 1;
	}
	this->inodes_per_group = SB->inodes_count / BGDS;

	// load the block group descriptors
	this->bgd_block_span = sizeof(ext2_bgdescriptor_t) * BGDS / this->block_size + 1;
	BGD = malloc(this->block_size * this->bgd_block_span);

	debug_print(INFO, "bgd_block_span = %d", this->bgd_block_span);

	this->bgd_offset = 2;

	if (this->block_size > 1024) {
		this->bgd_offset = 1;
	}

	for (int i = 0; i < this->bgd_block_span; ++i) {
		read_block(this, this->bgd_offset + i, (uint8_t *)((uintptr_t)BGD + this->block_size * i));
	}

	dprintf("ext2: %u BGDs, %u inodes, %u inodes per group\n",
		BGDS, SB->inodes_count, this->inodes_per_group);

#if 1 // DEBUG_BLOCK_DESCRIPTORS
	char * bg_buffer = malloc(this->block_size * sizeof(char));
	for (uint32_t i = 0; i < BGDS; ++i) {
		debug_print(INFO, "Block Group Descriptor #%d @ %d", i, this->bgd_offset + i * SB->blocks_per_group);
		debug_print(INFO, "\tBlock Bitmap @ %d", BGD[i].block_bitmap); {
			debug_print(INFO, "\t\tExamining block bitmap at %d", BGD[i].block_bitmap);
			read_block(this, BGD[i].block_bitmap, (uint8_t *)bg_buffer);
			uint32_t j = 0;
			while (BLOCKBIT(j)) {
				++j;
			}
			debug_print(INFO, "\t\tFirst free block in group is %d", j + BGD[i].block_bitmap - 2);
		}
		debug_print(INFO, "\tInode Bitmap @ %d", BGD[i].inode_bitmap); {
			debug_print(INFO, "\t\tExamining inode bitmap at %d", BGD[i].inode_bitmap);
			read_block(this, BGD[i].inode_bitmap, (uint8_t *)bg_buffer);
			uint32_t j = 0;
			while (BLOCKBIT(j)) {
				++j;
			}
			debug_print(INFO, "\t\tFirst free inode in group is %d", j + this->inodes_per_group * i + 1);
		}
		debug_print(INFO, "\tInode Table  @ %d", BGD[i].inode_table);
		debug_print(INFO, "\tFree Blocks =  %d", BGD[i].free_blocks_count);
		debug_print(INFO, "\tFree Inodes =  %d", BGD[i].free_inodes_count);
	}
	free(bg_buffer);
#endif

	ext2_inodetable_t *root_inode = read_inode(this, 2);
	RN = (fs_node_t *)malloc(sizeof(fs_node_t));
	if (!ext2_root(this, root_inode, RN)) {
		return NULL;
	}
	debug_print(NOTICE, "Mounted EXT2 disk, root VFS node is at %#zx", (uintptr_t)RN);
	return RN;
}

fs_node_t * ext2_fs_mount(const char * device, const char * mount_path) {
	char * arg = strdup(device);
	char * argv[10];
	int argc = tokenize(arg, ",", argv);

	fs_node_t * dev = kopen(argv[0], 0);
	if (!dev) {
		return NULL;
	}

	int flags = 0;

	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i],"rw")) {
			flags |= EXT2_FLAG_READWRITE;
		}
		if (!strcmp(argv[i],"verbose")) {
			flags |= EXT2_FLAG_LOUD;
		}
	}

	return mount_ext2(dev, flags);
}

static int init(int argc, char * argv[]) {
	vfs_register("ext2", ext2_fs_mount);
	return 0;
}

static int fini(void) {
	return 0;
}

struct Module metadata = {
	.name = "ext2",
	.init = init,
	.fini = fini,
};

