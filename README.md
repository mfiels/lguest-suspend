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
* Execute the setup shell script with sudo. This will take some time (1-2hrs) because it builds a custom version of the kernel with lguest support built in. To speed things up make sure your VM can use multiple cores and change the concurrency level in setup.sh to reflect your core count.
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
