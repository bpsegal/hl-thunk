// SPDX-License-Identifier: MIT

/*
 * Copyright 2019 HabanaLabs, Ltd.
 * All Rights Reserved.
 */

#include "hlthunk.h"
#include "hlthunk_tests.h"
#include "kvec.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <limits.h>
#include <cmocka.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

struct dma_chunk {
	void *input;
	void *output;
	uint64_t input_device_va;
	uint64_t output_device_va;
	uint64_t dram_addr;
};

void test_dma_entire_dram_random(void **state)
{
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	struct hltests_cs_chunk execute_arr[1];
	struct hlthunk_hw_ip_info hw_ip;
	struct hltests_pkt_info pkt_info;
	void *buf[2], *cb;
	uint64_t dram_size, dram_addr, dram_addr_end, device_va[2], seq;
	uint64_t dma_size = 1 << 21; /* 2MB */
	uint64_t zone_size = 1 << 24; /* 16MB */
	uint32_t offset, cb_size = 0, vec_len, packets_size;

	kvec_t(struct dma_chunk) array;
	struct dma_chunk chunk;
	int i, rc, fd = tests_state->fd;

	/* This test uses specific DRAM addresses, hence needs MMU to
	 * be disabled
	 */
	if (tests_state->mmu) {
		printf("Test is skipped because MMU is enabled\n");
		skip();
	}

	kv_init(array);

	rc = hlthunk_get_hw_ip_info(fd, &hw_ip);
	assert_int_equal(rc, 0);

	assert_true(hw_ip.dram_enabled);

	/* check alignment to 8B */
	assert_int_equal(dma_size & 0x7, 0);
	assert_int_equal(zone_size & 0x7, 0);

	assert_true(2 * dma_size <= zone_size);

	/* align to zone_size */
	dram_size = hw_ip.dram_size & ~(zone_size - 1);
	assert_true(zone_size < dram_size);

	dram_addr = hw_ip.dram_base_address;
	dram_addr_end = hw_ip.dram_base_address + dram_size - 1;

	while (dram_addr < (dram_addr_end - dma_size)) {
		buf[0] = hltests_allocate_host_mem(fd, dma_size, NOT_HUGE);
		assert_non_null(buf[0]);
		hltests_fill_rand_values(buf[0], dma_size);
		device_va[0] = hltests_get_device_va_for_host_ptr(fd, buf[0]);

		buf[1] = hltests_allocate_host_mem(fd, dma_size, NOT_HUGE);
		assert_non_null(buf[1]);
		memset(buf[1], 0, dma_size);
		device_va[1] = hltests_get_device_va_for_host_ptr(fd, buf[1]);

		hltests_fill_rand_values(&offset, sizeof(offset));

		/* need an offset inside a zone and aligned to 8B */
		offset = (offset & (zone_size - 1)) & ~0x7;
		if (offset > (zone_size - dma_size - 1))
			offset -= dma_size;

		chunk.input = buf[0];
		chunk.output = buf[1];
		chunk.input_device_va = device_va[0];
		chunk.output_device_va = device_va[1];
		chunk.dram_addr = dram_addr + offset;

		kv_push(struct dma_chunk, array, chunk);

		dram_addr += zone_size;
	}

	vec_len = kv_size(array);
	packets_size = 24 * vec_len;

	/* DMA down */
	cb = hltests_create_cb(fd, packets_size, EXTERNAL, 0);
	assert_non_null(cb);

	for (i = 0 ; i < vec_len ; i++) {
		chunk = kv_A(array, i);
		memset(&pkt_info, 0, sizeof(pkt_info));
		pkt_info.eb = EB_FALSE;
		pkt_info.mb = MB_TRUE;
		pkt_info.dma.src_addr = chunk.input_device_va;
		pkt_info.dma.dst_addr = (uint64_t) (uintptr_t) chunk.dram_addr;
		pkt_info.dma.size = dma_size;
		pkt_info.dma.dma_dir = GOYA_DMA_HOST_TO_DRAM;
		cb_size = hltests_add_dma_pkt(fd, cb, cb_size, &pkt_info);
	}

	hltests_submit_and_wait_cs(fd, cb, cb_size,
				hltests_get_dma_down_qid(fd, DCORE0, STREAM0),
				DESTROY_CB_TRUE, HL_WAIT_CS_STATUS_COMPLETED);
	cb_size = 0;

	/* DMA up */
	cb = hltests_create_cb(fd, packets_size, EXTERNAL, 0);
	assert_non_null(cb);

	for (i = 0 ; i < vec_len ; i++) {
		chunk = kv_A(array, i);
		memset(&pkt_info, 0, sizeof(pkt_info));
		pkt_info.eb = EB_FALSE;
		pkt_info.mb = MB_TRUE;
		pkt_info.dma.src_addr = (uint64_t) (uintptr_t) chunk.dram_addr;
		pkt_info.dma.dst_addr = chunk.output_device_va;
		pkt_info.dma.size = dma_size;
		pkt_info.dma.dma_dir = GOYA_DMA_DRAM_TO_HOST;
		cb_size = hltests_add_dma_pkt(fd, cb, cb_size, &pkt_info);
	}

	hltests_submit_and_wait_cs(fd, cb, cb_size,
				hltests_get_dma_up_qid(fd, DCORE0, STREAM0),
				DESTROY_CB_TRUE, HL_WAIT_CS_STATUS_COMPLETED);

	cb_size = 0;

	/* compare host memories */
	for (i = 0 ; i < vec_len ; i++) {
		chunk = kv_A(array, i);
		rc = hltests_mem_compare(chunk.input, chunk.output, dma_size);
		assert_int_equal(rc, 0);
	}

	/* cleanup */
	for (i = 0 ; i < vec_len ; i++) {
		chunk = kv_A(array, i);
		rc = hltests_free_host_mem(fd, chunk.input);
		assert_int_equal(rc, 0);

		rc = hltests_free_host_mem(fd, chunk.output);
		assert_int_equal(rc, 0);
	}

	kv_destroy(array);
}

DMA_TEST_INC_DRAM(test_dma_dram_size_1KB, state, 1 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_2KB, state, 2 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_3KB, state, 3 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_4KB, state, 4 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_5KB, state, 5 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_6KB, state, 6 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_7KB, state, 7 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_8KB, state, 8 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_9KB, state, 9 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_10KB, state, 10 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_11KB, state, 11 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_12KB, state, 12 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_13KB, state, 13 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_14KB, state, 14 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_15KB, state, 15 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_16KB, state, 16 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_20KB, state, 20 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_24KB, state, 24 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_28KB, state, 28 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_32KB, state, 32 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_36KB, state, 36 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_40KB, state, 40 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_44KB, state, 44 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_48KB, state, 48 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_52KB, state, 52 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_56KB, state, 56 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_60KB, state, 60 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_64KB, state, 64 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_96KB, state, 96 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_128KB, state, 128 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_160KB, state, 160 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_192KB, state, 192 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_224KB, state, 224 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_256KB, state, 256 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_288KB, state, 288 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_320KB, state, 320 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_352KB, state, 352 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_384KB, state, 384 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_416KB, state, 416 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_448KB, state, 448 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_480KB, state, 480 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_512KB, state, 512 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_640KB, state, 640 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_768KB, state, 768 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_896KB, state, 896 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_1024KB, state, 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_1152KB, state, 1152 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_1280KB, state, 1280 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_1408KB, state, 1408 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_1536KB, state, 1536 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_1664KB, state, 1664 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_1792KB, state, 1792 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_1920KB, state, 1920 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_2MB, state, 2 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_3MB, state, 3 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_4MB, state, 4 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_5MB, state, 5 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_6MB, state, 6 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_7MB, state, 7 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_8MB, state, 8 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_9MB, state, 9 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_10MB, state, 10 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_11MB, state, 11 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_12MB, state, 12 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_13MB, state, 13 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_14MB, state, 14 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_15MB, state, 15 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_16MB, state, 16 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_20MB, state, 20 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_24MB, state, 24 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_28MB, state, 28 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_32MB, state, 32 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_36MB, state, 36 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_40MB, state, 40 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_44MB, state, 44 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_48MB, state, 48 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_64MB, state, 64 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_128MB, state, 128 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_192MB, state, 192 * 1024 * 1024)
DMA_TEST_INC_DRAM(test_dma_dram_size_256MB, state, 256 * 1024 * 1024)

const struct CMUnitTest dma_dram_tests[] = {
	cmocka_unit_test_setup(test_dma_entire_dram_random,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_1KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_2KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_3KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_4KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_5KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_6KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_7KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_8KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_9KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_10KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_11KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_12KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_13KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_14KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_15KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_16KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_20KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_24KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_28KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_32KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_36KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_40KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_44KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_48KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_52KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_56KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_60KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_64KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_96KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_128KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_160KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_192KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_224KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_256KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_288KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_320KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_352KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_384KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_416KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_448KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_480KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_512KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_640KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_768KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_896KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_1024KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_1152KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_1280KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_1408KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_1536KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_1664KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_1792KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_1920KB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_2MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_3MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_4MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_5MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_6MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_7MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_8MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_9MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_10MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_11MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_12MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_13MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_14MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_15MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_16MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_20MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_24MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_28MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_32MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_36MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_40MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_44MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_48MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_64MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_128MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_192MB,
			hltests_ensure_device_operational),
	cmocka_unit_test_setup(test_dma_dram_size_256MB,
			hltests_ensure_device_operational),
};

static const char *const usage[] = {
	"dma_dram [options]",
	NULL,
};

int main(int argc, const char **argv)
{
	int num_tests = sizeof(dma_dram_tests) / sizeof((dma_dram_tests)[0]);

	hltests_parser(argc, argv, usage, HLTHUNK_DEVICE_DONT_CARE,
			dma_dram_tests, num_tests);

	return hltests_run_group_tests("dma_dram", dma_dram_tests, num_tests,
					hltests_setup, hltests_teardown);
}
