#! /bin/bash

# make kernel module
echo 'Making lg.ko...'
cd /home/mark/linux-3.11.0/drivers/lguest
make clean

# make launcher
echo 'Making lguest launcher...'
cd /home/mark/linux-3.11.0/tools/lguest
make clean
