/*
 * Copyright (c) 2026 Razvan Lazar
 */

#ifndef _E1000_H_
#define _E1000_H_

#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "vfio.h"
#include "e1000_regs.h"


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

// true if the lowest 'bits' bits of 'addr' are zero (aligned to 2^bits bytes)
#define E1000_IS_ALIGNED(addr, bits)	\
	((((uint64_t)(addr)) & ((1ULL << (bits)) - 1)) == 0)

// low / high 32 bits of a value (e.g. split a 64-bit iova in two registers);
// the value is widened to 64 bits first, so the shift is always valid
#define E1000_LOWER_32_BITS(val)		\
	((uint32_t)(((uint64_t)(val)) & 0xFFFFFFFFULL))
#define E1000_UPPER_32_BITS(val)		\
	((uint32_t)(((uint64_t)(val)) >> 32))


/*****************************************************************************/

#define E1000_REGISTERS_BAR_INDEX		0


/*****************************************************************************/

// pooling functions return codes
#define E1000_PKT_SUCCESS				0
#define E1000_NO_PKT					-1
#define E1000_PKT_FRGMT					-2
#define E1000_PKT_NO_MEM				-3


/*****************************************************************************/

// rx ring descriptor (16 bytes)
typedef struct e1000_rx_desc_s {

	uint64_t			buf_iova;
	uint16_t			length;
	uint16_t			checksum;
	uint8_t				status;
	uint8_t				errors;
	uint16_t			special;

} e1000_rx_desc_t;

static_assert(sizeof(e1000_rx_desc_t) == 16, "sizeof(e1000_rx_desc_t)");


/*****************************************************************************/

typedef struct e1000_dev_s {

	uint64_t					bar_size;
	volatile uint32_t			*bar_addr;
	vfio_dev_t					vfio_dev;
	//
	void						*rx_bufs;
	void						*tx_bufs;
	//
	volatile e1000_rx_desc_t	*rx_ring;
	void						*tx_ring;
	//
	int							rx_ring_idx;

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

//
int e1000_dev_rx_poll(e1000_dev_t *dev, void *buf, uint16_t size, uint16_t *len);

#endif	// _E1000_H_

