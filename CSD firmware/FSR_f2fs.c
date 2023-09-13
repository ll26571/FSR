/**
 * @file FSR_f2fs.c
 * @author Lin Li
 * @brief Inlcuding the implement of FSR based on F2FS.
 * 
 * @copyright Copyright (c) 2023 Chongqing University StarLab
 * 
 */
#include "FSR_f2fs.h"
#include "memory_map.h"

// Used for initialization
void init_metadata(){
	// reset the values
	ckpt.checkpoint_ver = 0;
	last_ckpt_ver = 0;
	nat_read_hit = 0;
	nat_read_miss = 0;
	data_read_hit = 0;
	data_read_miss = 0;
}

// used to update super block info
void f2fs_updateSB(unsigned int dataAddr){
	struct f2fs_super_block *SB_origin = (struct f2fs_super_block *)(dataAddr + 0x400);
	sb.log_blocks_per_seg = SB_origin->log_blocks_per_seg;
	sb.cp_blkaddr = SB_origin->cp_blkaddr;
	sb.nat_blkaddr = SB_origin->nat_blkaddr;
	sb.root_ino = SB_origin->root_ino;
	sb.cp_payload = SB_origin->cp_payload;

// #ifdef DEBUG
	printf("[updateSB] Super Block update sucessfully.\r\n");
// #endif
}

// used to update checkpoint info
void f2fs_updateCP(unsigned int dataAddr){
	struct f2fs_checkpoint *temp_ckpt = (struct f2fs_checkpoint *)dataAddr;
	unsigned int update = 0;

	if (temp_ckpt->checkpoint_ver == 0)
		return;
	
	if (ckpt.checkpoint_ver == 0)
		update = 1;

	if (update || temp_ckpt->checkpoint_ver == (ckpt.checkpoint_ver + 1) || temp_ckpt->checkpoint_ver == (last_ckpt_ver + 1)){  
		// update ckpt
		ckpt.checkpoint_ver = temp_ckpt->checkpoint_ver;
		ckpt.ckpt_flags = temp_ckpt->ckpt_flags;
		ckpt.sit_ver_bitmap_bytesize = temp_ckpt->sit_ver_bitmap_bytesize;
		ckpt.nat_ver_bitmap_bytesize = temp_ckpt->nat_ver_bitmap_bytesize;

		memcpy(&ckpt.sit_nat_version_bitmap, &temp_ckpt->sit_nat_version_bitmap, ckpt.sit_ver_bitmap_bytesize + ckpt.nat_ver_bitmap_bytesize);
		// printf("[updateCP] new CKPT version: %llx\r\n", ckpt.checkpoint_ver);

		// update nat journal
		if (temp_ckpt->cp_pack_start_sum > 3){
			printf("[updateCP] Warning: cp_pack_start_sum > 3 !!!\r\n");		
			ckpt.checkpoint_ver = 0;	
		}
		else{
			struct f2fs_summary_block *sum_block = (struct f2fs_summary_block *)(dataAddr + temp_ckpt->cp_pack_start_sum * 4096);

			if (sum_block->n_nats == 0)  // if no nat_journals in sum block, it is not necessary to memcpy
				sum.n_nats = 0;
			else{
				memcpy(&sum, sum_block, sizeof(struct f2fs_summary_block));	
			}
		}
		xil_printf("[updateCP] cp update successfully.\r\n");
// #ifdef DEBUG
// 	printf("[updateCP] n_nats in summary block: %x\r\n", sum.n_nats);
// #endif
	}
	else if (temp_ckpt->checkpoint_ver == ckpt.checkpoint_ver){  
		// only update the nat journal
		if (temp_ckpt->cp_pack_start_sum > 3)
			printf("[updateCP] Warning: cp_pack_start_sum > 3 !!!\r\n");
		else{
			struct f2fs_summary_block *sum_block = (struct f2fs_summary_block *)(dataAddr + temp_ckpt->cp_pack_start_sum * 4096);

			if (sum_block->n_nats != sum.n_nats){  // if no nat_journals in sum block, it is not necessary to memcpy
				memcpy(&sum, sum_block, sizeof(struct f2fs_summary_block));	
			}
		}
		printf("[updateCP] n_nats in summary block: %x\r\n", sum.n_nats);
	}
	else {
		printf("[updateCP] received version: %llx, cached version: %llx, update failed.\r\n", temp_ckpt->checkpoint_ver, ckpt.checkpoint_ver);
	}
	last_ckpt_ver = temp_ckpt->checkpoint_ver;
}


// This is used to calculate the lba of NAT block of nid;
inline int f2fs_test_bit(unsigned int nr, char *addr){
	int mask;

	addr += (nr >> 3);
	mask = 1 << (7 - (nr & 0x07));
	return mask & *addr;
}

unsigned int getNidNATLba(int nid, struct f2fs_super_block *sb, struct f2fs_checkpoint *ckpt){
	// 1.get bitmap
	__le32 nat_bitmap_bytesize = ckpt->nat_ver_bitmap_bytesize;
	char *version_nat_bitmap;
	int offset;

	if (ckpt->ckpt_flags & CP_LARGE_NAT_BITMAP_FLAG){
		offset = 0;
		version_nat_bitmap = &ckpt->sit_nat_version_bitmap + offset + sizeof(__le32);
	}
	else if (sb->cp_payload > 0){
		version_nat_bitmap = &ckpt->sit_nat_version_bitmap;
	}
	else{
		// actually it will execute this ...
		offset = ckpt->sit_ver_bitmap_bytesize;
		version_nat_bitmap = &ckpt->sit_nat_version_bitmap + offset;
	}

	char *nat_bitmap = version_nat_bitmap;

	// 2.get the current nat block page
	int block_off;
	int block_addr;
	block_off = NAT_BLOCK_OFFSET(nid);
	unsigned int blocks_per_seg = 1 << sb->log_blocks_per_seg;
	block_addr = (int)(sb->nat_blkaddr + (block_off << 1) - (block_off & (blocks_per_seg - 1)));
	if (f2fs_test_bit(block_off, nat_bitmap)){
		block_addr += blocks_per_seg;
	}
	// get lba of nid
	// 	printf("block_addr:%x\r\n",block_addr);
	return block_addr;
}

inline unsigned int getNidLba(int nid, unsigned int block_addr){
	unsigned int dramAddrNat = handle_dram_flash_read(block_addr / 4, 0);

	unsigned int entry_offset = nid - START_NID(nid);  // the offset from the first entry
	unsigned int start_addr = dramAddrNat + block_addr % 4 * 4096 + entry_offset * sizeof(struct f2fs_nat_entry);  // the block addr of the target entry
	unsigned int aligned_start_addr = start_addr & 0xfffffffc;  // get the aligned address
	unsigned int buffer_size = start_addr - aligned_start_addr + sizeof(struct f2fs_nat_entry);
	char buffer[buffer_size];
	memcpy(buffer, aligned_start_addr, buffer_size);
	struct f2fs_nat_entry *nat_entry = (struct f2fs_nat_entry*)(start_addr - aligned_start_addr + buffer);

	return nat_entry->block_addr;
}

inline unsigned int f2fs_read_NAT(int nid, struct f2fs_super_block *sb, struct f2fs_checkpoint *ckpt){
	unsigned int block_addr = getNidNATLba(nid, sb, ckpt);
	return getNidLba(nid, block_addr + FS_OFFSET);
}

void retrieve_address(unsigned int ino, unsigned int *blk_addr, unsigned int *blk_num){
	struct f2fs_inode *inode = (struct f2fs_inode *)(read_inode(ino));

	*blk_addr = inode->i_addr[0] + 4096;
	unsigned int i_size = inode->i_size & 0xffffffff;
	*blk_num = (i_size + 4095) / 4096;

#ifdef DEBUG
	xil_printf("[retrieve_address] ino %d , blk_addr: %d , blk_num: %d\r\n", ino, *blk_addr, *blk_num);
#endif
}

// Get the file size in blocks, including inode itself.
unsigned long long get_file_blocks(unsigned int ino){
	struct f2fs_inode *inode = (struct f2fs_inode *)(read_inode(ino));

	return inode->i_blocks;
}

// Get the file size in bytes
unsigned long long get_file_size(unsigned int ino){
	struct f2fs_inode *inode = (struct f2fs_inode *)(read_inode(ino));

	return inode->i_size;
}

// load data block
unsigned int read_data_sync(unsigned int blk_addr, unsigned int blk_num){
	if (blk_num > 12800){  // too much blocks
		xil_printf("[read_data_sync] Error! Too much blocks to load.\r\n");
		return 1;
	}

	for (unsigned int block = blk_addr; block <= blk_addr + blk_num - 1; block++){
		unsigned int addr = handle_dram_flash_read(block / 4, 1);
		addr = addr + (block % 4) * 4096;
		memcpy(DATA_SPACE_ADDR + (block - blk_addr) * 4096, addr, 4096);
	}

	return 0;
}

// Load the inode from flash and return the addr
unsigned int read_inode(unsigned int ino){
	unsigned int inode_pbn = 0;
	
	for (int i = 0; i < sum.n_nats; i++){
		if (sum.nat_j.entries[i].nid == ino){
			inode_pbn = sum.nat_j.entries[i].ne.block_addr;
			break;
		}
	}

#ifdef TIME_COUNTER
	XTime t_start, t_end;
	XTime_GetTime(&t_start);
#endif

	if (inode_pbn == 0)
		inode_pbn = FS_OFFSET + f2fs_read_NAT(ino, &sb, &ckpt);

#ifdef TIME_COUNTER
	XTime_GetTime(&t_end);
	t_read_nat += t_end - t_start;
#endif

	if (inode_pbn == FS_OFFSET){
		xil_printf("[read_inode] Error! read nat failed! ino: %x\r\n", ino);
		return 0;
	}

#ifdef TIME_COUNTER
	XTime_GetTime(&t_start);
#endif

	unsigned int data_page_addr = handle_dram_flash_read(inode_pbn / 4, 1);

#ifdef TIME_COUNTER
	XTime_GetTime(&t_end);
	t_read_block += t_end - t_start;
#endif

	return (inode_pbn % 4) * 4096 + data_page_addr;
}

// for hash
static void TEA_transform(unsigned int buf[4], unsigned int const in[]){
	__u32 sum = 0;
	__u32 b0 = buf[0], b1 = buf[1];
	__u32 a = in[0], b = in[1], c = in[2], d = in[3];
	int n = 16;

	do{
		sum += DELTA;
		b0 += ((b1 << 4) + a) ^ (b1 + sum) ^ ((b1 >> 5) + b);
		b1 += ((b0 << 4) + c) ^ (b0 + sum) ^ ((b0 >> 5) + d);
	} while (--n);

	buf[0] += b0;
	buf[1] += b1;
}

static void str2hashbuf(const unsigned char *msg, size_t len, unsigned int *buf, int num){
	unsigned pad, val;
	int i;

	pad = (__u32)len | ((__u32)len << 8);
	pad |= pad << 16;

	val = pad;
	if (len > num * 4)
		len = num * 4;
	for (i = 0; i < len; i++){
		if ((i % 4) == 0)
			val = pad;
		val = msg[i] + (val << 8);
		if ((i % 4) == 3){
			*buf++ = val;
			val = pad;
			num--;
		}
	}
	if (--num >= 0)
		*buf++ = val;
	while (--num >= 0)
		*buf++ = pad;
}

f2fs_hash_t f2fs_path_hash(char *name, __le16 len1){
	__u32 hash;
	f2fs_hash_t f2fs_hash;
	const unsigned char *p;
	__u32 in[8], buf[4];
	size_t len = len1;

	if (is_dot_dotdot(name, len1))
		return 0;

	buf[0] = 0x67452301;
	buf[1] = 0xefcdab89;
	buf[2] = 0x98badcfe;
	buf[3] = 0x10325476;

	p = name;
	while (1){
		str2hashbuf(p, len, in, 4);
		TEA_transform(buf, in);
		p += 16;
		if (len <= 16)
			break;
		len -= 16;
	}
	hash = buf[0];
	f2fs_hash = cpu_to_le32(hash & ~F2FS_HASH_COL_BIT);

	return f2fs_hash;
}

inline int is_dot_dotdot(char *str, __le16 len){
	if (len == 1 && str[0] == '.')
		return 1;

	if (len == 2 && str[0] == '.' && str[1] == '.')
		return 1;

	return 0;
}

// cut and extract the next dir from the path string
int extract_dir(const char* path, const unsigned int path_len, unsigned int *dir_offset, unsigned int *dir_len){
	if (path[*dir_offset + *dir_len] == '\0')  // it's already the last stage, there's no need to go down.
		return 0;
	
	unsigned int offset = *dir_offset + *dir_len + 1;
	unsigned int len = 0;
	for (unsigned int i = offset; i < path_len; i++, len++){
		if (path[i] == '/')
			break;
	}

	*dir_offset = offset;
	*dir_len = len;
	return 1;
}

// receive path of file, return the ino of this file
unsigned int f2fs_path_crawl(char *path, unsigned int path_len){

#ifdef DEBUG
	printf("[f2fs_path] target path is:%s\r\n", path);
#endif

	if ((path[0] == '/') && (path[1] == '\0')) {
		printf("[f2fs_path]: target path is root.\n");
		return sb.root_ino;
	}

	__le32 par_ino = sb.root_ino;
	__le32 file_ino = 0;
	unsigned int dir_index = 0, dir_len = 0;

	while (extract_dir(path, path_len, &dir_index, &dir_len)){
		// look for the child dir and return its ino
		file_ino = f2fs_find_dir(&sb, &ckpt, par_ino, path + dir_index, dir_len);
		if (file_ino == 0){
			xil_printf("[f2fs_path] !!! find dir failed !!!\n");
			return 0;
		}
		// dir is now the parent
		par_ino = file_ino;
	}

#ifdef DEBUG
	printf("[f2fs_path] the final fileino: %d\n", file_ino);
#endif

	return file_ino;
}

unsigned int f2fs_find_dir(struct f2fs_super_block *sb, struct f2fs_checkpoint *ckpt, __le32 par_ino, char *dir, unsigned int dir_len){
	if (par_ino == 0)
		return 0;

	unsigned int next_ino = 0;
	struct f2fs_inode *par_inode = (struct f2fs_inode *)(read_inode(par_ino));

	// calculate the hash value for dir
#ifdef TIME_COUNTER
	XTime_GetTime(&t_start);
#endif

	f2fs_hash_t dir_hash = f2fs_path_hash(dir, dir_len);

#ifdef TIME_COUNTER
	XTime_GetTime(&t_end);
	t_path_hash += t_end - t_start;
#endif
			 
	// the i_blocks includes the inode itself
	unsigned long long i_blocks = par_inode->i_blocks;

	if (i_blocks == 1)  // this indicates that the file data is stored inline
		next_ino = f2fs_lookup_in_inline_inode(par_inode, dir, dir_hash);
	else{
		i_blocks = i_blocks > 20? 20:i_blocks;

		for (int i = 0; i < i_blocks; i++){
			next_ino = f2fs_lookup_in_denblk(par_inode->i_addr[i], dir, dir_hash);
			if (next_ino != 0)
				break;
		}
	}

	return next_ino;
}

// look up dir in one dentry block
unsigned int f2fs_lookup_in_denblk(unsigned int denblk_in_root, char *dir, f2fs_hash_t dir_hash){
	unsigned int next_ino = 0;

	// read denblk_in_root address
#ifdef TIME_COUNTER
	XTime t_start, t_end;
	XTime_GetTime(&t_start);
#endif

	unsigned int dramAddrdenblk_in_root = handle_dram_flash_read((denblk_in_root + FS_OFFSET) / 4, 1);

#ifdef TIME_COUNTER
	XTime_GetTime(&t_end);
	t_read_block += t_end - t_start;
#endif

	struct f2fs_dentry_block *par_den_blk = (struct f2fs_dentry_block *)(dramAddrdenblk_in_root + denblk_in_root % 4 * 4096);
	// count the number of valid dentries
	__u8 valid_dentry = 0;

#ifdef TIME_COUNTER
	XTime_GetTime(&t_start);
#endif

	for (int i = 0; i < SIZE_OF_DENTRY_BITMAP; i++){
		if (par_den_blk->dentry_bitmap[i] == 0x00)
			break;
		else 
			valid_dentry = valid_dentry + f2fs_count_valid_dentry(par_den_blk->dentry_bitmap[i]);
	}

#ifdef TIME_COUNTER
	XTime_GetTime(&t_end);
	t_count_dentry += t_end - t_start;
#endif

#ifdef DEBUG
	printf("[lookup_in_denblk] valid dentry number is %X\n", valid_dentry);
#endif

	unsigned int buffer_size = 30 + valid_dentry * sizeof(struct f2fs_dir_entry);
	char dentries_buffer[buffer_size];

#ifdef TIME_COUNTER
	XTime_GetTime(&t_start);
#endif
	memcpy(dentries_buffer, par_den_blk, buffer_size);
#ifdef TIME_COUNTER
	XTime_GetTime(&t_end);
	t_memory_copy += t_end - t_start;
#endif

	struct f2fs_dir_entry *dentry = (struct f2fs_dir_entry *)(dentries_buffer + 30);

#ifdef TIME_COUNTER
	XTime_GetTime(&t_start);
#endif
	// iterate through dir_entry
	for (int i = 0; i < valid_dentry; i++, dentry++){
		if (dir_hash == dentry->hash_code) {
			// compare the filename
			short name_len = dentry->name_len;
			if (name_len == 0){
				printf("name len is 0\n");
				continue;
			}
			// printf("name_len is %X\n", name_len);
			int match = 1;
			for (int j = 0; j < name_len; j++) {
				if (dir[j] != par_den_blk->filename[i][j]) {
					match = 0;
					break;
				}
			}
			if (match) {
				next_ino = dentry->ino;
				break;
			}
		}
	}
#ifdef TIME_COUNTER
	XTime_GetTime(&t_end);
	t_find_dentry += t_end - t_start;
#endif

	return next_ino;
}

// look up dir in the inode
unsigned int f2fs_lookup_in_inline_inode(struct f2fs_inode *parent_inode, char *dir, f2fs_hash_t dir_hash){
	unsigned int next_ino = 0;

	__u8 valid_dentry = 0;
	char *inline_den = (char *)parent_inode + 16 * 22 + 12; // the inline_den begins at i_addr[1]
	
#ifdef TIME_COUNTER
	XTime t_start, t_end;
	XTime_GetTime(&t_start);
#endif
	for (int i = 0; i < SIZE_OF_DENTRY_BITMAP; i++){
		if (*(inline_den + i) == 0x00)
			break;
		else
			valid_dentry = valid_dentry + f2fs_count_valid_dentry(*(inline_den + i));
	}
#ifdef TIME_COUNTER
	XTime_GetTime(&t_end);
	t_count_dentry += t_end - t_start;
#endif

#ifdef DEBUG
	printf("[lookup_in_inline_inode] valid_entry in inline is %d\n",valid_dentry);
#endif

	// skip the bitmap and reserved words to get to the first dir_entry
	inline_den = inline_den + 30;

	unsigned int aligned_addr_start = (unsigned int)inline_den & 0xfffffffc;
	unsigned int addr_end = inline_den + valid_dentry * sizeof(struct f2fs_dir_entry) - 1;
	unsigned int buffer_size = addr_end - aligned_addr_start + 1;
	char entries_buffer[buffer_size];

#ifdef TIME_COUNTER
	XTime_GetTime(&t_start);
#endif
	memcpy(entries_buffer, aligned_addr_start, buffer_size);
#ifdef TIME_COUNTER
	XTime_GetTime(&t_end);
	t_memory_copy += t_end - t_start;
#endif

	struct f2fs_dir_entry *entry = (struct f2fs_dir_entry *)((unsigned int)inline_den - aligned_addr_start + entries_buffer);

#ifdef TIME_COUNTER
	XTime_GetTime(&t_start);
#endif
	for (int i = 0; i < valid_dentry; i++, entry++){
		if (entry->hash_code == dir_hash) {
			short name_len = entry->name_len;
			if (name_len == 0)
				continue;

			int match = 1;
			int offset = 125 * 16 + 2;
			char *fname = inline_den + offset;
			for (int j = 0; j < name_len; j++) {
				fname = inline_den + offset + i * F2FS_SLOT_LEN + j;
				if (dir[j] != *fname) {
					match = 0;
					break;
				}
			}
			if (match) {
				next_ino = entry->ino;
				break;
			}
		}
	}
#ifdef TIME_COUNTER
	XTime_GetTime(&t_end);
	t_find_dentry += t_end - t_start;
#endif

	return next_ino;
}

unsigned char f2fs_count_valid_dentry(unsigned char para){
	if (para & 0x01)
		para = para;
	if (para & 0x02)
		para = (para & 0xfd) + 1;
	if (para & 0x04)
		para = (para & 0xfb) + 1;
	if (para & 0x08)
		para = (para & 0xf7) + 1;
	if (para & 0x10)
		para = (para & 0xef) + 1;
	if (para & 0x20)
		para = (para & 0xdf) + 1;
	if (para & 0x40)
		para = (para & 0xbf) + 1;
	if (para & 0x80)
		para = (para & 0x7f) + 1;
	return para;
}
