/*
 * Copyright (c) 2026 Deveritec GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief DECTNRP This contains first rudimentary implementation of l2/decnrp.
 *
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dectnrp, CONFIG_SAMPLE_DRIVER_DECTNRP_LOG_LEVEL);

#include <zephyr/net/net_if.h>
#include <zephyr/net/dectnrp_driver.h>
#include "dectnrp_driver_utils.h"
#include "dectnrp_operation.h"

/* event fifo queue. */
struct k_fifo dectnrp_event_fifo;

struct dectnrp_event_wrapper {
    void *fifo_reserved; /* 1st word reserved for use by FIFO */
    struct dectnrp_driver_event event;
};

/**
 * @brief Callback event handler for dectnrp driver events.
 *
 * @attention This function may run in interrupt context!
 *
 * @param dev
 * @param event
 */
static void dectnrp_driver_event(const struct device *dev, const struct dectnrp_driver_event *event)
{
    __ASSERT(event != NULL, "event == NULL");

    switch (event->code) {
    case DECTNRP_EVENT_MSG_ERROR:
    case DECTNRP_EVENT_MSG_RECEIVED:
    case DECTNRP_EVENT_OP_FINISHED:
    case DECTNRP_EVENT_OP_RESULTS: {

        if(event->code == DECTNRP_EVENT_MSG_ERROR) {
            NET_DBG("DECTNRP_EVENT_MSG_ERROR");
        } else if(event->code == DECTNRP_EVENT_MSG_RECEIVED) {
            NET_DBG("DECTNRP_EVENT_MSG_RECEIVED");
        } else if(event->code == DECTNRP_EVENT_OP_FINISHED) {
            NET_DBG("DECTNRP_EVENT_OP_FINISHED");
        } else if(event->code == DECTNRP_EVENT_OP_RESULTS) {
            NET_DBG("DECTNRP_EVENT_OP_RESULTS");
        } else {
            NET_DBG("code=%u", event->code);
        }

        struct dectnrp_event_wrapper *item = k_malloc(sizeof(struct dectnrp_event_wrapper));
        __ASSERT(item != NULL, "fifo_item == NULL");
        memcpy(&item->event, event, sizeof(struct dectnrp_driver_event));
        k_fifo_put(&dectnrp_event_fifo, (void*)item);

        break;
    }
    default: {
        NET_WARN("unknown driver event, not handled, %u", event->code);
        break;
    }
	}
}

/**
 * @brief Function called by net framework when a rx-packet has to be handled by l2 layer.
 * 
 * @param iface 
 * @param pkt 
 * @return enum net_verdict 
 */
static enum net_verdict dectnrp_recv(struct net_if *iface, struct net_pkt *pkt)
{
    LOG_DBG("dectnrp_recv(%p)", iface);
	return NET_DROP;
}

/**
 * @brief Function called by net framework when a tx-packet has to be handled by l2 layer.
 * 
 * @param iface 
 * @param pkt 
 * @return int 
 */
static int dectnrp_send(struct net_if *iface, struct net_pkt *pkt)
{
    LOG_DBG("dectnrp_send(%p)", iface);
	return -EIO;
}

/**
 * @brief Function called by net framework when @p iface should be enabled/disabled.
 * 
 * @param iface 
 * @param state 
 * @return int 
 */
static int dectnrp_enable(struct net_if *iface, bool state)
{
	NET_DBG("iface %p %s", iface, state ? "up" : "down");

	int ret = 0;
    if (state) {
        ret = dectnrp_driver_start(iface);
        if (ret != 0) {
            ret = dectnrp_driver_stop(iface);
        }
    }
	return ret;
}

/**
 * @brief Function called by net framework when l2 flags are needed.
 * 
 * @param iface 
 * @return enum net_l2_flags 
 */
static enum net_l2_flags dectnrp_flags(struct net_if *iface)
{
    LOG_DBG("dectnrp_flags(%p)", iface);
	struct dectnrp_context *ctx = net_if_l2_data(iface);
	/* No need for locking as these flags as they are set once
	 * during L2 initialization and then never changed.
	 */
	return ctx->flags;
}

NET_L2_INIT(DECTNRP_L2, dectnrp_recv, dectnrp_send, dectnrp_enable, dectnrp_flags);

/**
 * @brief dectnrp_stack initialization callback.
 * 
 * This is the init callback which gets called from nrf91_net_if_init.
 * As we have no dectnrp l2 yet we implement it here.
 * 
 * @param iface 
 */
void dectnrp_l2_init(struct net_if *iface)
{
    struct dectnrp_context *ctx = net_if_l2_data(iface);
	LOG_DBG("dectnrp_l2_init(%p)", iface);

    ctx->flags = NET_L2_MULTICAST;
    if(dectnrp_driver_get_hw_capabilities(iface) && DECTNRP_HW_PROMISC) {
        ctx->flags |= NET_L2_PROMISC_MODE;
    }
    ctx->iface = iface;

    k_fifo_init(&dectnrp_event_fifo);

    dectnrp_driver_register_event_handler(iface, dectnrp_driver_event);

	LOG_DBG("L2 interface initialized");
	return;
}
