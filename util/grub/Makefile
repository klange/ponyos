BASE=../..

all: ponyos-grub.iso

MODULES=$(patsubst ${BASE}/fatbase/%,cdrom/%.gz,$(wildcard ${BASE}/fatbase/mod/*.ko))
GRUB_STUFF=cdrom/boot/grub/menus.cfg cdrom/boot/grub/grub.cfg cdrom/boot/grub/modules.cfg

directories:
	mkdir -p cdrom/mod

cdrom/%.gz: ${BASE}/fatbase/% | directories
	cat $< | gzip > $@

ponyos-grub.iso: cdrom/ramdisk.img.gz cdrom/kernel.gz ${MODULES} ${GRUB_STUFF}
	grub-mkrescue -d /usr/lib/grub/i386-pc --compress=xz -o $@ cdrom

.PHONY: clean
clean:
	rm -f cdrom/ramdisk.img.gz
	rm -f cdrom/kernel.gz
	rm -f cdrom/mod/*.ko.gz
