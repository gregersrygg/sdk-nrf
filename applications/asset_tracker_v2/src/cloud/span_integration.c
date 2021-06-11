#include "cloud/cloud_wrapper.h"
#include <zephyr.h>
#include <net/socket.h>

#define MODULE span_integration

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CLOUD_INTEGRATION_LOG_LEVEL);

const k_tid_t receive_thread;
K_SEM_DEFINE(connected_sem, 0, 1);

static cloud_wrap_evt_handler_t wrapper_evt_handler;

static void cloud_wrapper_notify_event(const struct cloud_wrap_event *evt)
{
	if ((wrapper_evt_handler != NULL) && (evt != NULL)) {
		wrapper_evt_handler(evt);
	} else {
		LOG_ERR("Library event handler not registered, or empty event");
	}
}
static struct k_delayed_work connect_work;

// Thread for receiving UDP packets
static void receive_listener()
{
	int sock;
	
	struct sockaddr_in addr = {
		sin_family : AF_INET,
		sin_port: htons(CONFIG_SPAN_IOT_RX_PORT),
	};

	struct sockaddr client_addr;
	socklen_t client_addr_len = sizeof(client_addr);

	struct cloud_wrap_event cloud_wrap_evt = { 0 };
	cloud_wrap_evt.type = CLOUD_WRAP_EVT_DATA_RECEIVED;

	int len;
	char *rx_buffer;
	const size_t RX_BUFFER_SIZE = 1280;

	// Wait until connect is called
	k_sem_take(&connected_sem, K_FOREVER);

	LOG_INF("Listening for UDP packets on port %d", CONFIG_SPAN_IOT_RX_PORT);

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("Failed to create UDP socket: %d", errno);
		// PANIC?
		return;
	}

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		LOG_ERR("Failed to bind UDP socket: %d", errno);
		// PANIC?
		return;
	}
	
	while (true) {
		rx_buffer = k_calloc(RX_BUFFER_SIZE, sizeof(RX_BUFFER_SIZE));
		if (!rx_buffer) {
			LOG_ERR("unable to allocate memory for packet");
			return;
		}

		len = recvfrom(sock, rx_buffer, RX_BUFFER_SIZE, 0, &client_addr,
					&client_addr_len);

		if (len < 0) {
			LOG_ERR("recvfrom error: %d", errno);
			continue;
		}

		if (len > 0) {
			LOG_DBG("\n*** Received %d bytes *** \n", len);
			cloud_wrap_evt.data.buf = rx_buffer;
			cloud_wrap_evt.data.len = len;
			cloud_wrapper_notify_event(&cloud_wrap_evt);
		}
	}
}

void notify_connect()
{
	struct cloud_wrap_event cloud_wrap_evt = { 0 };
	cloud_wrap_evt.type = CLOUD_WRAP_EVT_CONNECTED;
	cloud_wrapper_notify_event(&cloud_wrap_evt);
	cloud_wrap_evt.type = CLOUD_WRAP_EVT_CONNECTED;
	cloud_wrapper_notify_event(&cloud_wrap_evt);
}

/** Setup and initialize the configured cloud integration layer. */
int cloud_wrap_init(cloud_wrap_evt_handler_t event_handler)
{

	LOG_DBG("********************************************");
	LOG_DBG(" The Asset Tracker v2 has started");
	LOG_DBG(" Version:     %s", CONFIG_ASSET_TRACKER_V2_APP_VERSION);
	LOG_DBG(" Cloud:       %s", "Lab5e Span");
	LOG_DBG(" Endpoint:    %s", "172.16.15.14:1234");
	LOG_DBG("********************************************");

	k_delayed_work_init(&connect_work, notify_connect);

	wrapper_evt_handler = event_handler;

	return 0;
}

/** Connect to cloud. */
int cloud_wrap_connect(void)
{
	LOG_DBG("cloud_wrap_connect");

	k_sem_give(&connected_sem);

	// Use delayed work to send connect event async
	k_delayed_work_submit(&connect_work, K_SECONDS(1));

	return 0;
}

/** Disconnect from cloud. */
int cloud_wrap_disconnect(void)
{
	// Not implemented
	return 0;
}

/** Request device state from cloud. The device state contains the device
 * configuration.
 */
int cloud_wrap_state_get(void)
{
	// Not implemented
	return 0;
}

/** Send data to the device state. */
int cloud_wrap_state_send(char *buf, size_t len)
{
	// Not implemented
	return 0;
}

/** Send data to cloud. */
int cloud_wrap_data_send(char *buf, size_t len)
{
	LOG_DBG("cloud_wrap_data_send len: %d", len);

	if (len == 0) {
		LOG_ERR("Data can't have 0 length");
		return -1;
	}

	/* Create a new UDP socket */
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("Error opening socket: %d", sock);
		return -1;
	}

	LOG_DBG("socket number: %d", sock);

	static struct sockaddr_in remote_addr = {
		sin_family : AF_INET,
		sin_port: htons(CONFIG_SPAN_IOT_SERVER_PORT),
	};

	/* Set up the remote address for Span; 172.16.15.14 port 1234 */
	int err = net_addr_pton(AF_INET, CONFIG_SPAN_IOT_SERVER_IP, &remote_addr.sin_addr);
	if (err < 0) {
		LOG_ERR("Error configuring remote address: %d", err);
	}

	err = sendto(sock, buf, len, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
	close(sock);
	if (err < 0) {
		LOG_ERR("Unable to send data: %d", err);
		return err;
	} else {
		LOG_DBG("Send success!");
	}

	return 0;
}

/** Send batched data to cloud. */
int cloud_wrap_batch_send(char *buf, size_t len)
{
	return cloud_wrap_data_send(buf, len);
}

/** Send UI data to cloud. Button presses. */
int cloud_wrap_ui_send(char *buf, size_t len)
{
	return cloud_wrap_data_send(buf, len);
}

K_THREAD_DEFINE(receive_thread, CONFIG_SPAN_IOT_THREAD_STACK_SIZE,
		receive_listener, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);