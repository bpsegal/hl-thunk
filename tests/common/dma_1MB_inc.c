/*
 * Copyright (c) 2019 HabanaLabs Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "hlthunk.h"
#include "hlthunk_tests.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <limits.h>
#include <cmocka.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

DMA_1KB_INC_SRAM(test_dma_sram_size_2MB, state, 2 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_3MB, state, 3 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_4MB, state, 4 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_5MB, state, 5 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_6MB, state, 6 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_7MB, state, 7 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_8MB, state, 8 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_9MB, state, 9 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_10MB, state, 10 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_11MB, state, 11 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_12MB, state, 12 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_13MB, state, 13 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_14MB, state, 14 * 1024 * 1024)
DMA_1KB_INC_SRAM(test_dma_sram_size_15MB, state, 15 * 1024 * 1024)

DMA_1KB_INC_DDR(test_dma_ddr_size_2MB, state, 2 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_3MB, state, 3 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_4MB, state, 4 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_5MB, state, 5 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_6MB, state, 6 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_7MB, state, 7 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_8MB, state, 8 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_9MB, state, 9 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_10MB, state, 10 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_11MB, state, 11 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_12MB, state, 12 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_13MB, state, 13 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_14MB, state, 14 * 1024 * 1024)
DMA_1KB_INC_DDR(test_dma_ddr_size_15MB, state, 15 * 1024 * 1024)

const struct CMUnitTest dma_1MB_inc_tests[] = {
	cmocka_unit_test(test_dma_sram_size_2MB),
	cmocka_unit_test(test_dma_sram_size_3MB),
	cmocka_unit_test(test_dma_sram_size_4MB),
	cmocka_unit_test(test_dma_sram_size_5MB),
	cmocka_unit_test(test_dma_sram_size_6MB),
	cmocka_unit_test(test_dma_sram_size_7MB),
	cmocka_unit_test(test_dma_sram_size_8MB),
	cmocka_unit_test(test_dma_sram_size_9MB),
	cmocka_unit_test(test_dma_sram_size_10MB),
	cmocka_unit_test(test_dma_sram_size_11MB),
	cmocka_unit_test(test_dma_sram_size_12MB),
	cmocka_unit_test(test_dma_sram_size_13MB),
	cmocka_unit_test(test_dma_sram_size_14MB),
	cmocka_unit_test(test_dma_sram_size_15MB),

	cmocka_unit_test(test_dma_ddr_size_2MB),
	cmocka_unit_test(test_dma_ddr_size_3MB),
	cmocka_unit_test(test_dma_ddr_size_4MB),
	cmocka_unit_test(test_dma_ddr_size_5MB),
	cmocka_unit_test(test_dma_ddr_size_6MB),
	cmocka_unit_test(test_dma_ddr_size_7MB),
	cmocka_unit_test(test_dma_ddr_size_8MB),
	cmocka_unit_test(test_dma_ddr_size_9MB),
	cmocka_unit_test(test_dma_ddr_size_10MB),
	cmocka_unit_test(test_dma_ddr_size_11MB),
	cmocka_unit_test(test_dma_ddr_size_12MB),
	cmocka_unit_test(test_dma_ddr_size_13MB),
	cmocka_unit_test(test_dma_ddr_size_14MB),
	cmocka_unit_test(test_dma_ddr_size_15MB),
};

int main(void)
{
	char *test_names_to_run;
	int rc;

	test_names_to_run = getenv("HLTHUNK_TESTS_NAMES");
	if (test_names_to_run)
		cmocka_set_test_filter(test_names_to_run);

	rc = cmocka_run_group_tests(dma_1MB_inc_tests, hltests_setup,
					hltests_teardown);

	return rc;
}
