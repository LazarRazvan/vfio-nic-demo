# Stage 5 — Reset and bring the NIC up

## Goal

Perform the same "device bring-up" sequence a real driver's `probe()`
does, entirely through BAR0 MMIO: global reset, wait for it to
complete, then bring the link up. This is the point where you go from
"reading registers" to "controlling hardware state".

## Concepts

- Writing the reset bit in CTRL triggers a full device reset. After
  a reset, the driver must **not** assume immediate completion — it
  polls until the reset bit self-clears (or waits a datasheet-specified
  delay), because the hardware needs time to reinitialize internally.
- After reset, most registers return to documented power-on defaults —
  useful, because it means your later ring-setup stages always start
  from a known state.
- Bringing the link up (CTRL.SLU — Set Link Up) is what makes the
  device actually negotiate/present a link, needed before RX/TX will
  do anything meaningful.
- This is also a good place to introduce a **register access helper**
  pattern in your own code (a small read32/write32 wrapper around the
  BAR0 pointer) — not because it's "the implementation" for you to
  copy, but because every later stage will reuse it constantly.

## Steps to work out yourself

1. Write the reset bit into CTRL.
2. Poll CTRL until the reset bit reads back as cleared (with a
   sane timeout/retry bound — don't spin forever if something's wrong).
3. Re-read STATUS/CTRL and confirm they match documented post-reset
   defaults.
4. Set CTRL.SLU, optionally clear CTRL.LRST if the datasheet's
   reset/link sequence requires it for the emulated model.
5. Poll STATUS.LU (link up) and confirm it eventually asserts.

## Checkpoint

STATUS.LU transitions from 0 to 1 after your bring-up sequence, purely
as a result of your own MMIO writes — no kernel driver involved at any
point.

## References

- Intel 8254x SDM: Initialization chapter — general reset and link
  setup sequence (the "software initialization sequence" the real
  driver follows)
- QEMU source, `hw/net/e1000.c`, for exactly which parts of the real
  reset/link sequence the emulation actually models (useful once
  you're debugging a stuck poll loop, not before)
