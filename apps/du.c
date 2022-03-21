/**
 * @brief du - calculate file size usage
 *
 * TODO: Should use st_blocks, but we don't set that in the kernel yet?
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2018 K. Lange
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

static int show_total = 0;
static int human = 0;
static int all = 1;
static int is_arg = 0;

static uint64_t count_thing(char * tmp);

static int print_human_readable_size(char * _out, size_t s) {
	if (s >= 1<<20) {
		size_t t = s / (1 << 20);
		return sprintf(_out, "%d.%1dM", (int)t, (int)(s - t * (1 << 20)) / ((1 << 20) / 10));
	} else if (s >= 1<<10) {
		size_t t = s / (1 << 10);
		return sprintf(_out, "%d.%1dK", (int)t, (int)(s - t * (1 << 10)) / ((1 << 10) / 10));
	} else {
		return sprintf(_out, "%d", (int)s);
	}
}

static void print_size(uint64_t size, char * name) {
	char sizes[30];
	if (!human) {
		sprintf(sizes, "%-7llu", size/1024LLU);
	} else {
		print_human_readable_size(sizes, size);
	}
	if (strlen(name) > 2 && name[0] == '/' && name[1] == '/') {
		name = &name[1];
	}
	fprintf(stdout, "%7s %s\n", sizes, name);
}

static uint64_t count_directory(char * source) {
	DIR * dirp = opendir(source);
	if (dirp == NULL) {
		//fprintf(stderr, "could not open %s\n", source);
		return 0;
	}

	int was_arg = is_arg;
	is_arg = 0;

	uint64_t total = 0;

	struct dirent * ent = readdir(dirp);
	while (ent != NULL) {
		if (!strcmp(ent->d_name,".") || !strcmp(ent->d_name,"..")) {
			ent = readdir(dirp);
			continue;
		}
		char tmp[strlen(source)+strlen(ent->d_name)+2];
		sprintf(tmp, "%s/%s", source, ent->d_name);
		total += count_thing(tmp);
		ent = readdir(dirp);
	}
	closedir(dirp);

	if (all || was_arg) {
		print_size(total, source);
	}

	return total;
}

static uint64_t count_thing(char * tmp) {
	struct stat statbuf;
	lstat(tmp,&statbuf);
	if (S_ISDIR(statbuf.st_mode)) {
		return count_directory(tmp);
	} else {
		if (is_arg) {
			print_size(statbuf.st_size, tmp);
		}
		return statbuf.st_size;
	}
}


int main(int argc, char * argv[]) {
	int opt;
	while ((opt = getopt(argc, argv, "hsc")) != -1) {
		switch (opt) {
			case 'h': /* human readable */
				human = 1;
				break;
			case 'c':
				show_total = 1;
				break;
			case 's': /* summary */
				all = 0;
				break;
			default:
				fprintf(stderr, "rm: unrecognized option '%c'\n", opt);
				break;
		}
	}

	int ret = 0;
	uint64_t total = 0;

	for (int i = optind; i < argc; ++i) {
		is_arg = 1;
		total += count_thing(argv[i]);
	}

	if (show_total) {
		print_size(total, "total");
	}

	return ret;
}

