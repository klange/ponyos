#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/types.h>

#define SHM_PATH_SEPARATOR "."

/* Types */
struct shm_node;

typedef struct {
	struct shm_node * parent;
	volatile uint8_t lock;
	ssize_t ref_count;
	size_t num_frames;
	uintptr_t *frames;
} shm_chunk_t;

typedef struct shm_node {
	char name[256];
	shm_chunk_t * chunk;
} shm_node_t;

typedef struct {
	shm_chunk_t * chunk;
	uint8_t volatile lock;
	size_t num_vaddrs;
	uintptr_t *vaddrs;
} shm_mapping_t;

/* Syscalls */
extern void * shm_obtain(char * path, size_t * size);
extern int    shm_release(char * path);

/* Other exposed functions */
extern void shm_install(void);
extern void shm_release_all(process_t * proc);

