/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <sys/errno.h>
#include <storage/stream_flash.h>
#include <stdio.h>
#include <string.h>
#include <logging/log.h>
#include <modem/trace_backend.h>

LOG_MODULE_REGISTER(modem_trace_flash_backend, CONFIG_MODEM_TRACE_FLASH_BACKEND);

#define EXT_FLASH_DEVICE DEVICE_DT_GET(DT_ALIAS(ext_flash))
#define BUF_LEN 1024

static const struct device *flash_dev;
static struct stream_flash_ctx stream;
static uint8_t buf[BUF_LEN];

static uint32_t uptime_prev;
static uint32_t num_bytes_prev;
static uint32_t tot_bytes_rcvd;
static trace_backend_processed_cb trace_processed_callback;

static int read_data(void);

int trace_backend_init(trace_backend_processed_cb trace_processed_cb)
{
	printk("trace_backend_init\n");
	if (trace_processed_cb == NULL) {
		return -EFAULT;
	}

	trace_processed_callback = trace_processed_cb;

	printk("Modem trace flash backend init\n");

	flash_dev = EXT_FLASH_DEVICE;
	if (flash_dev == NULL) {
		printk("Failed to get flash device\n");
		return -ENODEV;
	}

	int err = stream_flash_init(&stream, flash_dev, buf, BUF_LEN, 0, 0, NULL);
	if (err) {
		printf("stream_flash_init failed (err %d)", err);
		return err;
	}

	num_bytes_prev = tot_bytes_rcvd;
	uptime_prev = k_uptime_get_32();

	printk("Modem trace flash backend initialized\n");

	return 0;
}

int trace_backend_deinit(void)
{
	printk("Flash trace backend deinitialized\n");


	// TODO add workqueue
	printf("%u bytes were written\n", tot_bytes_rcvd);

	read_data();

	return 0;
}

int trace_backend_write(const void *data, size_t len)
{
	NRF_P0_NS->DIRSET = (1 << 17);
	int64_t uptime_ref = k_uptime_get();

	NRF_P0_NS->OUTSET = (1 << 17);
	int err = stream_flash_buffered_write(&stream, data, len, true);

	if (err != 0) {
		printf("stream_flash_buffered_write error %d", err);
		return err;
	}
	NRF_P0_NS->OUTCLR = (1 << 17);

	uint64_t transfer_time = k_uptime_delta(&uptime_ref);

	// printf("Written %d bytes in %lld ms", len, transfer_time);
	// printf("Throughput = %lld bps ", ((sizeof(write_buf) * 8 / transfer_time) * 1000));

	tot_bytes_rcvd += len;
	trace_processed_callback(len);

	return (int)len;
}

static int read_data(void)
{
	NRF_P0_NS->DIRSET = (1 << 19);
	static uint8_t read_buf[1024];
	// static uint32_t tot_bytes_rcvd = 5; // TODO extern this
	// const uint32_t size_to_read = sizeof(read_buf);
	const uint32_t read_offset = 0;

	int64_t ticks_before_read = k_uptime_ticks();

	NRF_P0_NS->OUTSET = (1 << 19);
	int err = flash_read(flash_dev, read_offset, read_buf, tot_bytes_rcvd);

	if (err != 0) {
		printf("flash_read error %d", err);
		return err;
	}
	NRF_P0_NS->OUTCLR = (1 << 19);

	int64_t ticks_after_read = k_uptime_ticks();
	uint64_t time_taken_us = k_ticks_to_us_ceil64(ticks_after_read - ticks_before_read);

	printf("Read %u bytes in %lld us", tot_bytes_rcvd, time_taken_us);
	// printf("Throughput = %lld bps ", ((size_to_read * 8 / time_taken_us) * 1000000));

	return 0;
}
