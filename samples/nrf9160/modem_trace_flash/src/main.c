/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <logging/log.h>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <modem/nrf_modem_lib_trace.h>
#include <nrf_modem_at.h>
//#include <storage/stream_flash.h>

LOG_MODULE_REGISTER(modem_trace_flash_sample, CONFIG_MODEM_TRACE_FLASH_SAMPLE_LOG_LEVEL);

// #define EXT_FLASH_DEVICE DT_LABEL(DT_INST(0, jedec_spi_nor))

// static const struct device *flash_dev;
// static struct stream_flash_ctx stream;

/* define callback */
LTE_LC_ON_CFUN(cfun_hook, on_cfun, NULL);

/* callback implementation */
static void on_cfun(enum lte_lc_func_mode mode, void *context)
{
	LOG_INF("LTE mode changed to %d\n", mode);
}

void main(void)
{
	int err;

	LOG_INF("Modem trace backend sample started\n");

	err = nrf_modem_lib_trace_level_set(NRF_MODEM_LIB_TRACE_LEVEL_FULL);
	if (err) {
		LOG_ERR("Failed to enable modem traces");
	}

	LOG_INF("Connecting to network\n");

	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_NORMAL);
	if (err) {
		LOG_ERR("Failed to change LTE mode, err %d\n", err);
		return;
	}

	k_sleep(K_SECONDS(5));

	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_POWER_OFF);
	if (err) {
		LOG_ERR("Failed to change LTE mode, err %d\n", err);
		return;
	}

	err = nrf_modem_lib_trace_level_set(NRF_MODEM_LIB_TRACE_LEVEL_OFF);
	if (err) {
		LOG_ERR("Failed to turn off modem traces");
	}

	nrf_modem_lib_shutdown();

	LOG_INF("Bye\n");
}
