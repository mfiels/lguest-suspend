lguest-suspend
==============

Lguest with suspend, resume, snapshotting added.

Instructions
============
* Download [http://releases.ubuntu.com/13.10/ubuntu-13.10-server-i386.iso](Ubuntu Server 13.10 32 bit x86).
* Set up a new VM with the above iso, make sure to allocate enough disk space (30-40GB) should be good.
* Download the [setup shell script](https://raw.githubusercontent.com/mfiels/lguest-suspend/master/setup.sh) into the VM user's home directory.
```
wget https://raw.githubusercontent.com/mfiels/lguest-suspend/master/setup.sh ~/setup.sh
```
* Execute the setup shell script with sudo. This will take some time (1-2hrs) because it builds a custom version of the kernel with lguest support built in. To speed things up make sure your VM can use multiple cores and change the concurrency level in setup.sh to reflect your core count. Alternatively you can download the [header .deb package](https://drive.google.com/file/d/0Bxb_HgWHzr7gYTBPbWh0V0h2NW8/edit?usp=sharing) and the [image .deb package](https://drive.google.com/file/d/0Bxb_HgWHzr7gUXZ2bWJ1anBVOTQ/edit?usp=sharing) and install them yourself. Downloading these packages instead of building the kernel may cause some problems when trying to build the lguest kernel module but I am not sure.
```
chmod +x setup.sh && sudo ./setup.sh
```
* Wait...
* The custom kernel image should be installed and ready to use, restart your VM:
```
sudo reboot now
```
* Verify that the custom kernel image has been installed
```
# Should return something like "linux...lguest"
uname -r
```
* Next setup a virtual disk image for the lguest guest to use, this can be done with qemu. I do this on my host machine for simplicity. Alternatively you can download a pre configured vdi (username: guest, password: password) with the appropriate lguest kernel [here](https://drive.google.com/file/d/0Bxb_HgWHzr7gTFE3UWJTSFI5cnM/edit?usp=sharing), this is a 5 GB file so it *may* be faster to just follow the steps below.
```
cd
sudo apt-get install qemu
fallocate -l 5G hda.vdi
qemu-system-i386 -cdrom ubuntu-13.10-server-i386.iso -hda hda.vdi -boot c -net nic -net user
```
* When the above starts, install Ubuntu 13.10 on the vdi
* After installation completes, the lguest guest vdi needs to use the **same** kernel image as the lguest host does. Transfer the custom lguest linux headers and image .deb packages from the lguest host VM to the lguest guest vdi and install them.
```
# On qemu running the lguest guest hda.vdi, after transferring the .deb packages
sudo dpkg -i linux-image*
sudo dpkg -i linux-headers*
sudo update-grub
sudo reboot now
```
* Once the above completes verify that the kernel image has been installed on the lguest guest:
```
# Should return something like "linux...lguest"
uname -r
```
* Transfer the guest vdi to the host VM user's home directory
```
scp hda.vdi user@hostvm:/home/user
```
* Back on the host VM, build the lguest kernel module and launcher
```
cd linux-3.11.0
sudo ./makelguest.sh
```
* Take a deep breath, you can now finally start the lguest guest on the host VM!
```
cd linux-3.11.0
sudo ./start.sh
```
* If the above fails it is possible that the start.sh shell script is using a different name for the kernel image than the name that the kernel image built with, just update the kernel image name in both start.sh and resume.sh and things should work

Testing Suspend and Resume
==========================
* Start the host VM
* Start two SSH sessions with the host VM in two separate windows
* From the first window:
```
cd linux-3.11.0
sudo ./start.sh
```
* Wait for the lguest guest to finish booting
* Run some command in the lguest guest, i.e. find
* As this command is running issue the suspend signal from the second terminal window:
```
cd linux-3.11.0
sudo ./control.sh suspend
```
* Wait a few seconds and issue the resume command from the second terminal window:
```
cd linux-3.11.0
sudo ./control.sh resume
```
* The lguest guest will have suspended and resumed when these commands were issued

Testing Snapshot and Restore (incomplete)
=========================================
* Start the host VM
* Start two SSH sessions with the host VM in two separate windows
* From the first window:
```
cd linux-3.11.0
sudo ./start.sh
```
* Wait for the lguest guest to finish booting
* From the second window:
```
cd linux-3.11.0
sudo ./control.sh snapshot
```
* This will write out the guest memory, registers, idt, gdt, etc... to the following file:
```
sudo stat /root/.lguest/pages
```
* From the first window restore with:
```
# This currently is incomplete and results in an invalid pgdir entry which exits the launcher
sudo ./resume.sh
```
