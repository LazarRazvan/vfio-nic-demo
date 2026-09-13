# Stage 6 — Allocate DMA memory and map it into IOVA space

## Goal

Give the NIC something to DMA into/out of: userspace RAM that is
mapped through the IOMMU so the device can address it via IOVA, the
exact mechanism DPDK uses for mbufs and descriptor rings.

## Concepts

- This is unrelated to BAR/MMIO — it's the *other* VFIO capability:
  `VFIO_IOMMU_MAP_DMA` on the **container** fd (not the device fd),
  which tells the IOMMU "translate IOVA X to physical address behind
  virtual address Y, for size Z".
- You get to choose the IOVA values yourself (they don't need to
  relate to the memory's real physical address at all) — pick a
  layout that's easy to reason about, e.g. one region for the RX
  descriptor ring, one for the TX descriptor ring, one pool for packet
  buffers, each at a distinct, non-overlapping IOVA base you assign.
- Memory must be mapped with `mmap(MAP_ANONYMOUS|MAP_PRIVATE)` (or
  similar) first, at page granularity, *before* it's handed to
  `VFIO_IOMMU_MAP_DMA` — the kernel needs page-backed memory to pin.
- Think in terms of three logical allocations, even if you implement
  them as one bigger mapping: RX ring, TX ring, packet buffer pool.
  Decide a buffer size per packet (a common real-world choice is
  2048 bytes, matching typical mbuf data-room sizes) and a ring depth
  (e.g. 8 or 16 descriptors is plenty for this lab — you don't need
  DPDK's usual hundreds/thousands).

## Steps to work out yourself

1. Decide your IOVA layout on paper first: ring base(s), buffer pool
   base, sizes. Write it down before coding — this becomes the
   "memory map" comment at the top of your program.
2. `mmap` the backing userspace memory.
3. Build and issue one (or more) `VFIO_IOMMU_MAP_DMA` calls against
   the container fd, with `VFIO_DMA_MAP_FLAG_READ|WRITE`.
4. Handle and understand any failure here before moving on —
   alignment, size, or flag mistakes surface immediately as ioctl
   errors, not silent corruption later.

## Checkpoint

All `VFIO_IOMMU_MAP_DMA` calls succeed, and you have, printed out, a
clear map like:

```
RX ring   IOVA 0x10000000  size 0x1000
TX ring   IOVA 0x10001000  size 0x1000
RX bufs   IOVA 0x20000000  size N * 2048
TX bufs   IOVA 0x20100000  size N * 2048
```

## References

- `<linux/vfio.h>`: `struct vfio_iommu_type1_dma_map`
- Kernel docs: `Documentation/driver-api/vfio.rst`, DMA mapping section
- Your earlier EDU-device exercise — this is the identical ioctl,
  just organized as multiple regions with a purpose-built layout
  instead of one scratch buffer
