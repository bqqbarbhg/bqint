
// -- Types

#include <stdint.h>

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
	BQINT_TRUNCATED = 1 << 4,

	BQINT_OUT_OF_MEMORY = 1 << 9,
	BQINT_DIV_BY_ZERO = 1 << 11,

	BQINT_ERROR = 0
		| BQINT_OUT_OF_MEMORY
		| BQINT_DIV_BY_ZERO
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
void bqint_set_raw(bqint *a, void *data, size_t size);

// -- Compares

// Compare bqints, positive if a > b, negative if a < b, zero if equal
int bqint_cmp(bqint *a, bqint *b);

// -- Queries

// Returns a non-zero value if the value is what it's supposed to be represented
// ie. no error or truncation of the value has happened.
inline static int bqint_ok(bqint *a)
{
	return (a->flags & (BQINT_TRUNCATED | BQINT_ERROR)) == 0;
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

#ifdef BQINT_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>

void *bqint_alloc_memory(size_t size)
{
	return malloc(size);
}

void bqint_free_memory(void *ptr)
{
	free(ptr);
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
#ifndef BQINT_NO_IMPLICIT_DYNAMIC
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

void bqint_set_raw(bqint *a, void *data, size_t size)
{
	size_t rounded_size, num_words, size_to_copy;
	bqint_size sz;
	bqint_word *words;

	rounded_size = size + sizeof(bqint_word) - 1;
	if (rounded_size >= size) {
		num_words = rounded_size / sizeof(bqint_word);
	} else {
		// This should happen only if rounded_size wraps, which means size
		// is in [size_t_max - sizeof(bqint_word), size_t_max], which requires
		// rounding up, so round after division. In case of 8-bit words don't
		// round because it might also wrap.
#if BQINT_WORD_BITS == 8
		num_words = size;
#else
		num_words = size / sizeof(bqint_word) + 1;
#endif
	}

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

	// If we copied less than the requested mark the value truncated
	if (size_to_copy < size) {
		a->flags |= BQINT_TRUNCATED;
		BQINT_ASSERT_FLAG_SET(BQINT_TRUNCATED);
	}

	// Remove high zeroes
	while (sz > 0 && !words[sz]) {
		sz--;
	}

	a->size = sz;
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
	if (size != a->size)
		return size > b->size ? sign : -sign;

	// Equal sizes: Compare data
	aws = bqint_get_words(a);
	bws = bqint_get_words(b);
	for (i = 0; i < size; i++) {
		bqint_word aw = aws[i], bw = bws[i];

		if (aw != bw)
			return aw > bw ? sign : -sign;
	}

	// Everything is equal
	return 0;
}

#if 0
static bqint_word bqint__digit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'z';
	return ~(bqint_word)0;
}

const char *bqint_parse_string(bqint *a, const char *str, int base)
{
	bqint_word base_word = (bqint_word)base;
	const char *ch, c;

	for (ch = str; c = *ch; ch++) {
		bqint_word digit = bqint__digit(c);
		if (digit >= base_word)
			return ch;
		bqint_mul_in_word_carry(&a, base_word, digit);
	}

	return 0;
}
#endif

#endif
