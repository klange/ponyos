/**
 * @brief Toaru Text library - TrueType parser.
 * @file lib/text.c
 * @author K. Lange <klange@toaruos.org>
 *
 * Implementation of TrueType font file parsing and basic
 * glyph rendering.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2021 K. Lange
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <toaru/graphics.h>
#include <toaru/hashmap.h>
#include <toaru/decodeutf8.h>
#include <toaru/spinlock.h>

#undef min
#define min(a,b) ((a) < (b) ? (a) : (b))

struct TT_Table {
	off_t offset;
	size_t length;
};

struct TT_Coord {
	float x;
	float y;
};

struct TT_Edge {
	struct TT_Coord start;
	struct TT_Coord end;
	int direction;
};

struct TT_Contour {
	size_t edgeCount;
	size_t nextAlloc;
	size_t flags;
	size_t last_start;
	struct TT_Edge edges[];
};

struct TT_Intersection {
	float x;
	int affect;
};

struct TT_Shape {
	size_t edgeCount;
	int lastY;
	int startY;
	int lastX;
	int startX;
	struct TT_Edge edges[];
};

struct TT_Vertex {
	unsigned char flags;
	int x;
	int y;
};

struct TT_Font {
	int privFlags;
	FILE * filePtr;
	uint8_t * buffer;
	uint8_t * memPtr;

	struct TT_Table head_ptr;
	struct TT_Table cmap_ptr;
	struct TT_Table loca_ptr;
	struct TT_Table glyf_ptr;
	struct TT_Table hhea_ptr;
	struct TT_Table hmtx_ptr;
	struct TT_Table name_ptr;

	off_t cmap_start;

	size_t cmap_maxInd;

	float scale;
	float emSize;

	int cmap_type;
	int loca_type;
};


/* Currently, the edge sorter is disabled. It doesn't really help much,
 * and it's very slow with our horrible qsort implementation. */
#if 0
static int edge_sorter_high_scanline(const void * a, const void * b) {
	const struct TT_Edge * left  = a;
	const struct TT_Edge * right = b;

	if (left->start.y < right->start.y) return -1;
	if (left->start.y > right->start.y) return 1;
	return 0;
}

static void sort_edges(size_t edgeCount, struct TT_Edge edges[edgeCount]) {
	qsort(edges, edgeCount, sizeof(struct TT_Edge), edge_sorter_high_scanline);
}
#endif

static int intersection_sorter(const void * a, const void * b) {
	const struct TT_Intersection * left  = a;
	const struct TT_Intersection * right = b;

	if (left->x < right->x) return -1;
	if (left->x > right->x) return 1;
	return 0;
}

static inline void sort_intersections(size_t cnt, struct TT_Intersection intersections[cnt]) {
	qsort(intersections, cnt, sizeof(struct TT_Intersection), intersection_sorter);
}

static inline float edge_at(float y, const struct TT_Edge * edge) {
	float u = (y - edge->start.y) / (edge->end.y - edge->start.y);
	return edge->start.x + u * (edge->end.x - edge->start.x);
}

__attribute__((hot))
static inline size_t prune_edges(size_t edgeCount, float y, const struct TT_Edge edges[edgeCount], struct TT_Intersection into[edgeCount]) {
	size_t outWriter = 0;
	for (size_t i = 0; i < edgeCount; ++i) {
		if (y > edges[i].end.y || y <= edges[i].start.y) continue;
		into[outWriter].x = edge_at(y,&edges[i]);
		into[outWriter].affect = edges[i].direction;
		outWriter++;
	}
	return outWriter;
}

static void process_scanline(
		float _y,
		const struct TT_Shape * shape,
		size_t subsample_width,
		float subsamples[subsample_width],
		size_t cnt,
		const struct TT_Intersection crosses[cnt]
	) {
	int wind = 0;
	size_t j = 0;
	for (int x = shape->startX; x < shape->lastX && j < cnt; ++x) {
		while (j < cnt && x > crosses[j].x) {
			wind += crosses[j].affect;
			j++;
		}
		float last = x;
		while (j < cnt && (x+1) > crosses[j].x) {
			if (wind != 0) {
				subsamples[x - shape->startX] += crosses[j].x - last;
			}
			last = crosses[j].x;
			wind += crosses[j].affect;
			j++;
		}
		if (wind != 0) {
			subsamples[x - shape->startX] += (x+1) - last;
		}
	}
}

static inline uint32_t tt_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return (a << 24U) | (r << 16) | (g << 8) | (b);
}

static inline uint32_t tt_apply_alpha(uint32_t color, uint16_t alpha) {
	uint8_t r = ((uint32_t)(_RED(color) * alpha + 0x80) * 0x101) >> 16UL;
	uint8_t g = ((uint32_t)(_GRE(color) * alpha + 0x80) * 0x101) >> 16UL;
	uint8_t b = ((uint32_t)(_BLU(color) * alpha + 0x80) * 0x101) >> 16UL;
	uint8_t a = ((uint32_t)(_ALP(color) * alpha + 0x80) * 0x101) >> 16UL;
	return tt_rgba(r,g,b,a);
}

static inline uint32_t tt_alpha_blend_rgba(uint32_t bottom, uint32_t top) {
	if (_ALP(bottom) == 0) return top;
	if (_ALP(top) == 255) return top;
	if (_ALP(top) == 0) return bottom;
	uint8_t a = _ALP(top);
	uint16_t t = 0xFF ^ a;
	uint8_t d_r = _RED(top) + (((uint32_t)(_RED(bottom) * t + 0x80) * 0x101) >> 16UL);
	uint8_t d_g = _GRE(top) + (((uint32_t)(_GRE(bottom) * t + 0x80) * 0x101) >> 16UL);
	uint8_t d_b = _BLU(top) + (((uint32_t)(_BLU(bottom) * t + 0x80) * 0x101) >> 16UL);
	uint8_t d_a = _ALP(top) + (((uint32_t)(_ALP(bottom) * t + 0x80) * 0x101) >> 16UL);
	return tt_rgba(d_r, d_g, d_b, d_a);
}


static void paint_scanline(gfx_context_t * ctx, int y, const struct TT_Shape * shape, float * subsamples, uint32_t color) {
	for (int x = shape->startX < 0 ? 0 : shape->startX; x < shape->lastX && x < ctx->width; ++x) {
		uint16_t na = (int)(255 * subsamples[x - shape->startX]) >> 2;
		uint32_t nc = tt_apply_alpha(color, na);
		GFX(ctx, x, y) = tt_alpha_blend_rgba(GFX(ctx, x, y), nc);
		subsamples[x-shape->startX] = 0;
	}
}

void tt_path_paint(gfx_context_t * ctx, const struct TT_Shape * shape, uint32_t color) {
	size_t size = shape->edgeCount;
	struct TT_Intersection * crosses = malloc(sizeof(struct TT_Intersection) * size);

	size_t subsample_width = shape->lastX - shape->startX;
	float * subsamples = malloc(sizeof(float) * subsample_width);
	memset(subsamples, 0, sizeof(float) * subsample_width);

	int startY = shape->startY < 0 ? 0 : shape->startY;
	int endY = shape->lastY <= ctx->height ? shape->lastY : ctx->height;

	for (int y = startY; y < endY; ++y) {
		float _y = y + 0.0001;
		for (int l = 0; l < 4; ++l) {
			size_t cnt;
			if ((cnt = prune_edges(size, _y, shape->edges, crosses))) {
				sort_intersections(cnt, crosses);
				process_scanline(_y, shape, subsample_width, subsamples, cnt, crosses);
			}
			_y += 1.0/4.0;
		}
		paint_scanline(ctx, y, shape, subsamples, color);
	}

	free(subsamples);
	free(crosses);
}

struct TT_Contour * tt_contour_line_to(struct TT_Contour * shape, float x, float y) {
	if (shape->flags & 1) {
		shape->edges[shape->edgeCount].end.x = x;
		shape->edges[shape->edgeCount].end.y = y;
		shape->edgeCount++;
		shape->flags &= ~1;
	} else {
		if (shape->edgeCount + 1 == shape->nextAlloc) {
			shape->nextAlloc *= 2;
			shape = realloc(shape, sizeof(struct TT_Contour) + sizeof(struct TT_Edge) * (shape->nextAlloc));
		}
		shape->edges[shape->edgeCount].start.x = shape->edges[shape->edgeCount-1].end.x;
		shape->edges[shape->edgeCount].start.y = shape->edges[shape->edgeCount-1].end.y;
		shape->edges[shape->edgeCount].end.x = x;
		shape->edges[shape->edgeCount].end.y = y;
		shape->edgeCount++;
		shape->flags &= ~1;
	}
	return shape;
}

struct TT_Contour * tt_contour_move_to(struct TT_Contour * shape, float x, float y) {
	if (!(shape->flags & 1) && shape->edgeCount) {
		shape = tt_contour_line_to(shape, shape->edges[shape->last_start].start.x, shape->edges[shape->last_start].start.y);
	}
	if (shape->edgeCount + 1 == shape->nextAlloc) {
		shape->nextAlloc *= 2;
		shape = realloc(shape, sizeof(struct TT_Contour) + sizeof(struct TT_Edge) * (shape->nextAlloc));
	}
	shape->edges[shape->edgeCount].start.x = x;
	shape->edges[shape->edgeCount].start.y = y;
	shape->last_start = shape->edgeCount;
	shape->flags |= 1;
	return shape;
}

struct TT_Contour * tt_contour_start(float x, float y) {
	struct TT_Contour * shape = malloc(sizeof(struct TT_Contour) + sizeof(struct TT_Edge) * 2);
	shape->edgeCount = 0;
	shape->nextAlloc = 2;
	shape->flags = 0;
	shape->last_start = 0;
	shape->edges[shape->edgeCount].start.x = x;
	shape->edges[shape->edgeCount].start.y = y;
	shape->last_start = shape->edgeCount;
	shape->flags |= 1;
	return shape;
}

struct TT_Shape * tt_contour_finish(struct TT_Contour * in) {
	size_t size = in->edgeCount + 1;
	struct TT_Shape * tmp = malloc(sizeof(struct TT_Shape) + sizeof(struct TT_Edge) * size);
	memcpy(tmp->edges, in->edges, sizeof(struct TT_Edge) * in->edgeCount);

	if (in->flags & 1) {
		size--;
	} else {
		tmp->edges[in->edgeCount].start.x = in->edges[in->edgeCount-1].end.x;
		tmp->edges[in->edgeCount].start.y = in->edges[in->edgeCount-1].end.y;
		tmp->edges[in->edgeCount].end.x   = in->edges[in->last_start].start.x;
		tmp->edges[in->edgeCount].end.y   = in->edges[in->last_start].start.y;
	}

	for (size_t i = 0; i < size; ++i) {
		if (tmp->edges[i].start.y < tmp->edges[i].end.y) {
			tmp->edges[i].direction = 1;
		} else {
			tmp->edges[i].direction = -1;
			struct TT_Coord j = tmp->edges[i].start;
			tmp->edges[i].start = tmp->edges[i].end;
			tmp->edges[i].end = j;
		}
	}

	//sort_edges(size, tmp->edges);
	tmp->edgeCount = size;
	tmp->startY = INT_MAX;
	tmp->lastY = INT_MIN;
	tmp->startX = INT_MAX;
	tmp->lastX = INT_MIN;
	for (size_t i = 0; i < size; ++i) {
		if (tmp->edges[i].end.y + 1 > tmp->lastY) tmp->lastY = tmp->edges[i].end.y + 1;
		if (tmp->edges[i].start.y + 1 > tmp->lastY) tmp->lastY = tmp->edges[i].start.y + 1;
		if (tmp->edges[i].end.y < tmp->startY) tmp->startY = tmp->edges[i].end.y;
		if (tmp->edges[i].start.y < tmp->startY) tmp->startY = tmp->edges[i].start.y;

		if (tmp->edges[i].end.x + 2 > tmp->lastX) tmp->lastX = tmp->edges[i].end.x + 2;
		if (tmp->edges[i].start.x + 2 > tmp->lastX) tmp->lastX = tmp->edges[i].start.x + 2;
		if (tmp->edges[i].end.x < tmp->startX) tmp->startX = tmp->edges[i].end.x;
		if (tmp->edges[i].start.x < tmp->startX) tmp->startX = tmp->edges[i].start.x;
	}

	if (tmp->lastY < tmp->startY) tmp->startY = tmp->lastY;
	if (tmp->lastX < tmp->startX) tmp->startX = tmp->lastX;

	return tmp;
}

static inline int tt_seek(struct TT_Font * font, off_t offset) {
	if (font->privFlags & 1) {
		return fseek(font->filePtr, offset, SEEK_SET);
	} else {
		font->memPtr = font->buffer + offset;
		return 0;
	}
}

static inline long tt_tell(struct TT_Font * font) {
	if (font->privFlags & 1) {
		return ftell(font->filePtr);
	} else {
		return font->memPtr - font->buffer;
	}
}

static inline uint8_t tt_read_8(struct TT_Font * font) {
	if (font->privFlags & 1) {
		return fgetc(font->filePtr);
	} else {
		return *(font->memPtr++);
	}
}

static inline uint32_t tt_read_32(struct TT_Font * font) {
	int a = tt_read_8(font);
	int b = tt_read_8(font);
	int c = tt_read_8(font);
	int d = tt_read_8(font);
	if (a < 0 || b < 0 || c < 0 || d < 0) return 0;
	return ((a & 0xFF) << 24) |
	       ((b & 0xFF) << 16) |
	       ((c & 0xFF) << 8) |
	       ((d & 0xFF) << 0);
}

static inline uint16_t tt_read_16(struct TT_Font * font) {
	int a = tt_read_8(font);
	int b = tt_read_8(font);
	if (a < 0 || b < 0) return 0;
	return ((a & 0xFF) << 8) |
	       ((b & 0xFF) << 0);
}

int tt_xadvance_for_glyph(struct TT_Font * font, unsigned int ind) {
	tt_seek(font, font->hhea_ptr.offset + 2 * 17);
	uint16_t numLong = tt_read_16(font);

	if (ind < numLong) {
		tt_seek(font, font->hmtx_ptr.offset + ind * 4);
		return tt_read_16(font);
	}

	tt_seek(font, font->hmtx_ptr.offset + (numLong - 1) * 4);
	return tt_read_16(font);
}

void tt_set_size(struct TT_Font * font, float size) {
	font->scale = size / font->emSize;
}

void tt_set_size_px(struct TT_Font * font, float size) {
	tt_set_size(font, size * 4.0 / 3.0);
}

off_t tt_get_glyph_offset(struct TT_Font * font, unsigned int glyph) {
	if (font->loca_type == 0) {
		tt_seek(font, font->loca_ptr.offset + glyph * 2);
		return tt_read_16(font) * 2;
	} else {
		tt_seek(font, font->loca_ptr.offset + glyph * 4);
		return tt_read_32(font);
	}
}

int tt_glyph_for_codepoint(struct TT_Font * font, unsigned int codepoint) {
	if (font->cmap_type == 12) {
		/* Get group count */
		tt_seek(font, font->cmap_start + 4 + 8);
		uint32_t ngroups = tt_read_32(font);

		for (unsigned int i = 0; i < ngroups; ++i) {
			uint32_t start = tt_read_32(font);
			uint32_t end   = tt_read_32(font);
			uint32_t ind   = tt_read_32(font);

			if (codepoint >= start && codepoint <= end) {
				return ind + (codepoint - start);
			}
		}
	} else if (font->cmap_type == 4) {
		if (codepoint > 0xFFFF) return 0;

		tt_seek(font, font->cmap_start + 6);
		uint16_t segCount = tt_read_16(font) / 2;

		for (int i = 0; i < segCount; ++i) {
			tt_seek(font, font->cmap_start + 12 + 2 * i);
			uint16_t endCode = tt_read_16(font);
			if (endCode >= codepoint) {
				tt_seek(font, font->cmap_start + 12 + 2 * segCount + 2 + 2 * i);
				uint16_t startCode = tt_read_16(font);
				if (startCode > codepoint) {
					return 0;
				}
				tt_seek(font, font->cmap_start + 12 + 4 * segCount + 2 + 2 * i);
				int16_t idDelta = tt_read_16(font);
				tt_seek(font, font->cmap_start + 12 + 6 * segCount + 2 + 2 * i);
				uint16_t idRangeOffset = tt_read_16(font);
				if (idRangeOffset == 0) {
					return idDelta + codepoint;
				} else {
					tt_seek(font, font->cmap_start + 12 + 6 * segCount + 2 + 2 * i + idRangeOffset + (codepoint - startCode) * 2);
					return tt_read_16(font);
				}
			}
		}
	}

	return 0;
}

static void midpoint(float x_0, float y_0, float cx, float cy, float x_1, float y_1, float t, float * outx, float * outy) {
	float t2 = t * t;
	float nt = 1.0 - t;
	float nt2 = nt * nt;
	*outx = nt2 * x_0 + 2 * t * nt * cx + t2 * x_1;
	*outy = nt2 * y_0 + 2 * t * nt * cy + t2 * y_1;
}

static struct TT_Contour * tt_draw_glyph_into(struct TT_Contour * contour, struct TT_Font * font, float x_offset, float y_offset, unsigned int glyph) {
	off_t glyf_offset = tt_get_glyph_offset(font, glyph);
	if (tt_get_glyph_offset(font, glyph + 1) == glyf_offset) return contour;

	tt_seek(font, font->glyf_ptr.offset + glyf_offset);

	int16_t numContours = tt_read_16(font);
	/* int16_t xMin = */ tt_read_16(font);
	/* int16_t yMin = */ tt_read_16(font);
	/* int16_t xMax = */ tt_read_16(font);
	/* int16_t yMax = */ tt_read_16(font);

	tt_seek(font, font->glyf_ptr.offset + glyf_offset + 10);

	if (numContours > 0) {
		uint16_t endPt;
		for (int i = 0; i < numContours; ++i) {
			endPt = tt_read_16(font);
		}
		uint16_t numInstr = tt_read_16(font);
		for (unsigned int i = 0; i < numInstr; ++i) {
			tt_read_8(font);
		}
		struct TT_Vertex * vertices = malloc(sizeof(struct TT_Vertex) * (endPt + 1));
		for (int i = 0; i < endPt + 1; ) {
			uint8_t v = tt_read_8(font);
			vertices[i].flags = v;
			i++;
			if (v & 8) {
				uint8_t repC = tt_read_8(font);
				while (repC) {
					vertices[i].flags = v;
					repC--;
					i++;
				}
			}
		}
		int last_x = 0;
		int last_y = 0;
		for (int i = 0; i < endPt + 1; i++) {
			unsigned char flags = vertices[i].flags;
			if (flags & (1 << 1)) {
				/* One byte */
				if (flags & (1 << 4)) {
					/* Positive */
					vertices[i].x = last_x + tt_read_8(font);
				} else {
					vertices[i].x = last_x - tt_read_8(font);
				}
			} else {
				if (flags & (1 << 4)) {
					vertices[i].x = last_x;
				} else {
					int16_t diff = tt_read_16(font);
					vertices[i].x = last_x + diff;
				}
			}
			last_x = vertices[i].x;
		}
		for (int i = 0; i < endPt + 1; i++) {
			unsigned char flags = vertices[i].flags;
			if (flags & (1 << 2)) {
				/* One byte */
				if (flags & (1 << 5)) {
					/* Positive */
					vertices[i].y = last_y + tt_read_8(font);
				} else {
					vertices[i].y = last_y - tt_read_8(font);
				}
			} else {
				if (flags & (1 << 5)) {
					vertices[i].y = last_y;
				} else {
					int16_t diff = tt_read_16(font);
					vertices[i].y = last_y + diff;
				}
			}
			last_y = vertices[i].y;
		}

		tt_seek(font, font->glyf_ptr.offset + glyf_offset + 10);

		int move_next = 1;
		int next_end = tt_read_16(font);

		float lx = 0, ly = 0, cx = 0, cy = 0, x = 0, y = 0;
		float sx = 0, sy = 0;
		int wasControl = 0;

		for (int i = 0; i < endPt + 1; ++i) {
			x = ((float)vertices[i].x) * font->scale + x_offset;
			y = (-(float)vertices[i].y) * font->scale + y_offset;
			int isCurve = !(vertices[i].flags & (1 << 0));
			if (move_next) {
				contour = tt_contour_move_to(contour, x, y);
				if (isCurve) {
					/* Is the point before this on-curve? */
					float px = (float)vertices[next_end].x * font->scale + x_offset;
					float py = (-(float)vertices[next_end].y) * font->scale + y_offset;
					if (vertices[next_end].flags & (1 << 0)) {
						/* Else we're just a regular off-curve point? */
						sx = px;
						sy = py;
						lx = px;
						ly = py;
					} else {
						float dx = (px + x) / 2.0;
						float dy = (py + y) / 2.0;
						lx = dx;
						ly = dy;
						sx = dx;
						sy = dy;
					}
					cx = x;
					cy = y;
					wasControl = 1;
				} else {
					lx = x;
					ly = y;
					sx = x;
					sy = y;
					wasControl = 0;
				}
				move_next = 0;
			} else {
				if (isCurve) {
					if (wasControl) {
						float dx = (cx + x) / 2.0;
						float dy = (cy + y) / 2.0;
						for (int i = 1; i < 10; ++i) {
							float mx, my;
							midpoint(lx,ly,cx,cy,dx,dy,(float)i / 10.0,&mx,&my);
							contour = tt_contour_line_to(contour, mx, my);
						}
						contour = tt_contour_line_to(contour, dx, dy);
						lx = dx;
						ly = dy;
					}
					cx = x;
					cy = y;
					wasControl = 1;
				} else {
					if (wasControl) {
						for (int i = 1; i < 10; ++i) {
							float mx, my;
							midpoint(lx,ly,cx,cy,x,y,(float)i / 10.0,&mx,&my);
							contour = tt_contour_line_to(contour, mx, my);
						}
					}
					contour = tt_contour_line_to(contour, x, y);
					lx = x;
					ly = y;
					wasControl = 0;
				}
			}
			if (i == next_end) {
				if (wasControl) {
					for (int i = 1; i < 10; ++i) {
						float mx, my;
						midpoint(lx,ly,cx,cy,sx,sy,(float)i / 10.0,&mx,&my);
						contour = tt_contour_line_to(contour, mx, my);
					}
				}
				contour = tt_contour_line_to(contour, sx, sy);
				move_next = 1;
				next_end = tt_read_16(font);
			}
		}

		free(vertices);
	} else if (numContours < 0) {
		while (1) {
			uint16_t flags = tt_read_16(font);
			uint16_t ind   = tt_read_16(font);
			int16_t x, y;
			if (flags & (1 << 0)) {
				x = tt_read_16(font);
				y = tt_read_16(font);
			} else {
				x = tt_read_8(font);
				y = tt_read_8(font);
			}

			float x_f = x_offset;
			float y_f = y_offset;
			if (flags & (1 << 1)) {
				x_f = x_offset + x * font->scale;
				y_f = y_offset - y * font->scale;
			}

			if (flags & (1 << 3)) {
				/* TODO */
				tt_read_16(font);
			} else if (flags & (1 << 6)) {
				/* TODO */
				tt_read_16(font);
				tt_read_16(font);
			} else if (flags & (1 << 7)) {
				/* TODO */
				tt_read_16(font);
				tt_read_16(font);
				tt_read_16(font);
				tt_read_16(font);
			} else {
				long o = tt_tell(font);
				contour = tt_draw_glyph_into(contour,font,x_f,y_f,ind);
				tt_seek(font, o);
			}
			if (!(flags & (1 << 5))) break;
		}
	}

	return contour;
}

void tt_draw_glyph(gfx_context_t * ctx, struct TT_Font * font, int x, int y, unsigned int glyph, uint32_t color) {
	struct TT_Contour * contour = tt_contour_start(0, 0);
	contour = tt_draw_glyph_into(contour,font,x,y,glyph);
	if (contour->edgeCount) {
		struct TT_Shape * shape = tt_contour_finish(contour);
		tt_path_paint(ctx, shape, color);
		free(shape);
	}
	free(contour);
}

int tt_string_width(struct TT_Font * font, const char * s) {
	float x_offset = 0;
	uint32_t cp = 0;
	uint32_t istate = 0;

	for (const unsigned char * c = (const unsigned char*)s; *c; ++c) {
		if (!decode(&istate, &cp, *c)) {
			unsigned int glyph = tt_glyph_for_codepoint(font, cp);
			x_offset += tt_xadvance_for_glyph(font, glyph) * font->scale;
		}
	}

	return x_offset;
}

int tt_draw_string(gfx_context_t * ctx, struct TT_Font * font, int x, int y, const char * s, uint32_t color) {
	struct TT_Contour * contour = tt_contour_start(0, 0);

	float x_offset = x;
	uint32_t cp = 0;
	uint32_t istate = 0;

	for (const unsigned char * c = (const unsigned char*)s; *c; ++c) {
		if (!decode(&istate, &cp, *c)) {
			unsigned int glyph = tt_glyph_for_codepoint(font, cp);
			contour = tt_draw_glyph_into(contour,font,x_offset,y,glyph);
			x_offset += tt_xadvance_for_glyph(font, glyph) * font->scale;
		}
	}

	if (contour->edgeCount) {
		struct TT_Shape * shape = tt_contour_finish(contour);
		tt_path_paint(ctx, shape, color);
		free(shape);
	}
	free(contour);

	return x_offset - x;
}


static int tt_font_load(struct TT_Font * font) {
	if (tt_seek(font, 4)) {
		fprintf(stderr, "tt: failed to seek to 4\n");
		goto _fail_free;
	}
	uint16_t numTables = tt_read_16(font);
	if (tt_seek(font, 12)) {
		fprintf(stderr, "tt: failed to seek to 12\n");
		goto _fail_free;
	}

	for (unsigned int i = 0; i < numTables; ++i) {
		uint32_t tag = tt_read_32(font);
		/* uint32_t checkSum = */ tt_read_32(font);
		uint32_t offset = tt_read_32(font);
		uint32_t length = tt_read_32(font);

		switch (tag) {
			case 0x68656164: /* head */
				font->head_ptr.offset = offset;
				font->head_ptr.length = length;
				break;
			case 0x636d6170: /* cmap */
				font->cmap_ptr.offset = offset;
				font->cmap_ptr.length = length;
				break;
			case 0x676c7966: /* glyf */
				font->glyf_ptr.offset = offset;
				font->glyf_ptr.length = length;
				break;
			case 0x6c6f6361: /* loca */
				font->loca_ptr.offset = offset;
				font->loca_ptr.length = length;
				break;
			case 0x68686561: /* hhea */
				font->hhea_ptr.offset = offset;
				font->hhea_ptr.length = length;
				break;
			case 0x686d7478: /* hmtx */
				font->hmtx_ptr.offset = offset;
				font->hmtx_ptr.length = length;
				break;
			case 0x6e616d65: /* name */
				font->name_ptr.offset = offset;
				font->name_ptr.length = length;
				break;
		}
	}

	if (!font->head_ptr.offset) { fprintf(stderr, "tt: no head table\n"); goto _fail_free; }
	if (!font->glyf_ptr.offset) { fprintf(stderr, "tt: no glyf table\n"); goto _fail_free; }
	if (!font->cmap_ptr.offset) { fprintf(stderr, "tt: no cmap table\n"); goto _fail_free; }
	if (!font->loca_ptr.offset) { fprintf(stderr, "tt: no loca table\n"); goto _fail_free; }

	/* Get emSize */
	tt_seek(font, font->head_ptr.offset + 18);
	font->emSize = (float)tt_read_16(font);

	/* Try to pick a viable cmap */
	tt_seek(font, font->cmap_ptr.offset);

	uint32_t best = 0;
	int bestScore = 0;

	/* Read size */
	/* uint16_t cmap_vers = */ tt_read_16(font);
	uint16_t cmap_size = tt_read_16(font);
	for (unsigned int i = 0; i < cmap_size; ++i) {
		uint16_t platform = tt_read_16(font);
		uint16_t type     = tt_read_16(font);
		uint32_t offset   = tt_read_32(font);

		if ((platform == 3 || platform == 0) && type == 10) {
			best = offset;
			bestScore = 4;
		} else if (platform == 0 && type == 4) {
			best = offset;
			bestScore = 4;
		} else if (((platform == 0 && type == 3) || (platform == 3 && type == 1)) && bestScore < 2) {
			best = offset;
			bestScore = 2;
		}
	}

	if (!best) {
		fprintf(stderr, "tt: TODO: unsupported cmap (best = %#x bestScore = %d)\n", best, bestScore);
		goto _fail_free;
	}

	/* What type is this */
	tt_seek(font, font->cmap_ptr.offset + best);

	font->cmap_type = tt_read_16(font);
	if (font->cmap_type != 12 && font->cmap_type != 4) {
		fprintf(stderr, "tt: TODO: unsupported cmap indexing %d\n", font->cmap_type);
		goto _fail_free;
	}

	font->cmap_start = font->cmap_ptr.offset + best;

	tt_seek(font, font->head_ptr.offset + 50);
	font->loca_type = tt_read_16(font);

	return 1;

_fail_free:
	return 0;
	free(font);
}

struct TT_Font * tt_font_from_file(const char * fileName) {
	FILE * f = fopen(fileName, "r");
	if (!f) return NULL;

	struct TT_Font * font = calloc(sizeof(struct TT_Font), 1);
	font->filePtr = f;
	font->privFlags = 1;

	if (!tt_font_load(font)) goto _fail_close;

	return font;

_fail_close:
	fclose(f);
	return NULL;
}

struct TT_Font * tt_font_from_memory(uint8_t * buffer) {
	struct TT_Font * font = calloc(sizeof(struct TT_Font), 1);
	font->privFlags = 0;
	font->buffer = buffer;
	if (!tt_font_load(font)) return NULL;
	return font;
}

struct TT_Font * tt_font_from_file_mem(const char * fileName) {
	FILE * f = fopen(fileName, "r");
	if (!f) return NULL;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint8_t * buf = malloc(size);
	fread(buf, 1, size, f);

	fclose(f);

	return tt_font_from_memory(buf);
}

static hashmap_t * shm_font_cache = NULL;
static int volatile shm_font_lock = 0;

struct TT_Font * tt_font_from_shm(const char * identifier) {
	spin_lock(&shm_font_lock);

	if (!shm_font_cache) {
		shm_font_cache = hashmap_create(10);
	}

	void * fontData = hashmap_get(shm_font_cache, (char*)identifier);
	if (fontData) goto shm_success;

	char * display = getenv("DISPLAY");

	if (!display) goto shm_fail;

	char fullIdentifier[1024];
	snprintf(fullIdentifier, 1023, "sys.%s.fonts.%s", display, identifier);

	size_t fontSize = 0;
	fontData = shm_obtain(fullIdentifier, &fontSize);

	if (fontSize == 0) {
		shm_release(identifier);
		goto shm_fail;
	}

	hashmap_set(shm_font_cache, (char*)identifier, fontData);

shm_success:
	spin_unlock(&shm_font_lock);
	return tt_font_from_memory(fontData);

shm_fail:
	spin_unlock(&shm_font_lock);
	return NULL;
}

void tt_draw_string_shadow(gfx_context_t * ctx, struct TT_Font * font, char * string, int font_size, int left, int top, uint32_t text_color, uint32_t shadow_color, int blur) {
	tt_set_size(font, font_size);
	int w = tt_string_width(font, string);
	/* TODO: We need to check the bounds of descenders and ascenders so we can fit things more correctly... */
	sprite_t * _tmp_s = create_sprite(w + blur * 2, font_size + blur * 2 + 5, ALPHA_EMBEDDED);
	gfx_context_t * _tmp = init_graphics_sprite(_tmp_s);
	draw_fill(_tmp, rgba(0,0,0,0));
	tt_draw_string(_tmp, font, blur, blur + font_size, string, shadow_color);
	blur_context_box(_tmp, blur);
	blur_context_box(_tmp, blur);
	free(_tmp);
	draw_sprite(ctx, _tmp_s, left - blur, top - blur);
	sprite_free(_tmp_s);
	tt_draw_string(ctx, font, left, top + font_size, string, text_color);
}

static int to_eight(uint32_t codepoint, char * out) {
	memset(out, 0x00, 7);

	if (codepoint < 0x0080) {
		out[0] = (char)codepoint;
	} else if (codepoint < 0x0800) {
		out[0] = 0xC0 | (codepoint >> 6);
		out[1] = 0x80 | (codepoint & 0x3F);
	} else if (codepoint < 0x10000) {
		out[0] = 0xE0 | (codepoint >> 12);
		out[1] = 0x80 | ((codepoint >> 6) & 0x3F);
		out[2] = 0x80 | (codepoint & 0x3F);
	} else if (codepoint < 0x200000) {
		out[0] = 0xF0 | (codepoint >> 18);
		out[1] = 0x80 | ((codepoint >> 12) & 0x3F);
		out[2] = 0x80 | ((codepoint >> 6) & 0x3F);
		out[3] = 0x80 | ((codepoint) & 0x3F);
	} else if (codepoint < 0x4000000) {
		out[0] = 0xF8 | (codepoint >> 24);
		out[1] = 0x80 | (codepoint >> 18);
		out[2] = 0x80 | ((codepoint >> 12) & 0x3F);
		out[3] = 0x80 | ((codepoint >> 6) & 0x3F);
		out[4] = 0x80 | ((codepoint) & 0x3F);
	} else {
		out[0] = 0xF8 | (codepoint >> 30);
		out[1] = 0x80 | ((codepoint >> 24) & 0x3F);
		out[2] = 0x80 | ((codepoint >> 18) & 0x3F);
		out[3] = 0x80 | ((codepoint >> 12) & 0x3F);
		out[4] = 0x80 | ((codepoint >> 6) & 0x3F);
		out[5] = 0x80 | ((codepoint) & 0x3F);
	}

	return strlen(out);
}


char * tt_get_name_string(struct TT_Font * font, int identifier) {
	if (!font->name_ptr.offset) return NULL;

	tt_seek(font, font->name_ptr.offset);
	uint16_t nameFormat = tt_read_16(font);
	uint16_t count = tt_read_16(font);
	uint16_t stringOffset = tt_read_16(font);

	if (nameFormat != 0) return NULL; /* Unsupported table format */

	/* Read records until we find one that matches what we asked for, in a suitable format */
	for (unsigned int i = 0; i < count; ++i) {
		uint16_t platformId = tt_read_16(font);
		uint16_t platformSpecificId = tt_read_16(font);
		/* uint16_t languageId = */ tt_read_16(font);
		uint16_t nameId = tt_read_16(font);
		uint16_t length = tt_read_16(font);
		uint16_t offset = tt_read_16(font);

		if (nameId != identifier) continue;
		if (!(platformId == 3 && platformSpecificId == 1)) continue;

		char * tmp = calloc(length * 3 + 1,1); /* Should be enough ? */
		char * c = tmp;

		tt_seek(font, stringOffset + offset + font->name_ptr.offset);

		for (unsigned int j = 0; j < length; j += 2) {
			uint32_t cp = tt_read_16(font);
			if (cp > 0xD7FF && cp < 0xE000) {
				uint32_t highBits = cp - 0xD800;
				uint32_t lowBits = tt_read_16(font) - 0xDC00;
				cp = 0x10000 + (highBits << 10) + lowBits;
				j += 2;
			}
			c += to_eight(cp, c);
		}

		return tmp;
	}

	return NULL;
}

void tt_contour_stroke_bounded(gfx_context_t * ctx, struct TT_Contour * in, uint32_t color, float width,
		int x_0, int y_0, int w, int h) {
	/* This is a stupid slow thing */
	for (int y = y_0; y < y_0 + h; y++) {
		for (int x = x_0; x < x_0 + w; x++) {
			struct gfx_point p = {(float)x + 0.5, (float)y + 0.5};
			/* For every line in the contour... */
			float mindist = 100.0;
			for (size_t i = 0; i < in->edgeCount; ++i) {
				struct gfx_point v = { in->edges[i].start.x, in->edges[i].start.y };
				struct gfx_point w = { in->edges[i].end.x, in->edges[i].end.y };

				float mine = gfx_line_distance(&p,&v,&w);
				if (mine < mindist) mindist = mine;
			}

			if (mindist < width + 0.5) {
				if (mindist < width - 0.5) {
					GFX(ctx,x,y) = alpha_blend_rgba(GFX(ctx,x,y), color);
				} else {
					float alpha = 1.0 - (mindist - width + 0.5);
					GFX(ctx,x,y) = alpha_blend_rgba(GFX(ctx,x,y), premultiply(rgba(_RED(color),_GRE(color),_BLU(color),(int)((double)_ALP(color) * alpha))));
				}
			}
		}
	}
}
