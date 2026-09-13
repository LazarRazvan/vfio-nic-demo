# vfio-nic-demo

A hands-on lab for understanding how DPDK's PMD + VFIO stack works, by
building the equivalent of a minimal network driver **in userspace**,
against a virtual NIC, without DPDK itself.

## Goal

Reproduce, by hand, the mechanism DPDK relies on:

```
userspace process
   ├── VFIO: bind device, map its BARs into our address space (MMIO)
   ├── VFIO: map our RAM into the device's IOVA space (DMA)
   ├── program NIC registers through BAR MMIO (rings, control bits)
   └── receive / transmit real Ethernet frames via descriptor rings
```

No DPDK is used. Everything is direct VFIO ioctl + mmap, in C, against
QEMU's emulated Intel `e1000` NIC (82540EM) running inside a VM.

## Why `e1000` and not `virtio-net`

`virtio-net` is paravirtualized (virtqueues, avail/used rings) — it
teaches virtio, not "how a real NIC's BAR + descriptor ring works".
`e1000` is a full register-level emulation of a real Intel NIC: BAR0
MMIO region, RDBAL/RDBAH/RDLEN/RDH/RDT and TDBAL/TDBAH/TDLEN/TDH/TDT
registers, legacy descriptors with a physical/IOVA buffer address.
That is exactly the shape of the model built up in the DPDK/VFIO
discussion this repo grew out of, and DPDK ships a real `e1000`/`em`
PMD you can diff your own code against once it works.

## Repo layout

```
docs/     one file per stage: goal, concepts, checkpoint, references
host/     notes + commands for the QEMU VM and vfio-pci binding
src/      your C implementation (intentionally empty — see docs/)
```

## Prerequisites

- A Linux host with KVM (`kvm-ok` / `/dev/kvm` present)
- QEMU >= 6.x, with `intel-iommu` device support
- A guest VM image with a kernel built with `CONFIG_VFIO`,
  `CONFIG_VFIO_PCI`, `CONFIG_INTEL_IOMMU` enabled (most distro kernels
  already have these)
- Comfortable reading a datasheet: the **Intel 8254x Family GbE
  Controllers Software Developer's Manual** is the primary reference
  for every register offset used in this lab

## How to use this repo

Work through `docs/00-...` to `docs/10-...` in order. Each stage:

1. States the **goal** — what capability you're adding
2. Lists the **concepts** it depends on
3. Gives a **checkpoint** — an observable result proving it worked
4. Points to the exact **references** (datasheet section, kernel doc,
   VFIO header) you need, without giving you the code

Implement each stage in `src/` yourself before moving to the next one.
Nothing under `src/` is pre-written on purpose — the point of the lab
is writing the ioctl/mmap sequences yourself.
