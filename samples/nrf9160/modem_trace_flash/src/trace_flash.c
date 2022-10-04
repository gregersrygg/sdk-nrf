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
static int64_t tot_write_time;

int littlefs_init(void);
int littlefs_deinit(void);
int littlefs_write(const void *data, size_t len);
void read_file(void);

static trace_backend_processed_cb trace_processed_callback;
int trace_backend_init(trace_backend_processed_cb trace_processed_cb)
{
	if (trace_processed_cb == NULL) {
		return -EFAULT;
	}

	trace_processed_callback = trace_processed_cb;
	tot_write_time = 0;

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
	// err = littlefs_erase((uintptr_t) littlefs_mnt.storage_dev);
	// if (err) {
	//     return err;
	// }

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

	struct fs_dir_t dirp;

	fs_dir_t_init(&dirp);
	fs_file_t_init(&file);

	// err = lsdir(littlefs_mnt.mnt_point);
	// if (err < 0) {
	// 	LOG_ERR("lsdir %s: %d", littlefs_mnt.mnt_point, err);
	// 	fs_unmount(&littlefs_mnt);
	// 	return err;
	// }

	snprintf(filename, sizeof(filename), "%s/modem.trace", littlefs_mnt.mnt_point);
	LOG_INF("Storing modem traces to: %s", filename);

	// LOG_INF("Reading %s from previous run", filename);
	// read_file();

	// LOG_INF("Deleting %s", filename);
	// fs_unlink(filename);

	LOG_INF("Creating %s", filename);
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
	int file_num_bytes = 0;
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
		file_num_bytes += ret;
	}

	LOG_INF("Modem trace bytes read from flash: %d", file_num_bytes);

	if (tot_bytes_rcvd > 0) {
		LOG_INF("Modem trace bytes received from modem: %d", tot_bytes_rcvd);
		LOG_INF("Written %d bytes in %lld ms", file_num_bytes, tot_write_time);
		LOG_INF("Throughput = %lld kb/s ", (file_num_bytes * 8 / tot_write_time));
	}

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
	ssize_t ret;
	tot_bytes_rcvd += len;

	int64_t uptime_ref = k_uptime_get();
	int64_t write_time;

	ret = fs_write(&file, data, len);
	if (ret < 0) {
		LOG_ERR("fs_write error: %d", (int) ret);
	} else if (ret < len) {
		LOG_ERR("fs_write errno: %d (full flash?)", errno);
		ret = errno;
	}

	write_time = k_uptime_delta(&uptime_ref);
	tot_write_time += write_time;

	int err = trace_processed_callback(len);

	if (err) {
		LOG_ERR("trace_processed_callback error: %d", err);
		return err;
	}

	return ret;
}
