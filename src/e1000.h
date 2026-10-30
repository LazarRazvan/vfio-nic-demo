/*
 * Copyright (c) 2026 Razvan Lazar
 */

#ifndef _E1000_H_
#define _E1000_H_

#include <stdint.h>
#include <stdio.h>

#include "vfio.h"


/*****************************************************************************/
//
// Log config
//
#ifndef E1000_INF_ENABLE
#	define E1000_INF_ENABLE		1
#endif

#ifndef E1000_DBG_ENABLE
#	define E1000_DBG_ENABLE		1
#endif

#ifndef E1000_ERR_ENABLE
#	define E1000_ERR_ENABLE		1
#endif


/*****************************************************************************/


#if E1000_INF_ENABLE
#define E1000_INF(fmt, ...)	fprintf(stdout, "e1000_inf: "fmt, ##__VA_ARGS__)
#else
#define E1000_INF(fmt, ...)	do {} while (0)
#endif

#if E1000_DBG_ENABLE
#define E1000_DBG(fmt, ...)	fprintf(stdout, "e1000_dbg: "fmt, ##__VA_ARGS__)
#else
#define E1000_DBG(fmt, ...)	do {} while (0)
#endif

#if E1000_ERR_ENABLE
#define E1000_ERR(fmt, ...)	fprintf(stderr, "e1000_err: "fmt, ##__VA_ARGS__)
#else
#define E1000_ERR(fmt, ...)	do {} while (0)
#endif


/*****************************************************************************/

#define E1000_REGISTERS_BAR_INDEX		0

#define E1000_CTRL						0x0000
#define E1000_STATUS					0x0008

#define E1000_STATUS_FD					(1 << 0)	// full duplex
#define E1000_STATUS_LU					(1 << 1)	// link up

#define E1000_CTRL_SLU					(1 << 6)	// Set Link Up
#define E1000_CTRL_LRST					(1 << 3)	// Link Reset
#define E1000_CTRL_RST					(1 << 26)	// Global device Reset


/*****************************************************************************/

typedef struct e1000_dev_s {

	uint64_t			bar_size;
	volatile uint32_t	*bar_addr;
	vfio_dev_t			vfio_dev;

	//
	void				*rx_ring;
	void				*tx_ring;
	void				*rx_bufs;
	void				*tx_bufs;

} e1000_dev_t;


/*****************************************************************************/

static inline uint32_t e1000_read32(e1000_dev_t *dev, uint32_t reg)
{
	return *((volatile uint32_t *)((volatile uint8_t *)dev->bar_addr + reg));
}

static inline void e1000_write32(e1000_dev_t *dev, uint32_t reg, uint32_t val)
{
	*(volatile uint32_t *)((volatile uint8_t *)dev->bar_addr + reg) = val;
}


/*****************************************************************************/

int e1000_dev_open(const char *vfio_container_path, const char *vfio_group_path,
				const char *vfio_device_id, e1000_dev_t *e1000_dev);
void e1000_dev_close(e1000_dev_t *e1000_dev);

#endif	// _E1000_H_

