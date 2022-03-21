/**
 * @brief VGA palette conversion
 *
 * Converts 256-color index values to closest matching 16-color
 * value for the VGA terminal. Note that values here are terminal
 * color codes, not the VGA color codes - the terminal converts
 * them to VGA color codes later. This was automatically generated
 * from a script, but I don't know where that script went.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2012-2018 K. Lange
 */
#define PALETTE_COLORS 256
uint32_t vga_colors[PALETTE_COLORS] = {
	0x0,
	0x1,
	0x2,
	0x3,
	0x4,
	0x5,
	0x6,
	0x7,
	0x8,
	0x9,
	0xa,
	0xb,
	0xc,
	0xd,
	0xe,
	0xf,
	0x0, /* #000000 -> #000000 */
	0x4, /* #00005f -> #0000aa */
	0x4, /* #000087 -> #0000aa */
	0x4, /* #0000af -> #0000aa */
	0x4, /* #0000d7 -> #0000aa */
	0xc, /* #0000ff -> #5555ff */
	0x2, /* #005f00 -> #00aa00 */
	0x8, /* #005f5f -> #555555 */
	0x6, /* #005f87 -> #00aaaa */
	0x6, /* #005faf -> #00aaaa */
	0xc, /* #005fd7 -> #5555ff */
	0xc, /* #005fff -> #5555ff */
	0x2, /* #008700 -> #00aa00 */
	0xa, /* #00875f -> #55aa55 */
	0x6, /* #008787 -> #00aaaa */
	0x6, /* #0087af -> #00aaaa */
	0x6, /* #0087d7 -> #00aaaa */
	0xc, /* #0087ff -> #5555ff */
	0x2, /* #00af00 -> #00aa00 */
	0xa, /* #00af5f -> #55aa55 */
	0x6, /* #00af87 -> #00aaaa */
	0x6, /* #00afaf -> #00aaaa */
	0x6, /* #00afd7 -> #00aaaa */
	0xe, /* #00afff -> #55ffff */
	0x2, /* #00d700 -> #00aa00 */
	0xa, /* #00d75f -> #55aa55 */
	0x6, /* #00d787 -> #00aaaa */
	0x6, /* #00d7af -> #00aaaa */
	0x6, /* #00d7d7 -> #00aaaa */
	0xe, /* #00d7ff -> #55ffff */
	0x2, /* #00ff00 -> #00aa00 */
	0xa, /* #00ff5f -> #55aa55 */
	0x6, /* #00ff87 -> #00aaaa */
	0x6, /* #00ffaf -> #00aaaa */
	0xe, /* #00ffd7 -> #55ffff */
	0xe, /* #00ffff -> #55ffff */
	0x1, /* #5f0000 -> #aa0000 */
	0x8, /* #5f005f -> #555555 */
	0x5, /* #5f0087 -> #aa00aa */
	0x5, /* #5f00af -> #aa00aa */
	0x5, /* #5f00d7 -> #aa00aa */
	0xc, /* #5f00ff -> #5555ff */
	0x3, /* #5f5f00 -> #aa5500 */
	0x8, /* #5f5f5f -> #555555 */
	0x8, /* #5f5f87 -> #555555 */
	0x7, /* #5f5faf -> #aaaaaa */
	0xc, /* #5f5fd7 -> #5555ff */
	0xc, /* #5f5fff -> #5555ff */
	0x2, /* #5f8700 -> #00aa00 */
	0xa, /* #5f875f -> #55aa55 */
	0xa, /* #5f8787 -> #55aa55 */
	0x7, /* #5f87af -> #aaaaaa */
	0xc, /* #5f87d7 -> #5555ff */
	0xc, /* #5f87ff -> #5555ff */
	0x2, /* #5faf00 -> #00aa00 */
	0xa, /* #5faf5f -> #55aa55 */
	0xa, /* #5faf87 -> #55aa55 */
	0x7, /* #5fafaf -> #aaaaaa */
	0x7, /* #5fafd7 -> #aaaaaa */
	0xe, /* #5fafff -> #55ffff */
	0x2, /* #5fd700 -> #00aa00 */
	0xa, /* #5fd75f -> #55aa55 */
	0xa, /* #5fd787 -> #55aa55 */
	0x7, /* #5fd7af -> #aaaaaa */
	0xe, /* #5fd7d7 -> #55ffff */
	0xe, /* #5fd7ff -> #55ffff */
	0x2, /* #5fff00 -> #00aa00 */
	0xb, /* #5fff5f -> #ffff55 */
	0xb, /* #5fff87 -> #ffff55 */
	0x7, /* #5fffaf -> #aaaaaa */
	0xe, /* #5fffd7 -> #55ffff */
	0xe, /* #5fffff -> #55ffff */
	0x1, /* #870000 -> #aa0000 */
	0x8, /* #87005f -> #555555 */
	0x5, /* #870087 -> #aa00aa */
	0x5, /* #8700af -> #aa00aa */
	0x5, /* #8700d7 -> #aa00aa */
	0xc, /* #8700ff -> #5555ff */
	0x3, /* #875f00 -> #aa5500 */
	0x8, /* #875f5f -> #555555 */
	0x8, /* #875f87 -> #555555 */
	0x7, /* #875faf -> #aaaaaa */
	0xc, /* #875fd7 -> #5555ff */
	0xc, /* #875fff -> #5555ff */
	0x3, /* #878700 -> #aa5500 */
	0xa, /* #87875f -> #55aa55 */
	0x7, /* #878787 -> #aaaaaa */
	0x7, /* #8787af -> #aaaaaa */
	0x7, /* #8787d7 -> #aaaaaa */
	0xc, /* #8787ff -> #5555ff */
	0x2, /* #87af00 -> #00aa00 */
	0xa, /* #87af5f -> #55aa55 */
	0x7, /* #87af87 -> #aaaaaa */
	0x7, /* #87afaf -> #aaaaaa */
	0x7, /* #87afd7 -> #aaaaaa */
	0xe, /* #87afff -> #55ffff */
	0x2, /* #87d700 -> #00aa00 */
	0xa, /* #87d75f -> #55aa55 */
	0x7, /* #87d787 -> #aaaaaa */
	0x7, /* #87d7af -> #aaaaaa */
	0xe, /* #87d7d7 -> #55ffff */
	0xe, /* #87d7ff -> #55ffff */
	0x2, /* #87ff00 -> #00aa00 */
	0xb, /* #87ff5f -> #ffff55 */
	0xb, /* #87ff87 -> #ffff55 */
	0x7, /* #87ffaf -> #aaaaaa */
	0xe, /* #87ffd7 -> #55ffff */
	0xe, /* #87ffff -> #55ffff */
	0x1, /* #af0000 -> #aa0000 */
	0x5, /* #af005f -> #aa00aa */
	0x5, /* #af0087 -> #aa00aa */
	0x5, /* #af00af -> #aa00aa */
	0x5, /* #af00d7 -> #aa00aa */
	0xd, /* #af00ff -> #ff55ff */
	0x3, /* #af5f00 -> #aa5500 */
	0x9, /* #af5f5f -> #ff5555 */
	0x9, /* #af5f87 -> #ff5555 */
	0x7, /* #af5faf -> #aaaaaa */
	0xd, /* #af5fd7 -> #ff55ff */
	0xd, /* #af5fff -> #ff55ff */
	0x3, /* #af8700 -> #aa5500 */
	0xa, /* #af875f -> #55aa55 */
	0x7, /* #af8787 -> #aaaaaa */
	0x7, /* #af87af -> #aaaaaa */
	0x7, /* #af87d7 -> #aaaaaa */
	0xd, /* #af87ff -> #ff55ff */
	0x2, /* #afaf00 -> #00aa00 */
	0xa, /* #afaf5f -> #55aa55 */
	0x7, /* #afaf87 -> #aaaaaa */
	0x7, /* #afafaf -> #aaaaaa */
	0x7, /* #afafd7 -> #aaaaaa */
	0xf, /* #afafff -> #ffffff */
	0x2, /* #afd700 -> #00aa00 */
	0xb, /* #afd75f -> #ffff55 */
	0x7, /* #afd787 -> #aaaaaa */
	0x7, /* #afd7af -> #aaaaaa */
	0x7, /* #afd7d7 -> #aaaaaa */
	0xf, /* #afd7ff -> #ffffff */
	0x2, /* #afff00 -> #00aa00 */
	0xb, /* #afff5f -> #ffff55 */
	0xb, /* #afff87 -> #ffff55 */
	0x7, /* #afffaf -> #aaaaaa */
	0xf, /* #afffd7 -> #ffffff */
	0xf, /* #afffff -> #ffffff */
	0x1, /* #d70000 -> #aa0000 */
	0x9, /* #d7005f -> #ff5555 */
	0x5, /* #d70087 -> #aa00aa */
	0x5, /* #d700af -> #aa00aa */
	0x5, /* #d700d7 -> #aa00aa */
	0xd, /* #d700ff -> #ff55ff */
	0x3, /* #d75f00 -> #aa5500 */
	0x9, /* #d75f5f -> #ff5555 */
	0x9, /* #d75f87 -> #ff5555 */
	0x7, /* #d75faf -> #aaaaaa */
	0xd, /* #d75fd7 -> #ff55ff */
	0xd, /* #d75fff -> #ff55ff */
	0x3, /* #d78700 -> #aa5500 */
	0x9, /* #d7875f -> #ff5555 */
	0x7, /* #d78787 -> #aaaaaa */
	0x7, /* #d787af -> #aaaaaa */
	0x7, /* #d787d7 -> #aaaaaa */
	0xd, /* #d787ff -> #ff55ff */
	0x2, /* #d7af00 -> #00aa00 */
	0xa, /* #d7af5f -> #55aa55 */
	0x7, /* #d7af87 -> #aaaaaa */
	0x7, /* #d7afaf -> #aaaaaa */
	0x7, /* #d7afd7 -> #aaaaaa */
	0xf, /* #d7afff -> #ffffff */
	0x2, /* #d7d700 -> #00aa00 */
	0xb, /* #d7d75f -> #ffff55 */
	0x7, /* #d7d787 -> #aaaaaa */
	0x7, /* #d7d7af -> #aaaaaa */
	0xf, /* #d7d7d7 -> #ffffff */
	0xf, /* #d7d7ff -> #ffffff */
	0xb, /* #d7ff00 -> #ffff55 */
	0xb, /* #d7ff5f -> #ffff55 */
	0xb, /* #d7ff87 -> #ffff55 */
	0x7, /* #d7ffaf -> #aaaaaa */
	0xf, /* #d7ffd7 -> #ffffff */
	0xf, /* #d7ffff -> #ffffff */
	0x1, /* #ff0000 -> #aa0000 */
	0x9, /* #ff005f -> #ff5555 */
	0x5, /* #ff0087 -> #aa00aa */
	0x5, /* #ff00af -> #aa00aa */
	0x5, /* #ff00d7 -> #aa00aa */
	0xd, /* #ff00ff -> #ff55ff */
	0x3, /* #ff5f00 -> #aa5500 */
	0x9, /* #ff5f5f -> #ff5555 */
	0x9, /* #ff5f87 -> #ff5555 */
	0x7, /* #ff5faf -> #aaaaaa */
	0xd, /* #ff5fd7 -> #ff55ff */
	0xd, /* #ff5fff -> #ff55ff */
	0x3, /* #ff8700 -> #aa5500 */
	0x9, /* #ff875f -> #ff5555 */
	0x9, /* #ff8787 -> #ff5555 */
	0x7, /* #ff87af -> #aaaaaa */
	0xd, /* #ff87d7 -> #ff55ff */
	0xd, /* #ff87ff -> #ff55ff */
	0x2, /* #ffaf00 -> #00aa00 */
	0xb, /* #ffaf5f -> #ffff55 */
	0x7, /* #ffaf87 -> #aaaaaa */
	0x7, /* #ffafaf -> #aaaaaa */
	0x7, /* #ffafd7 -> #aaaaaa */
	0xf, /* #ffafff -> #ffffff */
	0x2, /* #ffd700 -> #00aa00 */
	0xb, /* #ffd75f -> #ffff55 */
	0xb, /* #ffd787 -> #ffff55 */
	0x7, /* #ffd7af -> #aaaaaa */
	0xf, /* #ffd7d7 -> #ffffff */
	0xf, /* #ffd7ff -> #ffffff */
	0xb, /* #ffff00 -> #ffff55 */
	0xb, /* #ffff5f -> #ffff55 */
	0xb, /* #ffff87 -> #ffff55 */
	0xf, /* #ffffaf -> #ffffff */
	0xf, /* #ffffd7 -> #ffffff */
	0xf, /* #ffffff -> #ffffff */
	0x0, /* #080808 -> #000000 */
	0x0, /* #121212 -> #000000 */
	0x0, /* #1c1c1c -> #000000 */
	0x0, /* #262626 -> #000000 */
	0x8, /* #303030 -> #555555 */
	0x8, /* #3a3a3a -> #555555 */
	0x8, /* #444444 -> #555555 */
	0x8, /* #4e4e4e -> #555555 */
	0x8, /* #585858 -> #555555 */
	0x8, /* #626262 -> #555555 */
	0x8, /* #6c6c6c -> #555555 */
	0x8, /* #767676 -> #555555 */
	0x7, /* #808080 -> #aaaaaa */
	0x7, /* #8a8a8a -> #aaaaaa */
	0x7, /* #949494 -> #aaaaaa */
	0x7, /* #9e9e9e -> #aaaaaa */
	0x7, /* #a8a8a8 -> #aaaaaa */
	0x7, /* #b2b2b2 -> #aaaaaa */
	0x7, /* #bcbcbc -> #aaaaaa */
	0x7, /* #c6c6c6 -> #aaaaaa */
	0x7, /* #d0d0d0 -> #aaaaaa */
	0xf, /* #dadada -> #ffffff */
	0xf, /* #e4e4e4 -> #ffffff */
	0xf, /* #eeeeee -> #ffffff */
};
