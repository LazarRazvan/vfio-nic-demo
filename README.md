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

## Implementation

The code is split into a generic layer and a chip-specific layer:

- **`src/vfio.c` / `src/vfio.h`** — the generic VFIO mechanism, with no
  knowledge of `e1000` at all: open/close the container → group →
  device chain, map a device's BAR into our address space, map/unmap
  process memory into the device's IOVA space for DMA, and enable PCI
  bus mastering.
- **`src/e1000_regs.h`** — register offsets and bit definitions, taken
  directly from the Intel 8254x datasheet.
- **`src/e1000.h` / `src/e1000.c`** — chip-specific logic built on top
  of `vfio.c`: reset/link-up sequencing, masking interrupts (this is a
  polling-mode design, like DPDK's PMDs), allocating DMA-backed RX/TX
  rings and buffer pools, programming the RX ring (base address,
  length, head/tail, multicast table, control register), and
  `e1000_dev_rx_poll()` — checks the next expected descriptor for
  completion, copies the frame out of its DMA buffer, and recycles the
  descriptor back to the NIC by advancing the tail register.
- **`src/main.c`** — opens the device, polls in a loop, hex-dumps
  whatever it receives.

**Currently working, verified end-to-end:** a real Ethernet frame
injected from the host (e.g. `arping` targeting the `tap0` device that
backs the lab NIC) is DMA'd by the NIC into userspace memory and read
back purely by polling the RX descriptor ring — no kernel network
driver involved on the guest side at all.

**Not yet implemented:** the TX path (construct and transmit a frame),
and interrupt-driven operation (currently polling-only).

## Repo layout

```
docs/     development notes, one file per stage worked through
host/     notes + commands for the QEMU VM and vfio-pci binding
src/      the C implementation
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

## Build & run

```sh
cd src && make
sudo ./vfio_lab
```

`main.c` hardcodes the VFIO container/group paths and the device's
PCI BDF for this lab's setup (`/dev/vfio/vfio`, `/dev/vfio/3`,
`0000:00:03.0`) — adjust them to match your own `lspci`/IOMMU group if
you're not using the VM scaffolding under `host/`.
