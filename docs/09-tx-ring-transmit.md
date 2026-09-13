# Stage 9 — Build the TX ring and transmit a frame

## Goal

Mirror Stage 7/8 in the other direction: construct a frame in your
IOVA-mapped memory, hand it to the NIC via a TX descriptor, and prove
it actually left the machine.

## Concepts

- The TX side has its own descriptor format (different fields than
  RX — notably command/status bits you set to request things like
  "insert CRC" and "end of packet"), and its own register group:
  `TDBAL`/`TDBAH`, `TDLEN`, `TDH`, `TDT`, and `TCTL` (TX control,
  needs its own enable bit plus collision/backoff parameters that
  matter for real hardware but you'll just set to sane defaults here).
- Direction of ownership is reversed from RX: you build the frame in a
  buffer, write that buffer's IOVA + length into the next TX
  descriptor, set the required command bits (end-of-packet, report
  status, insert CRC), then advance `TDT` — that's the doorbell telling
  the NIC "a new descriptor is ready, go DMA-read it and transmit".
- You can construct a minimal valid Ethernet frame by hand (dest MAC,
  src MAC — read your own from RAL/RAH, ethertype, payload) — an ARP
  request or a raw broadcast is simplest since you don't need a full
  IP/UDP stack for this lab.

## Steps to work out yourself

1. Program TDBAL/TDBAH/TDLEN, configure and enable TCTL.
2. Build a small valid Ethernet frame in one of your Stage 6 buffers.
3. Write that buffer's IOVA + length into the next TX descriptor, with
   the correct command bits set.
4. Advance TDT past that descriptor.
5. Poll the descriptor's status DD bit to confirm the NIC finished
   with it (don't reuse/overwrite the buffer before that).

## Checkpoint

`tcpdump` on the host-side tap/bridge interface shows your
hand-crafted frame arriving, with the exact bytes you constructed —
proof that userspace, through nothing but VFIO ioctls and MMIO/DMA,
just transmitted a real Ethernet frame.

## References

- Intel 8254x SDM: Transmit Descriptor Format, Transmit Initialization
  (TDBAL/TDBAH/TDLEN/TDH/TDT/TCTL) sections
- `tcpdump -i <host-tap-or-bridge> -XX` to inspect exact bytes
