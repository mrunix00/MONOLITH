# MONOLITH's Roadmap

## Branding

- [x] Figure out a name for the project.
- [x] Design a logo.

## CPU Architecture support

- [x] pc/x64 (x86_64).
- [x] pc/ia32 (x86/i686/IA-32).
- [ ] aarch64.
- [ ] riscv64.

## Basic I/O

- [x] Serial port.
- [x] PS/2 mouse/keyboard.
- [ ] USB mouse/keyboard.
- [ ] I2C touchpads.

## Memory Management

- [x] Physical memory management.
- [x] Virtual memory management.
- [x] Kernel heap allocator.
- [ ] Demand paging.
- [ ] Swap.

## Storage

- [x] Virtual File System.
- [x] tmpfs.
- [x] initramfs.
- [ ] ATA.
- [ ] AHCI.
- [ ] MBR.
- [ ] GPT.
- [ ] FAT32
  - [ ] Read.
  - [ ] Write.
  - [ ] Create files.
  - [ ] Format partitions.
- [ ] EXT2 filesystem support.
  - [ ] Read.
  - [ ] Write.
  - [ ] Create files.
  - [ ] Format partitions.

## CPU & Multiprocessing

- [x] SIMD (SSE, AVX, ...).
- [x] PIT.
- [x] APIC.
- [ ] APIC timer.
- [ ] HPET.
- [ ] SMP.
- [x] Multi-tasking.
- [x] Scheduling.

## Userspace

- [x] Loading user programs.
- [x] System calls.
- [x] Run programs in ring-3.
- [x] IPC.

## Video, Graphics and Desktop

- [x] GUI.
- [x] Graphics library (libgfx).
- [x] Font rendering.
- [x] Display protocol (libdesktop).
- [ ] UI Library ([BlitzUI](https://codeberg.org/MONOLITH-Project/BlitzUI)).
- [ ] Desktop icons.
- [ ] File manager.

## Hardware support

- [ ] PCI.
- [ ] PCIe.
- [ ] ACPI.
- [ ] USB.

## Networking

- [ ] Ethernet driver.
- [ ] TCP/IP stack.
  - [ ] IPv4 implementation.
  - [ ] IPv6 implementation.
  - [ ] TCP protocol.
  - [ ] UDP protocol.
  - [ ] ICMP protocol (ping).
  - [ ] Socket API.
- [ ] DHCP client.
- [ ] DNS client.
- [ ] TLS/SSL support.
