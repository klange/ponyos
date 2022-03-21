/**
 * @brief fgrep - dump grep
 *
 * Locates strings in files and prints the lines containing them,
 * with extra color identification if stdout is a tty.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2014-2018 K. Lange
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define LINE_SIZE 4096

int main(int argc, char ** argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s thing-to-grep-for\n", argv[0]);
		return 1;
	}

	char * needle = argv[1];
	char buf[LINE_SIZE];
	int ret = 1;
	int is_tty = isatty(STDOUT_FILENO);

	while (fgets(buf, LINE_SIZE, stdin)) {
		char * found = strstr(buf, needle);
		if (found) {
			if (is_tty) {
				*found = '\0';
				found += strlen(needle);
				fprintf(stdout, "%s\033[1;31m%s\033[0m%s", buf, needle, found);
			} else {
				fprintf(stdout, "%s", buf);
			}
			ret = 0;
		}
	}

	return ret;
}

