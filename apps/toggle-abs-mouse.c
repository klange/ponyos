/**
 * @brief toggle-abs-mouse - Toggle mouse modes
 *
 * Set the mouse mode under VirtualBox, VMware, or QEMU to either
 * relative or absolute via ioctl to the relevant absolute mouse
 * device driver interface.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2018 K. Lange
 */
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int main(int argc, char * argv[]) {
	if (argc < 2) {
		fprintf(stderr, "%s: argument (relative, absolute, get) expected\n", argv[0]);
		return 1;
	}

	int fd = open("/dev/absmouse",O_WRONLY);
	if (fd < 0) {
		/* try vmmouse */
		fd = open("/dev/vmmouse",O_WRONLY);
		if (fd < 0) {
			fprintf(stderr, "%s: no valid mouse interface found.\n", argv[0]);
			return 1;
		}
	}

	int flag = 0;
	if (!strcmp(argv[1],"relative")) {
		flag = 1;
	}
	if (!strcmp(argv[1],"absolute")) {
		flag = 2;
	}
	if (!strcmp(argv[1],"get")) {
		flag = 3;
	}

	if (!flag) {
		fprintf(stderr, "%s: invalid argument\n", argv[0]);
		return 1;
	}

	int result = ioctl(fd, flag, NULL);

	if (flag == 3) {
		if (result == 0) {
			fprintf(stdout, "relative\n");
		} else {
			fprintf(stdout, "absolute\n");
		}
		return 0;
	}

	return result;
}
