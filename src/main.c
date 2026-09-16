/*
 * Copyright (c) 2026 Razvan Lazar
 */

#include <stdlib.h>

#include "e1000.h"


/*****************************************************************************/

#define VFIO_CONTAINER_PATH				"/dev/vfio/vfio"
#define VFIO_GROUP_PATH					"/dev/vfio/3"
#define VFIO_DEVICE_ID					"0000:00:03.0"				


/*****************************************************************************/

int main()
{
	e1000_dev_t dev;

	// open e1000 device
	if (e1000_dev_open(VFIO_CONTAINER_PATH, VFIO_GROUP_PATH, VFIO_DEVICE_ID,
					&dev))
		return EXIT_FAILURE;

	// close e1000 device
	e1000_dev_close(&dev);

	return EXIT_SUCCESS;
}
