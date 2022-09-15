/* ----------
 * pg_lzcompress.h -
 *
 *	Definitions for the builtin LZ compressor
 *
 * src/include/common/pg_lzcompress.h
 * ----------
 */

#ifndef _PG_LZCOMPRESS_H_
#define _PG_LZCOMPRESS_H_

/*
 * intN
 *		Signed integer, EXACTLY N BITS IN SIZE,
 *		used for numerical computations and the
 *		frontend/backend protocol.
 */
#ifndef HAVE_INT8
typedef signed char int8;		/* == 8 bits */
typedef signed short int16;		/* == 16 bits */
typedef signed int int32;		/* == 32 bits */
#endif							/* not HAVE_INT8 */

/*
 * uintN
 *		Unsigned integer, EXACTLY N BITS IN SIZE,
 *		used for numerical computations and the
 *		frontend/backend protocol.
 */
#ifndef HAVE_UINT8
typedef unsigned char uint8;	/* == 8 bits */
typedef unsigned short uint16;	/* == 16 bits */
typedef unsigned int uint32;	/* == 32 bits */
#endif							/* not HAVE_UINT8 */

/*
 * bitsN
 *		Unit of bitwise operation, AT LEAST N BITS IN SIZE.
 */
typedef uint8 bits8;			/* >= 8 bits */
typedef uint16 bits16;			/* >= 16 bits */
typedef uint32 bits32;			/* >= 32 bits */

#ifndef HAVE_INT64
typedef long long int int64;
#endif
#ifndef HAVE_UINT64
typedef unsigned long long int uint64;
#endif

#ifndef bool
typedef unsigned char bool;
#endif

#ifndef true
#define true	((bool) 1)
#endif

#ifndef false
#define false	((bool) 0)
#endif

/*
 * Min
 *		Return the minimum of two numbers.
 */
#define Min(x, y)		((x) < (y) ? (x) : (y))

/*
 * Hints to the compiler about the likelihood of a branch. Both likely() and
 * unlikely() return the boolean value of the contained expression.
 *
 * These should only be used sparingly, in very hot code paths. It's very easy
 * to mis-estimate likelihoods.
 */
#if __GNUC__ >= 3
#define likely(x)	__builtin_expect((x) != 0, 1)
#define unlikely(x) __builtin_expect((x) != 0, 0)
#else
#define likely(x)	((x) != 0)
#define unlikely(x) ((x) != 0)
#endif

/* ----------
 * PGLZ_MAX_OUTPUT -
 *
 *		Macro to compute the buffer size required by pglz_compress().
 *		We allow 4 bytes for overrun before detecting compression failure.
 * ----------
 */
#define PGLZ_MAX_OUTPUT(_dlen)			((_dlen) + 4)


/* ----------
 * PGLZ_Strategy -
 *
 *		Some values that control the compression algorithm.
 *
 *		min_input_size		Minimum input data size to consider compression.
 *
 *		max_input_size		Maximum input data size to consider compression.
 *
 *		min_comp_rate		Minimum compression rate (0-99%) to require.
 *							Regardless of min_comp_rate, the output must be
 *							smaller than the input, else we don't store
 *							compressed.
 *
 *		first_success_by	Abandon compression if we find no compressible
 *							data within the first this-many bytes.
 *
 *		match_size_good		The initial GOOD match size when starting history
 *							lookup. When looking up the history to find a
 *							match that could be expressed as a tag, the
 *							algorithm does not always walk back entirely.
 *							A good match fast is usually better than the
 *							best possible one very late. For each iteration
 *							in the lookup, this value is lowered so the
 *							longer the lookup takes, the smaller matches
 *							are considered good.
 *
 *		match_size_drop		The percentage by which match_size_good is lowered
 *							after each history check. Allowed values are
 *							0 (no change until end) to 100 (only check
 *							latest history entry at all).
 * ----------
 */
typedef struct PGLZ_Strategy
{
	int32		min_input_size;
	int32		max_input_size;
	int32		min_comp_rate;
	int32		first_success_by;
	int32		match_size_good;
	int32		match_size_drop;
} PGLZ_Strategy;


/* ----------
 * The standard strategies
 *
 *		PGLZ_strategy_default		Recommended default strategy for TOAST.
 *
 *		PGLZ_strategy_always		Try to compress inputs of any length.
 *									Fallback to uncompressed storage only if
 *									output would be larger than input.
 * ----------
 */
extern const PGLZ_Strategy *const PGLZ_strategy_default;
extern const PGLZ_Strategy *const PGLZ_strategy_always;


/* ----------
 * Global function declarations
 * ----------
 */
extern int32 pglz_compress(const char *source, int32 slen, char *dest,
						   const PGLZ_Strategy *strategy);
extern int32 pglz_decompress(const char *source, int32 slen, char *dest,
							 int32 rawsize, bool check_complete);
extern int32 pglz_maximum_compressed_size(int32 rawsize,
										  int32 total_compressed_size);

#endif							/* _PG_LZCOMPRESS_H_ */
