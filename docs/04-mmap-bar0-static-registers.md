# Stage 4 — mmap BAR0 and read known-good registers

## Goal

Map BAR0 into your process's address space and prove the MMIO path
works end-to-end by reading registers whose values you can predict —
before you try to change any NIC state.

## Concepts

- `mmap()` on the device fd, using the `offset` value from BAR0's
  `vfio_region_info` (not the BAR's *bus* address — VFIO gives you an
  opaque fd offset that stands in for the region), maps BAR0's MMIO
  window into your virtual address space, exactly like the DPDK PMD
  does for the real NIC.
- MMIO reads/writes must go through `volatile` pointers of the correct
  width (mostly 32-bit for e1000 registers) — the compiler must not
  reorder, cache, or coalesce these accesses.
- Two registers are good first targets:
  - **STATUS** (offset `0x0008`): bit fields include link-up, speed,
    full/half duplex. Even with no cable/traffic, this register reads
    a defined value on a freshly-reset device.
  - **CTRL** (offset `0x0000`): device control — reset bit, link-up
    bit, speed selection, etc. Reading it (without writing yet) shows
    you the power-on defaults.

## Steps to work out yourself

1. `mmap(PROT_READ|PROT_WRITE, MAP_SHARED, device_fd, bar0_offset)`
   sized to BAR0's reported size.
2. Cast the returned pointer to `volatile uint32_t *`, index by
   register-offset/4.
3. Read CTRL and STATUS, print them in hex, and manually decode a
   couple of bits against the datasheet (e.g. STATUS.LU — link up).

## Checkpoint

You get non-garbage, datasheet-plausible values — not all-`0xFFFFFFFF`
(which usually means you're reading the wrong offset, an unmapped
region, or the device isn't actually responding) and not all-zero in a
way that contradicts the datasheet's stated reset defaults.

## References

- Intel 8254x SDM: Register Descriptions chapter — CTRL (0x0000) and
  STATUS (0x0008) bit layouts
- `mmap(2)`, in particular the `offset` argument semantics when the
  fd is a VFIO device fd rather than a regular file
