#! /bin/bash

cd /home/${SUDO_USER}

apt-get update
apt-get -y install build-essential kernel-package libncurses5-dev fakeroot wget bzip2 bc git socat

sudo -u ${SUDO_USER} git clone http://github.com/mfiels/lguest-suspend linux-3.11.0

echo "CONCURRENCY_LEVEL=5" >> /etc/kernel-pkg.conf

cd linux-3.11.0
sudo -u ${SUDO_USER} cp linux.config .config
sudo -u ${SUDO_USER} make oldconfig
sudo -u ${SUDO_USER} fakeroot make-kpkg --initrd --append-to-version=-lguest kernel_image kernel_headers

dpkg -i linux-image*
dpkg -i linux-headers*
update-grub

echo "Everything should be good to go now, reboot and execute uname -r to make sure the lguest kernel has been loaded."
