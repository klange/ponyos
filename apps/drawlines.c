/**
 * @brief drawlines - Draw random lines into a GUI window
 *
 * The original compositor demo application, this dates all the
 * way back to the original pre-Yutani compositor. Opens a very
 * basic window (no decorations) and randomly fills it with
 * colorful lines.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2013-2021 K. Lange
 */
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>

#include <sys/fswait.h>

#include <toaru/yutani.h>
#include <toaru/graphics.h>

static int left, top, width, height;

static yutani_t * yctx;
static yutani_window_t * wina;
static gfx_context_t * ctx;
static int should_exit = 0;
static int thick = 0;

static void draw(void) {
	if (thick) {
		draw_line_aa(ctx, rand() % width, rand() % width, rand() % height, rand() % height, rgb(rand() % 255,rand() % 255,rand() % 255), (float)thick);
	} else {
		draw_line(ctx, rand() % width, rand() % width, rand() % height, rand() % height, rgb(rand() % 255,rand() % 255,rand() % 255));
	}
	yutani_flip(yctx, wina);
}

static void show_usage(char * argv[]) {
	printf(
			"drawlines - graphical demo, draws lines randomly\n"
			"\n"
			"usage: %s [-t thickness]\n"
			"\n"
			" -t     \033[3mdraw with anti-aliasing and the specified thickness\033[0m\n"
			" -?     \033[3mshow this help text\033[0m\n"
			"\n", argv[0]);
}


int main (int argc, char ** argv) {
	left   = 100;
	top    = 100;
	width  = 500;
	height = 500;

	srand(time(NULL));

	int c;
	while ((c = getopt(argc, argv, "t:?")) != -1) {
		switch (c) {
			case 't':
				thick = atoi(optarg);
				break;
			case '?':
				show_usage(argv);
				return 0;
		}
	}

	yctx = yutani_init();
	if (!yctx) {
		fprintf(stderr, "%s: failed to connect to compositor\n", argv[0]);
		return 1;
	}

	wina = yutani_window_create(yctx, width, height);
	yutani_window_move(yctx, wina, left, top);
	yutani_window_advertise_icon(yctx, wina, "drawlines", "drawlines");

	ctx = init_graphics_yutani(wina);
	draw_fill(ctx, rgb(0,0,0));

	while (!should_exit) {
		int fds[1] = {fileno(yctx->sock)};
		int index = fswait2(1,fds,20);
		if (index == 0) {
			yutani_msg_t * m = yutani_poll(yctx);
			while (m) {
				switch (m->type) {
					case YUTANI_MSG_KEY_EVENT:
						{
							struct yutani_msg_key_event * ke = (void*)m->data;
							if (ke->event.action == KEY_ACTION_DOWN && ke->event.keycode == 'q') {
								should_exit = 1;
								sched_yield();
							}
						}
						break;
					case YUTANI_MSG_WINDOW_MOUSE_EVENT:
						{
							struct yutani_msg_window_mouse_event * me = (void*)m->data;
							if (me->command == YUTANI_MOUSE_EVENT_DOWN && me->buttons & YUTANI_MOUSE_BUTTON_LEFT) {
								yutani_window_drag_start(yctx, wina);
							}
						}
						break;
					case YUTANI_MSG_WINDOW_CLOSE:
					case YUTANI_MSG_SESSION_END:
						should_exit = 1;
						break;
					default:
						break;
				}
				free(m);
				m = yutani_poll_async(yctx);
			}
		}
		draw();
	}

	yutani_close(yctx, wina);

	return 0;
}
