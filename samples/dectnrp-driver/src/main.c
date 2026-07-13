/*
 * Copyright (c) 2026 Codium Electronique
 * Copyright (c) 2026 Deveritec GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dectnrp_driver_sample, CONFIG_SAMPLE_DRIVER_DECTNRP_LOG_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/random/random.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dectnrp_net_l2.h>
#include <zephyr/net/dectnrp_driver.h>
#include <opener/build_info.h>
#include "dectnrp_driver_utils.h"
#include "dectnrp_operation.h"
#include "dectnrp_mac_helper.h"


#define TIME_2_MODEMTICKS(TIME_MS) (DECTNRP_MODEMTICKS_PER_SYMBOL * DECTNRP_SYMBOLS_PER_FRAME * TIME_MS / 10)


extern struct k_fifo dectnrp_event_fifo;

static uint32_t network_id = CONFIG_DECTNRP_SAMPLE_NETWORK_ID;

enum state {
	DEVICE_STATE_UNSYNCHRONIZED,
	DEVICE_STATE_SYNCHRONIZING,
	DEVICE_STATE_SYNCHRONIZED,
	DEVICE_STATE_DESYNCHRONIZING,
};

static struct local_device {
	enum state state;
	uint32_t long_device_id;
	uint16_t short_device_id;
} local_device;

static struct remote_device {
	uint16_t short_device_id;
	net_time_t last_start_time;
} remote_device;

struct dectnrp_event_wrapper {
    void *fifo_reserved; /* 1st word reserved for use by FIFO */
    struct dectnrp_driver_event event;
};

/**
 * @brief Allocates and creates a rx-operation and schedules it imediately via dectnrp_driver.
 * 
 * @param iface 
 * @param duration_ms Length of the rx operation in modem ticks.
 * @return int 
 */
static int receive(struct net_if *iface, uint32_t duration_ms, bool *received) {

	*received = false;
	struct dectnrp_driver_op *op = NULL;
	int ret = dectnrp_alloc_rx_operation(&op, iface, CONFIG_DECTNRP_SAMPLE_CHANNEL);
	if(ret != 0) {
		LOG_ERR("dectnrp_alloc_rx_operation failed, %d", ret);
	} else {
		
		op->rx.duration = TIME_2_MODEMTICKS(duration_ms);

		LOG_INF("receive ...");

		ret = dectnrp_driver_schedule(iface, op);
		if(ret != 0) {
			LOG_ERR("dectnrp_driver_schedule(rx) failed, %d", ret);
			dectnrp_free_operation(op);
		} else {
			bool rx_finished = false;
			bool rx = false;
			while(!rx_finished && (ret == 0)) {
	
				uint32_t timeout = duration_ms + duration_ms/2;
				struct dectnrp_event_wrapper *item = k_fifo_get(&dectnrp_event_fifo, K_MSEC(timeout));

				// LOG_ERR("k_fifo_is_empty=%u", k_fifo_is_empty(&dectnrp_event_fifo));

				if(item == NULL) {
					LOG_ERR("receive event(s) missing");
					dectnrp_free_operation(op);
					op = NULL;
					ret = -EIO;
				} else {

					struct dectnrp_driver_event *event = &item->event;
	
					if(event->code == DECTNRP_EVENT_MSG_RECEIVED) {
						
						__ASSERT(event->msg_received.pcc != NULL, "event->msg_received.pcc == NULL");
						__ASSERT(event->msg_received.pdc != NULL, "event->msg_received.pdc == NULL");
						// LOG_ERR("EVENT_MSG_RECEIVED");

						uint8_t pcc_len = event->msg_received.phy_type == 0 ? DECTNRP_PHY_HEADER_TYPE1_SIZE : DECTNRP_PHY_HEADER_TYPE2_SIZE;

						remote_device.short_device_id = pcc_decode_transmitter_short_id(event->msg_received.pcc, pcc_len);
						remote_device.last_start_time = event->msg_received.start_time;
						rx = true;
	
						//LOG_INF("RX:");
						LOG_HEXDUMP_INF(event->msg_received.pcc, pcc_len, "RX:PCC:");
						LOG_HEXDUMP_INF(event->msg_received.pdc, event->msg_received.pdc_len, "PDC:");
	
					} else if(event->code == DECTNRP_EVENT_OP_FINISHED) {
						__ASSERT(event->op_finished.op != NULL, "event->op_finished.op == NULL");
						__ASSERT(op == event->op_finished.op, "op != event->op_finished.op");
						// LOG_ERR("EVENT_OP_FINISHED");
						//LOG_INF("OP_FINISHED:0x%x, 0x%x", event->op_finished.op, event->op_finished.op->status);
						dectnrp_free_operation(event->op_finished.op);
						LOG_INF("receive complete");
						rx_finished = true;
						*received = rx;
						
					} else if(event->code == DECTNRP_EVENT_MSG_ERROR) {
						LOG_ERR("MSG_ERROR:0x%x", event->message_error.status);
					} else {
						LOG_ERR("event->code=%u", event->code);
					}

					k_free(item);
				}

			}
		}
	}

    return ret;
}

/**
 * @brief Allocates and creates a net_pkt from given data and its parameters and transmits it via dectnrp_driver.
 * 
 * @param iface 
 * @param data 
 * @param size 
 * @param parameters 
 * @return int 
 */
static int transmit(struct net_if *iface, const uint8_t* data, size_t size, 
	uint8_t phy_header_type,
    const struct dectnrp_transport_parameters* parameters,
	net_time_t start_time) {

	__ASSERT(iface != NULL, "iface == NULL");
	__ASSERT(data != NULL, "data == NULL");
	__ASSERT(parameters != NULL, "parameters == NULL");
    int ret = 0;

    struct net_pkt *pkt = net_pkt_alloc_with_buffer(iface, DECTNRP_MTU, AF_PACKET, IPPROTO_RAW, K_MSEC(50));
    if (!pkt) {
        NET_ERR("Could not allocate pkt");
        ret = -ENOMEM;
    } else {

        struct dectnrp_driver_op *op = NULL;
        int ret = dectnrp_alloc_tx_operation(&op, iface, CONFIG_DECTNRP_SAMPLE_CHANNEL, pkt);
        if(ret != 0) {
            LOG_ERR("dectnrp_alloc_tx_operation failed, %d", ret);
            net_pkt_unref(pkt);
        } else {

			struct net_buf *frame_buf = pkt->frags;
			net_buf_add_mem(frame_buf, data, size);
			
			op->tx.transport_parameters = *parameters;
			op->tx.phy_header_type = phy_header_type;
			op->start_time = start_time;

			LOG_INF("transmit");

			ret = dectnrp_driver_schedule(iface, op);
			if(ret != 0) {
				LOG_ERR("dectnrp_driver_schedule(tx) failed, %d", ret);
			} else {
				/* Block unitil tx operation has been completed. */
				struct dectnrp_event_wrapper *item = k_fifo_get(&dectnrp_event_fifo, K_MSEC(10));
				if(item == NULL) {
					LOG_ERR("transmit complete event missing");
					dectnrp_free_operation(op);
					op = NULL;
					ret = -EIO;
				} else {
					struct dectnrp_driver_event *event = &item->event;
					__ASSERT(event->code == DECTNRP_EVENT_OP_FINISHED, "unexpected event");
					__ASSERT(event->op_finished.op == op, "event->op_finished.op != op");
					LOG_INF("transmit complete");

					k_free(item);
				}
			}
			dectnrp_free_operation(op);
		}
	}
    return ret;
}

int main()
{
	LOG_DBG("Opener version %d.%d.%d-%s%c", OPENER_VERSION_MAJOR, OPENER_VERSION_MINOR,
		OPENER_PATCHLEVEL, STRINGIFY(OPENER_GIT_COMMIT), OPENER_DIRTY ? '+' : '=');

	LOG_DBG("Opener dectnrp driver sample started");

	/* Get the DECT network interface */
	struct net_if *iface = net_if_get_first_by_type(&NET_L2_GET_NAME(DECTNRP));
	if (!iface) {
		LOG_ERR("net_if_get_first_by_type failed");
		return -ENODEV;
	} else if (!net_if_is_up(iface)) {
		LOG_ERR("net_if_is_up failed");
		return -ENOENT;

    } else {

		k_sleep(K_MSEC(1000));

		/* Assign our network id to the dectnrp_driver. */
		int ret = dectnrp_driver_set_network_id(iface, network_id);
		if(ret != 0) {
			LOG_ERR("dectnrp_driver_set_network_id failed, %d", ret);
			return -EIO;
		}

		/* Assign local long device id from hardware/chip id. */
		local_device.long_device_id = 0;
		size_t status = hwinfo_get_device_id((uint8_t *)&local_device.long_device_id, sizeof(local_device.long_device_id));
		if (status != sizeof(local_device.long_device_id)) {
			LOG_ERR("hwinfo_get_device_id failed,%u", ret);
			/* If long id is not assigned from chip we fall back to default long id. */
			local_device.long_device_id = CONFIG_DECTNRP_SAMPLE_LONG_DEVICE_ID;
		}
		/* Assign random short device id. */
		local_device.short_device_id = sys_rand32_get();
		local_device.state = DEVICE_STATE_UNSYNCHRONIZED;
		/* Remote peer not known yet.*/
		remote_device.short_device_id = DECTNRP_SHORT_BROADCAST_ADDRESS;
	
		LOG_INF("-------------------------");
		LOG_INF("Local device:");
		LOG_INF(" DEVICE_STATE_UNSYNCHRONIZED");
		LOG_INF(" - network id 0x%x", network_id);
		LOG_INF(" - long device id 0x%x", local_device.long_device_id);
		LOG_INF(" - short device id 0x%x", local_device.short_device_id);
		LOG_INF(" - channel %u", CONFIG_DECTNRP_SAMPLE_CHANNEL);
		LOG_INF("-------------------------");

		/* In unsynchronized mode we start with the full period. */
		uint32_t period = CONFIG_DECTNRP_SAMPLE_PERIOD;
		/* In unsynchronized mode we start with imediate messages the full period. */
		net_time_t tx_start_time = 0;
		
		/* Do periodically receive and send sample_frame via dectnrp_driver. */
		do {
			bool received = false;
			ret = receive(iface, period, &received);
			if(ret != 0) {
				LOG_ERR("receive failed, %d", ret);
				if(local_device.state != DEVICE_STATE_UNSYNCHRONIZED) {
					local_device.state = DEVICE_STATE_DESYNCHRONIZING;
				}
				/* If receive failed we idle for a full period. */
				k_sleep(K_MSEC(period));
			} else if(received) {
				if(local_device.state != DEVICE_STATE_SYNCHRONIZED) {
					local_device.state = DEVICE_STATE_SYNCHRONIZING;
				}
				k_sleep(K_MSEC(period/2));
			} else if(!received) {
				if(local_device.state != DEVICE_STATE_UNSYNCHRONIZED) {
					local_device.state = DEVICE_STATE_DESYNCHRONIZING;
				}
			}

			switch (local_device.state) {
				case DEVICE_STATE_SYNCHRONIZING:
					/* We have received a packet.
					   Now we know our remote peer and its timing to which we can synchronize. */
					local_device.state = DEVICE_STATE_SYNCHRONIZED;

					LOG_INF("-------------------------");
					LOG_INF("Local device:");
					LOG_INF(" DEVICE_STATE_SYNCHRONIZED");
					LOG_INF("Remote device:");
					LOG_INF(" - short device id 0x%x", remote_device.short_device_id);
					LOG_INF("-------------------------");

					__attribute__((fallthrough));
				case DEVICE_STATE_SYNCHRONIZED:
					/* Update the time of our next packet to be sent.
					   In synchornized mode we have a dedicated start time when to send. */
					tx_start_time = remote_device.last_start_time + TIME_2_MODEMTICKS(period/2);
					break;
				case DEVICE_STATE_DESYNCHRONIZING:
					/* We have not received a packet (or reception failed).
					   We go back to unsynchronized mode.
					   Remote peer not known anymore.*/
					remote_device.short_device_id = DECTNRP_SHORT_BROADCAST_ADDRESS;
					local_device.state = DEVICE_STATE_UNSYNCHRONIZED;

					LOG_INF("DEVICE_STATE_UNSYNCHRONIZED");

					__attribute__((fallthrough));
				case DEVICE_STATE_UNSYNCHRONIZED:
					/* In unsynchronized mode we send imediately. */
					tx_start_time = 0;
					break;
			}

			/* Update tx packet. */
			uint8_t* pcc = &sample_frame[0];
			pcc_set_short_network_id(pcc, network_id);
			pcc_set_transmitter_short_id(pcc, local_device.short_device_id);
			pcc_set_receiver_short_id(pcc, remote_device.short_device_id);

			ret = transmit(iface, &sample_frame[0], sample_frame_size, sample_frame_phy_header_type, &sample_frame_parameters,
				tx_start_time);
			if(ret != 0) {
				LOG_ERR("transmit failed, %d", ret);
			}

		} while(true);
	}

	return 0;
}
