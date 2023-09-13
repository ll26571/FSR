#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "fiemap.h"
#include "fsrlib.h"

struct addr_extent
{
    __u32 block_addr;
    __u32 block_num;
    __u32 end_flag;
};

int main(int argc, char *argv[]){
    int file_fd, num_extent = 0, extents_size = 0;
    struct fiemap *fiemap;
    // struct fiemap_extent* extents; //store extents of file

    char *txt_file;
    if(argc != 2){
        txt_file = "/home/nvme/d1/d2/d3/d4/d5/hello_16KB.txt";
        printf ("no file is provided, use default: %s\n", txt_file);
    }
    else {
        txt_file = argv[1];
    }

    // open file
    // file_fd = open(argv[1], O_RDWR);
    file_fd = open(txt_file, O_RDWR);
    if (file_fd < 0)
        return 1;

    fiemap = (struct fiemap *)malloc(sizeof(struct fiemap));
    memset(fiemap, 0, sizeof(struct fiemap));
    fiemap->fm_length = ~0;

    if (ioctl(file_fd, FS_IOC_FIEMAP, fiemap) < 0) {
        fprintf(stderr, "fiemap ioctl() failed\n");
        return NULL;
    }
    
    num_extent = fiemap->fm_mapped_extents;
    extents_size = sizeof(struct fiemap_extent) * (fiemap->fm_mapped_extents);
    if ((fiemap = (struct fiemap*)realloc(fiemap,sizeof(struct fiemap) + extents_size)) == NULL) {
        fprintf(stderr, "Out of memory allocating fiemap\n");
        return NULL;
    }

    memset(fiemap->fm_extents, 0, extents_size);
    fiemap->fm_extent_count = fiemap->fm_mapped_extents;
    fiemap->fm_mapped_extents = 0;

    if (ioctl(file_fd, FS_IOC_FIEMAP, fiemap) < 0) {
        fprintf(stderr, "fiemap ioctl() failed\n");
        return NULL;
    }

    //repare data buffer
    int buf_size = sizeof(struct addr_extent) * num_extent + sizeof(int) + 16;
    char* buf_start = (char*)malloc(buf_size);
    memset(buf_start,0,buf_size);

    char* buf_index = buf_start;

    //repare target string
    char target[16] = "hello";
    //copy target string
    memcpy(buf_index,target,16);
    buf_index += 16;

    //copy extent size 
    memcpy(buf_index,(char*)(&num_extent),sizeof(int));
    buf_index += sizeof(int);

    //reformat the extents to addr_extent
    struct addr_extent* content = (struct addr_extent*)malloc(sizeof(struct addr_extent));
    for (int i = 0; i < num_extent; i++) {
        content->block_addr = fiemap->fm_extents[i].fe_physical / 4096 + 4096;  // add 4096 offset, for /media/cosmos
        content->block_num = fiemap->fm_extents[i].fe_length / 4096;
        content->end_flag = i == (num_extent - 1) ? 1 : 0;

        //copy content
        memcpy(buf_index,(char*)content,sizeof(struct addr_extent));
        buf_index += sizeof(struct addr_extent);
    }
    
    issue_task("/dev/nvme0n1",buf_start,buf_size, 0);

    close(file_fd);
    free(buf_start);
    // free(extents);

    return 0;
}
