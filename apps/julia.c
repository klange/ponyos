/**
 * @brief julia - Julia Fractal Generator
 *
 * Displays Julia fractals in a window. Use the keyboard
 * to navigate, switch palettes, and change parameters.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2013-2021 K. Lange
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>

#include <toaru/yutani.h>
#include <toaru/graphics.h>
#include <toaru/decorations.h>
#include <toaru/menu.h>
#include <toaru/markup_text.h>

#define GFX_(xpt, ypt) (GFX(ctx,xpt+decor_left_width,ypt+decor_top_height))

/* Pointer to graphics memory */
static yutani_t * yctx;
static yutani_window_t * window = NULL;
static gfx_context_t * ctx = NULL;

static int decor_left_width = 0;
static int decor_top_height = 0;
static int decor_right_width = 0;
static int decor_bottom_height = 0;
static int decor_width = 0;
static int decor_height = 0;


/* Julia fractals elements */
float conx = -0.752;  /* real part of c */
float cony = 0.117;    /* imag part of c */
float Maxx = 2;      /* X bounds */
float Minx = -2;
float Maxy = 1;      /* Y bounds */
float Miny = -1;
float pixcorx;       /* Internal values */
float pixcory;
float rotation = 4.1888; /* Blue */
int maxiter = 1000; /* Iteration levels */

uint32_t * palette = NULL;

static uint32_t hsv_to_rgb(float h, float s, float v) {
	float c  = v * s;
	float hp = fmod(h, 2 * M_PI);
	float x = c * (1.0 - fabs(fmod(hp / 1.0472, 2) - 1.0));
	float m = v - c;
	float rp, gp, bp;
	if (hp <= 1.0472)      { rp = c; gp = x; bp = 0; }
	else if (hp <= 2.0944) { rp = x; gp = c; bp = 0; }
	else if (hp <= 3.1416) { rp = 0; gp = c; bp = x; }
	else if (hp <= 4.1888) { rp = 0; gp = x; bp = c; }
	else if (hp <= 5.2360) { rp = x; gp = 0; bp = c; }
	else                   { rp = c; gp = 0; bp = x; }
	return rgb((rp + m) * 255, (gp + m) * 255, (bp + m) * 255);
}

static uint32_t hue_palette(int k) {
	double ratio = (double)k / (double)maxiter;
	double hue   = sin(ratio * M_PI / 2.0);
	return hsv_to_rgb(4.18879 * hue + rotation, 1.0, 1.0);
}

static uint32_t rhue_palette(int k) {
	double ratio = (double)k / (double)maxiter;
	double hue   = sin(ratio * M_PI / 2.0);
	return hsv_to_rgb(-4.18879 * hue + rotation, 1.0, 1.0);
}

static uint32_t bnw_palette(int k) {
	return rgb(255 * k / maxiter, 255 * k / maxiter, 255 * k / maxiter);
}

static uint32_t mix(uint32_t base, uint32_t mixer, float ratio) {
	return rgb(
		_RED(base) * (1.0 - ratio) + _RED(mixer) * (ratio),
		_GRE(base) * (1.0 - ratio) + _GRE(mixer) * (ratio),
		_BLU(base) * (1.0 - ratio) + _BLU(mixer) * (ratio));
}

static uint32_t pony_palette(int k) {
	double ratio = (double)k / (double)maxiter;

	static const int colors[] = {
		0xFF9dd7f6, /* 0 */
		0xFFef3f33,
		0xFFf27835,
		0xFFf4e97f,
		0xFF7ac041,
		0xFF0091ce,
		0xFF672d87,
		0xFF343a70,
		0xFF6c278c, /* 9, these aren't used any more, but here in case */
		0xFFed4e8e, /*    I decide to fix this up later to look more like */
		0xFFc9a9d0, /*    the original color scheme */
		0xFF9562ad, /* 11 */
	};

	for (int i = 0; i < 100; ++i) {
		if (ratio <= 0.025) return mix(colors[0], colors[1], ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(colors[1], colors[2], ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(colors[2], colors[3], ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(colors[3], colors[4], ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(colors[4], colors[5], ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(colors[5], colors[6], ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(colors[6], colors[7], ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(colors[7], colors[8], ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (i < 99) {
			if (ratio <= 0.025) return mix(colors[8], colors[0], ratio / 0.025);
			ratio -= 0.025;
			ratio /= 0.975;
		}
	}
	return rgb(0,0,0);
}

static uint32_t wiki_palette(int k) {
	double ratio = (double)k / (double)maxiter;

	for (int i = 0; i < 100; ++i) {
		if (ratio <= 0.025) return mix(rgb(14,21,101), rgb(40,100,200), ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(rgb(40,100,200), rgb(90,200,225), ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(rgb(90,200,225), rgb(255,255,255), ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(rgb(255,255,255), rgb(255,255,100), ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(rgb(255,255,100), rgb(255,255,0), ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(rgb(255,255,0), rgb(255,120,0), ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(rgb(255,120,0), rgb(255,0,0), ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (ratio <= 0.025) return mix(rgb(255,0,0), rgb(0,0,0), ratio / 0.025);
		ratio -= 0.025;
		ratio /= 0.975;
		if (i < 99) {
			if (ratio <= 0.025) return mix(rgb(0,0,0), rgb(14,21,101), ratio / 0.025);
			ratio -= 0.025;
			ratio /= 0.975;
		}
	}
	return rgb(0,0,0);
}

static uint32_t (*palette_funcs[])(int) = {
	pony_palette,
	wiki_palette,
	hue_palette,
	rhue_palette,
	bnw_palette,
};

static int current_palette = 0;

static void initialize_palette(void) {
	if (!palette) {
		palette = malloc(sizeof(uint32_t) * (maxiter + 1));
	}
	for (int k = 0; k < maxiter; ++k) {
		palette[k] = palette_funcs[current_palette](k);
	}
	palette[maxiter] = rgb(0,0,0);
}

static void next_palette(void) {
	current_palette = (current_palette + 1) % (sizeof(palette_funcs) / sizeof(*palette_funcs));
	initialize_palette();
}

int left   = 40;
int top    = 40;
int width  = 300;
int height = 300;

static uint32_t julia(int xpt, int ypt) {
	long double x = xpt * pixcorx + Minx;
	long double y = Maxy - ypt * pixcory;
	long double xnew = 0;
	long double ynew = 0;

	int k = 0;
	for (k = 0; k < maxiter; k++) {
		xnew = x * x - y * y + conx;
		ynew = 2 * x * y     + cony;
		x    = xnew;
		y    = ynew;
		if ((x * x + y * y) > 4.0)
			break;
	}

	return palette[k];
}

#define T_I "\033[3m"
#define T_N "\033[0m"
void usage(char * argv[]) {
	printf(
			"Julia fractal generator.\n"
			"\n"
			"usage: %s [-i " T_I "iterations" T_N "] [-x " T_I "minx" T_N "]\n"
			"          [-X " T_I "maxx" T_N "] [-c " T_I "real" T_N "] [-C " T_I "imag" T_N "]\n"
			"          [-W " T_I "width" T_N "] [-H " T_I "height" T_N "] [-h]\n"
			"\n"
			" -i --iterations  " T_I "Number of iterations to run" T_N "\n"
			" -x --minx        " T_I "Minimum X value" T_N "\n"
			" -X --maxx        " T_I "Maximum X value" T_N "\n"
			" -c --creal       " T_I "Real component of c" T_N "\n"
			" -C --cimag       " T_I "Imaginary component of c" T_N "\n"
			" -r --rotate      " T_I "Hue rotation for color mapping" T_N "\n"
			" -W --width       " T_I "Window width" T_N "\n"
			" -H --height      " T_I "Window height" T_N "\n"
			" -h --help        " T_I "Show this help message." T_N "\n",
			argv[0]);
}

static void decors() {
	render_decorations(window, ctx, "Julia Fractals");
}

void redraw() {
	float _x = Maxx - Minx;
	float _y = _x / width * height;

	Miny = 0 - _y / 2;
	Maxy = _y / 2;

	decors();

	pixcorx = (Maxx - Minx) / width;
	pixcory = (Maxy - Miny) / height;

	clock_t time_before = clock();

	for (int j = 0; j < height; ++j) {
		for (int i = 0; i < width; ++i) {
			GFX_(i,j) = julia(i,j);
		}
		yutani_flip_region(yctx, window, decor_left_width, decor_top_height + j, width, 1);
	}

	clock_t time_after = clock();

	char description[100];
	snprintf(description, 100, "<i>c</i> = %g + %g<i>i</i>, %ld ms", conx, cony, (time_after - time_before) / 1000);
	markup_draw_string(ctx, decor_left_width + 2, window->height - decor_bottom_height - 2, description, rgb(255,255,255));
}

void resize_finish(int w, int h) {
	yutani_window_resize_accept(yctx, window, w, h);
	reinit_graphics_yutani(ctx, window);

	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);

	decor_left_width = bounds.left_width;
	decor_top_height = bounds.top_height;
	decor_right_width = bounds.right_width;
	decor_bottom_height = bounds.bottom_height;
	decor_width = bounds.width;
	decor_height = bounds.height;

	width  = w - decor_left_width - decor_right_width;
	height = h - decor_top_height - decor_bottom_height;

	draw_fill(ctx, rgb(0,0,0));
	decors();
	yutani_window_resize_done(yctx, window);

	redraw();
	yutani_flip(yctx, window);
}

static double amount(struct yutani_msg_key_event * ke) {
	double basis = 0.001;

	if (ke->event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT)) basis *= 10.0;
	if (ke->event.modifiers & (KEY_MOD_LEFT_CTRL | KEY_MOD_RIGHT_CTRL)) basis *= 5.0;

	return basis;
}

int main(int argc, char * argv[]) {

	static struct option long_opts[] = {
		{"iterations", required_argument, 0, 'i'},
		{"minx",       required_argument, 0, 'x'},
		{"maxx",       required_argument, 0, 'X'},
		{"creal",      required_argument, 0, 'c'},
		{"cimag",      required_argument, 0, 'C'},
		{"rotate",     required_argument, 0, 'r'},
		{"width",      required_argument, 0, 'W'},
		{"height",     required_argument, 0, 'H'},
		{"help",       no_argument,       0, 'h'},
		{0,0,0,0}
	};

	if (argc > 1) {
		/* Read some arguments */
		int index, c;
		while ((c = getopt_long(argc, argv, "ni:x:X:c:C:W:H:h", long_opts, &index)) != -1) {
			if (!c) {
				if (long_opts[index].flag == 0) {
					c = long_opts[index].val;
				}
			}
			switch (c) {
				case 'i':
					maxiter = atoi(optarg);
					if (maxiter < 10) maxiter = 10;
					if (maxiter > 1000) maxiter = 1000;
					break;
				case 'x':
					Minx = atof(optarg);
					break;
				case 'X':
					Maxx = atof(optarg);
					break;
				case 'c':
					conx = atof(optarg);
					break;
				case 'C':
					cony = atof(optarg);
					break;
				case 'r':
					rotation = atof(optarg);
					break;
				case 'W':
					width = atoi(optarg);
					break;
				case 'H':
					height = atoi(optarg);
					break;
				case 'h':
					usage(argv);
					exit(0);
					break;
				default:
					break;
			}
		}
	}

	yctx = yutani_init();
	if (!yctx) {
		fprintf(stderr, "%s: failed to connect to compositor\n", argv[0]);
		return 1;
	}
	init_decorations();

	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);

	decor_left_width = bounds.left_width;
	decor_top_height = bounds.top_height;
	decor_right_width = bounds.right_width;
	decor_bottom_height = bounds.bottom_height;
	decor_width = bounds.width;
	decor_height = bounds.height;

	window = yutani_window_create(yctx, width + decor_width, height + decor_height);
	yutani_window_move(yctx, window, left, top);

	yutani_window_advertise_icon(yctx, window, "Julia Fractals", "julia");

	ctx = init_graphics_yutani(window);

	initialize_palette();

	redraw();
	yutani_flip(yctx, window);

	int playing = 1;
	int needs_redraw = 0;

	while (playing) {
		yutani_msg_t * m = yutani_poll(yctx);
		while (m) {
			if (menu_process_event(yctx, m)) {
				/* just decorations should be fine */
				decors();
				yutani_flip(yctx, window);
			}
			switch (m->type) {
				case YUTANI_MSG_KEY_EVENT:
					{
						struct yutani_msg_key_event * ke = (void*)m->data;
						if (ke->event.action == KEY_ACTION_DOWN) {
							switch (ke->event.keycode) {
								case 'q':
									playing = 0;
									break;
								case KEY_ARROW_LEFT:
									conx -= amount(ke);
									needs_redraw = 1;
									break;
								case KEY_ARROW_RIGHT:
									conx += amount(ke);
									needs_redraw = 1;
									break;
								case KEY_ARROW_UP:
									cony += amount(ke);
									needs_redraw = 1;
									break;
								case KEY_ARROW_DOWN:
									cony -= amount(ke);
									needs_redraw = 1;
									break;
								case 'p':
									next_palette();
									needs_redraw = 1;
									break;
							}
						}
					}
					break;
				case YUTANI_MSG_WINDOW_FOCUS_CHANGE:
					{
						struct yutani_msg_window_focus_change * wf = (void*)m->data;
						yutani_window_t * win = hashmap_get(yctx->windows, (void*)(uintptr_t)wf->wid);
						if (win && win == window) {
							win->focused = wf->focused;
							decors();
							yutani_flip(yctx, window);
						}
					}
					break;
				case YUTANI_MSG_RESIZE_OFFER:
					{
						struct yutani_msg_window_resize * wr = (void*)m->data;
						resize_finish(wr->width, wr->height);
					}
					break;
				case YUTANI_MSG_WINDOW_MOUSE_EVENT:
					{
						struct yutani_msg_window_mouse_event * me = (void*)m->data;
						int result = decor_handle_event(yctx, m);
						switch (result) {
							case DECOR_CLOSE:
								playing = 0;
								break;
							case DECOR_RIGHT:
								/* right click in decoration, show appropriate menu */
								decor_show_default_menu(window, window->x + me->new_x, window->y + me->new_y);
								break;
							default:
								/* Other actions */
								break;
						}
					}
					break;
				case YUTANI_MSG_WINDOW_CLOSE:
				case YUTANI_MSG_SESSION_END:
					playing = 0;
					break;
				default:
					break;
			}
			free(m);
			m = yutani_poll_async(yctx);
		}

		if (needs_redraw) {
			redraw();
			yutani_flip(yctx, window);
			needs_redraw = 0;
		}
	}

	yutani_close(yctx, window);

	return 0;
}
