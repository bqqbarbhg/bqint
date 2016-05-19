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
		fprintf(stderr, "%llu: ", (lluint)num_asserts);
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

int main(int argc, char **argv)
{
	char *fixture_data;
	size_t fixture_data_size;

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

		for (fixi = 0; fixi < num_fixtures; fixi++) {
			read_bqint(&fixtures[fixi], &fixptr);
			test_assert_ok(&fixtures[fixi], "Fixture");
		}

		for (fixi = 0; fixi < num_fixtures; fixi++) {
			for (fixj = 0; fixj < num_fixtures; fixj++) {
				for (bini = 0; bini < num_binops; bini++) {
					size_t index = ((fixi * num_fixtures) + fixj) * num_binops + bini;
					read_bqint(&binop_res[index], &fixptr);
					test_assert_ok(&binop_res[index], "Fixture operation result");
				}
			}
		}

		for (fixi = 0; fixi < num_fixtures; fixi++) {
			for (fixj = 0; fixj < num_fixtures; fixj++) {
				char c = *fixptr++;
				int cmpref = c == '=' ? 0 : (c == '<' ? -1 : 1);
				int cmp = bqint_cmp(&fixtures[fixi], &fixtures[fixj]);
				test_assert(cmp == cmpref, "bqint_cmp(%u, %u)", fixi, fixj);
			}
		}
	}

	printf("%llu/%llu (%llu fails)", (lluint)(num_asserts - num_failed), (lluint)num_asserts, (lluint)num_failed);

	return 0;
}

