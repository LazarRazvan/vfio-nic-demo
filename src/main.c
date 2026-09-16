/*
 * Copyright (c) 2026 Razvan Lazar
 */

#include "vfio.h"


/*****************************************************************************/

#define VFIO_CONTAINER_PATH				"/dev/vfio/vfio"
#define VFIO_GROUP_PATH					"/dev/vfio/3"
#define VFIO_DEVICE_ID					"0000:00:03.0"				


/*****************************************************************************/

int main()
{
	vfio_dev_t dev;

	return vfio_dev_open(VFIO_CONTAINER_PATH, VFIO_GROUP_PATH, VFIO_DEVICE_ID,
						&dev);
}
