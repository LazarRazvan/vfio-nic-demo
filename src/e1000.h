/*
 * Copyright (c) 2026 Razvan Lazar
 */

#ifndef _E1000_H_
#define _E1000_H_

#include <stdint.h>

#include "vfio.h"


/*****************************************************************************/

#define E1000_REGISTERS_BAR_INDEX		0

#define E1000_CTRL						0x0000
#define E1000_STATUS					0x0008

#define E1000_STATUS_FD					(1 << 0) // full duplex
#define E1000_STATUS_LU					(1 << 1) // link up


/*****************************************************************************/

typedef struct e1000_dev_s {

	uint64_t			bar_size;
	volatile uint32_t	*bar_addr;
	vfio_dev_t			vfio_dev;

} e1000_dev_t;


/*****************************************************************************/

int e1000_dev_open(const char *vfio_container_path, const char *vfio_group_path,
				const char *vfio_device_id, e1000_dev_t *e1000_dev);
void e1000_dev_close(e1000_dev_t *e1000_dev);

#endif	// _E1000_H_

