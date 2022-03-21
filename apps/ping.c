/**
 * @brief Send ICMP pings
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2021 K. Lange
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <syscall.h>

#define BYTES_TO_SEND 56

struct IPV4_Header {
	uint8_t  version_ihl;
	uint8_t  dscp_ecn;
	uint16_t length;
	uint16_t ident;
	uint16_t flags_fragment;
	uint8_t  ttl;
	uint8_t  protocol;
	uint16_t checksum;
	uint32_t source;
	uint32_t destination;
	uint8_t  payload[];
};

struct ICMP_Header {
	uint8_t type, code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence_number;
	uint8_t payload[];
};

static uint16_t icmp_checksum(char * payload, size_t len) {
	uint32_t sum = 0;
	uint16_t * s = (uint16_t *)payload;
	for (size_t i = 0; i < (len) / 2; ++i) {
		sum += ntohs(s[i]);
	}
	if (sum > 0xFFFF) {
		sum = (sum >> 16) + (sum & 0xFFFF);
	}
	return ~(sum & 0xFFFF) & 0xFFFF;
}

static int break_from_loop = 0;

static void sig_break_loop(int sig) {
	(void)sig;
	break_from_loop = 1;
}

int main(int argc, char * argv[]) {
	if (argc < 2) return 1;

	int pings_sent = 0;

	struct hostent * host = gethostbyname(argv[1]);

	if (!host) {
		fprintf(stderr, "%s: not found\n", argv[1]);
		return 1;
	}

	char * addr = inet_ntoa(*(struct in_addr*)host->h_addr_list[0]);

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);

	if (sock < 0) {
		fprintf(stderr, "%s: No socket: %s\n", argv[1], strerror(errno));
		return 1;
	}

	signal(SIGINT, sig_break_loop);

	struct sockaddr_in dest;
	dest.sin_family = AF_INET;
	memcpy(&dest.sin_addr.s_addr, host->h_addr, host->h_length);

	printf("PING %s (%s) %d data bytes\n", argv[1], addr, BYTES_TO_SEND);

	struct ICMP_Header * ping = malloc(BYTES_TO_SEND);
	ping->type = 8; /* request */
	ping->code = 0;
	ping->identifier = 0;
	ping->sequence_number = 0;
	/* Fill in data */
	for (int i = 0; i < BYTES_TO_SEND - 8; ++i) {
		ping->payload[i] = i;
	}

	int responses_received = 0;

	while (!break_from_loop) {
		ping->sequence_number = htons(pings_sent+1);
		ping->checksum = 0;
		ping->checksum = htons(icmp_checksum((void*)ping, BYTES_TO_SEND));

		/* Send it and wait */
		clock_t sent_at = times(NULL);
		if (sendto(sock, (void*)ping, BYTES_TO_SEND, 0, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
			fprintf(stderr, "sendto: %s\n", strerror(errno));
		}

		pings_sent++;

		struct pollfd fds[1];
		fds[0].fd = sock;
		fds[0].events = POLLIN;
		int ret = poll(fds,1,1000);

		if (ret > 0) {
			char data[4096];
			ssize_t len = recv(sock, data, 4096, 0);
			clock_t rcvd_at = times(NULL);
			if (len > 0) {
				/* Is it actually a PING response ? */

				struct IPV4_Header * ipv4 = (void*)data;
				struct ICMP_Header * icmp = (void*)ipv4->payload;

				if (icmp->type == 0) {
					/* How much data, minus the header? */
					size_t len = ntohs(ipv4->length) - sizeof(struct IPV4_Header);
					/* Get the address */
					char * from = inet_ntoa(*(struct in_addr*)&ipv4->source);
					int time_taken = (rcvd_at - sent_at);
					printf("%zd bytes from %s: icmp_seq=%d ttl=%d time=%d",
						len, from, ntohs(icmp->sequence_number), ipv4->ttl,
						time_taken / 1000);
					if (time_taken < 1000) {
						printf(".%03d", time_taken % 1000);
					} else if (time_taken < 10000) {
						printf(".%02d", (time_taken / 10) % 100);
					} else if (time_taken < 100000) {
						printf(".%01d", (time_taken / 100) % 10);
					}
					printf(" ms\n");
					responses_received++;
				}

			}
		}

		if (!break_from_loop) {
			syscall_sleep(1,0);
		}
	}

	printf("--- %s statistics ---\n", argv[1]);
	printf("%d packets transmitted, %d received, %d%% packet loss\n",
		pings_sent, responses_received, 100*(pings_sent-responses_received)/pings_sent);


	return 0;
}
