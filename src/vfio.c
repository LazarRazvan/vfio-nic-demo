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

static inline void __vfio_device_get_region_info(int device_fd, uint32_t index)
{
	/**
	 * Get device region config.
	 */
	struct vfio_region_info region = { .argsz = sizeof(region), .index = index };
	if (ioctl(device_fd, VFIO_DEVICE_GET_REGION_INFO, &region) < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to get device %s region %d info: %s (errno=%d)\n",
				VFIO_DEVICE_ID, index, strerror(saved_errno), saved_errno);
		return;
	}

	//
	printf("[%s] region %d: R(%u) W(%u) M(%u) C(%u); size %llu; offset 0x%llx\n",
		VFIO_DEVICE_ID, index,
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
					VFIO_DEVICE_ID, strerror(saved_errno), saved_errno);
			return;
		}

		uint16_t vendor = buf[0] | (buf[1] << 8);
		uint16_t device = buf[2] | (buf[3] << 8);
		printf("[%s] region %d: venodr %x; device %d\n", VFIO_DEVICE_ID, index,
			vendor, device);
	}
}

static void __vfio_device_get_info(int device_fd)
{
	/**
	 * Get device capabilities.
	 */
	struct vfio_device_info info = { .argsz = sizeof(info) };
	if (ioctl(device_fd, VFIO_DEVICE_GET_INFO, &info) < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to get device %s info: %s (errno=%d)\n",
				VFIO_DEVICE_ID, strerror(saved_errno), saved_errno);
		goto finish;
	}

	//
	printf("[%s] num_regions %u; num_irqs %u\n", VFIO_DEVICE_ID,
		info.num_regions, info.num_irqs);

	// print regions
	for (uint32_t i = 0; i < info.num_regions; i++)
		__vfio_device_get_region_info(device_fd, i);

finish:
}


/*****************************************************************************/

int main()
{
	int version, container_fd, group_fd, device_fd;

	/**
	 * Create a new DMA address space.
	 *
	 * Validate the API version and make sure VFIO_TYPE1_IOMMU (default x86/
	 * x86_64 systems choice, supporting page fault and DMA mapping.
	 */
	container_fd = open(VFIO_CONTAINER_PATH, O_RDWR);
	if (container_fd < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to open %s: %s (errno=%d)\n",
				VFIO_CONTAINER_PATH, strerror(saved_errno), saved_errno);
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
	printf("VFIO API version %d successfully detected\n", version);

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

	printf("VFIO_TYPE1_IOMMU is supported!\n");


	/**
	 * Open group (note that there might be multiple devices in the same group)
	 *
	 * Before assigning to the container, make sure the group is viable (every
	 * PCIe device of the group has to be bound to the VFIO driver).
	 */
	group_fd = open(VFIO_GROUP_PATH, O_RDWR);
	if (group_fd < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to open %s: %s (errno=%d)\n",
				VFIO_GROUP_PATH, strerror(saved_errno), saved_errno);
		goto close_container;
	}

	struct vfio_group_status status = { .argsz = sizeof(status) };
	if (ioctl(group_fd, VFIO_GROUP_GET_STATUS, &status) < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to get group %s status: %s (errno=%d)\n",
				VFIO_GROUP_PATH, strerror(saved_errno), saved_errno);
		goto close_group;
	}

	if (!(status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		fprintf(stderr, "Group %s is not viable\n", VFIO_GROUP_PATH);
		goto close_group;
	}

	if (ioctl(group_fd, VFIO_GROUP_SET_CONTAINER, &container_fd) < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to set group %s container: %s (errno=%d)\n",
				VFIO_GROUP_PATH, strerror(saved_errno), saved_errno);
		goto close_group;
	}

	printf("Group %s successfully added to container\n", VFIO_GROUP_PATH);

	// Only valid now that a group is attached to the container (kernel requirement).
	if (ioctl(container_fd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU) < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to set container IOMMU type: %s (errno=%d)\n",
				strerror(saved_errno), saved_errno);
		goto close_group;
	}

	printf("Container IOMMU type set to VFIO_TYPE1_IOMMU\n");


	/**
	 * Get device by id in the group.
	 */
	device_fd = ioctl(group_fd, VFIO_GROUP_GET_DEVICE_FD, VFIO_DEVICE_ID);
	if (device_fd < 0) {
		int saved_errno = errno;
		fprintf(stderr, "Fail to get device %s of group %s: %s (errno=%d)\n",
				VFIO_DEVICE_ID, VFIO_GROUP_PATH, strerror(saved_errno), saved_errno);
		goto close_group;
	}

	printf("Device %s successfully obtained (fd=%d)\n", VFIO_DEVICE_ID, device_fd);

	/**
	 * Get device capabilities.
	 */
	__vfio_device_get_info(device_fd);

	return EXIT_SUCCESS;

close_group:
	close(group_fd);
close_container:
	close(container_fd);
error:
	return EXIT_FAILURE;
}
