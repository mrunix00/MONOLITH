# Build instructions

## Pre-requisites

### Fedora / RHEL

```console
sudo dnf install gcc gcc-c++ make bison flex gmp-devel libmpc-devel mpfr-devel texinfo isl-devel qemu nasm make grub2-tools-extra xorriso
```

### Arch / Manjaro

```console
sudo pacman -S base-devel qemu-desktop flex texinfo grub nasm libmpc gmp mpfr libisl flex
```

### Ubuntu / Debian

```console
sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libisl-dev nasm qemu-system xorriso mtools grub-pc-bin grub-common
```

### NixOS

```console
nix develop --extra-experimental-features 'nix-command flakes'
```

## Clone the repository

```console
git clone --recursive https://github.com/mrunix00/monolith.git
```

## Building the operating system

To run the OS in QEMU:

```console
make run
```

If you want to launch the OS without a graphical interface:

```console
make run-headless
```

## Cross-compilation

To build the operating system for a different architecture:

```console
ARCH="pc/x86_64" make
```

Supported architectures:

- `pc/i386`
- `pc/x86_64`

## Debugging

To run the OS in debug mode:

```console
make run-debug
gdb
(gdb) target remote localhost:1234
Remote debugging using localhost:1234
warning: No executable has been specified and target does not support
determining executable automatically.  Try using the "file" command.
0x000000000000fff0 in ?? ()
(gdb) symbol-file build/kernel.bin
Reading symbols from build/kernel.bin...
(gdb)
```

## Clangd configuration

The following `.clangd` file can be found in the root project directory:

```yaml
CompileFlags:
  Add: [-std=c99, -Wall, -Wextra, -xc]

Index:
  Background: Build
```

Add the following to the compiler flags:

```
-I/path/to/project/directory/
```
