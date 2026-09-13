# Lab VM

## One-time host setup: the lab NIC's tap device

The lab NIC (`e1000`, "net1" in `launch.sh`) is backed by a host tap
device, `tap0`, so that from Stage 8 onward you can inject/capture raw
Ethernet frames directly against it from the host side (no bridge —
it's a simple point-to-point pipe between `tap0` and the guest's
`e1000`).

Creating a persistent tap device needs `CAP_NET_ADMIN`, so run this
yourself once (not run automatically by anything in this repo):

```sh
sudo ip tuntap add dev tap0 mode tap user "$(whoami)"
sudo ip link set tap0 up
```

Verify: `ip link show tap0` should show it `UP`, owned by you.

This persists until reboot (or `sudo ip tuntap del dev tap0 mode tap`).
Re-run it after a host reboot before using `launch.sh` again.

## Boot the VM

```sh
./launch.sh
```

First boot: cloud-init applies `intel_iommu=on` via `grubby` and
reboots itself once automatically — expect two boot cycles before you
reach a login prompt. You're on the serial console (`-nographic`);
`Ctrl-a x` quits QEMU, `Ctrl-a c` toggles the QEMU monitor.

## Connect

```sh
ssh -i ssh/id_ed25519 -p 2222 lab@localhost
```

(`lab` has passwordless sudo, for binding devices / loading modules.)

## Verify Stage 0's checkpoint

Inside the guest:

```sh
dmesg | grep -i iommu       # IOMMU enabled, groups created
lspci -nn                   # look for 8086:100e (the e1000 lab NIC)
ip link                     # eth0/enp0s* = connectivity NIC, up, has an IP
```

The lab NIC (`8086:100e`) should currently still be bound to the
in-kernel `e1000` driver and visible as a second network interface —
that's expected; Stage 1 is what moves it to `vfio-pci`.

**Verified on this setup:** lab NIC is `0000:00:03.0`, alone in IOMMU
group 3 (no sibling devices to worry about when unbinding it in
Stage 1). Connectivity NIC is `enp0s2`; lab NIC is `enp0s3`.

## Files here

- `fedora-cloud-base.qcow2` — pristine downloaded image, never boot
  this directly
- `lab-vm.qcow2` — the actual VM disk (qcow2 overlay on the base
  image, so re-doing the lab is `rm lab-vm.qcow2 && qemu-img create
  -f qcow2 -F qcow2 -b fedora-cloud-base.qcow2 lab-vm.qcow2 20G`)
- `seed.iso` — cloud-init NoCloud seed (built from `cloud-init/`)
- `ssh/id_ed25519*` — dedicated keypair for this VM only
