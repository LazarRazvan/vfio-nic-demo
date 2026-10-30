/*
 * Copyright (c) 2026 Razvan Lazar
 */

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <linux/vfio.h>

#include "e1000.h"


/*****************************************************************************/

//
#define E1000_RESET_ATTEMPTS				100
#define E1000_RESET_WAIT_MS					10
//
#define E1000_LINK_UP_ATTEMPTS				100
#define E1000_LINK_UP_WAIT_MS				10


/*****************************************************************************/

//
#define E1000_RX_RING_IOVA					0x10000000
#define E1000_TX_RING_IOVA					0x10001000

//
#define E1000_RX_BUFS_IOVA					0x20000000
#define E1000_TX_BUFS_IOVA					0x20100000

//
#define E1000_DMA_MAP_FLAGS					(VFIO_DMA_MAP_FLAG_READ | \
											VFIO_DMA_MAP_FLAG_WRITE)


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

static int __e1000_map_data(e1000_dev_t *dev)
{
	/**
	 * Create memory pools to be used by iommu for dma mechanism. mmap() ensure
	 * page aligned virtual memory.
	 */

	//
	dev->rx_ring = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (dev->rx_ring == MAP_FAILED) {
		int saved_errno = errno;
		E1000_ERR("Fail to map device %d rx ring: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, strerror(saved_errno), saved_errno);
		goto error;
	}

	dev->tx_ring = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (dev->tx_ring == MAP_FAILED) {
		int saved_errno = errno;
		E1000_ERR("Fail to map device %d tx ring: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, strerror(saved_errno), saved_errno);
		goto rx_ring_unmap;
	}

	//
	dev->rx_bufs = mmap(NULL, 0x100000, PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (dev->rx_bufs == MAP_FAILED) {
		int saved_errno = errno;
		E1000_ERR("Fail to map device %d rx bufs: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, strerror(saved_errno), saved_errno);
		goto tx_ring_unmap;
	}

	//
	dev->tx_bufs = mmap(NULL, 0x100000, PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (dev->tx_bufs == MAP_FAILED) {
		int saved_errno = errno;
		E1000_ERR("Fail to map device %d rx bufs: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, strerror(saved_errno), saved_errno);
		goto rx_bufs_unmap;
	}

	/**
	 * Map virtual addresses returned by mmap to specific selected iova
	 * addresses for dma (program the iommu).
	 */
	if (vfio_dev_map_dma(&dev->vfio_dev, dev->rx_ring, 0x1000,
						E1000_RX_RING_IOVA, E1000_DMA_MAP_FLAGS)) {
		goto tx_bufs_unmap;
	}

	if (vfio_dev_map_dma(&dev->vfio_dev, dev->tx_ring, 0x1000,
						E1000_TX_RING_IOVA, E1000_DMA_MAP_FLAGS)) {
		goto rx_ring_dma_unmap;
	}

	if (vfio_dev_map_dma(&dev->vfio_dev, dev->rx_bufs, 0x100000,
						E1000_RX_BUFS_IOVA, E1000_DMA_MAP_FLAGS)) {
		goto tx_ring_dma_unmap;
	}

	if (vfio_dev_map_dma(&dev->vfio_dev, dev->tx_bufs, 0x100000,
						E1000_TX_BUFS_IOVA, E1000_DMA_MAP_FLAGS)) {
		goto rx_bufs_dma_unmap;
	}

// success:
	E1000_DBG("Device %d successfully map rx_ring %p, tx_ring %p, rx_bufs %p,"
			" tx_bufs %p\n", dev->vfio_dev.device_fd, dev->rx_ring,
			dev->tx_ring, dev->rx_bufs, dev->tx_bufs);

	return 0;


rx_bufs_dma_unmap:
	assert(!vfio_dev_unmap_dma(&dev->vfio_dev, 0x100000, E1000_RX_BUFS_IOVA));
tx_ring_dma_unmap:
	assert(!vfio_dev_unmap_dma(&dev->vfio_dev, 0x1000, E1000_TX_RING_IOVA));
rx_ring_dma_unmap:
	assert(!vfio_dev_unmap_dma(&dev->vfio_dev, 0x1000, E1000_RX_RING_IOVA));
tx_bufs_unmap:
	assert(!munmap(dev->tx_bufs, 0x100000));
rx_bufs_unmap:
	assert(!munmap(dev->rx_bufs, 0x100000));
tx_ring_unmap:
	assert(!munmap(dev->tx_ring, 0x1000));
rx_ring_unmap:
	assert(!munmap(dev->rx_ring, 0x1000));
error:
	dev->rx_ring = NULL;
	dev->tx_ring = NULL;
	dev->rx_bufs = NULL;
	dev->tx_bufs = NULL;
	return -1;
}

static void __e1000_unmap_data(e1000_dev_t *dev)
{
	/**
	 * Unmap iova from virtual address.
	 */
	vfio_dev_unmap_dma(&dev->vfio_dev, 0x1000, E1000_RX_RING_IOVA);
	vfio_dev_unmap_dma(&dev->vfio_dev, 0x1000, E1000_TX_RING_IOVA);
	vfio_dev_unmap_dma(&dev->vfio_dev, 0x100000, E1000_RX_BUFS_IOVA);
	vfio_dev_unmap_dma(&dev->vfio_dev, 0x100000, E1000_TX_BUFS_IOVA);

	/**
	 * Unmap virtual memory.
	 */
	if (munmap(dev->rx_ring, 0x1000)) {
		int saved_errno = errno;
		E1000_ERR("Fail to unmap device %d rx ring %p: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, dev->rx_ring, strerror(saved_errno),
				saved_errno);
	}

	if (munmap(dev->tx_ring, 0x1000)) {
		int saved_errno = errno;
		E1000_ERR("Fail to unmap device %d tx ring %p: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, dev->tx_ring, strerror(saved_errno),
				saved_errno);
	}

	if (munmap(dev->rx_bufs, 0x100000)) {
		int saved_errno = errno;
		E1000_ERR("Fail to unmap device %d rx bufs %p: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, dev->rx_bufs, strerror(saved_errno),
				saved_errno);
	}

	if (munmap(dev->tx_bufs, 0x100000)) {
		int saved_errno = errno;
		E1000_ERR("Fail to unmap device %d tx bufs %p: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, dev->tx_bufs, strerror(saved_errno),
				saved_errno);
	}
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

	// map NIC rings and bufs
	if (__e1000_map_data(e1000_dev)) {
		goto close_vfio_dev;
	}

	return EXIT_SUCCESS;

close_vfio_dev:
	vfio_dev_close(&e1000_dev->vfio_dev);
error:
	return EXIT_FAILURE;
}

void e1000_dev_close(e1000_dev_t *e1000_dev)
{
	__e1000_unmap_data(e1000_dev);
	vfio_dev_unmap_region((void *)e1000_dev->bar_addr, e1000_dev->bar_size);
	vfio_dev_close(&e1000_dev->vfio_dev);
}
