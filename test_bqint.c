#define inline
#define _CRT_SECURE_NO_WARNINGS
#define BQINT_IMPLEMENTATION
#include "bqint.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

uint32_t read_u32(const char **ptr)
{
	const char *p = *ptr;
	uint32_t val = 0;
	val |= p[0];
	val |= p[1] << 8;
	val |= p[2] << 16;
	val |= p[3] << 24;
	*ptr += 4;
	return val;
}

void read_bqint(bqint *val, const char **ptr)
{
	uint32_t size = read_u32(ptr);
	bqint_set_raw(val, *ptr, size);
	*ptr += size;
}

uint64_t num_asserts = 0;
uint64_t num_failed = 0;

#define BREAK_ASSERT_NUM 0

typedef long long unsigned lluint;

void test_assert(int val, const char *desc, ...)
{
	if (!val) {
		va_list args;
		fprintf(stderr, "Assert %llu failed: ", (lluint)num_asserts);
		va_start (args, desc);
		vfprintf(stderr, desc, args);
		va_end(args);
		fprintf(stderr, "\n");
		num_failed++;
	}
	num_asserts++;
#if BREAK_ASSERT_NUM > 0
	if (num_asserts == BREAK_ASSERT_NUM)
		__debugbreak();
#endif
}

void test_assert_ok(bqint *val, const char *name)
{
	char flagstr[256], *fs = flagstr;
	if (val->flags & BQINT_TRUNCATED) fs += sprintf(fs, "trunc ");
	if (val->flags & BQINT_OUT_OF_MEMORY) fs += sprintf(fs, "OOM ");
	if (val->flags & BQINT_DIV_BY_ZERO) fs += sprintf(fs, "x/0 ");

	test_assert(bqint_ok(val), "%s OK [%s]", name, flagstr);
}

const char *cmp_descs[] = {
	"value < ref",
	"value = ref",
	"value > ref",
};

void test_assert_equal(bqint *val, bqint *ref, const char *name)
{
	int cmp = bqint_cmp(val, ref);
	test_assert(cmp == 0, "%s equal to reference (%s)", name, cmp_descs[cmp + 1]);
}

struct bqtest_alloc_hdr
{
	struct bqtest_alloc_hdr *prev, *next;
	uint64_t size;
};

struct bqtest_alloc_hdr alloc_head;

void *bqtest_alloc(size_t size)
{
	struct bqtest_alloc_hdr *hdr = (struct bqtest_alloc_hdr*)malloc(
			size + sizeof(struct bqtest_alloc_hdr));

	if (alloc_head.prev)
		alloc_head.prev->next = hdr;
	hdr->prev = alloc_head.prev;
	hdr->next = &alloc_head;
	hdr->size = (uint64_t)size;

	alloc_head.prev = hdr;

	return hdr + 1;
}

void bqtest_free(void *mem)
{
	struct bqtest_alloc_hdr *hdr = (struct bqtest_alloc_hdr*)mem;
	hdr--;

	if (hdr->prev) hdr->prev->next = hdr->next;
	if (hdr->next) hdr->next->prev = hdr->prev;

	free(hdr);
}

int main(int argc, char **argv)
{
	int status = 0;
	char *fixture_data;
	size_t fixture_data_size;

	printf("Running bqint tests...\n");
	printf("  BQINT_WORD_BITS=%d\n", BQINT_WORD_BITS);

	bqint_set_allocators(bqtest_alloc, bqtest_free);

	{
		FILE *fixture_file = fopen(argv[1], "rb");
		fseek(fixture_file, 0, SEEK_END);
		fixture_data_size = ftell(fixture_file);
		fseek(fixture_file, 0, SEEK_SET);

		fixture_data = (char*)malloc(fixture_data_size);
		fread(fixture_data, 1, fixture_data_size, fixture_file);

		fclose(fixture_file);
	}

	{
		uint32_t num_binops = 2;
		uint32_t fixi, fixj, bini;
		const char *fixptr = fixture_data;
		uint32_t num_fixtures = read_u32(&fixptr);
		bqint *fixtures = (bqint*)calloc(sizeof(bqint), num_fixtures);
		bqint *binop_res = (bqint*)calloc(sizeof(bqint), num_fixtures*num_fixtures*num_binops);

		// Read fixtures
		for (fixi = 0; fixi < num_fixtures; fixi++) {
			read_bqint(&fixtures[fixi], &fixptr);
			test_assert_ok(&fixtures[fixi], "Fixture");
		}

		// Read fixture results
		for (fixi = 0; fixi < num_fixtures; fixi++) {
			for (fixj = 0; fixj < num_fixtures; fixj++) {
				for (bini = 0; bini < num_binops; bini++) {
					size_t index = ((fixi * num_fixtures) + fixj) * num_binops + bini;
					read_bqint(&binop_res[index], &fixptr);
					test_assert_ok(&binop_res[index], "Fixture operation result");
				}
			}
		}

		// Test comparison
		// - bqint_cmp
		for (fixi = 0; fixi < num_fixtures; fixi++) {
			for (fixj = 0; fixj < num_fixtures; fixj++) {
				char c = *fixptr++;
				int cmpref = c == '=' ? 0 : (c == '<' ? -1 : 1);
				int cmp = bqint_cmp(&fixtures[fixi], &fixtures[fixj]);
				test_assert(cmp == cmpref, "bqint_cmp(%u, %u)", fixi, fixj);
			}
		}

		// Test moving values
		// - bqint_set
		for (fixi = 0; fixi < num_fixtures; fixi++) {
			bqint copy = { 0 };
			bqint_set(&copy, &fixtures[fixi]);

			test_assert_ok(&copy, "Copied value");
			test_assert_equal(&copy, &fixtures[fixi], "Copied value");

			bqint_free(&copy);
		}

		for (fixi = 0; fixi < num_fixtures; fixi++) {
			for (fixj = 0; fixj < num_fixtures; fixj++) {
				bqint *results = binop_res + ((fixi * num_fixtures) + fixj) * num_binops;
				bqint sum = { 0 };

				bqint_add(&sum, &fixtures[fixi], &fixtures[fixj]);
				test_assert_ok(&sum, "Sum result");
				test_assert_equal(&sum, &results[0], "Sum result");

				bqint_free(&sum);
			}
		}

		for (fixi = 0; fixi < num_fixtures; fixi++) {
			bqint_free(&fixtures[fixi]);
		}

		for (fixi = 0; fixi < num_fixtures*num_fixtures*num_binops; fixi++) {
			bqint_free(&binop_res[fixi]);
		}
	}

	printf("%llu/%llu (%llu fails)\n", (lluint)(num_asserts - num_failed), (lluint)num_asserts, (lluint)num_failed);

	if (num_failed > 0)
		status |= 1;

	{
		uint64_t leaked_num = 0, leaked_amount = 0;
		struct bqtest_alloc_hdr *hdr = alloc_head.prev;
		while (hdr) {
			leaked_num++;
			leaked_amount += hdr->size;
			hdr = hdr->prev;
		}

		printf("Leaked %llu allocations (%llu bytes)\n", (lluint)leaked_num, (lluint)leaked_amount);

		if (leaked_amount > 0)
			status |= 2;
	}

	return status;
}

