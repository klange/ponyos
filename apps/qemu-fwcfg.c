/**
 * @brief qemu-fwcfg - Read QEMU fwcfg values.
 *
 * Provides easy access to values and files set by QEMU's -fw_cfg
 * flag. This is used by the QEMU harness, as well as the bootloader,
 * and can be used to provide files directly to the guest.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2018 K. Lange
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#define FW_CFG_PORT_OUT 0x510
#define FW_CFG_PORT_IN  0x511
#define FW_CFG_SELECT_QEMU 0x0000
#define FW_CFG_SELECT_LIST 0x0019

static int port_fd = -1;

/* outw / inb helper functions */
static void outports(unsigned short _port, unsigned short _data) {
	lseek(port_fd, _port, SEEK_SET);
	write(port_fd, &_data, 2);
}

static unsigned char inportb(unsigned short _port) {
	unsigned char out;
	lseek(port_fd, _port, SEEK_SET);
	read(port_fd, &out, 1);
	return out;
}

/* Despite primarily emulating x86, these are all big-endian */
static void swap_bytes(void * in, int count) {
	char * bytes = in;
	if (count == 4) {
		uint32_t * t = in;
		*t = (bytes[0] << 24) | (bytes[1] << 12) | (bytes[2] << 8) | bytes[3];
	} else if (count == 2) {
		uint16_t * t = in;
		*t = (bytes[0] << 8) | bytes[1];
	}
}

/* Layout of the information returned from the fw_cfg port */
struct fw_cfg_file {
	uint32_t size;
	uint16_t select;
	uint16_t reserved;
	char name[56];
};

static int usage(char * argv[]) {
	printf(
			"Obtain QEMU fw_cfg values\n"
			"\n"
			"usage: %s [-?ln] [config name]\n"
			"\n"
			" -l     \033[3mlist available config entries\033[0m\n"
			" -n     \033[3mdon't print a new line after data\033[0m\n"
			" -?     \033[3mshow this help text\033[0m\n"
			"\n", argv[0]);
	return 1;
}

static void sig_pass(int sig) {
	exit(1);
}

int main(int argc, char * argv[]) {

	uint32_t count = 0;
	uint8_t * bytes = (uint8_t *)&count;
	int found = 0;
	struct fw_cfg_file file;
	uint8_t * tmp = (uint8_t *)&file;

	int opt = 0;
	int list = 0;
	int no_newline = 0;
	int query_quietly = 0;

	while ((opt = getopt(argc, argv, "?lnq")) != -1) {
		switch (opt) {
			case '?':
				return usage(argv);
			case 'n':
				no_newline = 1;
				break;
			case 'q':
				query_quietly = 1;
				break;
			case 'l':
				list = 1;
				break;
		}
	}

	if (optind >= argc && !list) {
		return usage(argv);
	}

	port_fd = open("/dev/port", O_RDWR);

	if (port_fd < 0) {
		fprintf(stderr, "%s: could not open port IO device\n", argv[0]);
		return 1;
	}

	signal(SIGILL, sig_pass);

	/* First check for QEMU */
	outports(FW_CFG_PORT_OUT, FW_CFG_SELECT_QEMU);
	if (inportb(FW_CFG_PORT_IN) != 'Q' ||
		inportb(FW_CFG_PORT_IN) != 'E' ||
		inportb(FW_CFG_PORT_IN) != 'M' ||
		inportb(FW_CFG_PORT_IN) != 'U') {
		fprintf(stderr, "%s: this doesn't seem to be qemu\n", argv[0]);
		return 1;
	}

	/* Then get the list of "files" so we can look at names */
	outports(FW_CFG_PORT_OUT, FW_CFG_SELECT_LIST);
	for (int i = 0; i < 4; ++i) {
		bytes[i] = inportb(FW_CFG_PORT_IN);
	}
	swap_bytes(&count, sizeof(count));

	for (unsigned int i = 0; i < count; ++i) {

		/* read one file entry */
		for (unsigned int j = 0; j < sizeof(struct fw_cfg_file); ++j) {
			tmp[j] = inportb(FW_CFG_PORT_IN);
		}

		/* endian swap to get file size and selector ID */
		swap_bytes(&file.size, sizeof(file.size));
		swap_bytes(&file.select, sizeof(file.select));

		if (list) {
			/* 0x0020 org/whatever (1234 bytes) */
			fprintf(stdout, "0x%04x %s (%d byte%s)\n", file.select, file.name, (int)file.size, file.size == 1 ? "" : "s");
		} else {
			if (!strcmp(file.name, argv[optind])) {
				/* found the requested file */
				found = 1;
				break;
			}
		}
	}

	if (query_quietly) {
		return !found;
	}

	if (found) {
		/* if we found the requested file, read it from the port */
		outports(FW_CFG_PORT_OUT, file.select);

		for (unsigned int i = 0; i < file.size; ++i) {
			fputc(inportb(FW_CFG_PORT_IN), stdout);
		}

		if (!no_newline) {
			fprintf(stdout, "\n");
		} else {
			fflush(stdout);
		}

	} else if (!list) {
		fprintf(stderr, "%s: config option not found\n", argv[0]);
		return 1;
	}

	return 0;
}
