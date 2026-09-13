# Stage 1 — Bind the NIC to vfio-pci

## Goal

Detach the lab NIC from whatever kernel driver claims it (`e1000` /
`e1000e`) and bind it to `vfio-pci` instead, so no in-kernel driver
touches it anymore — only your userspace program will, via `/dev/vfio`.

## Concepts

- A PCI device is normally owned by exactly one kernel driver at a
  time. `vfio-pci` is itself a (trivial) kernel driver whose only job
  is to expose the device through the VFIO API instead of doing
  anything with it.
- `driver_override` lets you force a specific device to bind to
  `vfio-pci` on next probe, without needing the device's PCI ID
  statically registered to that driver.
- Every PCI device belongs to an **IOMMU group** — the smallest set of
  devices the IOMMU can isolate from each other. VFIO operates at the
  group granularity: to hand a device to userspace, every device in
  its group must be unbound from its kernel driver (or already itself
  vfio-pci-bound). In a QEMU VM with PCIe root ports, each device
  usually gets its own group, but you should still check.

## Steps to work out yourself

1. Identify the BDF of the lab NIC (from Stage 0).
2. Find its IOMMU group via the `/sys/bus/pci/devices/<BDF>/iommu_group`
   symlink.
3. Check what else is in that group
   (`/sys/kernel/iommu_groups/<N>/devices/`) — if there's more than
   your NIC, you need a plan for the rest too.
4. Unbind the current driver, set `driver_override` to `vfio-pci`,
   and re-trigger probing.
5. Confirm the new driver with `lspci -k`.

## Checkpoint

- `lspci -k -s <BDF>` reports `Kernel driver in use: vfio-pci`.
- `/dev/vfio/<group-number>` exists.
- The NIC no longer appears as a network interface (`ip link`) in the
  guest — that's expected and correct.

## References

- Linux kernel docs: `Documentation/driver-api/vfio.rst`
- `/sys/bus/pci/devices/<BDF>/driver_override`
- `/sys/bus/pci/drivers_probe`
- `/sys/bus/pci/devices/<BDF>/iommu_group`
