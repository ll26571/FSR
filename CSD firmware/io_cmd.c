/**
 * @file io_cmd.c
 * @author Lin Li
 * @brief 
 * 
 * @copyright Copyright (c) 2023 Chongqing University StarLab
 * 
 */
#include "io_cmd.h"

#include "FSR_f2fs.h"
#include "lru_buffer.h"
#include "low_level_scheduler.h"
#include "memory_map.h"



static void move_to_tail(unsigned int entry, unsigned int dieNo) {
	//unlink the entry
	if((bufMap->bufEntry[entry].nextEntry != 0x7fff) && (bufMap->bufEntry[entry].prevEntry != 0x7fff)) {
		bufMap->bufEntry[bufMap->bufEntry[entry].prevEntry].nextEntry = bufMap->bufEntry[entry].nextEntry;
		bufMap->bufEntry[bufMap->bufEntry[entry].nextEntry].prevEntry = bufMap->bufEntry[entry].prevEntry;
	}
	else if((bufMap->bufEntry[entry].nextEntry == 0x7fff) && (bufMap->bufEntry[entry].prevEntry != 0x7fff)) {
		bufMap->bufEntry[bufMap->bufEntry[entry].prevEntry].nextEntry = 0x7fff;
		bufLruList->bufLruEntry[dieNo].tail = bufMap->bufEntry[entry].prevEntry;
	}
	else if((bufMap->bufEntry[entry].nextEntry != 0x7fff) && (bufMap->bufEntry[entry].prevEntry== 0x7fff)) {
		bufMap->bufEntry[bufMap->bufEntry[entry].nextEntry].prevEntry  = 0x7fff;
		bufLruList->bufLruEntry[dieNo].head = bufMap->bufEntry[entry].nextEntry;
	}

	//move
	if(bufLruList->bufLruEntry[dieNo].tail != 0x7fff) {
		bufMap->bufEntry[entry].nextEntry = 0x7fff;
		bufMap->bufEntry[entry].prevEntry = bufLruList->bufLruEntry[dieNo].tail;
		bufMap->bufEntry[bufLruList->bufLruEntry[dieNo].tail].nextEntry = entry;
		bufLruList->bufLruEntry[dieNo].tail = entry;
	}
	else {
		bufMap->bufEntry[entry].nextEntry = 0x7fff;
		bufMap->bufEntry[entry].prevEntry = 0x7fff;
		bufLruList->bufLruEntry[dieNo].head = entry;
		bufLruList->bufLruEntry[dieNo].tail = entry;
	}
}

/**
 * @brief scan the lru buffer and flush half of them used by fs.
 * @param radio / 100.
 * @return the count of flushed pages.
 */
unsigned int flush_buffer(unsigned int radio)
{
	unsigned int dieNo;
	unsigned int valid[DIE_NUM];
	unsigned int valid_total = 0;

	// scan
	unsigned int entry;
	for (dieNo = 0; dieNo < DIE_NUM; dieNo++) {
		valid[dieNo] = 0;
		entry = bufLruList->bufLruEntry[dieNo].head;

		while (entry != 0x7fff) {
			if (bufMap->bufEntry[entry].lpn >= 10112 && bufMap->bufEntry[entry].lpn < 1310721) {
				valid[dieNo]++;
				valid_total++;
			}
			entry = bufMap->bufEntry[entry].nextEntry;
		}
	}

	unsigned int need_flush = valid_total * radio / 100;
	unsigned int flushed_count = 0;
	// move half of the target entries to the tail
	for (dieNo = 0; dieNo < DIE_NUM; dieNo++) {
		if (flushed_count == need_flush)
			return flushed_count;
		if (valid[dieNo] == 0)
			continue;

		entry = bufLruList->bufLruEntry[dieNo].head;
		// if (valid[dieNo] == 1) {
			while (entry != 0x7fff) {
				if (bufMap->bufEntry[entry].lpn >= 10112 && bufMap->bufEntry[entry].lpn < 1310721) {
					move_to_tail(entry, dieNo);
					flushed_count++;
					// break;
				}
				entry = bufMap->bufEntry[entry].nextEntry;
			}
			// continue;
		// }
		// else {
		// 	for (unsigned int i = 0; i < valid[dieNo] / 2; i++) {
		// 		if (bufMap->bufEntry[entry].lpn >= 10112 && bufMap->bufEntry[entry].lpn < 1310721) {
		// 			move_to_tail(entry, dieNo);
		// 			flushed_count++;
		// 		}
		// 		entry = bufMap->bufEntry[entry].nextEntry;
		// 	}
		// }
	}

    return flushed_count;
}

// 0:nat, 1:data
unsigned int handle_dram_flash_read(unsigned int lpn, unsigned int type)
{
    unsigned int bufferEntry, dataPageAddr;

	bufferEntry = CheckBufHit(lpn);
	if (bufferEntry != 0x7fff) {  // hit
		if (type == 0)
			nat_read_hit++;
		else
			data_read_hit++;
		return BUFFER_ADDR + bufferEntry * BUF_ENTRY_SIZE;
	}
	else{  // miss, need to read from flash
		if (type == 0)
			nat_read_miss++;
		else
			data_read_miss++;
		bufferEntry = AllocateBufEntry(lpn);
		bufMap->bufEntry[bufferEntry].dirty = 0;

		//link
		unsigned int dieNo = lpn % DIE_NUM;
		unsigned int dieLpn = lpn / DIE_NUM;
		if(bufLruList->bufLruEntry[dieNo].head != 0x7fff)
		{
			bufMap->bufEntry[bufferEntry].prevEntry = 0x7fff;
			bufMap->bufEntry[bufferEntry].nextEntry = bufLruList->bufLruEntry[dieNo].head;
			bufMap->bufEntry[bufLruList->bufLruEntry[dieNo].head].prevEntry = bufferEntry;
			bufLruList->bufLruEntry[dieNo].head = bufferEntry;
		}
		else
		{
			bufMap->bufEntry[bufferEntry].prevEntry = 0x7fff;
			bufMap->bufEntry[bufferEntry].nextEntry = 0x7fff;
			bufLruList->bufLruEntry[dieNo].head = bufferEntry;
			bufLruList->bufLruEntry[dieNo].tail = bufferEntry;
		}
		bufMap->bufEntry[bufferEntry].lpn = lpn;

		LOW_LEVEL_REQ_INFO lowLevelCmd;
		if (pageMap->pmEntry[dieNo][dieLpn].ppn != 0xffffffff)
		{
			lowLevelCmd.rowAddr = pageMap->pmEntry[dieNo][dieLpn].ppn;
			lowLevelCmd.spareDataBuf = SPARE_ADDR;
			lowLevelCmd.devAddr = BUFFER_ADDR + bufferEntry * BUF_ENTRY_SIZE;
			lowLevelCmd.chNo = dieNo % CHANNEL_NUM;
			lowLevelCmd.wayNo = dieNo / CHANNEL_NUM;
			lowLevelCmd.bufferEntry = bufferEntry;
			lowLevelCmd.request = V2FCommand_ReadPageTrigger;
			lowLevelCmd.search = 0;
			PushToReqQueue(&lowLevelCmd);
			reservedReq = 1;

			while (ExeLowLevelReqPerCh(lowLevelCmd.chNo, REQ_QUEUE)) ;

			return lowLevelCmd.devAddr;
		}
		else{
			xil_printf("[handle_dram_flash_read] lpn %d not has ppn!\r\n", lpn);
			return 0xffffffff;
		}
	}
}
