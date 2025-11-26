#!/bin/bash
set -e
set -u

CC="gcc"
CXX="g++"
AR="ar"
AS="as"
LD="ld"
RANLIB="ranlib"

echo "[*] Starting cross-compiler build process"
echo "[*] ARCH environment variable: ${ARCH:-not set}"

# Set target for x86_64 architecture
TARGET=x86_64-elf
ARCH_DIR=x86_64
echo "[*] Building for x86_64 architecture (ARCH=${ARCH:-pc/x86_64})"

# Configuration variables
PREFIX=$(pwd)/toolchain/$ARCH_DIR
echo "[*] Installing toolchain to: $PREFIX"

CACHE_DIR=$(pwd)/.cache
BUILD_DIR=$(pwd)/.cache/toolchain-build-$TARGET
JOBS=$(nproc)
BINUTILS_VERSION="2.44"  # Updated to 2.44
GCC_VERSION="14.2.0"     # Updated to 14.2.0

echo "[*] Configuration:"
echo "    - Target architecture: $TARGET"
echo "    - Installation directory: $PREFIX"
echo "    - Download cache: $CACHE_DIR"
echo "    - Build directory: $BUILD_DIR"
echo "    - Parallel jobs: $JOBS"
echo "    - Binutils version: $BINUTILS_VERSION"
echo "    - GCC version: $GCC_VERSION"

# Create directories
echo "[*] Creating necessary directories"
mkdir -p $PREFIX $BUILD_DIR $CACHE_DIR
echo "[+] Directories created successfully"

# Download sources
echo "[*] Moving to cache directory"
cd $CACHE_DIR
echo "[*] Starting download process"

# Download Binutils and signature
if [ ! -f "binutils-$BINUTILS_VERSION.tar.gz" ]; then
    echo "[*] Downloading binutils-$BINUTILS_VERSION..."
    wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz || { echo "[-] Failed to download binutils"; exit 1; }
    wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz.sig || { echo "[-] Failed to download binutils signature"; exit 1; }
    echo "[+] Binutils download completed"
else
    echo "[+] Binutils already downloaded, using cached version"
fi

# Download GCC and signature
if [ ! -f "gcc-$GCC_VERSION.tar.gz" ]; then
    echo "[*] Downloading gcc-$GCC_VERSION..."
    wget https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz || { echo "[-] Failed to download GCC"; exit 1; }
    wget https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz.sig || { echo "[-] Failed to download GCC signature"; exit 1; }
    echo "[+] GCC download completed"
else
    echo "[+] GCC already downloaded, using cached version"
fi

# Create a temporary GPG home directory
echo "[*] Setting up GPG for signature verification"
GNUPGHOME=$(mktemp -d)
export GNUPGHOME
trap "rm -rf '$GNUPGHOME'" EXIT
echo "[+] Temporary GPG home directory created at $GNUPGHOME"

# Verify signatures
echo "[*] Starting signature verification process"
# Import keys from keyserver
echo "[*] Importing GNU developer keys from keyserver..."
gpg --keyserver keyserver.ubuntu.com --recv-keys 3A24BC1E8FB409FA9F14371813FCEF89DD9E3C4F || echo "[-] Warning: Could not import Binutils key, will try keyring fallback"
gpg --keyserver keyserver.ubuntu.com --recv-keys B215C1633BCA0477615F1B35A5B3A004745C015A || echo "[-] Warning: Could not import GCC key, will try keyring fallback"

# Also try GNU keyring as fallback
echo "[*] Downloading GNU keyring as fallback..."
# Remove existing keyring if it exists to ensure we get the latest version
if [ ! -f "gnu-keyring.gpg" ]; then
    wget https://ftp.gnu.org/gnu/gnu-keyring.gpg || { echo "[-] Failed to download GNU keyring"; exit 1; }
fi
echo "[*] Importing GNU keyring..."
gpg --import gnu-keyring.gpg || echo "[-] Warning: Failed to import GNU keyring"

# Verify binutils
echo "[*] Verifying binutils signature..."
if ! gpg --verify binutils-$BINUTILS_VERSION.tar.gz.sig binutils-$BINUTILS_VERSION.tar.gz; then
    echo "[-] WARNING: binutils signature verification failed!"
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "[-] Aborted by user"
        exit 1
    fi
    echo "[*] Continuing despite verification failure"
else
    echo "[+] Binutils signature verified successfully"
fi

# Verify GCC
echo "[*] Verifying GCC signature..."
if ! gpg --verify gcc-$GCC_VERSION.tar.gz.sig gcc-$GCC_VERSION.tar.gz; then
    echo "[-] WARNING: GCC signature verification failed!"
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "[-] Aborted by user"
        exit 1
    fi
    echo "[*] Continuing despite verification failure"
else
    echo "[+] GCC signature verified successfully"
fi

echo "[+] Signature verification process completed"

# Go to build directory
echo "[*] Moving to build directory"
cd $BUILD_DIR
echo "[+] Now in build directory: $(pwd)"

# Copy source files to build directory if not present
echo "[*] Preparing source archives in build directory"
if [ ! -f "binutils-$BINUTILS_VERSION.tar.gz" ]; then
    echo "[*] Copying binutils source archive from cache"
    cp $CACHE_DIR/binutils-$BINUTILS_VERSION.tar.gz .
fi

if [ ! -f "gcc-$GCC_VERSION.tar.gz" ]; then
    echo "[*] Copying GCC source archive from cache"
    cp $CACHE_DIR/gcc-$GCC_VERSION.tar.gz .
fi
echo "[+] Source archives prepared"

# Build binutils
echo "[*] Starting binutils build process"
echo "[DEBUG] Using target: $TARGET for binutils"
if [ ! -d "binutils-$BINUTILS_VERSION" ]; then
    echo "[*] Extracting binutils source code"
    tar -xf binutils-$BINUTILS_VERSION.tar.gz || { echo "[-] Failed to extract binutils"; exit 1; }
    echo "[+] Binutils source code extracted"
else
    echo "[+] Binutils already extracted, using existing directory"
fi

echo "[*] Creating binutils build directory"
# Clean existing build directory if it contains files from a previous build for a different architecture
if [ -d "build-binutils" ] && [ -f "build-binutils/config.status" ]; then
    echo "[*] Cleaning existing binutils build directory to avoid architecture conflicts"
    rm -rf "build-binutils"
fi
mkdir -p build-binutils
cd build-binutils
echo "[+] Now in binutils build directory: $(pwd)"

echo "[*] Configuring binutils with target=$TARGET"
../binutils-$BINUTILS_VERSION/configure --target=$TARGET --prefix=$PREFIX --with-sysroot --disable-nls --disable-werror || { echo "[-] Binutils configuration failed"; exit 1; }
echo "[+] Binutils configured successfully"

echo "[*] Building binutils (this may take a while)"
make -j$JOBS || { echo "[-] Binutils build failed"; exit 1; }
echo "[+] Binutils built successfully"

echo "[*] Installing binutils to $PREFIX"
make install || { echo "[-] Binutils installation failed"; exit 1; }
echo "[+] Binutils installed successfully"

cd ..
echo "[+] Completed binutils build process"

# Build GCC
echo "[*] Starting GCC build process"
echo "[DEBUG] Using target: $TARGET for GCC"
if [ ! -d "gcc-$GCC_VERSION" ]; then
    echo "[*] Extracting GCC source code"
    tar -xf gcc-$GCC_VERSION.tar.gz || { echo "[-] Failed to extract GCC"; exit 1; }
    echo "[+] GCC source code extracted"
else
    echo "[+] GCC already extracted, using existing directory"
fi

# GCC requires the target bin directory to be in PATH
echo "[*] Adding toolchain bin directory to PATH"
export PATH="$PREFIX/bin:$PATH"
echo "[+] PATH updated: $PATH"

echo "[*] Creating GCC build directory"
# Clean existing build directory if it contains files from a previous build for a different architecture
if [ -d "build-gcc" ] && [ -f "build-gcc/config.status" ]; then
    echo "[*] Cleaning existing GCC build directory to avoid architecture conflicts"
    rm -rf "build-gcc"
fi
mkdir -p build-gcc
cd build-gcc
echo "[+] Now in GCC build directory: $(pwd)"

echo "[*] Configuring GCC with target=$TARGET"
../gcc-$GCC_VERSION/configure --target=$TARGET --prefix=$PREFIX --disable-nls --enable-languages=c,c++ --without-headers || { echo "[-] GCC configuration failed"; exit 1; }
echo "[+] GCC configured successfully"

echo "[*] Building GCC compiler (this may take a long time)"
make -j$JOBS all-gcc || { echo "[-] GCC compiler build failed"; exit 1; }
echo "[+] GCC compiler built successfully"

echo "[*] Building libgcc"
make -j$JOBS all-target-libgcc || { echo "[-] libgcc build failed"; exit 1; }
echo "[+] libgcc built successfully"

echo "[*] Installing GCC compiler to $PREFIX"
make install-gcc || { echo "[-] GCC compiler installation failed"; exit 1; }
echo "[+] GCC compiler installed successfully"

echo "[*] Installing libgcc to $PREFIX"
make install-target-libgcc || { echo "[-] libgcc installation failed"; exit 1; }
echo "[+] libgcc installed successfully"

echo "[+] Cross-compiler has been built successfully!"
echo "[+] Target: $TARGET"
echo "[+] Installation directory: $PREFIX"
echo ""
echo "[*] To use the cross-compiler, add the following to your PATH:"
echo "    export PATH=\"$PREFIX/bin:\$PATH\""
echo ""
echo "[*] You can now compile your kernel with:"
echo "    $TARGET-gcc -c kernel.c -o kernel.o -ffreestanding -g -Wall -Wextra"
echo "    $TARGET-ld -o kernel.bin kernel.o -T linker.ld"

function build_binutils {
    echo "[*] Building binutils"
    echo "[DEBUG] Using target: $TARGET for binutils"
    cd "${BUILD_DIR}/binutils-${BINUTILS_VERSION}"
    mkdir -p build && cd build
    ../configure --target=$TARGET \
                 --prefix=$PREFIX \
                 --with-sysroot \
                 --disable-nls \
                 --disable-werror
    make -j$JOBS
    make install
}

function build_gcc {
    echo "[*] Building GCC"
    echo "[DEBUG] Using target: $TARGET for GCC"
    cd "${BUILD_DIR}/gcc-${GCC_VERSION}"
    mkdir -p build && cd build
    ../configure --target=$TARGET \
                 --prefix=$PREFIX \
                 --disable-nls \
                 --enable-languages=c,c++ \
                 --without-headers
    make -j$JOBS all-gcc
    make -j$JOBS all-target-libgcc
    make install-gcc
    make install-target-libgcc
}
