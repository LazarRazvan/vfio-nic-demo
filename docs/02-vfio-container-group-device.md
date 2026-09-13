# Stage 2 — Open the VFIO container/group/device chain

## Goal

Get a valid device file descriptor for the NIC from pure userspace
code, with no library beyond the VFIO uapi headers. This is the
"become the driver" moment — every later stage builds on this `fd`.

## Concepts

The traditional VFIO API has three levels of object, and you open
them in order:

```
/dev/vfio/vfio         "container"  — owns the IOMMU mapping/address space
/dev/vfio/<group>      "group"      — the IOMMU-isolation unit
device fd (via group)  "device"     — the actual PCI function
```

- **Container**: created by opening `/dev/vfio/vfio`. You check its
  API version and that it supports the IOMMU model you intend to use
  (`VFIO_TYPE1_IOMMU` for this lab).
- **Group**: opened from `/dev/vfio/<N>` (N = IOMMU group number found
  in Stage 1). A group must be reported "viable" (every device in it
  is either unbound or vfio-bound) before you can use it. You then
  attach the group to your container.
- Only after the group is attached to the container can you call
  `VFIO_SET_IOMMU` on the container, and only after that can you ask
  the group for a device fd by BDF string.

## Steps to work out yourself

1. Open the container, check `VFIO_GET_API_VERSION`.
2. Check `VFIO_CHECK_EXTENSION` for `VFIO_TYPE1_IOMMU`.
3. Open the group device node, check `VFIO_GROUP_GET_STATUS` for the
   viable flag.
4. `VFIO_GROUP_SET_CONTAINER` to attach the group to the container.
5. `VFIO_SET_IOMMU` on the container.
6. `VFIO_GROUP_GET_DEVICE_FD` with the NIC's BDF string to get the
   device fd.

## Checkpoint

All ioctls return success (0 or a valid non-negative value), and step
6 hands you back a valid, distinct file descriptor for the NIC. A
good sanity print at this point: the fd number itself, plus confirming
`errno` is never set on the calls above.

## References

- `<linux/vfio.h>` — struct and ioctl number definitions
- Kernel docs: `Documentation/driver-api/vfio.rst`, section on the
  container/group/device model
- Man pages: `open(2)`, `ioctl(2)`
