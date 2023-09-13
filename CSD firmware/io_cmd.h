/**
 * @file io_cmd.h
 * @author Lin Li
 * @brief 
 * 
 * @copyright Copyright (c) 2023 Chongqing University StarLab
 * 
 */
#ifndef IO_CMD_H_
#define IO_CMD_H_

unsigned int flush_buffer(unsigned int radio);

// int handle_dram_flash_write(int logicaladdress);
unsigned int handle_dram_flash_read(unsigned int lpn, unsigned int type);

#endif
