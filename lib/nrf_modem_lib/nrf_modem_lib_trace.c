/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <kernel.h>
#include <zephyr.h>
#include <modem/nrf_modem_lib_trace.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <logging/log.h>
#ifdef CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_UART
#include <nrfx_uarte.h>
#define UNUSED_FLAGS 0
#endif
#ifdef CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_RTT
#include <SEGGER_RTT.h>
#endif

LOG_MODULE_REGISTER(nrf_modem_lib_trace, CONFIG_NRF_MODEM_LIB_LOG_LEVEL);

#ifdef CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_UART
static const nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(1);
K_SEM_DEFINE(tx_sem, 1, 1);
#endif

#ifdef CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_RTT
static int trace_rtt_channel;
static char rtt_buffer[CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_RTT_BUF_SIZE];
#endif

static bool is_transport_initialized;

struct trace_data_t {
	void *fifo_reserved; /* 1st word reserved for use by fifo */
	const uint8_t * const data;
	uint32_t len;
};

K_FIFO_DEFINE(trace_fifo);

#define TRACE_THREAD_STACK_SIZE 2048
#define TRACE_THREAD_PRIORITY CONFIG_NRF_MODEM_LIB_TRACE_THREAD_PRIO

void trace_handler_thread(void)
{
	while (1) {
		struct trace_data_t *trace_data = k_fifo_get(&trace_fifo, K_FOREVER);
		const uint8_t * const data = trace_data->data;
		const uint32_t len = trace_data->len;

#ifdef CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_UART
		/* Split RAM buffer into smaller chunks to be transferred using DMA. */
		const uint32_t MAX_BUF_LEN = (1 << UARTE1_EASYDMA_MAXCNT_SIZE) - 1;
		uint32_t remaining_bytes = len;
		nrfx_err_t err;

		while (remaining_bytes) {
			size_t transfer_len = MIN(remaining_bytes, MAX_BUF_LEN);
			uint32_t idx = len - remaining_bytes;

			if (k_sem_take(&tx_sem, K_MSEC(100)) != 0) {
				LOG_WRN("UARTE TX not available!");
				break;
			}
			err = nrfx_uarte_tx(&uarte_inst, &data[idx], transfer_len);
			if (err != NRFX_SUCCESS) {
				LOG_ERR("nrfx_uarte_tx error: %d", err);
				break;
			}
			remaining_bytes -= transfer_len;
		}
#endif

#ifdef CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_RTT
		uint32_t remaining_bytes = len;

		while (remaining_bytes) {
			uint16_t transfer_len = MIN(remaining_bytes,
						CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_RTT_BUF_SIZE);
			uint32_t idx = len - remaining_bytes;

			SEGGER_RTT_WriteSkipNoLock(trace_rtt_channel, &data[idx], transfer_len);
			remaining_bytes -= transfer_len;
		}

		int err = nrf_modem_trace_processed_callback(data, len);

		__ASSERT(err == 0,
			"nrf_modem_trace_processed_callback failed with error code %d", err);
#endif

		k_free(trace_data);
	}
}

K_THREAD_DEFINE(trace_thread_id, TRACE_THREAD_STACK_SIZE, trace_handler_thread,
	NULL, NULL, NULL, TRACE_THREAD_PRIORITY, 0, 0);

#ifdef CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_UART
static void uarte_callback(nrfx_uarte_event_t const *p_event, void *p_context)
{
	int err = 0;

	if (k_sem_count_get(&tx_sem) != 0) {
		LOG_ERR("uart semaphore not in use");
		return;
	}

	if (p_event->type == NRFX_UARTE_EVT_ERROR) {
		LOG_ERR("uarte error 0x%04x", p_event->data.error.error_mask);

		k_sem_give(&tx_sem);
		err = nrf_modem_trace_processed_callback(p_event->data.error.rxtx.p_data,
				p_event->data.error.rxtx.bytes);
	}

	if (p_event->type == NRFX_UARTE_EVT_TX_DONE) {
		k_sem_give(&tx_sem);
		err = nrf_modem_trace_processed_callback(p_event->data.rxtx.p_data,
				p_event->data.rxtx.bytes);
	}

	__ASSERT(err == 0, "nrf_modem_trace_processed_callback failed with error code %d", err);
}

static bool uart_init(void)
{
	const uint8_t irq_priority = DT_IRQ(DT_NODELABEL(uart1), priority);
	const nrfx_uarte_config_t config = {
		.pseltxd = DT_PROP(DT_NODELABEL(uart1), tx_pin),
		.pselrxd = DT_PROP(DT_NODELABEL(uart1), rx_pin),
		.pselcts = NRF_UARTE_PSEL_DISCONNECTED,
		.pselrts = NRF_UARTE_PSEL_DISCONNECTED,

		.hal_cfg.hwfc = NRF_UARTE_HWFC_DISABLED,
		.hal_cfg.parity = NRF_UARTE_PARITY_EXCLUDED,
		.baudrate = NRF_UARTE_BAUDRATE_1000000,

		.interrupt_priority = irq_priority,
		.p_context = NULL,
	};

	IRQ_CONNECT(DT_IRQN(DT_NODELABEL(uart1)),
		irq_priority,
		nrfx_isr,
		&nrfx_uarte_1_irq_handler,
		UNUSED_FLAGS);
	return (nrfx_uarte_init(&uarte_inst, &config, &uarte_callback) ==
		NRFX_SUCCESS);
}
#endif

#ifdef CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_RTT
static bool rtt_init(void)
{
	trace_rtt_channel = SEGGER_RTT_AllocUpBuffer("modem_trace", rtt_buffer, sizeof(rtt_buffer),
						     SEGGER_RTT_MODE_NO_BLOCK_SKIP);

	return (trace_rtt_channel > 0);
}
#endif

int nrf_modem_lib_trace_init(void)
{
#ifdef CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_UART
	is_transport_initialized = uart_init();
#endif
#ifdef CONFIG_NRF_MODEM_LIB_TRACE_MEDIUM_RTT
	is_transport_initialized = rtt_init();
#endif

	if (!is_transport_initialized) {
		return -EBUSY;
	}
	return 0;
}

int nrf_modem_lib_trace_start(enum nrf_modem_lib_trace_mode trace_mode)
{
	if (!is_transport_initialized) {
		return -ENXIO;
	}

	if (nrf_modem_at_printf("AT%%XMODEMTRACE=1,%hu", trace_mode) != 0) {
		return -EOPNOTSUPP;
	}

	return 0;
}

int nrf_modem_lib_trace_process(const uint8_t *data, uint32_t len)
{
	if (!is_transport_initialized) {
		int err = nrf_modem_trace_processed_callback(data, len);

		__ASSERT(err == 0,
			"nrf_modem_trace_processed_callback failed with error code %d", err);
		return -ENXIO;
	}

	struct trace_data_t trace_data = { .data = data, .len = len };
	size_t size = sizeof(struct trace_data_t);
	char *mem_ptr = k_malloc(size);
	__ASSERT(mem_ptr != 0, "Out of memory");

	memcpy(mem_ptr, &trace_data, size);

	k_fifo_put(&trace_fifo, mem_ptr);

	return 0;
}

int nrf_modem_lib_trace_stop(void)
{
	__ASSERT(!k_is_in_isr(),
		"nrf_modem_lib_trace_stop cannot be called from interrupt context");

	if (nrf_modem_at_printf("AT%%XMODEMTRACE=0") != 0) {
		return -EOPNOTSUPP;
	}

	return 0;
}
