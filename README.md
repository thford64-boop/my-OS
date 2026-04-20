ISO might not work *i don't recommend booting it on a pc use a VM if possible*

go to the biuld folder and type "./bigdos" in WSL or any linux distro before that make the following a .sh file in your main folder:

#!/usr/bin/env bash
# =============================================================================
#  BIG-DOS Build Pipeline
#  Run from WSL (Ubuntu) or any Debian/Ubuntu Linux machine.
#  Output: bigdos.iso  (bootable x86_64 live image)
#
#  Usage:
#    chmod +x build.sh
#    ./build.sh          # full build
#    ./build.sh --clean  # remove build artifacts first
#    ./build.sh --run    # build then launch in QEMU
# =============================================================================

set -euo pipefail

# ── colours ────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

info()    { echo -e "${CYAN}[*]${NC} $*"; }
success() { echo -e "${GREEN}[✓]${NC} $*"; }
warn()    { echo -e "${YELLOW}[!]${NC} $*"; }
die()     { echo -e "${RED}[✗]${NC} $*" >&2; exit 1; }
banner()  { echo -e "\n${BOLD}${CYAN}══ $* ══${NC}\n"; }

# ── configuration ──────────────────────────────────────────────────────────
BUILD_DIR="$(pwd)/build"
ROOTFS="$BUILD_DIR/rootfs"
ISO_DIR="$BUILD_DIR/iso"
SRC_DIR="$(pwd)/src"
OUTPUT_ISO="$(pwd)/bigdos.iso"

# Tiny Linux kernel download (Alpine's mini kernel works great for testing)
KERNEL_URL="https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/x86_64/netboot/vmlinuz-lts"
VMLINUZ="$BUILD_DIR/vmlinuz"

# ── parse arguments ─────────────────────────────────────────────────────────
CLEAN=0; RUN=0
for arg in "$@"; do
    [[ "$arg" == "--clean" ]] && CLEAN=1
    [[ "$arg" == "--run"   ]] && RUN=1
done

# ── clean ───────────────────────────────────────────────────────────────────
if [[ $CLEAN -eq 1 ]]; then
    info "Cleaning build directory..."
    rm -rf "$BUILD_DIR" "$OUTPUT_ISO"
    success "Clean complete."
fi

# ── 0. install build dependencies ──────────────────────────────────────────
banner "Step 0: Dependencies"
DEPS=(gcc grub-pc-bin grub-common xorriso cpio gzip wget)
MISSING=()
for dep in "${DEPS[@]}"; do
    command -v "$dep" &>/dev/null || MISSING+=("$dep")
done
if [[ ${#MISSING[@]} -gt 0 ]]; then
    warn "Missing tools: ${MISSING[*]}"
    info "Installing via apt..."
    sudo apt-get update -qq
    sudo apt-get install -y -qq "${MISSING[@]}" \
        || die "Failed to install dependencies."
fi
success "All dependencies present."

# ── 1. compile BIG-DOS ──────────────────────────────────────────────────────
banner "Step 1: Compile BIG-DOS"
mkdir -p "$BUILD_DIR"

# Check for Raylib (optional — build without GUI if not present)
if pkg-config --exists raylib 2>/dev/null; then
    RAYLIB_FLAGS="$(pkg-config --cflags --libs raylib)"
    GUI_FLAG=""
    info "Raylib found — building with GUI support."
else
    RAYLIB_FLAGS="-lm"
    GUI_FLAG="-DBIGDOS_NO_GUI"
    warn "Raylib not found — building without GUI (text mode only)."
    warn "Install with: sudo apt install libraylib-dev"
fi

gcc -O2 -Wall -Wextra \
    $GUI_FLAG \
    "$SRC_DIR/main.c" \
    "$SRC_DIR/commands.c" \
    "$SRC_DIR/gui.c" \
    "$SRC_DIR/pkgmgr.c" \
    -o "$BUILD_DIR/bigdos" \
    $RAYLIB_FLAGS \
    || die "Compilation failed."

success "Compiled → $BUILD_DIR/bigdos"

# ── 2. scaffold rootfs ──────────────────────────────────────────────────────
banner "Step 2: Build rootfs"
rm -rf "$ROOTFS"

# Standard Linux directory hierarchy
for dir in \
    bin sbin usr/bin usr/sbin usr/lib usr/share \
    dev proc sys tmp var/log var/run \
    etc/bigdos home/user lib lib64 \
    mnt media opt run; do
    mkdir -p "$ROOTFS/$dir"
done

# Install our binary as /sbin/bigdos and /sbin/init
cp "$BUILD_DIR/bigdos" "$ROOTFS/sbin/bigdos"
chmod 755 "$ROOTFS/sbin/bigdos"

# Create /sbin/init — the kernel hands control here after mounting initrd
cat > "$ROOTFS/sbin/init" << 'INIT_EOF'
#!/bin/sh
# BIG-DOS init script — PID 1

# Mount essential pseudo-filesystems
mount -t proc  proc  /proc  2>/dev/null || true
mount -t sysfs sysfs /sys   2>/dev/null || true
mount -t devtmpfs devtmpfs /dev 2>/dev/null || true

# Console setup
exec < /dev/console > /dev/console 2>&1

echo ""
echo "  ██████╗ ██╗ ██████╗      ██████╗  ██████╗ ███████╗"
echo "  ██╔══██╗██║██╔════╝      ██╔══██╗██╔═══██╗██╔════╝"
echo "  ██████╔╝██║██║  ███╗     ██║  ██║██║   ██║███████╗"
echo "  ██╔══██╗██║██║   ██║     ██║  ██║██║   ██║╚════██║"
echo "  ██████╔╝██║╚██████╔╝     ██████╔╝╚██████╔╝███████║"
echo "  ╚═════╝ ╚═╝ ╚═════╝      ╚═════╝  ╚═════╝ ╚══════╝"
echo ""
echo "  BIG-DOS v1.0  |  type 'help' for commands"
echo ""

# Set up environment
export HOME=/home/user
export PATH=/sbin:/bin:/usr/bin:/usr/sbin
export TERM=linux
export PS1='bigdos> '

cd /home/user

# Drop into BIG-DOS
exec /sbin/bigdos

# Fallback: busybox shell (if bigdos crashes)
exec /bin/sh
INIT_EOF
chmod 755 "$ROOTFS/sbin/init"

# /etc/hostname, /etc/passwd
echo "bigdos" > "$ROOTFS/etc/hostname"
cat > "$ROOTFS/etc/passwd" << 'EOF'
root:x:0:0:root:/root:/bin/sh
user:x:1000:1000:BIG-DOS User:/home/user:/sbin/bigdos
EOF

# Seed pkg databases
mkdir -p "$ROOTFS/etc/bigdos"
cat > "$ROOTFS/etc/bigdos/repo.db" << 'EOF'
nano|8.0|https://example.com/pkgs/nano.tar.gz|220|Tiny terminal text editor
htop|3.3.0|https://example.com/pkgs/htop.tar.gz|180|Interactive process viewer
curl|8.5.0|https://example.com/pkgs/curl.tar.gz|400|Transfer data with URLs
vim|9.1|https://example.com/pkgs/vim.tar.gz|1800|Vi IMproved text editor
git|2.44.0|https://example.com/pkgs/git.tar.gz|2600|Distributed version control
EOF
touch "$ROOTFS/etc/bigdos/installed.db"

# /dev nodes (cpio from /dev is tricky; pre-create the essential ones)
# These will be replaced by devtmpfs at runtime, but some initrd environments
# need them for the pivot_root phase.
mknod -m 600 "$ROOTFS/dev/console" c 5 1  2>/dev/null || true
mknod -m 666 "$ROOTFS/dev/null"    c 1 3  2>/dev/null || true
mknod -m 666 "$ROOTFS/dev/tty"     c 5 0  2>/dev/null || true
mknod -m 666 "$ROOTFS/dev/zero"    c 1 5  2>/dev/null || true
mknod -m 444 "$ROOTFS/dev/random"  c 1 8  2>/dev/null || true

success "rootfs scaffolded at $ROOTFS"

# ── 3. download kernel ──────────────────────────────────────────────────────
banner "Step 3: Kernel"
if [[ ! -f "$VMLINUZ" ]]; then
    info "Downloading vmlinuz from Alpine netboot..."
    wget -q --show-progress -O "$VMLINUZ" "$KERNEL_URL" \
        || die "Failed to download kernel. Check your internet connection."
    success "Kernel downloaded → $VMLINUZ"
else
    info "Kernel already cached at $VMLINUZ — skipping download."
fi

# ── 4. create initrd via cpio ───────────────────────────────────────────────
banner "Step 4: initrd (cpio)"
INITRD="$BUILD_DIR/initrd.gz"

info "Packing rootfs into cpio archive..."
(
    cd "$ROOTFS"
    find . | cpio -oH newc 2>/dev/null | gzip -9 > "$INITRD"
)
INITRD_SIZE=$(du -sh "$INITRD" | cut -f1)
success "initrd created → $INITRD  ($INITRD_SIZE)"

# ── 5. assemble ISO with GRUB ───────────────────────────────────────────────
banner "Step 5: ISO assembly"
rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot/grub"

# Copy kernel and initrd into ISO tree
cp "$VMLINUZ" "$ISO_DIR/boot/vmlinuz"
cp "$INITRD"  "$ISO_DIR/boot/initrd.gz"

# GRUB configuration
cat > "$ISO_DIR/boot/grub/grub.cfg" << 'GRUB_EOF'
set default=0
set timeout=3

insmod all_video

menuentry "BIG-DOS 1.0" {
    linux   /boot/vmlinuz  quiet console=ttyS0 console=tty0 init=/sbin/init
    initrd  /boot/initrd.gz
}

menuentry "BIG-DOS 1.0 (verbose)" {
    linux   /boot/vmlinuz  console=ttyS0 console=tty0 init=/sbin/init
    initrd  /boot/initrd.gz
}

menuentry "BIG-DOS GUI mode" {
    linux   /boot/vmlinuz  quiet console=tty0 init=/sbin/init BIGDOS_GUI=1
    initrd  /boot/initrd.gz
}
GRUB_EOF

# Build the ISO
info "Running grub-mkrescue..."
grub-mkrescue \
    --output="$OUTPUT_ISO" \
    "$ISO_DIR" \
    -- -volid "BIGDOS_1.0" \
    2>&1 | grep -v "^$" || true

if [[ -f "$OUTPUT_ISO" ]]; then
    ISO_SIZE=$(du -sh "$OUTPUT_ISO" | cut -f1)
    success "ISO created → $OUTPUT_ISO  ($ISO_SIZE)"
else
    die "grub-mkrescue did not produce an ISO."
fi

# ── 6. summary ──────────────────────────────────────────────────────────────
banner "Build Complete"
echo -e "  ${GREEN}bigdos.iso${NC} is ready."
echo ""
echo "  Boot in QEMU (text mode):"
echo -e "  ${YELLOW}qemu-system-x86_64 -m 512M -cdrom bigdos.iso -boot d -nographic -serial stdio${NC}"
echo ""
echo "  Boot in QEMU (with VGA window):"
echo -e "  ${YELLOW}qemu-system-x86_64 -m 512M -cdrom bigdos.iso -boot d -vga std${NC}"
echo ""
echo "  Boot in QEMU (with GUI + KVM acceleration):"
echo -e "  ${YELLOW}qemu-system-x86_64 -m 1G -cdrom bigdos.iso -boot d -vga std -enable-kvm -cpu host${NC}"
echo ""

# ── 7. auto-launch QEMU if --run ────────────────────────────────────────────
if [[ $RUN -eq 1 ]]; then
    banner "Launching QEMU"
    if command -v qemu-system-x86_64 &>/dev/null; then
        exec qemu-system-x86_64 \
            -m 512M \
            -cdrom "$OUTPUT_ISO" \
            -boot d \
            -vga std \
            -serial stdio \
            -name "BIG-DOS 1.0"
    else
        die "qemu-system-x86_64 not found. Install with: sudo apt install qemu-system-x86"
    fi
fi
