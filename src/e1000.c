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
#define E1000_RX_RING_DESC					256
#define E1000_TX_RING_DESC					256

// ring size in bytes (legacy tx descriptor is 16 bytes, same as rx)
#define E1000_RX_RING_SIZE					(E1000_RX_RING_DESC * \
											sizeof(e1000_rx_desc_t))
#define E1000_TX_RING_SIZE					(E1000_TX_RING_DESC * 16)

//
#define E1000_RX_RING_IOVA					0x10000000
#define E1000_TX_RING_IOVA					(E1000_RX_RING_IOVA + \
											E1000_RX_RING_SIZE)


/*****************************************************************************/

// each descriptor permanently owns one buffer
#define E1000_RX_BUF_NUM					E1000_RX_RING_DESC
#define E1000_TX_BUF_NUM					E1000_TX_RING_DESC

//
#define E1000_RX_BUF_SIZE					4096
#define E1000_TX_BUF_SIZE					4096

// buffers pool size in bytes
#define E1000_RX_BUFS_SIZE					(E1000_RX_BUF_NUM * E1000_RX_BUF_SIZE)
#define E1000_TX_BUFS_SIZE					(E1000_TX_BUF_NUM * E1000_TX_BUF_SIZE)

//
#define E1000_RX_BUFS_IOVA					0x20000000
#define E1000_TX_BUFS_IOVA					(E1000_RX_BUFS_IOVA + \
											E1000_RX_BUFS_SIZE)


/*****************************************************************************/

// vfio dma map requires page aligned sizes (and so iova, as they are chained)
static_assert(E1000_RX_RING_SIZE % 0x1000 == 0, "rx ring not page aligned");
static_assert(E1000_TX_RING_SIZE % 0x1000 == 0, "tx ring not page aligned");
static_assert(E1000_RX_BUFS_SIZE % 0x1000 == 0, "rx bufs not page aligned");
static_assert(E1000_TX_BUFS_SIZE % 0x1000 == 0, "tx bufs not page aligned");

// rings and buffers iova ranges must not overlap
static_assert(E1000_TX_RING_IOVA + E1000_TX_RING_SIZE <= E1000_RX_BUFS_IOVA,
			"rings overlap buffers");


/*****************************************************************************/

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

static int __e1000_disable_irqs(e1000_dev_t *dev)
{
	uint32_t val;

	E1000_DBG("Device %d disable interrupts ...\n", dev->vfio_dev.device_fd);

	// Interrupt Mask Clear; mask all interrupts
	val = 0xFFFFFFFF;
	e1000_write32(dev, E1000_IMC, val);

	// read Interrupt Cause; by reading it we clear stale events
	val = e1000_read32(dev, E1000_ICR);
	E1000_DBG("Device %d ICR: 0x%x\n", dev->vfio_dev.device_fd, val);

	// read Interrupt Mask Set
	val = e1000_read32(dev, E1000_IMS);
	if (val) {
		E1000_ERR("Device %d disabled interrupts failed 0x%x\n",
				dev->vfio_dev.device_fd, val);
		return -1;
	}

	E1000_INF("Device %d successfully disabled interrupts\n",
			dev->vfio_dev.device_fd);
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
	dev->rx_ring = mmap(NULL, E1000_RX_RING_SIZE, PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (dev->rx_ring == MAP_FAILED) {
		int saved_errno = errno;
		E1000_ERR("Fail to map device %d rx ring: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, strerror(saved_errno), saved_errno);
		goto error;
	}

	dev->tx_ring = mmap(NULL, E1000_TX_RING_SIZE, PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (dev->tx_ring == MAP_FAILED) {
		int saved_errno = errno;
		E1000_ERR("Fail to map device %d tx ring: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, strerror(saved_errno), saved_errno);
		goto rx_ring_unmap;
	}

	//
	dev->rx_bufs = mmap(NULL, E1000_RX_BUFS_SIZE, PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (dev->rx_bufs == MAP_FAILED) {
		int saved_errno = errno;
		E1000_ERR("Fail to map device %d rx bufs: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, strerror(saved_errno), saved_errno);
		goto tx_ring_unmap;
	}

	//
	dev->tx_bufs = mmap(NULL, E1000_TX_BUFS_SIZE, PROT_READ|PROT_WRITE,
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
	if (vfio_dev_map_dma(&dev->vfio_dev, (void *)dev->rx_ring, E1000_RX_RING_SIZE,
						E1000_RX_RING_IOVA, E1000_DMA_MAP_FLAGS)) {
		goto tx_bufs_unmap;
	}

	if (vfio_dev_map_dma(&dev->vfio_dev, dev->tx_ring, E1000_TX_RING_SIZE,
						E1000_TX_RING_IOVA, E1000_DMA_MAP_FLAGS)) {
		goto rx_ring_dma_unmap;
	}

	if (vfio_dev_map_dma(&dev->vfio_dev, dev->rx_bufs, E1000_RX_BUFS_SIZE,
						E1000_RX_BUFS_IOVA, E1000_DMA_MAP_FLAGS)) {
		goto tx_ring_dma_unmap;
	}

	if (vfio_dev_map_dma(&dev->vfio_dev, dev->tx_bufs, E1000_TX_BUFS_SIZE,
						E1000_TX_BUFS_IOVA, E1000_DMA_MAP_FLAGS)) {
		goto rx_bufs_dma_unmap;
	}

// success:
	E1000_DBG("Device %d successfully map rx_ring %p, tx_ring %p, rx_bufs %p,"
			" tx_bufs %p\n", dev->vfio_dev.device_fd, dev->rx_ring,
			dev->tx_ring, dev->rx_bufs, dev->tx_bufs);

	return 0;


rx_bufs_dma_unmap:
	assert(!vfio_dev_unmap_dma(&dev->vfio_dev, E1000_RX_BUFS_SIZE,
							E1000_RX_BUFS_IOVA));
tx_ring_dma_unmap:
	assert(!vfio_dev_unmap_dma(&dev->vfio_dev, E1000_TX_RING_SIZE,
							E1000_TX_RING_IOVA));
rx_ring_dma_unmap:
	assert(!vfio_dev_unmap_dma(&dev->vfio_dev, E1000_RX_RING_SIZE,
							E1000_RX_RING_IOVA));
tx_bufs_unmap:
	assert(!munmap(dev->tx_bufs, E1000_TX_BUFS_SIZE));
rx_bufs_unmap:
	assert(!munmap(dev->rx_bufs, E1000_RX_BUFS_SIZE));
tx_ring_unmap:
	assert(!munmap(dev->tx_ring, E1000_TX_RING_SIZE));
rx_ring_unmap:
	assert(!munmap((void *)dev->rx_ring, E1000_RX_RING_SIZE));
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
	vfio_dev_unmap_dma(&dev->vfio_dev, E1000_RX_RING_SIZE, E1000_RX_RING_IOVA);
	vfio_dev_unmap_dma(&dev->vfio_dev, E1000_TX_RING_SIZE, E1000_TX_RING_IOVA);
	vfio_dev_unmap_dma(&dev->vfio_dev, E1000_RX_BUFS_SIZE, E1000_RX_BUFS_IOVA);
	vfio_dev_unmap_dma(&dev->vfio_dev, E1000_TX_BUFS_SIZE, E1000_TX_BUFS_IOVA);

	/**
	 * Unmap virtual memory.
	 */
	if (munmap((void *)dev->rx_ring, E1000_RX_RING_SIZE)) {
		int saved_errno = errno;
		E1000_ERR("Fail to unmap device %d rx ring %p: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, dev->rx_ring, strerror(saved_errno),
				saved_errno);
	}

	if (munmap(dev->tx_ring, E1000_TX_RING_SIZE)) {
		int saved_errno = errno;
		E1000_ERR("Fail to unmap device %d tx ring %p: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, dev->tx_ring, strerror(saved_errno),
				saved_errno);
	}

	if (munmap(dev->rx_bufs, E1000_RX_BUFS_SIZE)) {
		int saved_errno = errno;
		E1000_ERR("Fail to unmap device %d rx bufs %p: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, dev->rx_bufs, strerror(saved_errno),
				saved_errno);
	}

	if (munmap(dev->tx_bufs, E1000_TX_BUFS_SIZE)) {
		int saved_errno = errno;
		E1000_ERR("Fail to unmap device %d tx bufs %p: %s (errno=%d)\n",
				dev->vfio_dev.device_fd, dev->tx_bufs, strerror(saved_errno),
				saved_errno);
	}
}


/*****************************************************************************/

static int __e1000_dev_set_rx(e1000_dev_t *dev)
{
	volatile e1000_rx_desc_t *rx_desc = dev->rx_ring;
	uint32_t rdbah, rdbal;

	E1000_DBG("Device %d configure RX ...\n", dev->vfio_dev.device_fd);

	// split the 64-bit ring iova into the two 32-bit base address registers
	rdbah = E1000_UPPER_32_BITS(E1000_RX_RING_IOVA);
	rdbal = E1000_LOWER_32_BITS(E1000_RX_RING_IOVA);

	// set the iova address for each descriptor to packets pool
	for (int i = 0; i < E1000_RX_RING_DESC; i++) {
		rx_desc[i].buf_iova = E1000_RX_BUFS_IOVA + i * E1000_RX_BUF_SIZE;
		rx_desc[i].length   = 0;
		rx_desc[i].checksum = 0;
		rx_desc[i].status   = 0;
		rx_desc[i].errors   = 0;
		rx_desc[i].special  = 0;
	}

	// disable RX; ring registers must not be changed while RX is running
	e1000_write32(dev, E1000_RCTL, 0);

	// set rx ring iova
	e1000_write32(dev, E1000_RDBAL, rdbal);
	e1000_write32(dev, E1000_RDBAH, rdbah);

	// set rx ring size (in bytes, multiple of 128)
	e1000_write32(dev, E1000_RDLEN, E1000_RX_RING_SIZE);

	/**
	 * Head and tail.
	 *
	 * RDH: next descriptor the NIC will fill (NIC moves it).
	 * RDT: first descriptor the NIC must NOT use (software moves it).
	 *
	 * The NIC owns descriptors [RDH, RDT), so RDH == RDT means it owns none.
	 * Hence one slot always stays with software: RDT = N - 1 hands
	 * descriptors 0 .. N - 2 to the NIC.
	 */
	e1000_write32(dev, E1000_RDH, 0);
	e1000_write32(dev, E1000_RDT, E1000_RX_RING_DESC - 1);

	// disable all multicast for now
	for (int i = 0; i < 128; i++)
		e1000_write32(dev, E1000_MTA + i * 4, 0);

	/**
	 * Validate configuration.
	 *
	 * Validate ring registers before enabling RX, because an incoming
	 * packet moves RDH.
	 */
	if (e1000_read32(dev, E1000_RDBAL) != rdbal) {
		E1000_ERR("Fail to set device %d iova low addr\n",
				dev->vfio_dev.device_fd);
		goto error;
	}

	if (e1000_read32(dev, E1000_RDBAH) != rdbah) {
		E1000_ERR("Fail to set device %d iova high addr\n",
				dev->vfio_dev.device_fd);
		goto error;
	}

	if (e1000_read32(dev, E1000_RDLEN) != E1000_RX_RING_SIZE) {
		E1000_ERR("Fail to set device %d rx ring size\n",
				dev->vfio_dev.device_fd);
		goto error;
	}

	if (e1000_read32(dev, E1000_RDH) != 0) {
		E1000_ERR("Fail to set device %d rx ring head\n",
				dev->vfio_dev.device_fd);
		goto error;
	}

	if (e1000_read32(dev, E1000_RDT) != E1000_RX_RING_DESC - 1) {
		E1000_ERR("Fail to set device %d rx ring tail\n",
				dev->vfio_dev.device_fd);
		goto error;
	}

	for (int i = 0; i < 128; i++) {
		if (e1000_read32(dev, E1000_MTA + i * 4) != 0) {
			E1000_ERR("Fail to set device %d mcast index %d\n",
					dev->vfio_dev.device_fd, i);
			goto error;
		}
	}

	/**
	 * Default rx configuration.
	 *
	 * Promiscuous (all unicast and multicast) plus broadcast, CRC stripped.
	 *
	 * BSIZE/BSEX left 0 means 2048 B buffers: the NIC writes at most 2048 B
	 * at each descriptor address. It must be <= E1000_RX_BUF_SIZE (4096 B
	 * apart in the pool), so the NIC never writes into the next buffer.
	 * With LPE off frames are at most 1522 B, so every frame fits in one
	 * descriptor (EOP always set).
	 */
	uint32_t rx_def_val = (E1000_RCTL_EN | E1000_RCTL_UPE | E1000_RCTL_MPE |
						E1000_RCTL_BAM | E1000_RCTL_SECRC);
	e1000_write32(dev, E1000_RCTL, rx_def_val);

	// validate rx configuration
	if (e1000_read32(dev, E1000_RCTL) != rx_def_val) {
		E1000_ERR("Fail to set device %d rx config\n",
				dev->vfio_dev.device_fd);
		goto error;
	}

	E1000_DBG("Device %d RX successfully configured...\n",
			dev->vfio_dev.device_fd);

//success:
	return 0;

error:
	return -1;
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

	// disable all interrupts (we are doing polling mode)
	if (__e1000_disable_irqs(e1000_dev))
		goto close_vfio_dev;

	// link up
	if (__e1000_link_up(e1000_dev))
		goto close_vfio_dev;

	// map NIC rings and bufs
	if (__e1000_map_data(e1000_dev)) {
		goto close_vfio_dev;
	}

	// allow the device to dma (only now that dma memory is mapped)
	if (vfio_dev_enable_bus_master(&e1000_dev->vfio_dev))
		goto close_vfio_dev;

	// configure rx
	if (__e1000_dev_set_rx(e1000_dev))
		goto close_vfio_dev;

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
