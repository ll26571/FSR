/**
 * @file search.h
 * @author Lin Li
 * @brief define data struct and functions for searching
 * 
 * @copyright Copyright (c) 2023 Chongqing University StarLab
 * 
 */
#ifndef SEARCH_H_
#define SEARCH_H_
#include "xtime_l.h"

#define MAX_SEARCH_PAGE_NUM 10*1024*1024/16  // the num of pages containeed in 10GB

struct addressBlock
{
    unsigned int blockAddr;
    unsigned int blockNum;
    unsigned int endFlag;
};

struct searchTask
{
    unsigned int cmdSlotTag;
    unsigned int taskValid;
    unsigned int need_path_walk;
    unsigned int totalHitCounts;
    unsigned int searchPageNum;
    unsigned int pageCompleteCount;
    char targetString[32];

    unsigned int  rxDmaExe : 1;
	unsigned int  rxDmaTail : 8;
	unsigned int  rxDmaOverFlowCnt;
    unsigned int  reserved1 : 23;

    // struct targetPage
    // {
    //     unsigned int done;
    //     unsigned int hitCount;
    // };
    // struct targetPage pageTable[MAX_SEARCH_PAGE_NUM];
};

XTime time_start_search, time_end_search;
XTime time_start_retrieve, time_end_retrieve;

extern struct searchTask* searchTask;

void delay_ms(unsigned int mseconds);
void delay_us(unsigned int useconds);

void initSearchTask();

void analysisTask(unsigned int startSec, unsigned int nlb);
void CheckTaskDone();
void abort_task();

int Sunday_FindIndex(char *target, char temp);
unsigned int Sunday(char *source, char *target);

void searchInPage(unsigned int pageDataBufAddr, unsigned int searchPageIndex);

#endif
