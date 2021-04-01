/* vim: tabstop=4 shiftwidth=4 noexpandtab
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2013 K. Lange
 *
 * sleep - Do nothing, efficiently.
 */
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <string.h>

int main(int argc, char ** argv) {
	int ret = 0;

	if (argc < 2) {
		fprintf(stderr, "%s: missing operand\n", argv[0]);
		return 1;
	}

	char * arg = strdup(argv[1]);

	float time = atof(arg);

	unsigned int seconds = (unsigned int)time;
	unsigned int subsecs = (unsigned int)((time - (float)seconds) * 100);

	ret = syscall_nanosleep(seconds, subsecs);

	return ret;
}

