# Stage 8 — Receive a real packet

## Goal

Get an actual Ethernet frame, generated from outside your process,
DMA'd by the NIC into your userspace buffer — and detect and read it
using nothing but polling your own descriptor ring.

## Concepts

- Since the lab NIC has no kernel netdev anymore (it's vfio-pci
  bound), you can't just `ping` it. You need to inject a frame from
  the *host* side of whatever QEMU backend the lab NIC uses — e.g. if
  it's attached to a tap device bridged on the host, send a raw frame
  from the host targeting the guest NIC's MAC (broadcast ARP is an
  easy, reliable choice: every host on the segment sends those
  unprompted, or you can craft one deliberately with a small
  host-side script).
- Detecting a completed receive is pure polling: read back descriptor
  `status` field for the slot you expect next (tracked by your own
  software index, since `RDH` is only a hint) and check the DD
  (descriptor done) bit.
- Once DD is set, the descriptor's `length` field tells you how many
  bytes the NIC DMA'd into that buffer — read the buffer's *virtual*
  address (the one from your Stage 6 `mmap`, not the IOVA) to get at
  the actual bytes.
- After consuming a descriptor, you must give it back to the NIC:
  reset its status, and eventually advance `RDT` again so the ring
  keeps circulating — otherwise you'll receive exactly one packet and
  stall forever.

## Steps to work out yourself

1. Figure out how to get a frame onto the wire from the host side
   toward the lab NIC's MAC address (check the NIC's MAC via a CTRL/
   RAL-RAH register read, or QEMU's device configuration).
2. Poll your RX ring's next expected descriptor for the DD bit.
3. On DD set: read `length`, dump the buffer bytes, parse at least the
   Ethernet header (dest MAC, src MAC, ethertype) to prove you
   actually decoded a real frame.
4. Recycle the descriptor and advance RDT.

## Checkpoint

Your program prints something like: `RX: 60 bytes, src=aa:bb:cc:.. dst=ff:ff:ff:ff:ff:ff ethertype=0x0806`
for a frame you generated externally — proving the full path
`host → wire → NIC DMA engine → your IOVA-mapped RAM → your polling loop`
works, entirely without a kernel network driver.

## References

- Intel 8254x SDM: Receive Descriptor status/length field layout
- A packet-crafting tool on the host side (e.g. `scapy`, or even
  `arping`) to generate the test frame
- `tcpdump` on the host-side tap/bridge interface, to confirm what you
  actually sent, if the guest-side result looks wrong
