# Stage 0 — Environment setup

## Goal

Have a QEMU VM running with:

- An emulated Intel IOMMU (`intel-iommu`) so the guest kernel can use
  VFIO the same way a real DPDK host would use VT-d.
- One NIC used for normal guest connectivity (so you don't lock
  yourself out) — e.g. a `virtio-net-pci` or `e1000` device attached
  to a `-netdev user` or tap backend.
- A **second**, separate `e1000` PCI device that you will dedicate to
  this lab and eventually detach from the guest kernel entirely.

Keeping the lab NIC separate from your connectivity NIC matters: once
you bind it to `vfio-pci` it disappears from the guest's normal
network stack (no `ethX`, no IP, no routing) — that's the whole
point, but you don't want that to be your only way to reach the VM.

## Concepts

- Why VFIO needs an IOMMU: DMA remapping is what makes it safe to let
  an *unprivileged userspace process* control a device that does raw
  memory reads/writes (DMA). Without IOMMU translation, a userspace
  driver bug (or malice) could point the device's DMA engine at
  arbitrary host physical memory.
- `intel-iommu` in QEMU emulates VT-d *inside the guest*, purely for
  nested learning purposes — the guest kernel's VFIO stack talks to
  this emulated IOMMU exactly as it would talk to real hardware.
- `kernel-irqchip=split` is required alongside `intel-iommu` on QEMU's
  `q35` machine type for interrupt remapping to work correctly.

## Checkpoint

Inside the guest:

- `dmesg | grep -i iommu` shows the IOMMU being enabled and groups
  being created.
- `lspci -nn` shows two NICs: your connectivity NIC and a second
  device with vendor:device id `8086:100e` (82540EM / `e1000`).
- You can still reach the guest over the network via the first NIC.

## References

- QEMU docs: `q35` machine type + `intel-iommu` device options
- Guest kernel command line: `intel_iommu=on`
- `lspci -nn` — note the BDF (bus:device.function, e.g. `0000:00:05.0`)
  of the *second* NIC; that's your target for every later stage
