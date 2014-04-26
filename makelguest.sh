#! /bin/bash

# make kernel module
echo 'Making lg.ko...'
cd /home/mark/linux-3.11.0/drivers/lguest
make -C /home/mark/linux-3.11.0 ARCH=x86 M=`pwd` modules

# make launcher
echo 'Making lguest launcher...'
cd /home/mark/linux-3.11.0/tools/lguest
make
