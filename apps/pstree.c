/**
 * @brief pstree - Display a tree of running process
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2014-2018 K. Lange
 */
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <toaru/list.h>
#include <toaru/tree.h>

typedef struct process {
	int pid;
	int ppid;
	int tgid;
	char name[100];
	char path[200];
} p_t;

#define LINE_LEN 4096

p_t * build_entry(struct dirent * dent) {
	char tmp[300];
	FILE * f;
	char line[LINE_LEN];

	sprintf(tmp, "/proc/%s/status", dent->d_name);
	f = fopen(tmp, "r");

	p_t * proc = malloc(sizeof(p_t));

	while (fgets(line, LINE_LEN, f) != NULL) {
		char * n = strstr(line,"\n");
		if (n) { *n = '\0'; }
		char * tab = strstr(line,"\t");
		if (tab) {
			*tab = '\0';
			tab++;
		}
		if (strstr(line, "Pid:") == line) {
			proc->pid = atoi(tab);
		} else if (strstr(line, "PPid:") == line) {
			proc->ppid = atoi(tab);
		} else if (strstr(line, "Tgid:") == line) {
			proc->tgid = atoi(tab);
		} else if (strstr(line, "Name:") == line) {
			strcpy(proc->name, tab);
		} else if (strstr(line, "Path:") == line) {
			strcpy(proc->path, tab);
		}
	}

	if (strstr(proc->name,"python") == proc->name) {
		char * name = proc->path + strlen(proc->path) - 1;

		while (1) {
			if (*name == '/') {
				name++;
				break;
			}
			if (name == proc->name) break;
			name--;
		}

		memcpy(proc->name, name, strlen(name)+1);
	}

	if (proc->tgid != proc->pid) {
		char tmp[100] = {0};
		sprintf(tmp, "{%s}", proc->name);
		memcpy(proc->name, tmp, strlen(tmp)+1);
	}

	fclose(f);

	return proc;
}

uint8_t find_pid(void * proc_v, void * pid_v) {
	p_t * p = proc_v;
	pid_t i = (pid_t)(uintptr_t)pid_v;

	return (uint8_t)(p->pid == i);
}

void print_process_tree_node(tree_node_t * node, size_t depth, int indented, int more, char lines[]) {

	p_t * proc = node->value;

	for (int i = 0; i < (int)strlen(proc->name)+3; ++i) {
		lines[depth+i] = 0;
	}

	if (!indented && depth) {
		if (more) {
			printf("─┬─");
			lines[depth+1] = 1;
		} else {
			printf("───");
		}
		depth += 3;
	} else if (depth) {
		for (int i = 0; i < (int)depth; ++i) {
			if (lines[i]) {
				printf("│");
			} else {
				printf(" ");
			}
		}
		if (more) {
			printf(" ├─");
			lines[depth+1] = 1;
		} else {
			printf(" └─");
		}
		depth += 3;
	}

	printf(proc->name);

	if (!node->children->length) {
		printf("\n");
	} else {
		depth += strlen(proc->name);

		int t = 0;
		foreach(child, node->children) {
			/* Recursively print the children */
			print_process_tree_node(child->value, depth, !!(t), ((t+1)!=(int)node->children->length), lines);
			t++;
		}
	}

	for (int i = 0; i < (int)strlen(proc->name)+3; ++i) {
		lines[depth+i] = 0;
	}
}

int main (int argc, char * argv[]) {

	/* Open the directory */
	DIR * dirp = opendir("/proc");

	/* Read the entries in the directory */
	tree_t * procs = tree_create();

	struct dirent * ent = readdir(dirp);
	while (ent != NULL) {
		if (ent->d_name[0] >= '0' && ent->d_name[0] <= '9') {
			p_t * proc = build_entry(ent);

			if (proc->ppid == 0 && proc->pid == 1) {
				tree_set_root(procs, proc);
			} else {
				tree_node_t * parent = tree_find(procs,(void *)(uintptr_t)proc->ppid,find_pid);
				if (parent) {
					tree_node_insert_child(procs, parent, proc);
				}
			}
		}
		ent = readdir(dirp);
	}
	closedir(dirp);

	char lines[500] = {0};
	print_process_tree_node(procs->root, 0, 0, 0, lines);

	return 0;
}

