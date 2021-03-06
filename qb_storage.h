/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Chung Leong <cleong@cal.berkeley.edu>                        |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef QB_STORAGE_H_
#define QB_STORAGE_H_

typedef struct qb_memory_segment			qb_memory_segment;
typedef struct qb_storage					qb_storage;
typedef struct qb_dimension_mappings		qb_dimension_mappings;

enum {
	QB_SEGMENT_PREALLOCATED			= 0x00000001,
	QB_SEGMENT_SEPARATE_ON_FORK		= 0x00000002,
	QB_SEGMENT_SEPARATE_ON_REENTRY	= 0x00000004,
	QB_SEGMENT_CLEAR_ON_CALL		= 0x00000008,
	QB_SEGMENT_REALLOCATE_ON_CALL	= 0x00000010,
	QB_SEGMENT_FREE_ON_RETURN		= 0x00000020,
	QB_SEGMENT_EMPTY_ON_RETURN		= 0x00000040,

	QB_SEGMENT_BORROWED				= 0x00000100,
	QB_SEGMENT_MAPPED				= 0x00000200,
	QB_SEGMENT_IMPORTED				= 0x00000400,
};

struct qb_memory_segment {
	int8_t *memory;
	uint32_t flags;
	uint32_t byte_count;						// number of bytes in this segment
	uint32_t current_allocation;				// number of bytes allocated
	php_stream *stream;							// memory-mapped file
	qb_memory_segment *imported_segment;		// imported segment
	qb_memory_segment *next_dependent;
	uintptr_t **references;
	uint32_t reference_count;
};

enum {
	// constant scalar (no separation on fork, no separation on reentry, no clearing on call)
	QB_SELECTOR_CONSTANT_SCALAR		= 0,
	// static scalars (no separation on fork, no separation on reentry, no clearing on call)
	QB_SELECTOR_STATIC_SCALAR		= 1,
	// shared scalars (no separation on fork, separation on reentry, clearing on call) 
	QB_SELECTOR_SHARED_SCALAR		= 2,
	// local scalars (separation on fork, separation on reentry, clearing on call)
	QB_SELECTOR_LOCAL_SCALAR		= 3,
	// temporary scalars (seperation on fork, separation on reentry, no clearing on call)
	QB_SELECTOR_TEMPORARY_SCALAR	= 4,

	// constant arrays (no separation on fork, no separation on reentry, no clearing on call)
	QB_SELECTOR_CONSTANT_ARRAY		= 9,
	// static arrays (no separation on fork, no separation on reentry, no clearing on call)
	QB_SELECTOR_STATIC_ARRAY		= 8,
	// shared fixed-length arrays (no separation on fork, separation on reentry, clearing on call) 
	QB_SELECTOR_SHARED_ARRAY		= 7,
	// local fixed-length arrays (separation on fork, separation on reentry, clearing on call)
	QB_SELECTOR_LOCAL_ARRAY			= 6,
	// temporary fixed-length arrays (seperation on fork, separation on reentry, no clearing on call)
	QB_SELECTOR_TEMPORARY_ARRAY		= 5,

	// note how the order is reverse for the array segments
	// this is done so that the segments requiring separation are back-to-back,
	// maing it easier to see if a given pointer requires relocation or not
	//
	// the arrangement also brings variables likely to be active closer together
	//
	// segments that need to be cleared when the function is called are also placed 
	// back-to-back, so we only need two memset() to wipe all variables clean
	//
	// parameters are found in the shared segments; separation is needed when 
	// we reenter the function

	QB_SELECTOR_FIRST_PREALLOCATED	= QB_SELECTOR_CONSTANT_SCALAR,
	QB_SELECTOR_LAST_PREALLOCATED	= QB_SELECTOR_CONSTANT_ARRAY,

	// global variables
	QB_SELECTOR_GLOBAL_SCALAR		= 10,
	QB_SELECTOR_GLOBAL_ARRAY		= 11,

	// class (static) variables
	QB_SELECTOR_CLASS_SCALAR		= 12,
	QB_SELECTOR_CLASS_ARRAY			= 13,

	// object instance variables 
	QB_SELECTOR_OBJECT_SCALAR		= 14,
	QB_SELECTOR_OBJECT_ARRAY		= 15,

	// variable length arrays are stored in segment 16 and above
	QB_SELECTOR_ARRAY_START			= 16,

	QB_SELECTOR_INVALID 			= 0xFFFFFFFF,
};

enum {
	QB_OFFSET_INVALID 				= 0xFFFFFFFF,
};

struct qb_storage {
	uint32_t size;
	qb_memory_segment *segments;
	uint32_t segment_count;
	uint32_t flags;
};

enum {
	QB_TRANSFER_CAN_BORROW_MEMORY	= 0x0001,
	QB_TRANSFER_CAN_SEIZE_MEMORY	= 0x0002,
	QB_TRANSFER_CAN_ENLARGE_SEGMENT	= 0x0004,
	QB_TRANSFER_CAN_AUTOVIVIFICATE	= 0x0008,
};

struct qb_dimension_mappings {
	uint32_t dst_dimension_count;
	uint32_t dst_dimensions[MAX_DIMENSION];
	uint32_t dst_array_sizes[MAX_DIMENSION];
	qb_primitive_type dst_element_type;
	uint32_t dst_address_flags;
	qb_index_alias_scheme *dst_index_alias_schemes[MAX_DIMENSION];
	uint32_t src_dimension_count;
	uint32_t src_dimensions[MAX_DIMENSION];
	uint32_t src_array_sizes[MAX_DIMENSION];
	qb_primitive_type src_element_type;
	uint32_t src_address_flags;
	qb_index_alias_scheme *src_index_alias_schemes[MAX_DIMENSION];
};

#define gdTrueColorAlpha(r, g, b, a) (((a) << 24) + \
	((r) << 16) + \
	((g) << 8) + \
	(b))

#define gdMaxColors 256

#define gdAlphaMax 127
#define gdAlphaOpaque 0
#define gdAlphaTransparent 127
#define gdRedMax 255
#define gdGreenMax 255
#define gdBlueMax 255
#define gdTrueColorGetAlpha(c) (((c) & 0x7F000000) >> 24)
#define gdTrueColorGetRed(c) (((c) & 0xFF0000) >> 16)
#define gdTrueColorGetGreen(c) (((c) & 0x00FF00) >> 8)
#define gdTrueColorGetBlue(c) ((c) & 0x0000FF)

#define gdImagePalettePixel(im, x, y) (im)->pixels[(y)][(x)]
#define gdImageTrueColorPixel(im, x, y) (im)->tpixels[(y)][(x)]

typedef enum {
	GD_DEFAULT          = 0,
	GD_BELL,
	GD_BESSEL,
	GD_BILINEAR_FIXED,
	GD_BICUBIC,
	GD_BICUBIC_FIXED,
	GD_BLACKMAN,
	GD_BOX,
	GD_BSPLINE,
	GD_CATMULLROM,
	GD_GAUSSIAN,
	GD_GENERALIZED_CUBIC,
	GD_HERMITE,
	GD_HAMMING,
	GD_HANNING,
	GD_MITCHELL,
	GD_NEAREST_NEIGHBOUR,
	GD_POWER,
	GD_QUADRATIC,
	GD_SINC,
	GD_TRIANGLE,
	GD_WEIGHTED4,
	GD_METHOD_COUNT = 21
} gdInterpolationMethod;

/* resolution affects ttf font rendering, particularly hinting */
#define GD_RESOLUTION           96      /* pixels per inch */

typedef double (* interpolation_method )(double);

typedef struct gdImageStruct {
	/* Palette-based image pixels */
	unsigned char ** pixels;
	int sx;
	int sy;
	/* These are valid in palette images only. See also
		'alpha', which appears later in the structure to
		preserve binary backwards compatibility */
	int colorsTotal;
	int red[gdMaxColors];
	int green[gdMaxColors];
	int blue[gdMaxColors];
	int open[gdMaxColors];
	/* For backwards compatibility, this is set to the
		first palette entry with 100% transparency,
		and is also set and reset by the
		gdImageColorTransparent function. Newer
		applications can allocate palette entries
		with any desired level of transparency; however,
		bear in mind that many viewers, notably
		many web browsers, fail to implement
		full alpha channel for PNG and provide
		support for full opacity or transparency only. */
	int transparent;
	int *polyInts;
	int polyAllocated;
	struct gdImageStruct *brush;
	struct gdImageStruct *tile;
	int brushColorMap[gdMaxColors];
	int tileColorMap[gdMaxColors];
	int styleLength;
	int stylePos;
	int *style;
	int interlace;
	/* New in 2.0: thickness of line. Initialized to 1. */
	int thick;
	/* New in 2.0: alpha channel for palettes. Note that only
		Macintosh Internet Explorer and (possibly) Netscape 6
		really support multiple levels of transparency in
		palettes, to my knowledge, as of 2/15/01. Most
		common browsers will display 100% opaque and
		100% transparent correctly, and do something
		unpredictable and/or undesirable for levels
		in between. TBB */
	int alpha[gdMaxColors];
	/* Truecolor flag and pixels. New 2.0 fields appear here at the
		end to minimize breakage of existing object code. */
	int trueColor;
	int ** tpixels;
	/* Should alpha channel be copied, or applied, each time a
		pixel is drawn? This applies to truecolor images only.
		No attempt is made to alpha-blend in palette images,
		even if semitransparent palette entries exist.
		To do that, build your image as a truecolor image,
		then quantize down to 8 bits. */
}
gdImage;

typedef gdImage * gdImagePtr;

#define ARRAY_IN(storage, type, address)	((CTYPE(type) *) (storage->segments[address->segment_selector].memory + address->segment_offset))
#define VALUE_IN(storage, type, address)	*ARRAY_IN(storage, type, address)
#define ARRAY_SIZE_IN(storage, address)		VALUE_IN(storage, U32, address->array_size_address)

#define ARRAY(type, address)				ARRAY_IN(cxt->storage, type, address)
#define VALUE(type, address)				VALUE_IN(cxt->storage, type, address)
#define ARRAY_SIZE(address)					VALUE(U32, address->array_size_address)

gdImagePtr qb_get_gd_image(zval *resource);

void qb_copy_wrap_around(int8_t *memory, uint32_t filled_byte_count, uint32_t required_byte_count);
void qb_copy_elements(uint32_t source_type, int8_t *restrict source_memory, uint32_t source_count, uint32_t dest_type, int8_t *restrict dest_memory, uint32_t dest_count);
void qb_copy_element(uint32_t source_type, int8_t *restrict source_memory, uint32_t dest_type, int8_t *restrict dest_memory);

int32_t qb_transfer_value_from_zval(qb_storage *storage, qb_address *address, zval *zvalue, int32_t transfer_flags);
int32_t qb_transfer_value_from_storage_location(qb_storage *storage, qb_address *address, qb_storage *src_storage, qb_address *src_address, uint32_t transfer_flags);
int32_t qb_transfer_value_to_zval(qb_storage *storage, qb_address *address, zval *zvalue);
int32_t qb_transfer_value_to_storage_location(qb_storage *storage, qb_address *address, qb_storage *dst_storage, qb_address *dst_address);

void qb_allocate_segment_memory(qb_memory_segment *segment, uint32_t byte_count);
void qb_release_segment(qb_memory_segment *segment);
intptr_t qb_resize_segment(qb_memory_segment *segment, uint32_t new_size);

void qb_import_segment(qb_memory_segment *segment, qb_memory_segment *other_segment);

qb_storage * qb_create_storage_copy(qb_storage *base, intptr_t instruction_shift, int32_t reentrance);
void qb_copy_storage_contents(qb_storage *src_storage, qb_storage *dst_storage);

#endif
