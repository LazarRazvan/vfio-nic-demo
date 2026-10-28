/*
 * Copyright (c) 2026 Razvan Lazar
 */

#include <stdlib.h>
#include <unistd.h>

#include "e1000.h"


/*****************************************************************************/

//
#define E1000_RESET_ATTEMPTS				100
#define E1000_RESET_WAIT_MS					10
//
#define E1000_LINK_UP_ATTEMPTS				100
#define E1000_LINK_UP_WAIT_MS				10


/*****************************************************************************/

static int __e1000_reset(e1000_dev_t *dev)
{
	uint32_t val, attempts = 0;

	E1000_DBG("Device %d resetting...\n", dev->vfio_dev.device_fd);

	// send the reset signal
	val = e1000_read32(dev, E1000_CTRL);
	val |= E1000_CTRL_RST;
	e1000_write32(dev, E1000_CTRL, val);

	// pool until reset flag is cleared by hardware
	val = e1000_read32(dev, E1000_CTRL);
	while ((attempts++ < E1000_RESET_ATTEMPTS) && (val & E1000_CTRL_RST)) {
		usleep(E1000_RESET_WAIT_MS * 1000UL);
		val = e1000_read32(dev, E1000_CTRL);
	}

	if (val & E1000_CTRL_RST) {
		E1000_ERR("Device %d reset failed\n", dev->vfio_dev.device_fd);
		return -1;
	}

	E1000_INF("Device %d reset successfully\n", dev->vfio_dev.device_fd);
	return 0;
}

static int __e1000_link_up(e1000_dev_t *dev)
{
	uint32_t val, attempts = 0;

	E1000_DBG("Device %d link up ...\n", dev->vfio_dev.device_fd);

	// send link up signal
	val = e1000_read32(dev, E1000_CTRL);
	val |= E1000_CTRL_SLU;
	e1000_write32(dev, E1000_CTRL, val);

	// pool until link up flag is cleared by hardware
	val = e1000_read32(dev, E1000_STATUS);
	while ((attempts++ < E1000_LINK_UP_ATTEMPTS) && !(val & E1000_STATUS_LU)) {
		usleep(E1000_LINK_UP_WAIT_MS * 1000UL);
		val = e1000_read32(dev, E1000_STATUS);
	}

	// safety validation
	if (!(val & E1000_STATUS_LU)) {
		E1000_ERR("Device %d link up failed\n", dev->vfio_dev.device_fd);
		return -1;
	}

	E1000_INF("Device %d link up successfully\n", dev->vfio_dev.device_fd);
	return 0;
}


/*****************************************************************************/

int e1000_dev_open(const char *vfio_container_path, const char *vfio_group_path,
				const char *vfio_device_id, e1000_dev_t *e1000_dev)
{
	void *bar_addr;
	uint64_t bar_size;

	// open vfio device
	if (vfio_dev_open(vfio_container_path, vfio_group_path, vfio_device_id,
						&e1000_dev->vfio_dev))
		goto error;

	// map e1000 BAR
	bar_addr = vfio_dev_map_region(&e1000_dev->vfio_dev,
								E1000_REGISTERS_BAR_INDEX, &bar_size);
	if (!bar_addr)
		goto close_vfio_dev;

	e1000_dev->bar_size = bar_size;
	e1000_dev->bar_addr = bar_addr;

	// reset
	if (__e1000_reset(e1000_dev))
		goto close_vfio_dev;

	// link up
	if (__e1000_link_up(e1000_dev))
		goto close_vfio_dev;

	return EXIT_SUCCESS;

close_vfio_dev:
	vfio_dev_close(&e1000_dev->vfio_dev);
error:
	return EXIT_FAILURE;
}

void e1000_dev_close(e1000_dev_t *e1000_dev)
{
	vfio_dev_unmap_region((void *)e1000_dev->bar_addr, e1000_dev->bar_size);
	vfio_dev_close(&e1000_dev->vfio_dev);
}
