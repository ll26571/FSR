#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "fsrlib.h"

int main(int argc, char const *argv[])
{
    if(argc != 2){
        printf ("Usage: fsr-search file_path(started from /).\n");
        return 1;
    }

    int buf_size = 16 + 4 + 256;  // 16 for target_str, 4 for path_len, 256 for path
    char *buf_start = (char *)malloc(buf_size);
    memset(buf_start, 0, buf_size);
    char * buf_index = buf_start;

    char target[16] = "hello";
    memcpy(buf_index, target, 16);
    buf_index += 16;

    int path_len = strlen(argv[1]);
    if (path_len > 256){
        printf("the length of file path is longer than 256!\n");
        return 1;
    }
    *((unsigned int *)buf_index) = path_len;
    buf_index += 4;
    // printf("path len: %d\n", path_len);
    memcpy(buf_index, argv[1], path_len);

    issue_task("/dev/nvme0n1", buf_start, buf_size, 1);

    return 0;
}

