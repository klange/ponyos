# ToaruOS GRUB Disk

While ToaruOS ships with its own bootloaders for BIOS and EFI, they are not very robust. For users interested in trying ToaruOS on real hardware, GRUB will probably provide a better experience. This repository contains a GRUB configuration and tools to build a GRUB "Rescue CD" image, which can be written to a CD, hard disk, or USB storage device.

Note that ToaruOS may not be able to mount media you install this on. This shouldn't matter, though, unless you are attempting to add additional data to the image, as everything important should be included in the ramdisk.

This configuration is based on the older ToaruOS GRUB configs from before the native bootloader was merged, and it provides a menu interface for selecting display resolutions and disabling some optional modules, but does not have all the configuration options of the native loader. If you want to set additional configuration options, you can use GRUB's command line or config editor.

## Using This Repo

First, build ToaruOS.

Then run:

    make BASE=/path/to/your/toaruos/checkout

This will produce a `toaruos-grub.iso` which you can then burn to a CD, write to a hard disk, write to a USB stick, or boot with a virtual machine.
