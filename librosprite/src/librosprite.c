/*
 * This file is part of librosprite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 James Shaw <js102@zepler.net>
 */

/**
 * \file
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "librosprite.h"

/**
 * Reads four bytes, 00, 11, 22 and 33, of a byte array b to give 0x33221100.
 */
#define BTUINT(b)              \
		((unsigned)b[0]        | \
		((unsigned)b[1] <<  8) | \
		((unsigned)b[2] << 16) | \
		((unsigned)b[3] << 24))

/**
 * Reverse the byte order of a word such that 0xAABBCCDD becomes 0xDDCCBBAA.
 */
#define BSWAP(word) (((word & (0x000000ff)) << 24) | ((word & 0x0000ff00) << 8) | ((word & 0x00ff0000) >> 8) | ((word & 0xff000000) >> 24))

/* AUDIT-FIX R3: hard limits applied to values taken from the file */
#define ROSPRITE_HEADER_SIZE 44u           /**< bytes in a sprite control block */
#define ROSPRITE_MAX_PIXELS (1u << 24)     /**< largest width * height decoded */
#define ROSPRITE_MAX_EXTENSION (1u << 20)  /**< largest extension area accepted */

#define ERRCHK(x) do { \
	rosprite_error err = x; \
	if (err != ROSPRITE_OK) return err; \
} while(0)

struct rosprite_header {
	uint32_t width_words; /* width in words */
	/* height defined in sprite struct */
	uint32_t first_used_bit; /* old format only (spriteType = 0) */
	uint32_t last_used_bit;

	/**
	 * Image size in bytes
	 */
	uint32_t image_size;

	/**
	 * Mask size in bytes
	 */
	uint32_t mask_size;
};

struct rosprite_mask_state {
	uint32_t x;
	uint32_t y;
	uint32_t first_used_bit;
	uint32_t row_max_bit;
	uint32_t height;
	uint32_t current_byte_index;
	uint32_t current_word;
	uint32_t bpp;
	uint32_t mask_size; /* AUDIT-FIX R5: bytes of mask data available */
};

struct rosprite_file_context {
	FILE* f;
};

struct rosprite_mem_context {
	uint8_t* base;
	unsigned long offset;
	bool known_size;
	unsigned long size;
};

static const struct rosprite_mode oldmodes[] = {
/*0*/{ .colorbpp = 1, .maskbpp = 1, .mask_width = 1, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*1*/{ .colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 45, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*2*/{ .colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 22, .ydpi = 45, .color_model = ROSPRITE_RGB },

/*3*/{ .colorbpp = 0, .maskbpp = 0, .mask_width = 0, .xdpi = 0,  .ydpi = 0,  .color_model = ROSPRITE_RGB },

/*4*/{ .colorbpp = 1, .maskbpp = 1, .mask_width = 1, .xdpi = 45, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*5*/{ .colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 22, .ydpi = 45, .color_model = ROSPRITE_RGB },

/*6*/{ .colorbpp = 0, .maskbpp = 0, .mask_width = 0, .xdpi = 0,  .ydpi = 0,  .color_model = ROSPRITE_RGB },
/*7*/{ .colorbpp = 0, .maskbpp = 0, .mask_width = 0, .xdpi = 0,  .ydpi = 0,  .color_model = ROSPRITE_RGB },

/*8*/{ .colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*9*/{ .colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 45, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*10*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 22, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*11*/{.colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*12*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*13*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 45, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*14*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*15*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*16*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*17*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*18*/{.colorbpp = 1, .maskbpp = 1, .mask_width = 1, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*19*/{.colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*20*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*21*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*22*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi =180, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*23*/{.colorbpp = 1, .maskbpp = 1, .mask_width = 1, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*24*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*25*/{.colorbpp = 1, .maskbpp = 1, .mask_width = 1, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*26*/{.colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*27*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*28*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*29*/{.colorbpp = 1, .maskbpp = 1, .mask_width = 1, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*30*/{.colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*31*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*32*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 90, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*33*/{.colorbpp = 1, .maskbpp = 1, .mask_width = 1, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*34*/{.colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*35*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*36*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*37*/{.colorbpp = 1, .maskbpp = 1, .mask_width = 1, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*38*/{.colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*39*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*40*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*41*/{.colorbpp = 1, .maskbpp = 1, .mask_width = 1, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*42*/{.colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*43*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*44*/{.colorbpp = 1, .maskbpp = 1, .mask_width = 1, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*45*/{.colorbpp = 2, .maskbpp = 1, .mask_width = 2, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*46*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 90, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*47*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 45, .ydpi = 45, .color_model = ROSPRITE_RGB },
/*48*/{.colorbpp = 4, .maskbpp = 1, .mask_width = 4, .xdpi = 45, .ydpi = 90, .color_model = ROSPRITE_RGB },
/*49*/{.colorbpp = 8, .maskbpp = 1, .mask_width = 8, .xdpi = 45, .ydpi = 90, .color_model = ROSPRITE_RGB }
};

/* table for converting a 5bit channel into an 8bit channel (used for 16bpp to 32bpp conversion) */
static const uint8_t sprite_16bpp_translate[] = {
	0x00, 0x08, 0x10, 0x18, 0x20, 0x29, 0x31, 0x39,
	0x41, 0x4a, 0x52, 0x5a, 0x62, 0x6a, 0x73, 0x7b,
	0x83, 0x8b, 0x94, 0x9c, 0xa4, 0xac, 0xb4, 0xbd,
	0xc5, 0xcd, 0xd5, 0xde, 0xe6, 0xee, 0xf6, 0xff
};

/* palettes generated with palette2c.c
 * which in turn requires rosprite_load_palette(FILE* f)
 * defined in this file
 */
static const uint32_t sprite_1bpp_palette[] = { 0xffffffff, 0xff };

static const uint32_t sprite_2bpp_palette[] = { 0xffffffff, 0xbbbbbbff, 0x777777ff, 0xff };

static const uint32_t sprite_4bpp_palette[] = {
0xffffffff, 0xddddddff, 0xbbbbbbff, 0x999999ff,
0x777777ff, 0x555555ff, 0x333333ff, 0xff,
0x4499ff, 0xeeee00ff, 0xcc00ff, 0xdd0000ff,
0xeeeebbff, 0x558800ff, 0xffbb00ff, 0xbbffff
};

static const uint32_t sprite_8bpp_palette[] = {
0xff, 0x111111ff, 0x222222ff, 0x333333ff,
0x440000ff, 0x551111ff, 0x662222ff, 0x773333ff,
0x44ff, 0x111155ff, 0x222266ff, 0x333377ff,
0x440044ff, 0x551155ff, 0x662266ff, 0x773377ff,
0x880000ff, 0x991111ff, 0xaa2222ff, 0xbb3333ff,
0xcc0000ff, 0xdd1111ff, 0xee2222ff, 0xff3333ff,
0x880044ff, 0x991155ff, 0xaa2266ff, 0xbb3377ff,
0xcc0044ff, 0xdd1155ff, 0xee2266ff, 0xff3377ff,
0x4400ff, 0x115511ff, 0x226622ff, 0x337733ff,
0x444400ff, 0x555511ff, 0x666622ff, 0x777733ff,
0x4444ff, 0x115555ff, 0x226666ff, 0x337777ff,
0x444444ff, 0x555555ff, 0x666666ff, 0x777777ff,
0x884400ff, 0x995511ff, 0xaa6622ff, 0xbb7733ff,
0xcc4400ff, 0xdd5511ff, 0xee6622ff, 0xff7733ff,
0x884444ff, 0x995555ff, 0xaa6666ff, 0xbb7777ff,
0xcc4444ff, 0xdd5555ff, 0xee6666ff, 0xff7777ff,
0x8800ff, 0x119911ff, 0x22aa22ff, 0x33bb33ff,
0x448800ff, 0x559911ff, 0x66aa22ff, 0x77bb33ff,
0x8844ff, 0x119955ff, 0x22aa66ff, 0x33bb77ff,
0x448844ff, 0x559955ff, 0x66aa66ff, 0x77bb77ff,
0x888800ff, 0x999911ff, 0xaaaa22ff, 0xbbbb33ff,
0xcc8800ff, 0xdd9911ff, 0xeeaa22ff, 0xffbb33ff,
0x888844ff, 0x999955ff, 0xaaaa66ff, 0xbbbb77ff,
0xcc8844ff, 0xdd9955ff, 0xeeaa66ff, 0xffbb77ff,
0xcc00ff, 0x11dd11ff, 0x22ee22ff, 0x33ff33ff,
0x44cc00ff, 0x55dd11ff, 0x66ee22ff, 0x77ff33ff,
0xcc44ff, 0x11dd55ff, 0x22ee66ff, 0x33ff77ff,
0x44cc44ff, 0x55dd55ff, 0x66ee66ff, 0x77ff77ff,
0x88cc00ff, 0x99dd11ff, 0xaaee22ff, 0xbbff33ff,
0xcccc00ff, 0xdddd11ff, 0xeeee22ff, 0xffff33ff,
0x88cc44ff, 0x99dd55ff, 0xaaee66ff, 0xbbff77ff,
0xcccc44ff, 0xdddd55ff, 0xeeee66ff, 0xffff77ff,
0x88ff, 0x111199ff, 0x2222aaff, 0x3333bbff,
0x440088ff, 0x551199ff, 0x6622aaff, 0x7733bbff,
0xccff, 0x1111ddff, 0x2222eeff, 0x3333ffff,
0x4400ccff, 0x5511ddff, 0x6622eeff, 0x7733ffff,
0x880088ff, 0x991199ff, 0xaa22aaff, 0xbb33bbff,
0xcc0088ff, 0xdd1199ff, 0xee22aaff, 0xff33bbff,
0x8800ccff, 0x9911ddff, 0xaa22eeff, 0xbb33ffff,
0xcc00ccff, 0xdd11ddff, 0xee22eeff, 0xff33ffff,
0x4488ff, 0x115599ff, 0x2266aaff, 0x3377bbff,
0x444488ff, 0x555599ff, 0x6666aaff, 0x7777bbff,
0x44ccff, 0x1155ddff, 0x2266eeff, 0x3377ffff,
0x4444ccff, 0x5555ddff, 0x6666eeff, 0x7777ffff,
0x884488ff, 0x995599ff, 0xaa66aaff, 0xbb77bbff,
0xcc4488ff, 0xdd5599ff, 0xee66aaff, 0xff77bbff,
0x8844ccff, 0x9955ddff, 0xaa66eeff, 0xbb77ffff,
0xcc44ccff, 0xdd55ddff, 0xee66eeff, 0xff77ffff,
0x8888ff, 0x119999ff, 0x22aaaaff, 0x33bbbbff,
0x448888ff, 0x559999ff, 0x66aaaaff, 0x77bbbbff,
0x88ccff, 0x1199ddff, 0x22aaeeff, 0x33bbffff,
0x4488ccff, 0x5599ddff, 0x66aaeeff, 0x77bbffff,
0x888888ff, 0x999999ff, 0xaaaaaaff, 0xbbbbbbff,
0xcc8888ff, 0xdd9999ff, 0xeeaaaaff, 0xffbbbbff,
0x8888ccff, 0x9999ddff, 0xaaaaeeff, 0xbbbbffff,
0xcc88ccff, 0xdd99ddff, 0xeeaaeeff, 0xffbbffff,
0xcc88ff, 0x11dd99ff, 0x22eeaaff, 0x33ffbbff,
0x44cc88ff, 0x55dd99ff, 0x66eeaaff, 0x77ffbbff,
0xccccff, 0x11ddddff, 0x22eeeeff, 0x33ffffff,
0x44ccccff, 0x55ddddff, 0x66eeeeff, 0x77ffffff,
0x88cc88ff, 0x99dd99ff, 0xaaeeaaff, 0xbbffbbff,
0xcccc88ff, 0xdddd99ff, 0xeeeeaaff, 0xffffbbff,
0x88ccccff, 0x99ddddff, 0xaaeeeeff, 0xbbffffff,
0xccccccff, 0xddddddff, 0xeeeeeeff, 0xffffffff
};

static inline rosprite_error rosprite_read_word(reader reader, void* ctx, uint32_t* result);
static rosprite_error rosprite_get_mode(uint32_t spriteMode, struct rosprite_mode* result);
static uint32_t rosprite_palette_lookup(struct rosprite* sprite, uint32_t pixel);
static inline uint32_t rosprite_cmyk_to_rgb(uint32_t cmyk);
static uint32_t rosprite_next_mask_pixel(uint8_t* mask, struct rosprite_mask_state* mask_state);
static rosprite_error rosprite_load_high_color(uint8_t* image_in, uint8_t* mask, struct rosprite* sprite, struct rosprite_header* header);
static rosprite_error rosprite_load_low_color(uint8_t* image_in, uint8_t* mask, struct rosprite* sprite, struct rosprite_header* header);
rosprite_error rosprite_load_sprite(reader reader, void* ctx, struct rosprite** result);
static rosprite_error rosprite_init_mask_state(struct rosprite* sprite, struct rosprite_header* header, uint8_t* mask, struct rosprite_mask_state** result);
static uint32_t rosprite_upscale_color(uint32_t pixel, struct rosprite_mode* mode, bool* has_alpha_pixel_data);
static inline void rosprite_fix_alpha(uint32_t* image, unsigned long pixels);

/**
 * Read exactly \a count bytes; a short read is a failure.
 * AUDIT-FIX R3: reader results were previously ignored or only loosely
 * compared, leaving buffers part filled.
 */
static bool rosprite_read_exact(reader reader, void* ctx, uint8_t* buf, uint32_t count)
{
	if (count == 0) return true;
	if (count > (uint32_t) INT32_MAX) return false; /* reader returns int */
	return reader(buf, count, ctx) == (int) count;
}

/**
 * Fetch a little-endian word from \a buf at \a *index and advance the index.
 * AUDIT-FIX R4/R5: returns \a fallback instead of reading out of bounds.
 */
static inline uint32_t rosprite_get_word(const uint8_t* buf, uint32_t size, uint32_t* index, uint32_t fallback)
{
	uint32_t word = fallback;
	if (buf != NULL && *index <= size && size - *index >= 4) {
		const uint8_t* b = buf + *index;
		word = BTUINT(b);
	}
	*index += 4;
	return word;
}

/** Free one sprite; safe on NULL and on a partly loaded sprite. */
static void rosprite_free_sprite(struct rosprite* sprite)
{
	if (sprite == NULL) return;
	free(sprite->palette);
	free(sprite->image);
	free(sprite);
}

rosprite_error rosprite_load(reader reader, void* ctx, struct rosprite_area** result)
{
	/* AUDIT-FIX R1: this function was rewritten.  Previously:
	 *  - no allocation was checked, so a failed malloc() was written through;
	 *  - the extension size was computed as "16 - firstSpriteOffset", which is
	 *    backwards (the extension area lies between the 16 byte area header
	 *    and the first sprite) and wraps to ~4GB for any offset above 16,
	 *    after which the NULL result of that 4GB malloc() was handed to the
	 *    reader (fuzzer crash: SEGV in fread);
	 *  - the pointer array was sized from the untrusted sprite count, so one
	 *    32-bit field in the file requested up to 32GB;
	 *  - every error return leaked the area and all sprites loaded so far.
	 */
	uint32_t firstSpriteOffset, firstFreeWordOffset, sprite_count;
	uint32_t capacity = 0;
	uint32_t i;
	rosprite_error err;
	struct rosprite_area* sprite_area;

	sprite_area = calloc(1, sizeof(struct rosprite_area));
	if (sprite_area == NULL) return ROSPRITE_NOMEM;

	err = rosprite_read_word(reader, ctx, &sprite_count);
	if (err != ROSPRITE_OK) goto fail;
	err = rosprite_read_word(reader, ctx, &firstSpriteOffset);
	if (err != ROSPRITE_OK) goto fail;
	err = rosprite_read_word(reader, ctx, &firstFreeWordOffset);
	if (err != ROSPRITE_OK) goto fail;
	(void) firstFreeWordOffset;

	if (firstSpriteOffset < 16 ||
	    firstSpriteOffset - 16 > ROSPRITE_MAX_EXTENSION) {
		err = ROSPRITE_BADMODE;
		goto fail;
	}
	sprite_area->extension_size = firstSpriteOffset - 16;

	if (sprite_area->extension_size > 0) {
		sprite_area->extension_words = malloc(sprite_area->extension_size);
		if (sprite_area->extension_words == NULL) {
			err = ROSPRITE_NOMEM;
			goto fail;
		}
		if (!rosprite_read_exact(reader, ctx,
				sprite_area->extension_words,
				sprite_area->extension_size)) {
			err = ROSPRITE_EOF;
			goto fail;
		}
	}

	/* Grow the pointer array as sprites actually arrive, so a bogus count
	 * simply runs into end-of-data instead of driving a huge allocation.
	 * sprite_area->sprite_count always equals the number of valid entries,
	 * which is what lets the failure path destroy a partial area. */
	for (i = 0; i < sprite_count; i++) {
		struct rosprite* sprite;

		if (i == capacity) {
			struct rosprite** grown;
			uint32_t newcap = (capacity == 0) ? 4 : capacity * 2;
			if (newcap > sprite_count || newcap < capacity)
				newcap = sprite_count;
			grown = realloc(sprite_area->sprites,
					sizeof(struct rosprite*) * (size_t) newcap);
			if (grown == NULL) {
				err = ROSPRITE_NOMEM;
				goto fail;
			}
			sprite_area->sprites = grown;
			capacity = newcap;
		}

		err = rosprite_load_sprite(reader, ctx, &sprite);
		if (err != ROSPRITE_OK) goto fail;

		sprite_area->sprites[i] = sprite;
		sprite_area->sprite_count = i + 1;
	}

	*result = sprite_area;

	return ROSPRITE_OK;

fail:
	rosprite_destroy_sprite_area(sprite_area);
	return err;
}

void rosprite_destroy_sprite_area(struct rosprite_area* sprite_area)
{
	uint32_t i;

	/* AUDIT-FIX R1: tolerate NULL and partially constructed areas so the
	 * loader's failure path can use this function. */
	if (sprite_area == NULL) return;

	for (i = 0; i < sprite_area->sprite_count; i++) {
		rosprite_free_sprite(sprite_area->sprites[i]);
	}

	free(sprite_area->sprites);
	free(sprite_area->extension_words);
	free(sprite_area);
}

rosprite_error rosprite_load_palette(reader reader, void* ctx, struct rosprite_palette** result)
{
	uint32_t c;
	uint8_t b[6];
	int bytesRead;

	/* TODO: currently assume palette has linear entries (2nd byte in is 00, 01, 02 etc) */
	struct rosprite_palette* palette = malloc(sizeof(struct rosprite_palette));
	if (palette == NULL) return ROSPRITE_NOMEM; /* AUDIT-FIX R2: unchecked */

	palette->palette = malloc(sizeof(uint32_t) * 256); /* allocate 256 whether we need them all or not */
	if (palette->palette == NULL) {
		free(palette);
		return ROSPRITE_NOMEM;
	}

	c = 0;

	/* AUDIT-FIX R2: the format checks here were assert()s, which vanish in
	 * a release (NDEBUG) build and abort the host application otherwise.
	 * In particular "assert(c <= 256)" came after the store, so the 257th
	 * logical colour in a file wrote past the 256 entry table.  They are
	 * now real error returns and the bound is tested before the write. */
	bytesRead = reader(b, 6, ctx);
	while (bytesRead == 6) {
		if (b[0] != 19) { /* VDU 19 */
			rosprite_destroy_palette(palette);
			return ROSPRITE_BADMODE;
		}

		/* only process logical colours */
		if (b[2] == 16) {
			uint32_t entry;

			if (c >= 256) {
				rosprite_destroy_palette(palette);
				return ROSPRITE_BADMODE;
			}

			entry = ((uint32_t) b[3] << 24) | (b[4] << 16) | (b[5] << 8) | 0xff; /* last byte is alpha */
			palette->palette[c] = entry;

			c++;
		}

		bytesRead = reader(b, 6, ctx);
	}

	if (bytesRead != 0) { /* trailing partial record */
		rosprite_destroy_palette(palette);
		return ROSPRITE_EOF;
	}

	palette->size = c;

	*result = palette;

	return ROSPRITE_OK;
}

void rosprite_destroy_palette(struct rosprite_palette* palette)
{
	free(palette->palette);
	free(palette);
}

rosprite_error rosprite_create_file_context(FILE* f, struct rosprite_file_context** result)
{
	struct rosprite_file_context* ctx = malloc(sizeof(struct rosprite_file_context));
	if (!ctx) return ROSPRITE_NOMEM;
	ctx->f = f;
	*result = ctx;

	return ROSPRITE_OK;
}

void rosprite_destroy_file_context(struct rosprite_file_context* ctx)
{
	free(ctx);
}

int rosprite_file_reader(uint8_t* buf, size_t count, void* ctx)
{
	return fread(buf, 1 /*size*/, count, ((struct rosprite_file_context*) ctx)->f);
}

rosprite_error rosprite_create_mem_context(uint8_t* p, unsigned long total_size, struct rosprite_mem_context** result)
{
	struct rosprite_mem_context* ctx = malloc(sizeof(struct rosprite_mem_context));
	if (!ctx) return ROSPRITE_NOMEM;
 
	ctx->base = p;
	ctx->offset = 0;
	ctx->size = total_size;
	*result = ctx;
 
	return ROSPRITE_OK;
}

void rosprite_destroy_mem_context(struct rosprite_mem_context* ctx)
{
	free(ctx);
}

int rosprite_mem_reader(uint8_t* buf, size_t count, void* ctx)
{
	size_t copy_size;
	struct rosprite_mem_context* memctx = ctx;

	/* if we're asked for more memory than the block contains,
	 * only copy as much as we can 
	 */
	if ((memctx->offset + count) > memctx->size) {
		copy_size = memctx->size - memctx->offset;
	} else {
		copy_size = count;
	}
	memcpy(buf, memctx->base + memctx->offset, copy_size);
	memctx->offset += copy_size;
	return copy_size;
}

/**
 * Load a single sprite.
 *
 * \param[out] result
 */
rosprite_error rosprite_load_sprite(reader reader, void* ctx, struct rosprite** result)
{
	/* AUDIT-FIX R3: this function was rewritten.  Every header field comes
	 * from the file, yet the only validation was a set of assert()s (absent
	 * in release builds, a remote abort in debug builds).  With them gone:
	 *  - width/height/offset arithmetic wrapped in 32 bits, so the output
	 *    buffer could be far smaller than the pixel loops assumed;
	 *  - image and mask buffers were sized from unrelated offsets and then
	 *    read by loops driven by width_words/height (heap over-reads and,
	 *    via the output index, heap over-writes - 8 distinct sanitizer
	 *    crashes from the fuzzer);
	 *  - short reads were ignored, leaving buffers uninitialised;
	 *  - no malloc() was checked and all error paths leaked.
	 * All fields are now validated before anything is allocated from them. */
	uint32_t nextSpriteOffset;
	uint32_t imageOffset;
	uint32_t maskOffset, spriteModeWord;
	uint32_t paletteEntries;
	int64_t row_bits;
	uint64_t image_bytes;
	uint8_t* image = NULL;
	uint8_t* mask = NULL;
	rosprite_error err;

	struct rosprite* sprite = calloc(1, sizeof(struct rosprite));
	struct rosprite_header* header = calloc(1, sizeof(struct rosprite_header));
	if (sprite == NULL || header == NULL) {
		err = ROSPRITE_NOMEM;
		goto fail;
	}

#define ROSPRITE_RD(dst) do { \
	err = rosprite_read_word(reader, ctx, (dst)); \
	if (err != ROSPRITE_OK) goto fail; \
} while (0)

	ROSPRITE_RD(&nextSpriteOffset);

	if (!rosprite_read_exact(reader, ctx, sprite->name, 12)) {
		err = ROSPRITE_EOF;
		goto fail;
	}
	sprite->name[12] = '\0';

	ROSPRITE_RD(&header->width_words); /* file has width - 1 and height - 1 */
	ROSPRITE_RD(&sprite->height);
	ROSPRITE_RD(&header->first_used_bit); /* old format only (spriteType = 0) */
	ROSPRITE_RD(&header->last_used_bit);
	ROSPRITE_RD(&imageOffset);
	ROSPRITE_RD(&maskOffset);
	ROSPRITE_RD(&spriteModeWord);
#undef ROSPRITE_RD

	err = rosprite_get_mode(spriteModeWord, &(sprite->mode));
	if (err != ROSPRITE_OK) goto fail;

	/* Everything below this point that fails is a malformed sprite */
	err = ROSPRITE_BADMODE;

	/* dimensions (checked before the +1 so that cannot wrap) */
	if (header->width_words >= ROSPRITE_MAX_PIXELS ||
	    sprite->height >= ROSPRITE_MAX_PIXELS)
		goto fail;
	header->width_words += 1;
	sprite->height += 1;

	if (header->first_used_bit > 31 || header->last_used_bit > 31)
		goto fail;
	if ((header->last_used_bit + 1) % sprite->mode.colorbpp != 0)
		goto fail;

	/* TODO left-hand wastage */
	row_bits = (int64_t) header->width_words * 32
			- header->first_used_bit
			- (31 - header->last_used_bit);
	if (row_bits < (int64_t) sprite->mode.colorbpp)
		goto fail;
	sprite->width = (uint32_t) (row_bits / sprite->mode.colorbpp);

	if ((uint64_t) sprite->width * sprite->height > ROSPRITE_MAX_PIXELS)
		goto fail;
	image_bytes = (uint64_t) header->width_words * 4 * sprite->height;
	if (image_bytes > (uint64_t) INT32_MAX)
		goto fail;

	/* offsets: header | palette | image | mask | next sprite */
	if (nextSpriteOffset < ROSPRITE_HEADER_SIZE ||
	    imageOffset < ROSPRITE_HEADER_SIZE ||
	    imageOffset > nextSpriteOffset ||
	    maskOffset < imageOffset ||
	    maskOffset > nextSpriteOffset)
		goto fail;

	sprite->palettesize     = imageOffset - ROSPRITE_HEADER_SIZE;
	sprite->has_palette     = (sprite->palettesize > 0);
	if (sprite->palettesize % 8 != 0 || sprite->palettesize > 256 * 8)
		goto fail;

	/* sprite has no mask if imageOffset == maskOffset */
	if (imageOffset == maskOffset) {
		sprite->has_mask   = false;
		header->image_size = nextSpriteOffset - imageOffset;
		header->mask_size  = 0;
	} else {
		sprite->has_mask   = true;
		header->image_size = maskOffset - imageOffset;
		header->mask_size  = nextSpriteOffset - maskOffset;
	}

	/* The image must be exactly width_words * 4 * height bytes (this was
	 * an assert); the mask is at most 8 bits per pixel. */
	if ((uint64_t) header->image_size != image_bytes)
		goto fail;
	if (sprite->has_mask &&
	    (header->mask_size < 4 ||
	     (uint64_t) header->mask_size > image_bytes * 8 + 8 ||
	     header->mask_size > (uint32_t) INT32_MAX))
		goto fail;

	if (sprite->has_palette) {
		uint32_t j, word1, word2, entry;
		sprite->palette = malloc(sizeof(uint32_t) * sprite->palettesize);
		if (sprite->palette == NULL) {
			err = ROSPRITE_NOMEM;
			goto fail;
		}
		paletteEntries = sprite->palettesize / 8;

		/* Each palette entry is two words big
		 * The second word is a duplicate of the first
		 * I think this is in case you ever wanted flashing colours
		 * PRM1-730
		 */
		for (j = 0; j < paletteEntries; j++) {
			err = rosprite_read_word(reader, ctx, &word1);
			if (err != ROSPRITE_OK) goto fail;
			err = rosprite_read_word(reader, ctx, &word2);
			if (err != ROSPRITE_OK) goto fail;
			/* if they aren't equal, flashing colours are desired,
			 * which we don't support: the first colour is used
			 * (this was an assert) */

			/* swap rr and bb parts -- PRM1-731 */
			entry = ((word1 & 0xff000000) >> 16) | (word1 & 0x00ff0000) | ((word1 & 0x0000ff00) << 16) | 0xff;
			sprite->palette[j] = entry;
		}
	}

	image = malloc(header->image_size);
	if (image == NULL) {
		err = ROSPRITE_NOMEM;
		goto fail;
	}
	if (!rosprite_read_exact(reader, ctx, image, header->image_size)) {
		err = ROSPRITE_EOF;
		goto fail;
	}

	if (sprite->has_mask) {
		mask = malloc(header->mask_size);
		if (mask == NULL) {
			err = ROSPRITE_NOMEM;
			goto fail;
		}
		if (!rosprite_read_exact(reader, ctx, mask, header->mask_size)) {
			err = ROSPRITE_EOF;
			goto fail;
		}
	}

	if (sprite->mode.colorbpp > 8) {
		err = rosprite_load_high_color(image, mask, sprite, header);
	} else {
		err = rosprite_load_low_color(image, mask, sprite, header);
	}
	if (err != ROSPRITE_OK) goto fail;

	free(image);
	free(mask);
	free(header);

	*result = sprite;

	return ROSPRITE_OK;

fail:
	free(image);
	free(mask);
	free(header);
	rosprite_free_sprite(sprite);
	return err;
}

/**
 * Determine the sprite_mode for the specified sprite_mode_word.
 *
 * \param[out] result
 */
static rosprite_error rosprite_get_mode(uint32_t sprite_mode_word, struct rosprite_mode* result)
{
	struct rosprite_mode mode;

	uint32_t spriteType = (sprite_mode_word & (15 << 27)) >> 27; /* preserve bits 27-30 only */

	if (spriteType != 0) {
		bool hasEightBitAlpha = sprite_mode_word >> 31; /* bit 31 */
		/* new modes have 1bpp masks (PRM5a-111)
		 * unless bit 31 is set (http://select.riscos.com/prm/graphics/sprites/alphachannel.html)
		 */
		mode.maskbpp = (hasEightBitAlpha ? 8 : 1);
		mode.mask_width = mode.maskbpp;
		mode.xdpi = (sprite_mode_word & (8191 << 14)) >> 14; /* preserve bits 14-26 only */
		mode.ydpi = (sprite_mode_word & (8191 << 1)) >> 1; /* preserve bits 1-13 only */

		mode.color_model = ROSPRITE_RGB;
		switch (spriteType) {
		case 1:
			mode.colorbpp = 1; break;
		case 2:
			mode.colorbpp = 2; break;
		case 3:
			mode.colorbpp = 4; break;
		case 4:
			mode.colorbpp = 8; break;
		case 5:
			mode.colorbpp = 16; break;
		case 6:
			mode.colorbpp = 32; break;
		case 7:
			mode.colorbpp = 32;
			mode.color_model = ROSPRITE_CMYK;
			break;
		case 8:
			mode.colorbpp = 24; break;
		default:
			return ROSPRITE_BADMODE;
		}
	} else {
		/* AUDIT-FIX R6: the mode number indexes a fixed table.  The only
		 * guard was an assert() against 256, which is larger than the
		 * table and compiled out in release builds, so any mode word
		 * read from the file indexed arbitrary memory (fuzzer crash:
		 * SEGV in rosprite_get_mode). */
		if (sprite_mode_word >= sizeof(oldmodes) / sizeof(oldmodes[0])) {
			return ROSPRITE_BADMODE;
		}

		mode = oldmodes[sprite_mode_word];
	}

	/* illegal mode check */
	if ((mode.colorbpp == 0) || (mode.xdpi == 0) || (mode.ydpi == 0)) {
		return ROSPRITE_BADMODE;
	}

	memcpy(result, &mode, sizeof(struct rosprite_mode));

	return ROSPRITE_OK;
}

/**
 * Load a sprite image with 16 or more bpp.
 *
 * \param[out] sprite On exit, sprite.image will be populated
 */
static rosprite_error rosprite_load_high_color(uint8_t* image_in, uint8_t* mask, struct rosprite* sprite, struct rosprite_header* header)
{
	struct rosprite_mask_state* mask_state = NULL;
	uint32_t currentByteIndex = 0;
	uint32_t j, x, y, x_pixels, pixel;
	bool has_alpha_pixel_data = false;
	uint32_t b;
	bool old_has_alpha;
	rosprite_error err = ROSPRITE_OK;

	/* Spec says that there must be no left-hand wastage.
	 * AUDIT-FIX R4: was an assert(); now a format error. */
	if (header->first_used_bit != 0) return ROSPRITE_BADMODE;

	if (sprite->has_mask) {
		ERRCHK(rosprite_init_mask_state(sprite, header, mask, &mask_state));
	}

	/* all image data is 32bpp going out.
	 * AUDIT-FIX R4: the size was computed in 32 bits (it wrapped) and the
	 * result was not checked.  The caller has bounded width * height, the
	 * multiplication is done in size_t, and calloc() means a pixel the
	 * loop does not reach is transparent black, not heap contents. */
	sprite->image = calloc((size_t) sprite->width * sprite->height, 4);
	if (sprite->image == NULL) {
		free(mask_state);
		return ROSPRITE_NOMEM;
	}

	{
	const uint32_t bpp = sprite->mode.colorbpp;
	const uint32_t bytesPerPixel = bpp / 8;
	const uint32_t row_max_bit = header->width_words * 32 - (31 - header->last_used_bit); /* Last used bit in row */
	const uint32_t stride = header->width_words * 4; /* bytes per input row */

	for (y = 0; y < sprite->height; y++) {
		/* AUDIT-FIX R4: start every row at its real offset instead of
		 * rounding a running index, so a row can never drift */
		currentByteIndex = y * stride;
		x_pixels = 0;
		/* AUDIT-FIX R4: also stop at the computed width.  For 24bpp
		 * row_max_bit is not a multiple of the pixel size, so this
		 * loop produced one more pixel per row than was allocated. */
		for (x = 0; x < row_max_bit && x_pixels < sprite->width; x += bpp) {
			/* AUDIT-FIX R4: never read past the input buffer */
			if (currentByteIndex + bytesPerPixel > header->image_size) {
				err = ROSPRITE_EOF;
				goto out;
			}
			pixel = 0;
			for (j = 0; j < bytesPerPixel; j++) {
				b = image_in[currentByteIndex++];
				pixel = pixel | (b << (j * 8));
			}

			old_has_alpha = has_alpha_pixel_data;
			pixel = rosprite_upscale_color(pixel, &(sprite->mode), &has_alpha_pixel_data);
			if (old_has_alpha != has_alpha_pixel_data && (y > 0 || x_pixels > 0)) {
				rosprite_fix_alpha(sprite->image, (y * sprite->width) + x_pixels - 1);
			}
			if (sprite->has_mask) {
				uint8_t mask_pixel = rosprite_next_mask_pixel(mask, mask_state);
				pixel = (pixel & 0xffffff00) | mask_pixel;
			}
			sprite->image[y*sprite->width + x_pixels] = pixel;
			x_pixels++;
		}
	}
	}
out:
	free(mask_state);
	return err;
}

/**
 * Iterate over the specified number of pixels, starting at the image pointer,
 * and set the alpha channel to 0x00.
 */
static inline void rosprite_fix_alpha(uint32_t* image, unsigned long pixels)
{
	uint32_t i;
	for (i = 0; i <= pixels; i++) {
		image[i] = image[i] & 0xffffff00;
	}
}

/**
 * Load a sprite image with 8 or fewer bpp.
 *
 * \param[out] sprite On exit, sprite.image will be populated
 */
static rosprite_error rosprite_load_low_color(uint8_t* image_in, uint8_t* mask, struct rosprite* sprite, struct rosprite_header* header)
{
	uint32_t current_byte_index, currentword;
	uint32_t x, y, x_pixels, pixel;
	uint8_t mask_pixel;

	struct rosprite_mask_state* mask_state = NULL;
	if (sprite->has_mask) {
		ERRCHK(rosprite_init_mask_state(sprite, header, mask, &mask_state));
	}

	/* all image data is 32bpp going out (AUDIT-FIX R4: see high colour) */
	sprite->image = calloc((size_t) sprite->width * sprite->height, 4);
	if (sprite->image == NULL) {
		free(mask_state);
		return ROSPRITE_NOMEM;
	}

	{
	const uint32_t bpp = sprite->mode.colorbpp;
	const uint32_t row_max_bit = header->width_words * 32 - (31 - header->last_used_bit); /* Last used bit in row */
	const uint32_t bitmask = (1 << bpp) - 1; /* creates a mask of 1s that is bpp bits wide */
	const uint32_t stride = header->width_words * 4; /* bytes per input row */

	for (y = 0; y < sprite->height; y++) {
		/* AUDIT-FIX R4: each row starts at its own offset and every
		 * word fetch is bounds checked against the input size */
		current_byte_index = y * stride;
		currentword = rosprite_get_word(image_in, header->image_size, &current_byte_index, 0);

		x_pixels = 0;
		/* AUDIT-FIX R4: bound the output index by the computed width */
		for (x = header->first_used_bit; x < row_max_bit && x_pixels < sprite->width; x += bpp) {
			const uint32_t offset_into_word = x % 32;

			pixel = (currentword & (bitmask << offset_into_word)) >> offset_into_word;
			pixel = rosprite_palette_lookup(sprite, pixel); /* lookup returns 32bpp */
			if (sprite->has_mask) {
				mask_pixel = rosprite_next_mask_pixel(mask, mask_state);
				pixel = (pixel & 0xffffff00) | mask_pixel;
			}
			sprite->image[y*sprite->width + x_pixels] = pixel;
			x_pixels++;

			/* If we're not at the end of the row and we've processed all of this word, fetch the next one */
			if (x + bpp < row_max_bit && offset_into_word + bpp == 32) {
				currentword = rosprite_get_word(image_in, header->image_size, &current_byte_index, 0);
			}
		}
	}
	}
	free(mask_state);

	return ROSPRITE_OK;
}

static uint32_t rosprite_palette_lookup(struct rosprite* sprite, uint32_t pixel)
{
	uint32_t translated_pixel;
	 /* because we're dealing with 8bpp or less */
	if (sprite->has_palette) {

		if(pixel < (sprite->palettesize / 8)) {
			translated_pixel = sprite->palette[pixel];
		} else {
			translated_pixel = sprite_8bpp_palette[pixel];
		}
	} else {
		switch (sprite->mode.colorbpp) {
		case 8:
			assert(pixel < 256);
			translated_pixel = sprite_8bpp_palette[pixel];
			break;
		case 4:
			assert(pixel < 16);
			translated_pixel = sprite_4bpp_palette[pixel];
			break;
		case 2:
			assert(pixel < 4);
			translated_pixel = sprite_2bpp_palette[pixel];
			break;
		case 1:
			assert(pixel < 2);
			translated_pixel = sprite_1bpp_palette[pixel];
			break;
		default:
			translated_pixel = 0;
			assert(false);
		}
	}
	return translated_pixel;
}

static rosprite_error rosprite_init_mask_state(struct rosprite* sprite, struct rosprite_header* header, uint8_t* mask, struct rosprite_mask_state** result)
{
	struct rosprite_mask_state* mask_state = malloc(sizeof(struct rosprite_mask_state));
	if (!mask_state) return ROSPRITE_NOMEM;

	mask_state->x = header->first_used_bit;
	mask_state->y = 0;
	mask_state->first_used_bit = header->first_used_bit;
	mask_state->row_max_bit = sprite->width * sprite->mode.mask_width;
	mask_state->height = sprite->height;
	mask_state->bpp = sprite->mode.mask_width;
	/* AUDIT-FIX R5: remember how much mask data exists; the first word and
	 * every later one used to be read with no bounds check at all */
	mask_state->mask_size = header->mask_size;
	mask_state->current_byte_index = 0;
	mask_state->current_word = rosprite_get_word(mask, mask_state->mask_size, &mask_state->current_byte_index, 0xffffffff);

	*result = mask_state;

	return ROSPRITE_OK;
}

/**
 * Get the next mask byte.
 * A mask of 0xff denotes 100% opaque, 0x00 denotes 100% transparent.
 * 
 * \param[in,out] mask_state
 */
static uint32_t rosprite_next_mask_pixel(uint8_t* mask, struct rosprite_mask_state* mask_state)
{
	/* a 1bpp mask (for new mode sprites), each row is word aligned (therefore potential righthand wastage */
	const uint32_t bitmask = (1 << mask_state->bpp) - 1;
	const uint32_t offset_into_word = mask_state->x % 32;
	uint32_t pixel = (mask_state->current_word & (bitmask << offset_into_word)) >> offset_into_word;

	/* AUDIT-FIX R5: both word fetches below are now bounds checked.  A mask
	 * shorter than the image implies reads as fully opaque rather than as
	 * whatever follows the allocation. */
	if (mask_state->x + mask_state->bpp < mask_state->row_max_bit && offset_into_word + mask_state->bpp == 32) {
		mask_state->current_word = rosprite_get_word(mask, mask_state->mask_size, &mask_state->current_byte_index, 0xffffffff);
	}

	mask_state->x += mask_state->bpp;
	if (mask_state->x >= mask_state->row_max_bit) {
		mask_state->x = mask_state->first_used_bit;

		if (mask_state->y + 1 < mask_state->height) {
			mask_state->current_word = rosprite_get_word(mask, mask_state->mask_size, &mask_state->current_byte_index, 0xffffffff);
		}

		mask_state->y++;
	}

	/*
	 * if the mask is 1bpp, make sure the whole byte is 0x00 or 0xff
	 */
	if (mask_state->bpp < 8) {
		pixel = (pixel & 1) ? 0xff : 0x00;
	}

	return pixel;
}

/**
 * Upscale a 16, 24 or 32bpp sprite to the 0xRRGGBBAA representation.
 * Do not call this function with a sprite with less than 16bpp,
 * but use a palette lookup instead.
 */
static uint32_t rosprite_upscale_color(uint32_t pixel, struct rosprite_mode* mode, bool* has_alpha_pixel_data)
{
	uint8_t alpha;
	switch (mode->colorbpp) {
	case 32:
		if (mode->color_model == ROSPRITE_RGB) {
			/* swap from 0xAABBGGRR to 0xRRGGBBAA */
			pixel = BSWAP(pixel);
		} else {
			pixel = rosprite_cmyk_to_rgb(pixel);
		}
		break;
	case 24:
		/* reverse byte order */
		pixel = BSWAP(pixel);
		break;
	case 16:
		/* incoming format is b_00000000000000000bbbbbgggggrrrrr */
		{
			uint8_t red   = pixel & 31;
			uint8_t green = (pixel & (31 << 5)) >> 5;
			uint8_t blue  = (pixel & (31 << 10)) >> 10;

			/* sanity check */
			assert(red < 32);
			assert(green < 32);
			assert(blue < 32);

			pixel = ((uint32_t)sprite_16bpp_translate[red]   << 24)|
			        ((uint32_t)sprite_16bpp_translate[green] << 16)|
			        ((uint32_t)sprite_16bpp_translate[blue]  <<  8);
		}
		break;
	case 8:
	case 4:
	case 2:
	case 1:
		assert(false); /* shouldn't need to call for <= 8bpp, since a palette lookup will return 32bpp */
	default:
		assert(false); /* unknown bpp */
	}

	alpha = pixel & 0xff;
	if (alpha == 0x00) {
		if (!(*has_alpha_pixel_data)) {
			pixel = pixel | 0xff;
		}
	} else {
		*has_alpha_pixel_data = true;
	}
	return pixel;
}

static inline uint32_t rosprite_cmyk_to_rgb(uint32_t cmyk)
{
        uint8_t c = cmyk & 0xff;
        uint8_t m = (cmyk & 0xff00) >> 8;
        uint8_t y = (cmyk & 0xff0000) >> 16;
        uint8_t k = cmyk >> 24;
 
        /* Convert to CMY colourspace */
        uint8_t C = c + k;
        uint8_t M = m + k;
        uint8_t Y = y + k;
 
        /* And to RGB */
        uint8_t r = 255 - C;
        uint8_t g = 255 - M;
        uint8_t b = 255 - Y;
 
        /* AUDIT-FIX R9: "r << 24" shifted a promoted (signed) int into the
         * sign bit, which is undefined behaviour for r >= 128 */
        return ((uint32_t) r << 24) | ((uint32_t) g << 16) | ((uint32_t) b << 8);
}

static inline rosprite_error rosprite_read_word(reader reader, void* ctx, uint32_t* result)
{
	unsigned char b[4];
	if (reader(b, 4, ctx) < 4) {
		return ROSPRITE_EOF;
	}
	*result = BTUINT(b);
	return ROSPRITE_OK;
}
