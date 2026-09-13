#!/usr/bin/env bash
# Boots the vfio-nic-demo lab VM.
#
# Two NICs:
#   net0 / virtio-net-pci  -> connectivity NIC, user-mode networking,
#                             SSH reachable at localhost:2222. This is
#                             how you always get back in, even after
#                             the lab NIC is unbound from the guest kernel.
#   net1 / e1000           -> the lab NIC. Backed by host tap device
#                             "tap0" (see ../README.md for one-time
#                             setup of tap0 -- requires sudo, run once).
set -euo pipefail
cd "$(dirname "$0")"

exec qemu-system-x86_64 \
  -enable-kvm \
  -machine q35,kernel-irqchip=split \
  -cpu host \
  -smp 2 \
  -m 2G \
  -device intel-iommu,intremap=on,caching-mode=on \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0,id=connectivity-nic,mac=52:54:00:00:00:10 \
  -netdev tap,id=net1,ifname=tap0,script=no,downscript=no \
  -device e1000,netdev=net1,id=lab-nic,mac=52:54:00:12:34:56 \
  -drive file=lab-vm.qcow2,if=virtio,format=qcow2 \
  -drive file=seed.iso,if=virtio,format=raw,readonly=on \
  -nographic
