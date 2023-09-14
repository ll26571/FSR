/**
 * @file FSR_f2fs.h
 * @author Lin Li
 * @brief FSR based on F2FS.
 * 
 * @copyright Copyright (c) 2023 Chongqing University StarLab
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include "nvme/io_access.h"
#include "xil_printf.h"
#include "xtime_l.h"

// uncomment this to compile the DEBUG version
// #define DEBUG

// keep off by default, uncomment this to turn it on
// #define TIME_COUNTER
// used for latency breakdown
XTime t_read_block, t_memory_copy, t_read_nat, t_subset, t_path_hash, t_count_dentry, t_find_dentry;

// used to count the page hit radio
unsigned int nat_read_hit, nat_read_miss, data_read_hit, data_read_miss, hit_total, miss_total;

//************** file system meta *****************
#define FS_OFFSET 4096  // start block address of the partition, 4096 by default

#define __le64  unsigned long long
#define __le32  unsigned int
#define __le16  short
#define __u8  char
typedef unsigned short __u16;
typedef unsigned int __u32;
typedef unsigned long long __u64;

//for linux define
#define FS_PAGE_SIZE 4096

// #define CACHE_ENTRY_NAT 5

//for f2fs define
#define BLOCKS_PER_SEGMENT 512 //THIS IS USE TO BE CALCULATE

#define F2FS_SUPER_OFFSET		1024	/* byte-size offset */
#define F2FS_MIN_LOG_SECTOR_SIZE	9	/* 9 bits for 512 bytes */
#define F2FS_MAX_LOG_SECTOR_SIZE	12	/* 12 bits for 4096 bytes */
#define F2FS_LOG_SECTORS_PER_BLOCK	3	/* log number for sector/blk */
#define F2FS_BLKSIZE			4096	/* support only 4KB block */
#define F2FS_BLKSIZE_BITS		12	/* bits for F2FS_BLKSIZE */
#define F2FS_MAX_EXTENSION		64	/* # of extension entries */
#define F2FS_EXTENSION_LEN		8	/* max size of extension */
#define F2FS_BLK_ALIGN(x)	(((x) + F2FS_BLKSIZE - 1) >> F2FS_BLKSIZE_BITS)

/*
 * For further optimization on multi-head logs, on-disk layout supports maximum
 * 16 logs by default. The number, 16, is expected to cover all the cases
 * enoughly. The implementaion currently uses no more than 6 logs.
 * Half the logs are used for nodes, and the other half are used for data.
 */
#define MAX_ACTIVE_LOGS	16
#define MAX_ACTIVE_NODE_LOGS	8
#define MAX_ACTIVE_DATA_LOGS	8

#define VERSION_LEN	256
#define MAX_VOLUME_NAME		512
#define MAX_PATH_LEN		64
#define MAX_DEVICES		8
#define F2FS_MAX_QUOTAS		3

/*
 * For checkpoint
 */
#define CP_DISABLED_QUICK_FLAG		0x00002000
#define CP_DISABLED_FLAG		0x00001000
#define CP_QUOTA_NEED_FSCK_FLAG		0x00000800
#define CP_LARGE_NAT_BITMAP_FLAG	0x00000400
#define CP_NOCRC_RECOVERY_FLAG	0x00000200
#define CP_TRIMMED_FLAG		0x00000100
#define CP_NAT_BITS_FLAG	0x00000080
#define CP_CRC_RECOVERY_FLAG	0x00000040
#define CP_FASTBOOT_FLAG	0x00000020
#define CP_FSCK_FLAG		0x00000010
#define CP_ERROR_FLAG		0x00000008
#define CP_COMPACT_SUM_FLAG	0x00000004
#define CP_ORPHAN_PRESENT_FLAG	0x00000002
#define CP_UMOUNT_FLAG		0x00000001

#define F2FS_CP_PACKS		2	/* # of checkpoint packs */

/*
 * For NODE structure
 */
#define F2FS_NAME_LEN		255
/* 200 bytes for inline xattrs by default */
#define DEFAULT_INLINE_XATTR_ADDRS	50
#define DEF_ADDRS_PER_INODE	923	/* Address Pointers in an Inode */
#define CUR_ADDRS_PER_INODE(inode)	(DEF_ADDRS_PER_INODE - \
					get_extra_isize(inode))
#define DEF_NIDS_PER_INODE	5	/* Node IDs in an Inode */
#define ADDRS_PER_INODE(inode)	addrs_per_inode(inode)
#define DEF_ADDRS_PER_BLOCK	1018	/* Address Pointers in a Direct Block */
#define ADDRS_PER_BLOCK(inode)	addrs_per_block(inode)
#define NIDS_PER_BLOCK		1018	/* Node IDs in an Indirect Block */

#define ADDRS_PER_PAGE(page, inode)	\
	(IS_INODE(page) ? ADDRS_PER_INODE(inode) : ADDRS_PER_BLOCK(inode))

#define	NODE_DIR1_BLOCK		(DEF_ADDRS_PER_INODE + 1)
#define	NODE_DIR2_BLOCK		(DEF_ADDRS_PER_INODE + 2)
#define	NODE_IND1_BLOCK		(DEF_ADDRS_PER_INODE + 3)
#define	NODE_IND2_BLOCK		(DEF_ADDRS_PER_INODE + 4)
#define	NODE_DIND_BLOCK		(DEF_ADDRS_PER_INODE + 5)

#define F2FS_INLINE_XATTR	0x01	/* file inline xattr flag */
#define F2FS_INLINE_DATA	0x02	/* file inline data flag */
#define F2FS_INLINE_DENTRY	0x04	/* file inline dentry flag */
#define F2FS_DATA_EXIST		0x08	/* file inline data exist flag */
#define F2FS_INLINE_DOTS	0x10	/* file having implicit dot dentries */
#define F2FS_EXTRA_ATTR		0x20	/* file having extra attribute */
#define F2FS_PIN_FILE		0x40	/* file should not be gced */

#pragma pack (1)
/*
 * For superblock
 */

struct f2fs_super_block {
	__le32 magic;			/* Magic Number */
	__le16 major_ver;		/* Major Version */
	__le16 minor_ver;		/* Minor Version */
	__le32 log_sectorsize;		/* log2 sector size in bytes */
	__le32 log_sectors_per_block;	/* log2 # of sectors per block */
	__le32 log_blocksize;		/* log2 block size in bytes */
	__le32 log_blocks_per_seg;	/* log2 # of blocks per segment */
	__le32 segs_per_sec;		/* # of segments per section */
	__le32 secs_per_zone;		/* # of sections per zone */
	__le32 checksum_offset;		/* checksum offset inside super block */
	__le64 block_count;		/* total # of user blocks */
	__le32 section_count;		/* total # of sections */
	__le32 segment_count;		/* total # of segments */
	__le32 segment_count_ckpt;	/* # of segments for checkpoint */
	__le32 segment_count_sit;	/* # of segments for SIT */
	__le32 segment_count_nat;	/* # of segments for NAT */
	__le32 segment_count_ssa;	/* # of segments for SSA */
	__le32 segment_count_main;	/* # of segments for main area */
	__le32 segment0_blkaddr;	/* start block address of segment 0 */
	__le32 cp_blkaddr;		/* start block address of checkpoint */
	__le32 sit_blkaddr;		/* start block address of SIT */
	__le32 nat_blkaddr;		/* start block address of NAT */
	__le32 ssa_blkaddr;		/* start block address of SSA */
	__le32 main_blkaddr;		/* start block address of main area */
	__le32 root_ino;		/* root inode number */
	__le32 node_ino;		/* node inode number */
	__le32 meta_ino;		/* meta inode number */
	__u8 uuid[16];			/* 128-bit uuid for volume */
	__le16 volume_name[MAX_VOLUME_NAME];	/* volume name */
	__le32 extension_count;		/* # of extensions below */
	__u8 extension_list[F2FS_MAX_EXTENSION][F2FS_EXTENSION_LEN];/* extension array */
	__le32 cp_payload;
} ;

struct f2fs_checkpoint {
	__le64 checkpoint_ver;		/* checkpoint block version number */
	__le64 user_block_count;	/* # of user blocks */
	__le64 valid_block_count;	/* # of valid blocks in main area */
	__le32 rsvd_segment_count;	/* # of reserved segments for gc */
	__le32 overprov_segment_count;	/* # of overprovision segments */
	__le32 free_segment_count;	/* # of free segments in main area */

	/* information of current node segments */
	__le32 cur_node_segno[MAX_ACTIVE_NODE_LOGS];
	__le16 cur_node_blkoff[MAX_ACTIVE_NODE_LOGS];
	/* information of current data segments */
	__le32 cur_data_segno[MAX_ACTIVE_DATA_LOGS];
	__le16 cur_data_blkoff[MAX_ACTIVE_DATA_LOGS];
	__le32 ckpt_flags;		/* Flags : umount and journal_present */
	__le32 cp_pack_total_block_count;	/* total # of one cp pack */
	__le32 cp_pack_start_sum;	/* start block number of data summary */
	__le32 valid_node_count;	/* Total number of valid nodes */
	__le32 valid_inode_count;	/* Total number of valid inodes */
	__le32 next_free_nid;		/* Next free node number */
	__le32 sit_ver_bitmap_bytesize;	/* Default value 64 */
	__le32 nat_ver_bitmap_bytesize; /* Default value 256 */
	__le32 checksum_offset;		/* checksum offset inside cp block */
	__le64 elapsed_time;		/* mounted time */
	/* allocation type of current segment */
	unsigned char alloc_type[MAX_ACTIVE_LOGS];//just 192......

	/* SIT and NAT version bitmap */
	unsigned char sit_nat_version_bitmap[1];
	unsigned char sit_nat_version_bitmap_extend[4651];// 1003+3648=4651
} ;

/*
 * For NODE structure
 */

struct f2fs_extent {
	__le32 fofs;		/* start file offset of the extent */
	__le32 blk;		/* start block address of the extent */
	__le32 len;		/* length of the extent */
} ;
struct f2fs_inode {
	__le16 i_mode;			/* file mode */
	__u8 i_advise;			/* file hints */
	__u8 i_inline;			/* file inline flags */
	__le32 i_uid;			/* user ID */
	__le32 i_gid;			/* group ID */
	__le32 i_links;			/* links count */
	__le64 i_size;			/* file size in bytes *///5,6
	__le64 i_blocks;		/* file size in blocks *///7,8
	__le64 i_atime;			/* access time */
	__le64 i_ctime;			/* change time */
	__le64 i_mtime;			/* modification time */
	__le32 i_atime_nsec;		/* access time in nano scale */
	__le32 i_ctime_nsec;		/* change time in nano scale */
	__le32 i_mtime_nsec;		/* modification time in nano scale */
	__le32 i_generation;		/* file version (for NFS) */
	union {
		__le32 i_current_depth;	/* only for directory depth */
		__le16 i_gc_failures;	/*
					 * # of gc failures on pinned file.
					 * only for regular files.
					 */
	};
	__le32 i_xattr_nid;		/* nid to save xattr */
	__le32 i_flags;			/* file attributes */
	__le32 i_pino;			/* parent inode number */
	__le32 i_namelen;		/* file name length */
	__u8 i_name[F2FS_NAME_LEN];	/* file name for SPOR */
	__u8 i_dir_level;		/* dentry_level for large dir */

	struct f2fs_extent i_ext;	/* caching a largest extent */

	union {
		struct {
			__le16 i_extra_isize;	/* extra inode attribute size */
			__le16 i_inline_xattr_size;	/* inline xattr size, unit: 4 bytes */
			__le32 i_projid;	/* project id */
			__le32 i_inode_checksum;/* inode meta checksum */
			__le64 i_crtime;	/* creation time */
			__le32 i_crtime_nsec;	/* creation time in nano scale */
			__le32 i_extra_end[0];	/* for attribute size calculation */
		} __packed;
		__le32 i_addr[DEF_ADDRS_PER_INODE];	/* Pointers to data blocks */
	};
	__le32 i_nid[DEF_NIDS_PER_INODE];	/* direct(2), indirect(2),
						double_indirect(1) node id */
} ;

struct direct_node {
	__le32 addr[DEF_ADDRS_PER_BLOCK];	/* array of data block address */
} ;

struct indirect_node {
	__le32 nid[NIDS_PER_BLOCK];	/* array of data block address */
} ;
/*
 * For NAT entries
 */
#define NAT_ENTRY_PER_BLOCK (FS_PAGE_SIZE / sizeof(struct f2fs_nat_entry))
struct f2fs_nat_entry {
	__u8 version;		/* latest version of cached nat entry */
	__le32 ino;		/* inode number */
	__le32 block_addr;	/* block address */
} ;
struct f2fs_nat_block {
	struct f2fs_nat_entry entries[NAT_ENTRY_PER_BLOCK];
} ;

// for nat journal
struct nat_journal_entry {
	__le32 nid;
	struct f2fs_nat_entry ne;
} ;

#define SUM_JOURNAL_SIZE	4096 - 5 - 7*512  // (F2FS_BLKSIZE - SUM_FOOTER_SIZE - SUM_ENTRY_SIZE)
#define NAT_JOURNAL_ENTRIES	((SUM_JOURNAL_SIZE - 2) / sizeof(struct nat_journal_entry))
#define NAT_JOURNAL_RESERVED	((SUM_JOURNAL_SIZE - 2) % sizeof(struct nat_journal_entry))

struct nat_journal {
	struct nat_journal_entry entries[NAT_JOURNAL_ENTRIES];
	__u8 reserved[NAT_JOURNAL_RESERVED];
} ;

/* only reserve nat journal */
struct f2fs_summary_block {
	// struct f2fs_summary entries[ENTRIES_IN_SUM];
	// union {
		__le16 n_nats;
		// __le16 n_sits;
	// };
	/* spare area is used by NAT or SIT journals */
	// union {
		struct nat_journal nat_j;
		// struct sit_journal sit_j;
	// };
	// struct summary_footer footer;
} ;

/* node block offset on the NAT area dedicated to the given start node id */
#define	NAT_BLOCK_OFFSET(start_nid) ((start_nid) / NAT_ENTRY_PER_BLOCK)
/* start node id of a node block dedicated to the given node id */
#define	START_NID(nid) (((nid) / NAT_ENTRY_PER_BLOCK) * NAT_ENTRY_PER_BLOCK)

#define BITS_PER_BYTE		8
#define F2FS_SLOT_LEN		8
#define SIZE_OF_DIR_ENTRY	11	/* by byte */
#define NR_DENTRY_IN_BLOCK	214	/* the number of dentry in a block */
#define SIZE_OF_DENTRY_BITMAP	((NR_DENTRY_IN_BLOCK + BITS_PER_BYTE - 1) / BITS_PER_BYTE)
#define SIZE_OF_RESERVED	3
struct f2fs_dir_entry {
	__le32 hash_code;	/* hash code of file name */
	__le32 ino;		/* inode number */
	__le16 name_len;	/* lengh of file name */
	__u8 file_type;		/* file type */
} ;

/* 4KB-sized directory entry block */
struct f2fs_dentry_block {
	/* validity bitmap for directory entries in each block */
	__u8 dentry_bitmap[SIZE_OF_DENTRY_BITMAP];
	__u8 reserved[SIZE_OF_RESERVED];
	struct f2fs_dir_entry dentry[NR_DENTRY_IN_BLOCK];
	__u8 filename[NR_DENTRY_IN_BLOCK][F2FS_SLOT_LEN];
} ;


// define global parameters
struct f2fs_super_block sb;
__u64 last_ckpt_ver;
struct f2fs_checkpoint ckpt;
struct f2fs_summary_block sum;



//************** fs-related function *****************
void f2fs_updateSB(unsigned int dataAddr);
void f2fs_updateCP(unsigned int dataAddr);

int f2fs_test_bit(unsigned int nr, char *addr);
unsigned int getNidNATLba(int nid,struct f2fs_super_block  *sb,struct f2fs_checkpoint *ckpt);
unsigned int getNidLba(int nid,unsigned int block_addr);
unsigned int f2fs_read_NAT(int nid,struct f2fs_super_block  *sb,struct f2fs_checkpoint *ckpt);
//this is for hash
#define DELTA 0x9E3779B9
#define F2FS_HASH_COL_BIT	((0x1ULL) << 63)
#define cpu_to_le32 __cpu_to_le32
#define __force
#define __cpu_to_le32(x) ((__force __le32)(__u32)(x))
typedef __le32	f2fs_hash_t;

static void str2hashbuf(const unsigned char *msg, size_t len,unsigned int *buf, int num);
static void TEA_transform(unsigned int buf[4], unsigned int const in[]);
int is_dot_dotdot(char *str,__le16 len);
f2fs_hash_t f2fs_path_hash(char* name,__le16 len1);
unsigned char f2fs_count_valid_dentry(unsigned char para);
unsigned int f2fs_lookup_in_denblk(unsigned int denblk_in_root, char* dir,f2fs_hash_t dir_hash);
unsigned int f2fs_lookup_in_inline_inode(struct f2fs_inode * parent_inode,char* dir,f2fs_hash_t dir_hash);
unsigned int f2fs_find_dir(struct f2fs_super_block* sb, struct f2fs_checkpoint *ckpt, unsigned int par_ino, char *dir, unsigned int dir_len);

//************* fsr function ********************
void init_metadata();
int extract_dir(const char* path, const unsigned int path_len, unsigned int *dir_offset, unsigned int *dir_len);
unsigned int read_inode(unsigned int ino);
/*receive path of file,return the LBA of inode of this file*/
unsigned int f2fs_path_crawl(char* filename, unsigned int len);

void retrieve_address(unsigned int ino, unsigned int *blk_addr, unsigned int *blk_num);
unsigned long long get_file_blocks(unsigned int ino);
unsigned long long get_file_size(unsigned int ino);
unsigned int read_data_sync(unsigned int blk_addr, unsigned int blk_num);

