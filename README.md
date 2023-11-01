FSR
==============================

This repository contains the reference implementation of **Optimizing the Performance of NDP Operations by Retrieving File Semantics in Storage, DAC'23**.

### FSR directory structure
```
├─CSD firmware                    # the firmware of the CSD (FSR handler included)
└─host                            # user tools and examples
   ├─fiemap.h
   ├─flush_ftl_buffer.sh          # flush the FTL buffer
   ├─flush_half_ftl_buffer.sh     # only flush half of the FTL buffer
   ├─fsr-search.c                 # the host-side application of FSR-Search
   ├─fsrlib.h                     # userspace library (FSRLib)
   ├─generate_hello_file.py       # generate the file for searching
   └─host-search.c                # the host-side application of Host-Search
```

### Hardware Environment

We currently adopt the [CosmosPlus](https://github.com/Cosmos-OpenSSD/Cosmos-plus-OpenSSD) OpenSSD as the hardware platform. For newer platforms such as DaisyPlus, additional transplantation may be required to accommodate the differences in hardware architecture.

The source code of this firmware is mainly based on GreedyFTL-2.7.1.d.

### Getting Start on Ubuntu 16.04 kernel 

Compile the CSD firmware and build the binary file of the OpenSSD. Specific details about this step can be found in the user manual of the OpenSSD.

When the device is ready, create and format the partition:
```
sudo echo "n
p
1
4096
5242880
w
" | sudo fdisk /dev/nvme0n1

sudo mkfs.f2fs -l f2fs -d 3 /dev/nvme0n1p1
sudo mount /dev/nvme0n1p1 /home/nvme
```
NOTE: By default, the partition starts at the block address `4096`.

Generate the source file for searching:
```
python3 generate_hello_file.py 64 k /home/nvme/hello_64KB.txt
sync
```

Compile the applications and run:
```
gcc fsr-search.c -o fsr-search
sudo ./fsr-search /hello_64KB.txt
```

