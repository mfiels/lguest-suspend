#! /bin/bash

rmmod lg

insmod /home/${SUDO_USER}/linux-3.11.0/drivers/lguest/lg.ko

/home/${SUDO_USER}/linux-3.11.0/tools/lguest/lguest 256 /boot/vmlinuz-3.11.10.5-lguest+ --verbose --tunnet=192.168.19.1 --block=/home/${SUDO_USER}/hda.vdi --initrd=/boot/initrd.img-3.11.10.5-lguest+ --snapshot=hello root=/dev/vda1 init=/sbin/init
