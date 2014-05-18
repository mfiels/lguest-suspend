#! /bin/bash

# make kernel module
echo 'Cleaning lg.ko...'
cd /home/${SUDO_USER}/linux-3.11.0/drivers/lguest
rm *.o

# make launcher
echo 'Cleaning lguest launcher...'
cd /home/${SUDO_USER}/linux-3.11.0/tools/lguest
make clean
