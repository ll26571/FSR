/**
 * @file fsrlib.h
 * @author Lin Li
 * @brief the implementation of FSR library
 * 
 * @copyright Copyright (c) 2023 Chongqing University StarLab
 * 
 */
#ifndef FSRLIB_H
#define FSRLIB_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define NVME_IOCTL_ADMIN_CMD	_IOWR('N', 0x41, struct nvme_admin_cmd)
#define ADMIN_GET_FEATURES 0x0A
#define MAX_HOST_CMD 4096

// define for nvme admin cmd
struct nvme_passthru_cmd {
	__u8	opcode;
	__u8	flags;
	__u16	rsvd1;
	__u32	nsid;
	__u32	cdw2;
	__u32	cdw3;
	__u64	metadata;
	__u64	addr;
	__u32	metadata_len;
	__u32	data_len;
	__u32	cdw10;
	__u32	cdw11;
	__u32	cdw12;
	__u32	cdw13;
	__u32	cdw14;
	__u32	cdw15;
	__u32	timeout_ms;
	__u32	result;
};

#define nvme_admin_cmd nvme_passthru_cmd

/**
 * @brief issue the task to the CSD.
 * 
 * @param dev_nvme the path of the device
 * @param buf including the configurations of the task
 * @param buf_len the length of the buffer
 * @param retrieve 1 for in-storage retrieving, 0 for not
 */
void issue_task(char* dev_nvme, char* buf, unsigned int buf_len, unsigned int retrieve){
    __u32 namespace_id = 0;
    __u32 feature_id = retrieve ? 0x12 : 0x11;  // not 0x11
    __u8 opcode= ADMIN_GET_FEATURES;

    //allocate a aligned buf to send
    void *buf_posix_memalign = NULL;
    if (posix_memalign(&buf_posix_memalign, getpagesize(),MAX_HOST_CMD)) {
        printf("can not allocate feature payload\n");
    }

    memset(buf_posix_memalign, 0, MAX_HOST_CMD);
    //copy from buf which user inputted to aligined buffer
    memcpy((void *)buf_posix_memalign,(void *)buf,buf_len);

    //start to send
    //Open nvme devices
    int fd= open(dev_nvme,O_RDONLY);
    if (fd < 0) 
        printf("Wrong args:dev_nvme.can't open dev_nvme.\n");

    //fill in DMA struct
    struct nvme_admin_cmd cmd = {
    .opcode		= opcode,
    .nsid		= namespace_id,
    .cdw10		= feature_id,
    .cdw11		= 32,
    .cdw12		= 22,
    .addr		= (__u64)(uintptr_t) buf_posix_memalign,
    .data_len	= MAX_HOST_CMD,
	};

    //send to devices
    int err = ioctl(fd, NVME_IOCTL_ADMIN_CMD, cmd);

    if(err < 0){
      printf("[dma] ioctl failed!\n");
    }

    return;
}

#endif
