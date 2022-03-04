#pragma once

#include <stdint.h>
#include <sys/types.h>

#if defined(__x86_64__)
#include <kernel/arch/x86_64/regs.h>
#elif defined(__aarch64__)
#include <kernel/arch/aarch64/regs.h>
#else
#error "no regs"
#endif

typedef struct {
	int signum;
	uintptr_t handler;
	struct regs registers_before;
} signal_t;

extern void fix_signal_stacks(void);
extern int send_signal(pid_t process, int signal, int force_root);
extern int group_send_signal(pid_t group, int signal, int force_root);
extern void handle_signal(process_t * proc, signal_t * sig, struct regs *r);
extern void process_check_signals(struct regs*);

