/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <storage/stream_flash.h>
#include <stdio.h>
#include <string.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(ext_flash_test, CONFIG_EXT_FLASH_TEST_LOG_LEVEL);

#define EXT_FLASH_DEVICE DT_LABEL(DT_INST(0, jedec_spi_nor))

static const struct device *flash_dev;

#define BUF_LEN 512
#define MAX_PAGE_SIZE 0x1000 /* Max supported page size to run test on */
#define MAX_NUM_PAGES 4      /* Max number of pages used in these tests */
#define TESTBUF_SIZE (MAX_PAGE_SIZE * MAX_NUM_PAGES)

static struct stream_flash_ctx stream;
static uint8_t buf[BUF_LEN];
const static uint8_t write_buf[TESTBUF_SIZE] = {[0 ... TESTBUF_SIZE - 1] = 0xaa};

static int write_data(void)
{
	NRF_P0_NS->DIRSET = (1 << 17);
	int64_t uptime_ref = k_uptime_get();

	NRF_P0_NS->OUTSET = (1 << 17);
	int err = stream_flash_buffered_write(&stream, write_buf, TESTBUF_SIZE, true);
	if (err != 0) {
		LOG_ERR("stream_flash_buffered_write error %d", err);
		return err;
	}
	NRF_P0_NS->OUTCLR = (1 << 17);

	uint64_t transfer_time = k_uptime_delta(&uptime_ref);

	LOG_INF("Written %d bytes in %lld ms", sizeof(write_buf), transfer_time);
	LOG_INF("Throughput = %lld bps ", ((sizeof(write_buf) * 8 / transfer_time) * 1000));

	return 0;
}

static int read_data(void)
{
	NRF_P0_NS->DIRSET = (1 << 19);

	static uint8_t read_buf[TESTBUF_SIZE];
	const uint32_t size_to_read = sizeof(read_buf);
	const uint32_t read_offset = 0;

	int64_t ticks_before_read = k_uptime_ticks();

	NRF_P0_NS->OUTSET = (1 << 19);
	int err = flash_read(flash_dev, read_offset, read_buf, size_to_read);

	if (err != 0) {
		LOG_ERR("flash_read error %d", err);
		return err;
	}
	NRF_P0_NS->OUTCLR = (1 << 19);

	int64_t ticks_after_read = k_uptime_ticks();
	uint64_t time_taken_us = k_ticks_to_us_ceil64(ticks_after_read - ticks_before_read);


	LOG_INF("Read %d bytes in %lld us", size_to_read, time_taken_us);
	LOG_INF("Throughput = %lld bps ", ((size_to_read * 8 / time_taken_us) * 1000000));

	return 0;
}

void main(void)
{
	int err;

	printk("External flash test started\n");

	flash_dev = device_get_binding(EXT_FLASH_DEVICE);
	if (flash_dev == NULL) {
		printk("Failed to get flash device: %s\n", EXT_FLASH_DEVICE);
		return;
	}

	err = stream_flash_init(&stream, flash_dev, buf, BUF_LEN,
				0, 0, NULL);
	if (err) {
		LOG_ERR("stream_flash_init failed (err %d)", err);
	}

	err = write_data();

	if (err) {
		LOG_ERR("write_data returns error = %d", err);
	}

	err = read_data();

	if (err) {
		LOG_ERR("read_data returns error = %d", err);
	}
}
