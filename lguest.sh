#! /bin/bash
insmod /home/mark/linux-3.11.0/drivers/lguest/lg.ko

/home/mark/linux-3.11.0/tools/lguest/lguest 256 /home/mark/linux-3.11.0/vmlinux --tunnet=192.168.19.1 --block=/home/mark/hda.vdi --initrd=/boot/initrd.img-3.11.10.5-lguest root=/dev/vda1 init=/sbin/init
