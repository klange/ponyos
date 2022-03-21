/**
 * @brief about - Show an "About <Application>" dialog.
 *
 * By default, shows "About ToaruOS", suitable for use as an application
 * menu entry. Optionally, takes arguments specifying another application
 * to describe, suitable for the "Help > About" menu bar entry.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2018-2019 K. Lange
 */
#include <toaru/yutani.h>
#include <toaru/graphics.h>
#include <toaru/decorations.h>
#include <toaru/menu.h>
#include <toaru/text.h>

#include <sys/utsname.h>

static yutani_t * yctx;
static yutani_window_t * window = NULL;
static gfx_context_t * ctx = NULL;
static sprite_t logo;

static int32_t width = 350;
static int32_t height = 250;
static char * version_str;

static struct TT_Font * _tt_font_thin = NULL;
static struct TT_Font * _tt_font_bold = NULL;

static char * icon_path;
static char * title_str;
static char * version_str;
static char * copyright_str[20] = {NULL};

static int center_x(int x) {
	return (width - x) / 2;
}

static void draw_string(int y, const char * string, struct TT_Font * font, uint32_t color) {

	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);

	tt_set_size(font, 13);
	tt_draw_string(ctx, font, bounds.left_width + center_x(tt_string_width(font, string)), bounds.top_height + 10 + logo.height + 10 + y + 13, string, color);
}

static void redraw(void) {

	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);

	draw_fill(ctx, rgb(204,204,204));
	draw_sprite(ctx, &logo, bounds.left_width + center_x(logo.width), bounds.top_height + 10);

	draw_string(0, version_str, _tt_font_bold, rgb(0,0,0));

	int offset = 20;

	for (char ** copy_str = copyright_str; *copy_str; ++copy_str) {
		if (**copy_str == '-') {
			offset += 10;
		} else if (**copy_str == '%') {
			draw_string(offset, *copy_str+1, _tt_font_thin, rgb(0,0,255));
			offset += 20;
		} else {
			draw_string(offset, *copy_str, _tt_font_thin, rgb(0,0,0));
			offset += 20;
		}
	}

	window->decorator_flags |= DECOR_FLAG_NO_MAXIMIZE;
	render_decorations(window, ctx, title_str);

	flip(ctx);
	yutani_flip(yctx, window);
}

static void init_default(void) {
	title_str = "About PonyOS";
	icon_path = "/usr/share/logo_small.png";

	{
		version_str = malloc(100);
		struct utsname u;
		uname(&u);
		char * tmp = strstr(u.release, "-");
		if (tmp) {
			*tmp = '\0';
		}
		sprintf(version_str, "PonyOS %s", u.release);
	}

	copyright_str[0] = "© 2011-2022 K. Lange, et al.";
	copyright_str[1] = "-";
	copyright_str[2] = "PonyOS is free software released under the";
	copyright_str[3] = "NCSA/University of Illinois license.";
	copyright_str[4] = "-";
	copyright_str[5] = "%https://ponyos.org";
	copyright_str[6] = "%https://github.com/klange/ponyos";
}

void resize_finish(int w, int h) {
	yutani_window_resize_accept(yctx, window, w, h);
	reinit_graphics_yutani(ctx, window);
	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);
	width  = w - bounds.width;
	height = h - bounds.height;
	redraw();
	yutani_window_resize_done(yctx, window);
}

int main(int argc, char * argv[]) {
	int req_center_x, req_center_y;
	yctx = yutani_init();
	if (!yctx) {
		fprintf(stderr, "%s: failed to connect to compositor\n", argv[0]);
		return 1;
	}
	init_decorations();

	_tt_font_thin = tt_font_from_shm("sans-serif");
	_tt_font_bold = tt_font_from_shm("sans-serif.bold");

	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);

	window = yutani_window_create_flags(yctx, width + bounds.width, height + bounds.height, YUTANI_WINDOW_FLAG_DIALOG_ANIMATION);
	req_center_x = yctx->display_width / 2;
	req_center_y = yctx->display_height / 2;

	if (argc < 2) {
		init_default();
	} else if (argc < 5) {
		fprintf(stderr, "Invalid arguments.\n");
		return 1;
	} else {
		title_str = argv[1];
		icon_path = argv[2];
		version_str = argv[3];

		int i = 0;
		char * me = argv[4], * end;
		do {
			copyright_str[i] = me;
			i++;
			end = strchr(me,'\n');
			if (end) {
				*end = '\0';
				me = end+1;
			}
		} while (end);

		if (argc > 6) {
			req_center_x = atoi(argv[5]);
			req_center_y = atoi(argv[6]);
		}
	}

	yutani_window_move(yctx, window, req_center_x - window->width / 2, req_center_y - window->height / 2);

	yutani_window_advertise_icon(yctx, window, title_str, "star");

	ctx = init_graphics_yutani_double_buffer(window);
	load_sprite(&logo, icon_path);

	redraw();

	int playing = 1;
	while (playing) {
		yutani_msg_t * m = yutani_poll(yctx);
		while (m) {
			if (menu_process_event(yctx, m)) {
				redraw();
			}
			switch (m->type) {
				case YUTANI_MSG_KEY_EVENT:
					{
						struct yutani_msg_key_event * ke = (void*)m->data;
						if (ke->event.action == KEY_ACTION_DOWN && ke->event.keycode == 'q') {
							playing = 0;
						}
					}
					break;
				case YUTANI_MSG_WINDOW_FOCUS_CHANGE:
					{
						struct yutani_msg_window_focus_change * wf = (void*)m->data;
						yutani_window_t * win = hashmap_get(yctx->windows, (void*)(uintptr_t)wf->wid);
						if (win) {
							win->focused = wf->focused;
							redraw();
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
	}

	yutani_close(yctx, window);

	return 0;
}
