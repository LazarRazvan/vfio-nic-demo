# Stage 3 — Enumerate the device's regions

## Goal

Before touching any register, discover programmatically what regions
(BARs + config space) the device actually exposes, their sizes, and
whether they're mmap-able — the same discovery a real driver (or the
DPDK EAL) performs instead of hardcoding offsets.

## Concepts

- `VFIO_DEVICE_GET_INFO` tells you how many regions and how many
  interrupt types the device has.
- `VFIO_DEVICE_GET_REGION_INFO`, called once per region index
  (`VFIO_PCI_BAR0_REGION_INDEX` .. `VFIO_PCI_BAR5_REGION_INDEX`, plus
  `VFIO_PCI_CONFIG_REGION_INDEX`), returns that region's `size`,
  `offset` (used as the `mmap` offset argument later), and `flags`
  (readable / writable / mmap-able).
- The **config space region** is itself accessible through VFIO
  (read/write via `pread`/`pwrite` at its region offset) — you don't
  need raw `/sys/bus/pci/devices/<BDF>/config` for this; going through
  VFIO is the point of the exercise.
- For the `e1000`, expect BAR0 to be the MMIO register space
  (non-prefetchable) and BAR1/BAR2 to be I/O-port or flash regions you
  can ignore for this lab.

## Steps to work out yourself

1. Call `VFIO_DEVICE_GET_INFO`, print region/IRQ counts.
2. Loop over region indices 0..5, call `VFIO_DEVICE_GET_REGION_INFO`
   for each, print size/offset/flags. Note which ones report
   `VFIO_REGION_INFO_FLAG_MMAP`.
3. Also query the config-space region index, and `pread` the first 4
   bytes from it at file offset `region.offset` — confirm the
   vendor/device ID matches `8086:100e`.

## Checkpoint

You have a printed table of every BAR: index, size, flags. BAR0 is a
sensible size for a register window (64 KiB or similar, not gigantic —
that's the framebuffer-style prefetchable BAR case, not this device).
Reading config space through VFIO returns `8086:100e` (or whatever the
guest's exact e1000 flavor is — cross-check against `lspci -nn`).

## References

- `<linux/vfio.h>`: `struct vfio_device_info`, `struct
  vfio_region_info`, `VFIO_PCI_BAR0_REGION_INDEX` etc.
- Intel 8254x Software Developer's Manual, section on PCI
  configuration space and base address registers
