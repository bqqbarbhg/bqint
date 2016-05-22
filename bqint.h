
// -- Types

#include <stdint.h>
#include <stddef.h>

// Operations are done on double words, so default to 32-bit words only
// on 64-bit context. Otherwise use 32-bit operations (16-bit words).
// Never defaults to 8-bit words.
#ifndef BQINT_WORD_BITS
	#if INTPTR_MAX == INT64_MAX
		#define BQINT_WORD_BITS 32
	#else
		#define BQINT_WORD_BITS 16
	#endif
#endif

#if BQINT_WORD_BITS == 32
typedef uint32_t bqint_word;
typedef uint64_t bqint_dword;
#elif BQINT_WORD_BITS == 16
typedef uint16_t bqint_word;
typedef uint32_t bqint_dword;
#elif BQINT_WORD_BITS == 8
typedef uint8_t bqint_word;
typedef uint16_t bqint_dword;
#else
#error "Unsupported BQINT_WORD_BITS"
#endif

#ifdef BQINT_TYPE_BQINT_SIZE
typedef BQINT_TYPE_BQINT_SIZE bqint_size;
#else
typedef uint32_t bqint_size;
#endif

#ifdef BQINT_TYPE_BQINT_FLAGS
typedef BQINT_TYPE_BQINT_FLAGS bqint_flags;
#else
typedef uint32_t bqint_flags;
#endif

#ifndef BQINT_ASSERT
#include <assert.h>
#define BQINT_ASSERT(x) assert(x)
#endif

#ifndef BQINT_ASSERT_FLAGS
#define BQINT_ASSERT_FLAGS (BQINT_ERROR)
#endif

#define BQINT_MAX_WORDS (~(bqint_size)0)

#define BQINT_ASSERT_FLAG_SET(flag) BQINT_ASSERT(!((flag) & BQINT_ASSERT_FLAGS))

// -- Flags

enum
{
	BQINT_NEGATIVE = 1 << 0,
	BQINT_STATIC = 1 << 1,
	BQINT_ALLOCATED = 1 << 2,
	BQINT_DYNAMIC = 1 << 3,
	BQINT_INLINED = 1 << 4,

	BQINT_TRUNCATED = 1 << 8,
	BQINT_OUT_OF_MEMORY = 1 << 9,
	BQINT_DIV_BY_ZERO = 1 << 11,

	BQINT_ERROR = 0
		| BQINT_TRUNCATED
		| BQINT_OUT_OF_MEMORY
		| BQINT_DIV_BY_ZERO,
};

// -- Structure

#ifndef BQINT_INLINE_CAPACITY
#define BQINT_INLINE_CAPACITY (sizeof(bqint_word*) / sizeof(bqint_word))
#endif

typedef struct bqint
{
	union {
		bqint_word *words;
		bqint_word inline_words[BQINT_INLINE_CAPACITY];
	} data;

	bqint_size size;
	bqint_size capacity;
	bqint_flags flags;
} bqint;

// -- Initialization and de-initialization

// Initialize a zero bqint that will be dynamically allocated as it grows
// Not necessary if BQINT_NO_IMPLICIT_DYNAMIC is not defined
bqint bqint_dynamic();

// Initialize a zero bqint to static buffer, results will be truncated if too large
bqint bqint_static(void *buffer, size_t size);

// Initialize a zero bqint to an initial buffer, if the number grows too large
// dynamically allocate a new one
bqint bqint_dynamic_initial(void *buffer, size_t size);

// If the bqint has allocated it's own memory it must be released by this,
// will also reset the value to zero
void bqint_free(bqint *a);

// -- Setting values

// Set bqint to the value of another bqint
void bqint_set(bqint *result, bqint *b);

// Set bqint to zero
// Note: Newly initialized values are already zero, so this is rarely needed
void bqint_set_zero(bqint *a);

// Set bqint to an unsigned int value
void bqint_set_u32(bqint *a, uint32_t val);

// Set bqint to a signed int value
void bqint_set_i32(bqint *a, int32_t val);

// Set bqint to a raw value
// data: Pointer to little-endian number data
// size: Length of the number in bytes
void bqint_set_raw(bqint *a, const void *data, size_t size);

// -- Arithmetic

// Add bqints a and b and store the value in result
void bqint_add(bqint *result, bqint *a, bqint *b);

// -- Compares

// Compare bqints, positive if a > b, negative if a < b, zero if equal
int bqint_cmp(bqint *a, bqint *b);

// -- Queries

// Returns a non-zero value if the value is what it's supposed to be represented
// ie. no error or truncation of the value has happened.
inline static int bqint_ok(bqint *a)
{
	return (a->flags & BQINT_ERROR) == 0;
}

// Get the size of the bqint in words
static inline size_t bqint_get_size(bqint *a)
{
	return a->size;
}

// Returns the raw data of the bqint
// bqint_get_size() returns the number of words returned
static inline bqint_word *bqint_get_words(bqint *a)
{
	if (a->flags & BQINT_INLINED) {
		return a->data.inline_words;
	} else {
		return a->data.words;
	}
}

// -- Strings

// Parse a number from string
// Does not accept prefixes such as 0x or 0b
// str: Zero-terminated string containing digits 0-9, A-Z, a-z
// base: Radix to use for the number, digits past 9 are A-Z or a-z
// returns: NULL on success, pointer to first invalid char if failed
const char *bqint_parse_string(bqint *a, const char *str, int base);

// -- Allocators

typedef void*(*bqint_alloc_fn)(size_t);
typedef void(*bqint_free_fn)(void*);

// Set global allocators for bqint functions
void bqint_set_allocators(bqint_alloc_fn alloc_fn, bqint_free_fn free_fn);

#ifdef BQINT_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>

#define BQINT__HI(dw) ((dw) >> BQINT_WORD_BITS)
#define BQINT__LO(dw) ((dw) & (((bqint_dword)1 << BQINT_WORD_BITS) - 1))

#define BQINT__BYTESWAP_32(w) ((w) << 24 | ((w) & 0x0000FF00U) << 8 \
		| ((w) & 0x00FF0000U) >> 8 | ((w) >> 24))
#define BQINT__BYTESWAP_16(w) ((w) << 8 | (w) >> 8)

#if BQINT_WORD_BITS == 32
#define BQINT__BYTESWAP_WORD(w) BQINT__BYTESWAP_32(w)
#elif BQINT_WORD_BITS == 16
#define BQINT__BYTESWAP_WORD(w) BQINT__BYTESWAP_16(w)
#elif BQINT_WORD_BITS == 8
#define BQINT__BYTESWAP_WORD(w) (w)
#endif

static bqint_alloc_fn bqint_alloc_memory = malloc;
static bqint_free_fn bqint_free_memory = free;

void bqint_set_allocators(bqint_alloc_fn alloc_fn, bqint_free_fn free_fn)
{
	bqint_alloc_memory = alloc_fn;
	bqint_free_memory = free_fn;
}

static int bqint__is_big_endian()
{
	uint32_t one = 1;
	return *(unsigned char*)&one != 1;
}

bqint bqint_dynamic()
{
	bqint result;
	result.data.words = 0;
	result.size = 0;
	result.capacity = 0;
	result.flags = BQINT_DYNAMIC;
	return result;
}

bqint bqint_static(void *buffer, size_t size)
{
	bqint result;
	result.data.words = (bqint_word*)buffer;
	result.size = 0;
	result.capacity = size / sizeof(bqint_word);
	result.flags = BQINT_STATIC;
	return result;
}

bqint bqint_dynamic_initial(void *buffer, size_t size)
{
	bqint result;
	result.data.words = (bqint_word*)buffer;
	result.size = 0;
	result.capacity = size / sizeof(bqint_word);
	result.flags = BQINT_DYNAMIC;
	return result;
}

void bqint_free(bqint *a)
{
	if (a->flags & BQINT_ALLOCATED) {
		bqint_free_memory(a->data.words);
	}
	a->data.words = 0;
	a->size = 0;
	a->capacity = 0;
	a->flags = 0;
}

static bqint_word *bqint__reserve(bqint *a, bqint_size* size)
{
	bqint_flags flags = a->flags;
	bqint_size sz = *size;

	// Fits in current storage
	if (sz <= a->capacity) {
		if (flags & BQINT_INLINED) {
			return a->data.inline_words;
		} else {
			return a->data.words;
		}
	}

	// Statically allocated buffer: Truncate size
	if (flags & BQINT_STATIC) {
		// Can't be static and inlined
		BQINT_ASSERT(!(flags & BQINT_INLINED));
		*size = a->capacity;
		return a->data.words;
	}

	// Fits inline
	if (sz <= BQINT_INLINE_CAPACITY) {
		a->capacity = BQINT_INLINE_CAPACITY;
		a->flags |= BQINT_INLINED;
		return a->data.inline_words;
	}

	// Dynamically allocated (implicit or explicit)
#ifdef BQINT_NO_IMPLICIT_DYNAMIC
	if ((flags & BQINT_DYNAMIC))
#endif
	{
		bqint_size new_size = a->capacity * 2;
		new_size = new_size >= sz ? new_size : sz;

		if (flags & BQINT_ALLOCATED) {
			bqint_free_memory(a->data.words);
		}

		a->data.words = (bqint_word*)bqint_alloc_memory(sizeof(bqint_word) * new_size);
		a->capacity = new_size;

		a->flags &= ~BQINT_INLINED;
		a->flags |= BQINT_ALLOCATED;
		return a->data.words;
	}

	// No implicit dynamic, just truncate to inline storage
	*size = BQINT_INLINE_CAPACITY;
	a->capacity = BQINT_INLINE_CAPACITY;
	a->flags |= BQINT_INLINED;
	return a->data.inline_words;
}

inline static void bqint__truncate(bqint *a, bqint_size size)
{
	bqint_size cap = a->capacity;
	if (size <= cap) {
		a->size = size;
	} else {
		a->size = cap;
		a->flags |= BQINT_TRUNCATED;
		BQINT_ASSERT_FLAG_SET(BQINT_TRUNCATED);
	}
}

inline static bqint_flags bqint__combine_flags(bqint_flags result, bqint_flags a, bqint_flags mask)
{
	return (result & ~mask) | (a & mask);
}

void bqint_set(bqint *result, bqint *a)
{
	bqint_size size = a->size;
	bqint_word *a_words = bqint_get_words(a);
	bqint_word *words = bqint__reserve(result, &size);

	memcpy(words, a_words, size * sizeof(bqint_word));

	result->flags = bqint__combine_flags(result->flags, a->flags, BQINT_NEGATIVE|BQINT_ERROR);
	bqint__truncate(result, a->size);
}

static void bqint__set_raw_u32(bqint *a, uint32_t val)
{
	bqint_size size = 0, cap = sizeof(uint32_t) / sizeof(bqint_word);
	bqint_word *words;

	if (val == 0) {
		a->size = 0;
		return;
	}

	words = bqint__reserve(a, &cap);

#if BQINT_WORD_BITS == 32
	size = 1;
	if (cap > 0)
		words[0] = val;
#elif BQINT_WORD_BITS == 16
	if (val & 0xFFFF0000) {
		size = 2;
		if (cap >= 2) {
			words[0] = val & 0xFFFF;
			words[1] = val >> 16;
		} else if (cap > 1) {
			words[0] = val & 0xFFFF;
		}
	} else {
		size = 1;
		words[0] = val & 0xFFFF;
	}
#elif BQINT_WORD_BITS == 8
	while (val) {
		if (size < cap)
			words[size] = val & 0xFF;
		size++;
		val >>= 8;
	}
#endif

	bqint__truncate(a, size);
}

void bqint_set_zero(bqint *a)
{
	a->size = 0;
	a->flags &= ~BQINT_NEGATIVE;
}

void bqint_set_u32(bqint *a, uint32_t val)
{
	bqint__set_raw_u32(a, val);
	a->flags &= ~BQINT_NEGATIVE;
}

void bqint_set_i32(bqint *a, int32_t val)
{
	bqint__set_raw_u32(a, (uint32_t)(val & 0x7FFFFFFF));
	if (val < 0) {
		a->flags |= BQINT_NEGATIVE;
	} else {
		a->flags &= ~BQINT_NEGATIVE;
	}
}

void bqint_set_raw(bqint *a, const void *data, size_t size)
{
	size_t num_words, size_to_copy;
	bqint_size sz;
	bqint_word *words;

	num_words = (size + sizeof(bqint_word) - 1) / sizeof(bqint_word);

	// Clamp to representable size
	if (num_words > BQINT_MAX_WORDS) {
		sz = BQINT_MAX_WORDS;
	} else {
		sz = (bqint_size)num_words;
	}
	words = bqint__reserve(a, &sz);

	size_to_copy = sz * sizeof(bqint_word);
	if (size_to_copy > size) {
		size_to_copy = size;
		// Pad the end with zeroes
		words[sz - 1] = (bqint_word)0;
	}
	memcpy(words, data, size_to_copy);

	if (bqint__is_big_endian()) {
		bqint_size i;
		for (i = 0; i < sz; i++) {
			words[i] = BQINT__BYTESWAP_WORD(words[i]);
		}
	}

	// If we copied less than the requested mark the value truncated
	if (size_to_copy < size) {
		a->flags |= BQINT_TRUNCATED;
		BQINT_ASSERT_FLAG_SET(BQINT_TRUNCATED);
	}

	// Remove high zeroes
	while (sz > 0 && !words[sz - 1]) {
		sz--;
	}

	a->size = sz;
}

bqint_size bqint__add_words(
		bqint_word *r_words, bqint_size r_size,
		bqint_word *a_words, bqint_size a_size,
		bqint_word *b_words, bqint_size b_size,
		bqint_word initial_carry)
{
	int truncated = 0;
	bqint_word *long_words, *short_words;
	bqint_size long_size, short_size;
	bqint_size pos;
	bqint_word carry;

	// Sort the inputs to short and long
	if (a_size > b_size) {
		long_words = a_words;
		long_size = a_size;
		short_words = b_words;
		short_size = b_size;
	} else {
		long_words = b_words;
		long_size = b_size;
		short_words = a_words;
		short_size = a_size;
	}

	// If the long value does not fit in the result truncate
	if (long_size > r_size) {
		truncated = 1;
		long_size = r_size;

		// Note: long_size >= short_size > r_size, so this can be contained
		// in this if statement safely
		if (short_size > r_size) {
			short_size = r_size;
		}
	}

	// 1. Add the words together for the duration of the short number
	carry = initial_carry;
	for (pos = 0; pos < short_size; pos++) {
		bqint_dword sum
			= (bqint_dword)short_words[pos]
			+ (bqint_dword)long_words[pos]
			+ (bqint_dword)carry;

		r_words[pos] = BQINT__LO(sum);
		carry = BQINT__HI(sum);
	}

	// 2. Keep adding carry as long as it's present
	for (; pos < long_size && carry; pos++) {
		bqint_dword sum
			= (bqint_dword)long_words[pos]
			+ (bqint_dword)carry;

		r_words[pos] = BQINT__LO(sum);
		carry = BQINT__HI(sum);
	}

	// 3. Copy the rest of the long words (if any)
	for (; pos < long_size; pos++) {
		r_words[pos] = long_words[pos];
	}

	// 4. Overflow the carry to the next word (check truncation)
	if (carry) {
		if (pos < r_size)
			r_words[pos] = carry;
		pos++;
	}

	// Note: Return ~0 if we truncated indicating the algorithm didn't calculate
	// the correct length of the number.
	return truncated ? ~(bqint_size)0 : pos;
}

void bqint_add(bqint *result, bqint *a, bqint *b)
{
	// TODO: Signs
	bqint_size res_size = (a->size > b->size ? a->size : b->size) + 1;
	bqint_word *res_words = bqint__reserve(result, &res_size);
	bqint_size size;

	size = bqint__add_words(
			res_words, res_size,
			bqint_get_words(a), a->size,
			bqint_get_words(b), b->size,
			0);

	// Propagate error flags, note: this overwrites the error of `result` because it's
	// result doesn't matter at this point anymore
	result->flags = bqint__combine_flags(result->flags, a->flags | b->flags, BQINT_ERROR);
	bqint__truncate(result, size);
}

int bqint_cmp(bqint *a, bqint *b)
{
	int sign, signdiff;
	bqint_size i, size;
	bqint_word *aws, *bws;

	// Special case to handle negative zero
	if (a->size == 0 && b->size == 0)
		return 0;

	// Note: Reversed subtraction direction compared to normal ordering,
	// since the larger of the negative bits is the smaller number in total.
	signdiff = (int)(b->flags & BQINT_NEGATIVE) - (int)(a->flags & BQINT_NEGATIVE);
	if (signdiff != 0)
		return signdiff;

	// The result depends on the sign, we compare the unsigned values so when
	// the number is negative the greater value is actually the smaller one.
	sign = a->flags & BQINT_NEGATIVE ? -1 : 1;

	// Equal signs: Compare sizes
	size = a->size;
	if (size != b->size)
		return size > b->size ? sign : -sign;

	// Equal sizes: Compare data
	aws = bqint_get_words(a);
	bws = bqint_get_words(b);
	for (i = size - 1; i < size; i--) {
		bqint_word aw = aws[i], bw = bws[i];

		if (aw != bw)
			return aw > bw ? sign : -sign;
	}

	// Everything is equal
	return 0;
}

#endif
