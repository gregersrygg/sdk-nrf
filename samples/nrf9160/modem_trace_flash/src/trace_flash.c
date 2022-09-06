/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/zephyr.h>
#include <stdio.h>
#include <stddef.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <modem/trace_backend.h>

LOG_MODULE_REGISTER(modem_trace_flash_backend, CONFIG_MODEM_TRACE_FLASH_BACKEND_LOG_LEVEL);

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t littlefs_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)FLASH_AREA_ID(storage),
	.mnt_point = "/lfs",
};
static char filename[20];
static struct fs_file_t file;
static uint32_t tot_bytes_rcvd = 0;

int littlefs_init(void);
int littlefs_deinit(void);
int littlefs_write(const void *data, size_t len);

int trace_backend_init(trace_backend_processed_cb trace_processed_cb)
{
	return littlefs_init();
}

int trace_backend_deinit(void)
{
	return littlefs_deinit();
}

int trace_backend_write(const void *data, size_t len)
{
	return littlefs_write(data, len);
}


static int littlefs_erase(unsigned int id)
{
	const struct flash_area *fa;
	
	int err = flash_area_open(id, &fa);
	if (err) {
		LOG_ERR("Error opening flash area %u: %d\n", id, err);
		return err;
	}

	err = flash_area_erase(fa, 0, fa->fa_size);
	if (err) {
		LOG_ERR("Error erasing flash area %d", err);
		return err;
	}
	
	flash_area_close(fa);
	return 0;
}

int littlefs_init(void)
{
	int err;
	struct fs_statvfs sbuf;

	LOG_INF("Flash trace backend init");

	// TODO Kconfig for erasing flash or not
	err = littlefs_erase((uintptr_t) littlefs_mnt.storage_dev);
	if (err) {
		return err;
	}

	err = fs_mount(&littlefs_mnt);
	if (err) {
		LOG_ERR("fs_mount error: %d", err);
		return err;
	}
	LOG_INF("Mounted %s\n", littlefs_mnt.mnt_point);
	
	err = fs_statvfs(littlefs_mnt.mnt_point, &sbuf);
	if (err < 0) {
		LOG_ERR("statvfs: %d", err);
		fs_unmount(&littlefs_mnt);
		return err;
	}

	LOG_INF("%s: bsize = %lu ; frsize = %lu ;"
		   " blocks = %lu ; bfree = %lu",
		   littlefs_mnt.mnt_point,
		   sbuf.f_bsize, sbuf.f_frsize,
		   sbuf.f_blocks, sbuf.f_bfree);

	// err = lsdir(littlefs_mnt.mnt_point);
	// if (err < 0) {
	// 	LOG_ERR("lsdir %s: %d", littlefs_mnt.mnt_point, err);
	// 	fs_unmount(&littlefs_mnt);
	// 	return err;
	// }

	snprintf(filename, sizeof(filename), "%s/modem.trace", littlefs_mnt.mnt_point);
	LOG_INF("Storing modem traces to: %s", filename);

	struct fs_dir_t dirp;
	fs_dir_t_init(&dirp);

	fs_file_t_init(&file);
	int ret = fs_open(&file, filename, FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
	if (ret) {
		LOG_ERR("Error opening file %s: %d", filename, ret);
		return ret;
	}

	return 0;
}

void read_file(void)
{
	const uint16_t BUF_SIZE = 255;
	uint8_t data[BUF_SIZE];
	int bytes_read = 0;
	int ret = fs_open(&file, filename, FS_O_READ);
	if (ret) {
		LOG_ERR("Error opening file %s: %d", filename, ret);
		return;
	}

	while(true) {
		ret = fs_read(&file, data, BUF_SIZE);
		if (ret < 0) {
			LOG_ERR("fs_read error: %d", ret);
			break;
		} else if (ret == 0) {
			break;
		}
		bytes_read += ret;
	}

	LOG_INF("Modem trace bytes received from modem: %d", tot_bytes_rcvd);
	LOG_INF("Modem trace bytes read from flash: %d", bytes_read);

	ret = fs_close(&file);
	if (ret) {
		LOG_ERR("Error closing %s: %d", filename, ret);
	}
}

int littlefs_deinit(void)
{
	int err = fs_close(&file);
	if (err) {
		LOG_ERR("Error closing %s: %d", filename, err);
	}

	read_file();

	err = fs_unmount(&littlefs_mnt);
	if (err) {
		LOG_ERR("Error unmounting %s: %d", littlefs_mnt.mnt_point, err);
		return err;
	}

	LOG_INF("Unmounted %s", littlefs_mnt.mnt_point);
	return 0;
}

int littlefs_write(const void *data, size_t len)
{
	tot_bytes_rcvd += len;



	return fs_write(&file, data, len);
}

	// char fname1[MAX_PATH_LEN];
	// char fname2[MAX_PATH_LEN];

	// snprintf(fname1, sizeof(fname1), "%s/boot_count", littlefs_mnt.mnt_point);
	// err = littlefs_increase_infile_value(fname1);
	// if (err) {
	// 	LOG_ERR("littlefs_increase_infile_value error: %d", err);
	// 	fs_unmount(&littlefs_mnt);
	// 	return err;
	// }

	// snprintf(fname2, sizeof(fname2), "%s/pattern.bin", littlefs_mnt.mnt_point);
	// err = littlefs_binary_file_adj(fname2);
	// if (err) {
	// 	LOG_ERR("littlefs_binary_file_adj error: %d", err);
	// 	fs_unmount(&littlefs_mnt);
	// 	return err;
	// }

/*
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
	num_bytes_prev = tot_bytes_rcvd;
	uptime_prev = k_uptime_get_32();
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
*/