/*
 * Copyright (c) 2026 Deveritec GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dectnrp_sample, CONFIG_SAMPLE_DECTNRP_LOG_LEVEL);

#include <opener/build_info.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/net/dectnrp_net_l2.h>
#include <zephyr/net/net_if.h>

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

		do {

			k_sleep(K_MSEC(1000));
			LOG_ERR("ping");

		} while (true);
	}

	return 0;
}
