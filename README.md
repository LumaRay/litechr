# Character Device Driver Example

The driver adds a character device file `/dev/litechr` which can be used for reading and writing and acts as a queue.

## Details

The file can contain 1000 bytes maximum.
A read or write offset is ignored.
When reading, the read data is automatically cleared from the file content.

Is is possible to open the device in three modes:

* **Shared** - when opened with no extra flags.
	In this mode the file can be simultaneously opened multiple times (up to 1000). It's content will be shared and remain intact when closed.
* **Exclusive** - when opened with O_EXCL flag.
	In this mode the file can be opened only once at a time. The mode will only work if there are no open descriptors of the file. It's content will remain intact when closed. If the file is opened in this mode, the file will remain locked for opening in any mode until it is closed.
* **Multi** - when opened with O_CREAT flag.
	In this mode the file can be opened multiple times, but each opened descriptor will start with a dedicated empty file content which will be deleted when the associated file descriptor is closed.

The file content is shared between *Shared* and *Exclusive* modes.
It is possible to open the file in *Shared* and *Multi* mode at the same time.

## Make options

* `make` - build the driver without debug information
* `make debug` - build the driver with debug information
* `make all` - build the driver without debug information and test executable
* `make install` - run insmod on the driver
* `make uninstall` - run rmmod on the driver
* `make clean` - clean build files of the driver and the test
* `make log` - display the last 10 driver output messages
* `make log-cont` - continuous display of driver output messages
* `make test-make` - build test executable
* `make test` - build test executable and run it
* `make test-mem-install` - install prerequisites for memory leak test of test executable
* `make test-mem` - build test executable and run it with memory leak analyzer

## Build prerequisites

The driver is targeted for Linux kernel v6.1.6.
Tested gcc versions: 10, 12.

### To install the kernel on Ubuntu 22.10 x64, use the following steps:

```
cd ~/Downloads
sudo apt-get install git fakeroot build-essential ncurses-dev xz-utils libssl-dev bc flex libelf-dev bison
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.1.6.tar.xz
tar xvf linux-6.1.6.tar.xz
cd linux-6.1.6
cp -v /boot/config-$(uname -r) .config
make oldconfig
make menuconfig
scripts/config --disable SYSTEM_TRUSTED_KEYS
scripts/config --disable SYSTEM_REVOCATION_KEYS
make
sudo make modules_install
sudo make install
sudo update-initramfs -c -k 6.1.6
sudo update-grub
sudo reboot
```

### To install the kernel on Debian 11.6 x64, use the following steps:

```
su -
usermod -aG sudo test
reboot
cd ~/Downloads
sudo apt-get install git fakeroot build-essential ncurses-dev xz-utils libssl-dev bc flex bison
sudo apt-get install libelf-dev

# If libelf-dev not found:
#
# sudo apt-get install zlib1g-dev
# wget http://ftp.us.debian.org/debian/pool/main/e/elfutils/libelf-dev_0.183-1_amd64.deb
# sudo dpkg -i libelf-dev_0.183-1_amd64.deb

wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.1.6.tar.xz
tar xvf linux-6.1.6.tar.xz
cd linux-6.1.6
cp -v /boot/config-$(uname -r) .config
make oldconfig
make menuconfig
scripts/config --disable SYSTEM_TRUSTED_KEYS
scripts/config --disable SYSTEM_REVOCATION_KEYS
make

# If there is I2C segmentation fault during kernel build, then you need to install gcc 12:
#
# git clone https://gcc.gnu.org/git/gcc.git gcc-source
# cd gcc-source/
# git branch -a
# git checkout remotes/origin/releases/gcc-12
# ./contrib/download_prerequisites
# mkdir ../gcc-12-build
# cd ../gcc-12-build/
# ./../gcc-source/configure --prefix=$HOME/install/gcc-12 --enable-languages=c,c++ --disable-multilib
# make
# make install
# sudo update-alternatives --install /usr/bin/gcc gcc ~/install/gcc-12/bin/gcc 120 --slave /usr/bin/g++ g++ ~/install/gcc-12/bin/g++ --slave /usr/bin/gcov gcov ~/install/gcc-12/bin/gcov

sudo make modules_install
sudo make install
sudo update-initramfs -c -k 6.1.6
sudo update-grub
sudo reboot
```

