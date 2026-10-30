/*
 * Copyright (c) 2026 Razvan Lazar
 */

#ifndef _VFIO_H_
#define _VFIO_H_

#include <stdint.h>
#include <stdio.h>


/*****************************************************************************/
//
// Log config
//
#ifndef VFIO_INF_ENABLE
#	define VFIO_INF_ENABLE		1
#endif

#ifndef VFIO_DBG_ENABLE
#	define VFIO_DBG_ENABLE		1
#endif

#ifndef VFIO_ERR_ENABLE
#	define VFIO_ERR_ENABLE		1
#endif


/*****************************************************************************/

#if VFIO_INF_ENABLE
#define VFIO_INF(fmt, ...)	fprintf(stdout, "vfio_inf: "fmt, ##__VA_ARGS__)
#else
#define VFIO_INF(fmt, ...)	do {} while (0)
#endif

#if VFIO_DBG_ENABLE
#define VFIO_DBG(fmt, ...)	fprintf(stdout, "vfio_dbg: "fmt, ##__VA_ARGS__)
#else
#define VFIO_DBG(fmt, ...)	do {} while (0)
#endif

#if VFIO_ERR_ENABLE
#define VFIO_ERR(fmt, ...)	fprintf(stderr, "vfio_err: "fmt, ##__VA_ARGS__)
#else
#define VFIO_ERR(fmt, ...)	do {} while (0)
#endif


/*****************************************************************************/

typedef struct vfio_dev_s {

	int container_fd;
	int group_fd;
	int device_fd;

} vfio_dev_t;


/*****************************************************************************/

//
int vfio_dev_map_dma(vfio_dev_t *vfio_dev, void *vaddr, uint64_t size,
						uint64_t iova, uint64_t flags);
int vfio_dev_unmap_dma(vfio_dev_t *vfio_dev, uint64_t size, uint64_t iova);

//
void *vfio_dev_map_region(vfio_dev_t *vfio_dev, uint32_t index, uint64_t *size);
void vfio_dev_unmap_region(void *addr, uint64_t size);

//
int vfio_dev_open(const char *vfio_container_path, const char *vfio_group_path,
				const char *vfio_device_id, vfio_dev_t *vfio_dev);
void vfio_dev_close(vfio_dev_t *vfio_dev);

#endif	// _VFIO_H_
