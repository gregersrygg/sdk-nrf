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

int write_data(void) {
	int err = stream_flash_buffered_write(&stream, write_buf, TESTBUF_SIZE, false);
	if (err != 0) {
		LOG_ERR("stream_flash_buffered_write error %d", err);
		return err;
	}
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

	write_data();
}
