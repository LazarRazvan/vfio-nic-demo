/*
 * Copyright (c) 2026 Razvan Lazar
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/vfio.h>

#include "vfio.h"


/*****************************************************************************/

static inline void __vfio_device_get_region_info(const char *vfio_device_id,
												int device_fd, uint32_t index)
{
	/**
	 * Get device region config.
	 */
	struct vfio_region_info region = { .argsz = sizeof(region), .index = index };
	if (ioctl(device_fd, VFIO_DEVICE_GET_REGION_INFO, &region) < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to get device %s region %d info: %s (errno=%d)\n",
				vfio_device_id, index, strerror(saved_errno), saved_errno);
		return;
	}

	//
	printf("[%s] region %d: R(%u) W(%u) M(%u) C(%u); size %llu; offset 0x%llx\n",
		vfio_device_id, index,
		!!(region.flags & VFIO_REGION_INFO_FLAG_READ),
		!!(region.flags & VFIO_REGION_INFO_FLAG_WRITE),
		!!(region.flags & VFIO_REGION_INFO_FLAG_MMAP),
		!!(region.flags & VFIO_REGION_INFO_FLAG_CAPS),
		region.size, region.offset);

	// validate vendor and device id (same as 'lspci -nn')
	if (index == VFIO_PCI_CONFIG_REGION_INDEX) {
		unsigned char buf[4];
		int saved_errno = errno;

		if (pread(device_fd, buf, 4, region.offset) < 0) {
			fprintf(stderr, "Fail to get device %s vendor id: %s (errno=%d)\n",
					vfio_device_id, strerror(saved_errno), saved_errno);
			return;
		}

		uint16_t vendor = buf[0] | (buf[1] << 8);
		uint16_t device = buf[2] | (buf[3] << 8);
		printf("[%s] region %d: venodr %x; device %d\n", vfio_device_id, index,
			vendor, device);
	}
}

static void __vfio_dev_device_get_info(const char *vfio_device_id, int device_fd)
{
	/**
	 * Get device capabilities.
	 */
	struct vfio_device_info info = { .argsz = sizeof(info) };
	if (ioctl(device_fd, VFIO_DEVICE_GET_INFO, &info) < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to get device %s info: %s (errno=%d)\n",
				vfio_device_id, strerror(saved_errno), saved_errno);
		return;
	}

	//
	printf("[%s] num_regions %u; num_irqs %u\n", vfio_device_id,
		info.num_regions, info.num_irqs);

	// print regions
	for (uint32_t i = 0; i < info.num_regions; i++)
		__vfio_device_get_region_info(vfio_device_id, device_fd, i);
}


/*****************************************************************************/

static inline int __vfio_dev_device_open(const char *vfio_device_id, int group_fd)
{
	int device_fd;

	device_fd = ioctl(group_fd, VFIO_GROUP_GET_DEVICE_FD, vfio_device_id);
	if (device_fd < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to open device %s: %s (errno=%d)\n",
				vfio_device_id, strerror(saved_errno), saved_errno);
		goto error;
	}

	// print device capabilities
	__vfio_dev_device_get_info(vfio_device_id, device_fd);

//success
	return device_fd;
error:
	return EXIT_FAILURE;
}

static inline void __vfio_dev_device_close(int device_fd)
{
	close(device_fd);
}


/*****************************************************************************/

static inline int __vfio_dev_group_open(const char *vfio_group_path,
										int container_fd)
{
	int group_fd;

	/**
	 * Open group (note that there might be multiple devices in the same group)
	 *
	 * Before assigning to the container, make sure the group is viable (every
	 * PCIe device of the group has to be bound to the VFIO driver).
	 */
	group_fd = open(vfio_group_path, O_RDWR);
	if (group_fd < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to open %s: %s (errno=%d)\n",
				vfio_group_path, strerror(saved_errno), saved_errno);
		goto error;
	}

	struct vfio_group_status status = { .argsz = sizeof(status) };
	if (ioctl(group_fd, VFIO_GROUP_GET_STATUS, &status) < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to get group %s status: %s (errno=%d)\n",
				vfio_group_path, strerror(saved_errno), saved_errno);
		goto close_group;
	}

	if (!(status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		fprintf(stderr, "Group %s is not viable\n", vfio_group_path);
		goto close_group;
	}

	if (ioctl(group_fd, VFIO_GROUP_SET_CONTAINER, &container_fd) < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to set group %s container: %s (errno=%d)\n",
				vfio_group_path, strerror(saved_errno), saved_errno);
		goto close_group;
	}

	if (ioctl(container_fd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU) < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to set container IOMMU type: %s (errno=%d)\n",
				strerror(saved_errno), saved_errno);
		goto close_group;
	}

//success
	return group_fd;

close_group:
	close(group_fd);
error:
	return EXIT_FAILURE;
}

static inline void __vfio_dev_group_close(int group_fd)
{
	close(group_fd);
}


/*****************************************************************************/

static inline int __vfio_dev_container_open(const char *vfio_container_path)
{
	int version, container_fd;

	/**
	 * Create a new DMA address space.
	 *
	 * Validate the API version and make sure VFIO_TYPE1_IOMMU (default x86/
	 * x86_64 systems choice, supporting page fault and DMA mapping.
	 */
	container_fd = open(vfio_container_path, O_RDWR);
	if (container_fd < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to open %s: %s (errno=%d)\n",
				vfio_container_path, strerror(saved_errno), saved_errno);
		goto error;
	}

	// Confirm the kernel's VFIO API matches what this program was written against.
	version = ioctl(container_fd, VFIO_GET_API_VERSION);
	if (version < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to get VFIO API version: %s (errno=%d)\n",
				strerror(saved_errno), saved_errno);
		goto close_container;
	}
	if (version != VFIO_API_VERSION) {
		fprintf(stderr, "Unsupported VFIO API version %d (expected %d)\n",
				version, VFIO_API_VERSION);
		goto close_container;
	}

	// The container must support the TYPE1 IOMMU before we can use it.
	int type1_supported = ioctl(container_fd, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU);
	if (type1_supported < 0) {
		/* Actual ioctl failure - errno is valid here. */
		int saved_errno = errno;
		fprintf(stderr, "Fail to check VFIO_TYPE1_IOMMU extension: %s (errno=%d)\n",
				strerror(saved_errno), saved_errno);
		goto close_container;
	}
	if (type1_supported == 0) {
		/* A documented, non-error answer ("not supported") - errno is not set here. */
		fprintf(stderr, "VFIO_TYPE1_IOMMU is not supported by this container\n");
		goto close_container;
	}

//success
	return container_fd;

close_container:
	close(container_fd);
error:
	return EXIT_FAILURE;
}

static inline void __vfio_dev_container_close(int container_fd)
{
	close(container_fd);
}


/*****************************************************************************/

int vfio_dev_open(const char *vfio_container_path, const char *vfio_group_path,
				const char *vfio_device_id, vfio_dev_t *vfio_dev)
{
	int container_fd, group_fd, device_fd;

	container_fd = __vfio_dev_container_open(vfio_container_path);
	if (container_fd < 0)
		goto error;

	group_fd = __vfio_dev_group_open(vfio_group_path, container_fd);
	if (group_fd < 0)
		goto close_container;

	device_fd = __vfio_dev_device_open(vfio_device_id, group_fd);
	if (device_fd < 0)
		goto close_group;

// success
	printf("Successfully open container %s, group %s, device %s\n",
		vfio_container_path, vfio_group_path, vfio_device_id);

	vfio_dev->container_fd = container_fd;
	vfio_dev->group_fd = group_fd;
	vfio_dev->device_fd = device_fd;

	return EXIT_SUCCESS;

close_group:
	__vfio_dev_group_close(group_fd);
close_container:
	__vfio_dev_container_close(container_fd);
error:
	return EXIT_FAILURE;
}

void vfio_device_close(vfio_dev_t *vfio_dev)
{
	__vfio_dev_device_close(vfio_dev->device_fd);
	__vfio_dev_group_close(vfio_dev->group_fd);
	__vfio_dev_container_close(vfio_dev->container_fd);
}
