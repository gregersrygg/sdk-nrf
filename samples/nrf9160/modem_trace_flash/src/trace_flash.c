/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/zephyr.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <modem/trace_backend.h>

LOG_MODULE_REGISTER(modem_trace_flash_backend, CONFIG_MODEM_TRACE_FLASH_BACKEND_LOG_LEVEL);

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
*/

/*
 * Copyright (c) 2022 Lukasz Majewski, DENX Software Engineering GmbH
 * Copyright (c) 2019 Peter Bigot Consulting, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Sample which uses the filesystem API with littlefs */

/*
#include <stdio.h>

#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(main);
*/

/* Matches LFS_NAME_MAX */
#define MAX_PATH_LEN 255
#define TEST_FILE_SIZE 547

static uint8_t file_test_pattern[TEST_FILE_SIZE];
static int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;

	fs_dir_t_init(&dirp);

	/* Verify fs_opendir() */
	res = fs_opendir(&dirp, path);
	if (res) {
		LOG_ERR("Error opening dir %s [%d]\n", path, res);
		return res;
	}

	LOG_PRINTK("\nListing dir %s ...\n", path);
	for (;;) {
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0) {
			if (res < 0) {
				LOG_ERR("Error reading dir [%d]\n", res);
			}
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			LOG_PRINTK("[DIR ] %s\n", entry.name);
		} else {
			LOG_PRINTK("[FILE] %s (size = %zu)\n",
				   entry.name, entry.size);
		}
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);

	return res;
}

static int littlefs_increase_infile_value(char *fname)
{
	uint8_t boot_count = 0;
	struct fs_file_t file;
	int rc, ret;

	fs_file_t_init(&file);
	rc = fs_open(&file, fname, FS_O_CREATE | FS_O_RDWR);
	if (rc < 0) {
		LOG_ERR("FAIL: open %s: %d", fname, rc);
		return rc;
	}

	rc = fs_read(&file, &boot_count, sizeof(boot_count));
	if (rc < 0) {
		LOG_ERR("FAIL: read %s: [rd:%d]", fname, rc);
		goto out;
	}
	LOG_INF("%s read count:%u (bytes: %d)", fname, boot_count, rc);

	rc = fs_seek(&file, 0, FS_SEEK_SET);
	if (rc < 0) {
		LOG_ERR("FAIL: seek %s: %d", fname, rc);
		goto out;
	}

	boot_count += 1;
	rc = fs_write(&file, &boot_count, sizeof(boot_count));
	if (rc < 0) {
		LOG_ERR("FAIL: write %s: %d", fname, rc);
		goto out;
	}

	LOG_INF("%s write new boot count %u: [wr:%d]", fname,
		   boot_count, rc);

 out:
	ret = fs_close(&file);
	if (ret < 0) {
		LOG_ERR("FAIL: close %s: %d", fname, ret);
		return ret;
	}

	return (rc < 0 ? rc : 0);
}

static void incr_pattern(uint8_t *p, uint16_t size, uint8_t inc)
{
	uint8_t fill = 0x55;

	if (p[0] % 2 == 0) {
		fill = 0xAA;
	}

	for (int i = 0; i < (size - 1); i++) {
		if (i % 8 == 0) {
			p[i] += inc;
		} else {
			p[i] = fill;
		}
	}

	p[size - 1] += inc;
}

static void init_pattern(uint8_t *p, uint16_t size)
{
	uint8_t v = 0x1;

	memset(p, 0x55, size);

	for (int i = 0; i < size; i += 8) {
		p[i] = v++;
	}

	p[size - 1] = 0xAA;
}

static void print_pattern(uint8_t *p, uint16_t size)
{
	int i, j = size / 16, k;

	for (k = 0, i = 0; k < j; i += 16, k++) {
		LOG_PRINTK("%02x %02x %02x %02x %02x %02x %02x %02x ",
			   p[i], p[i+1], p[i+2], p[i+3],
			   p[i+4], p[i+5], p[i+6], p[i+7]);
		LOG_PRINTK("%02x %02x %02x %02x %02x %02x %02x %02x\n",
			   p[i+8], p[i+9], p[i+10], p[i+11],
			   p[i+12], p[i+13], p[i+14], p[i+15]);

		/* Mark 512B (sector) chunks of the test file */
		if ((k + 1) % 32 == 0) {
			LOG_PRINTK("\n");
		}
	}

	for (; i < size; i++) {
		LOG_PRINTK("%02x ", p[i]);
	}

	LOG_PRINTK("\n");
}

static int littlefs_binary_file_adj(char *fname)
{
	struct fs_dirent dirent;
	struct fs_file_t file;
	int rc, ret;

	/*
	 * Uncomment below line to force re-creation of the test pattern
	 * file on the littlefs FS.
	 */
	/* fs_unlink(fname); */
	fs_file_t_init(&file);

	rc = fs_open(&file, fname, FS_O_CREATE | FS_O_RDWR);
	if (rc < 0) {
		LOG_ERR("FAIL: open %s: %d", fname, rc);
		return rc;
	}

	rc = fs_stat(fname, &dirent);
	if (rc < 0) {
		LOG_ERR("FAIL: stat %s: %d", fname, rc);
		goto out;
	}

	/* Check if the file exists - if not just write the pattern */
	if (rc == 0 && dirent.type == FS_DIR_ENTRY_FILE && dirent.size == 0) {
		LOG_INF("Test file: %s not found, create one!",
			fname);
		init_pattern(file_test_pattern, sizeof(file_test_pattern));
	} else {
		rc = fs_read(&file, file_test_pattern,
			     sizeof(file_test_pattern));
		if (rc < 0) {
			LOG_ERR("FAIL: read %s: [rd:%d]",
				fname, rc);
			goto out;
		}
		incr_pattern(file_test_pattern, sizeof(file_test_pattern), 0x1);
	}

	// LOG_INF("------ FILE: %s ------", fname);
	// print_pattern(file_test_pattern, sizeof(file_test_pattern));

	rc = fs_seek(&file, 0, FS_SEEK_SET);
	if (rc < 0) {
		LOG_ERR("FAIL: seek %s: %d", fname, rc);
		goto out;
	}

	rc = fs_write(&file, file_test_pattern, sizeof(file_test_pattern));
	if (rc < 0) {
		LOG_ERR("FAIL: write %s: %d", fname, rc);
	}

 out:
	ret = fs_close(&file);
	if (ret < 0) {
		LOG_ERR("FAIL: close %s: %d", fname, ret);
		return ret;
	}

	return (rc < 0 ? rc : 0);
}

static int littlefs_flash_erase(unsigned int id)
{
	const struct flash_area *pfa;
	int rc;

	rc = flash_area_open(id, &pfa);
	if (rc < 0) {
		LOG_ERR("unable to find flash area %u: %d\n",
			id, rc);
		return rc;
	}

	LOG_PRINTK("Area %u at 0x%x on %s for %u bytes\n",
		   id, (unsigned int)pfa->fa_off, pfa->fa_dev->name,
		   (unsigned int)pfa->fa_size);

	/* Optional wipe flash contents */
	if (IS_ENABLED(CONFIG_APP_WIPE_STORAGE)) {
		rc = flash_area_erase(pfa, 0, pfa->fa_size);
		LOG_ERR("Erasing flash area ... %d", rc);
	}

	flash_area_close(pfa);
	return rc;
}
// #define PARTITION_NODE DT_NODELABEL(lfs1)

// #if DT_NODE_EXISTS(PARTITION_NODE)
// FS_FSTAB_DECLARE_ENTRY(PARTITION_NODE);
// #else /* PARTITION_NODE */
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)FLASH_AREA_ID(storage),
	.mnt_point = "/lfs",
};
// #endif /* PARTITION_NODE */

	struct fs_mount_t *mp =
// #if DT_NODE_EXISTS(PARTITION_NODE)
// 		&FS_FSTAB_ENTRY(PARTITION_NODE)
// #else
		&lfs_storage_mnt
// #endif
		;

static int littlefs_mount(struct fs_mount_t *mp)
{
	int rc;

	rc = littlefs_flash_erase((uintptr_t)mp->storage_dev);
	if (rc < 0) {
		return rc;
	}

	/* Do not mount if auto-mount has been enabled */
// #if !DT_NODE_EXISTS(PARTITION_NODE) ||						\
// 	!(FSTAB_ENTRY_DT_MOUNT_FLAGS(PARTITION_NODE) & FS_MOUNT_FLAG_AUTOMOUNT)
	rc = fs_mount(mp);
	if (rc < 0) {
		LOG_ERR("mount id %" PRIuPTR " at %s: %d",
		       (uintptr_t)mp->storage_dev, mp->mnt_point, rc);
		return rc;
	}
	LOG_PRINTK("%s mount: %d\n", mp->mnt_point, rc);
// #else
// 	LOG_PRINTK("%s automounted\n", mp->mnt_point);
// #endif

	return 0;
}

int trace_backend_init(trace_backend_processed_cb trace_processed_cb)
{
	char fname1[MAX_PATH_LEN];
	char fname2[MAX_PATH_LEN];
	struct fs_statvfs sbuf;
	int rc;

	LOG_INF("Flash trace backend init");

	rc = littlefs_mount(mp);
	if (rc < 0) {
		LOG_ERR("littlefs_mount: %d", rc);
		return rc;
	}

	rc = fs_statvfs(mp->mnt_point, &sbuf);
	if (rc < 0) {
		LOG_ERR("statvfs: %d", rc);
		fs_unmount(mp);
		return rc;
	}

	LOG_INF("%s: bsize = %lu ; frsize = %lu ;"
		   " blocks = %lu ; bfree = %lu",
		   mp->mnt_point,
		   sbuf.f_bsize, sbuf.f_frsize,
		   sbuf.f_blocks, sbuf.f_bfree);

	rc = lsdir(mp->mnt_point);
	if (rc < 0) {
		LOG_ERR("lsdir %s: %d", mp->mnt_point, rc);
		fs_unmount(mp);
		return rc;
	}

	snprintf(fname1, sizeof(fname1), "%s/boot_count", mp->mnt_point);
	rc = littlefs_increase_infile_value(fname1);
	if (rc) {
		LOG_ERR("littlefs_increase_infile_value error: %d", rc);
		fs_unmount(mp);
		return rc;
	}

	snprintf(fname2, sizeof(fname2), "%s/pattern.bin", mp->mnt_point);
	rc = littlefs_binary_file_adj(fname2);
	if (rc) {
		LOG_ERR("littlefs_binary_file_adj error: %d", rc);
		fs_unmount(mp);
		return rc;
	}

	LOG_INF("Flash trace backend initialized");
	return 0;
}

int trace_backend_deinit(void)
{
	LOG_INF("Flash trace backend deinitialized");

	int rc = fs_unmount(mp);
	LOG_INF("%s unmount: %d", mp->mnt_point, rc);

	return 0;
}

int trace_backend_write(const void *data, size_t len)
{
	return (int)len;
}
