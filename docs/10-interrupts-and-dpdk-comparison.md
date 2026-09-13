# Stage 10 — Interrupts, then compare against DPDK (stretch goals)

## 10a. Interrupts

### Goal

Replace polling with an event-driven wakeup: the NIC raises an
interrupt on RX completion, and your userspace process blocks on a
file descriptor until it fires, instead of spinning on the DD bit.

### Concepts

- VFIO exposes device interrupts through `eventfd`, not signals. You
  create an eventfd, hand its fd number to the kernel via
  `VFIO_DEVICE_SET_IRQS` (specifying the IRQ index — MSI-X if the
  device/emulation supports it, legacy INTx otherwise), and then a
  plain `read()` on that eventfd blocks until the device fires that
  interrupt.
- You still need to service the actual condition (drain completed RX
  descriptors) after waking up — the interrupt just tells you "look
  now", it doesn't hand you the data.
- Compare this to how `rte_epoll_wait()` and DPDK's interrupt mode
  (as opposed to its default poll-mode driver behavior) work — DPDK
  is *usually* pure polling for throughput, but the same eventfd
  mechanism underlies its optional interrupt-driven mode.

### Checkpoint

Your program blocks on `read(event_fd, ...)`, and unblocks exactly
when a new frame arrives (Stage 8's traffic-injection method), without
any polling loop running in between.

### References

- `<linux/vfio.h>`: `struct vfio_irq_set`, `VFIO_DEVICE_SET_IRQS`
- `eventfd(2)`
- Intel 8254x SDM: Interrupts chapter (ICR/IMS/IMC registers) — note
  these still need configuring via BAR0 MMIO even though delivery goes
  through the eventfd

## 10b. Compare against DPDK

### Goal

Once RX and TX both work by hand, read the real thing and map every
piece of your code to its DPDK equivalent — this is what turns the
lab into durable understanding rather than a one-off exercise.

### Where to look

- `lib/eal/linux/eal_vfio.c` in the DPDK source — the container/
  group/device open sequence and `VFIO_IOMMU_MAP_DMA` calls you wrote
  by hand in Stages 2 and 6, generalized to handle multiple devices
  and hugepage-backed memory.
- `drivers/net/e1000/em_rxtx.c` (or `igb_rxtx.c`) — the real PMD's
  descriptor definitions and RX/TX burst functions, doing exactly what
  you did in Stages 7–9 but batched and optimized.
- `drivers/net/e1000/e1000_ethdev.c` / `base/e1000_hw.h` — where the
  actual register offsets you looked up by hand in the datasheet are
  defined as named constants.

### A question to answer for yourself at the end

For each of the following, be able to point at the exact line of your
own code and the exact DPDK equivalent:

1. Where does DPDK open the VFIO container/group/device?
2. Where does DPDK map hugepage memory as DMA-able?
3. Where does DPDK map the NIC's BAR0?
4. Where does DPDK write the RX/TX ring base IOVA into registers?
5. Where does DPDK's `rte_eth_rx_burst()` check the descriptor DD bit?
6. Where does DPDK's `rte_eth_tx_burst()` update the TX tail register?

If you can answer all six by pointing at real source lines, the lab
has done its job.
