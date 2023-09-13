#!/bin/bash

a=$((RANDOM%1000))
b=$((RANDOM%999))
c=$((a*b))
random_num=$((c-c%4))

addr=$((13107200 + random_num))
echo $addr

for ((i=addr;i<addr+5120;i=i+256));do
    nvme read /dev/nvme0n1 -s $i -c 255 -z 0x100000 -d flush.dat
done

rm -f flush.dat
