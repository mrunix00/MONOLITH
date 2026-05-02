# Roadmap

## Branding

- [x] Figure out a name for the project.
- [x] Design logo and visual identity.

## Boot & Initial System Setup

- [x] Build a cross-compiler toolchain.
- [x] Boot into x86_64.
- [x] UEFI support.
- [x] GDT Initialization.
- [x] IDT Initialization.

## Basic I/O

- [x] Serial port driver.
- [x] PS/2 keyboard driver.
- [x] PS/2 Mouse driver.
- [ ] USB keyboard driver.
- [ ] USB mouse driver.

## Memory Management

- [x] Physical memory manager.
- [x] Virtual memory manager.
- [x] Heap allocator.

## Storage

- [x] Virtual File System.
- [x] Initial ramdisk (initrd) file system.
- [ ] ISO9660 filesystem support.
- [ ] FAT filesystem support.
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
- [x] PIT Timer.
- [x] APIC.
- [ ] APIC timer.
- [ ] HPET.
- [ ] SMP.

## User Mode & Process Management

- [x] Loading user programs.
- [x] System calls.
- [x] Task scheduler.
- [x] Run programs in userspace.
- [x] IPC.

## Architecture support

- [x] x86_64.
- [x] ~~i386.~~ (Deprecated)
- [ ] aarch64.
- [ ] riscv64.

## Hardware Support

- [ ] ACPI.
- [ ] PCI.
- [ ] USB.
- [ ] IDE/ATA disk driver.
- [ ] AHCI disk driver.

## Video and Graphics

- [x] GUI.
- [x] Graphics library.
- [x] Font rendering.
- [x] Window manager.
- [x] Display protocol.
- [ ] UI library.

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
