/*
 * Copyright (c) 2026 Razvan Lazar
 */

#include <stdlib.h>

#include "e1000.h"


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

//success
	e1000_dev->bar_size = bar_size;
	e1000_dev->bar_addr = bar_addr;

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
