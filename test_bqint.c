#define BQINT_IMPLEMENTATION
#include "bqint.h"
#include <stdio.h>

int main(int argc, char **argv)
{
	uint32_t buf[4];
	uint32_t val = 0xABCDEF12;
	bqint a = bqint_dynamic();
	bqint b = bqint_static(buf, sizeof(buf));

	if (bqint_ok(&a)) {
		printf("a ok\n");
	}
	if (bqint_ok(&b)) {
		printf("b ok\n");
	}

	printf("%d\n", bqint_cmp(&a, &b));

	return 0;
}

