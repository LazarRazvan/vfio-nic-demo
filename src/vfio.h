/*
 * Copyright (c) 2026 Razvan Lazar
 */

#ifndef _VFIO_H_
#define _VFIO_H_


/*****************************************************************************/

typedef struct vfio_dev_s {

	int container_fd;
	int group_fd;
	int device_fd;

} vfio_dev_t;


/*****************************************************************************/

int vfio_dev_open(const char *vfio_container_path, const char *vfio_group_path,
				const char *vfio_device_id, vfio_dev_t *vfio_dev);
void vfio_device_close(vfio_dev_t *vfio_dev);

#endif	// _VFIO_H_
