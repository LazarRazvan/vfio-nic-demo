# Stage 7 — Build the RX descriptor ring and program the NIC

## Goal

Connect the two things you've built so far — BAR0 MMIO control and
IOVA-mapped RAM — by constructing an RX descriptor ring in that RAM
and telling the NIC where it is via BAR registers. This is the exact
two-level structure from the earlier discussion: register → ring →
buffer.

## Concepts

- The e1000 **legacy RX descriptor** is a small fixed layout containing
  a 64-bit buffer address (the *IOVA* of a packet buffer you mapped in
  Stage 6) plus length/status/error fields the NIC fills in after DMA.
  Look this struct up in the datasheet and define it yourself.
- Registers you'll program through BAR0 (all under the RX Ring
  register group): `RDBAL`/`RDBAH` (ring base address, low/high 32
  bits of the ring's IOVA), `RDLEN` (ring length in bytes), `RDH`
  (head — NIC-owned, read-only from your side), `RDT` (tail — you
  advance this to hand descriptors to the NIC).
- `RCTL` (RX Control register) has to be configured (buffer size,
  strip-CRC, broadcast-accept, and finally the enable bit) before
  anything will actually happen.
- Ownership protocol: initially you own all descriptors. You fill each
  one with a buffer IOVA, then set `RDT` to the index of the *last*
  descriptor you've handed over — everything from just-after-head to
  tail is now NIC-owned. When the NIC completes a receive into a
  descriptor, it sets a status bit (DD — descriptor done) and you take
  ownership of that slot back.

## Steps to work out yourself

1. Define the RX descriptor struct exactly per the datasheet.
2. Populate every descriptor slot in your RX ring with the IOVA of a
   distinct packet buffer from Stage 6.
3. Program RDBAL/RDBAH with the ring's IOVA, RDLEN with its size.
4. Set RDH = 0 (or leave it; it's NIC-owned) and RDT to hand every
   descriptor to the NIC (typically ring depth − 1).
5. Configure and enable RCTL.

## Checkpoint

No traffic yet — this stage's checkpoint is purely: RDBAL/RDBAH/RDLEN
read back exactly what you wrote (confirming BAR writes for 64-bit
values split across two 32-bit registers are done correctly), and
RCTL's enable bit reads back as set.

## References

- Intel 8254x SDM: Receive Descriptor Format, and Receive Initialization
  (RAL/RDBAL/RDBAH/RDLEN/RDH/RDT/RCTL) sections
