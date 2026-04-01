#!/bin/sh
# FreeBSD aarch64 integration test — native HVF on Apple Silicon
# Downloads FreeBSD 15.0 aarch64 cloud image, boots with QEMU + HVF,
# injects SSH key via cloud-init, provisions jails, validates.
#
# Usage: sh scripts/freebsd-test.sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "${SCRIPT_DIR}/.." && pwd)"
CACHE_DIR="${REPO}/.cache/freebsd-test"

IMAGE_BASE="FreeBSD-15.0-RELEASE-arm64-aarch64-BASIC-CLOUDINIT-ufs.qcow2"
IMAGE_URL="https://download.freebsd.org/releases/VM-IMAGES/15.0-RELEASE/aarch64/Latest/${IMAGE_BASE}.xz"

SSH_PORT=2222
SSH_KEY="${CACHE_DIR}/test_key"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5 -o LogLevel=ERROR -i ${SSH_KEY}"
VM_RAM=2048
VM_CPUS=2
TIMEOUT_BOOT=180

# Colors (only if terminal)
if [ -t 1 ]; then
  GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'; NC='\033[0m'
else
  GREEN=''; RED=''; YELLOW=''; NC=''
fi

info()  { printf "${GREEN}[+]${NC} %s\n" "$1"; }
warn()  { printf "${YELLOW}[!]${NC} %s\n" "$1"; }
error() { printf "${RED}[-]${NC} %s\n" "$1"; }
die()   { error "$1"; cleanup; exit 1; }

QEMU_PID=""
cleanup() {
  if [ -n "${QEMU_PID}" ]; then
    info "Shutting down QEMU (pid ${QEMU_PID})..."
    kill "${QEMU_PID}" 2>/dev/null || true
    wait "${QEMU_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

# Run command on VM as root
vm_ssh() {
  ssh ${SSH_OPTS} -p "${SSH_PORT}" root@127.0.0.1 "$@"
}

# ── Check dependencies ───────────────────────────────────
check_deps() {
  for cmd in qemu-system-aarch64 qemu-img ssh ssh-keygen xz; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
      die "Required command not found: ${cmd}"
    fi
  done

  if ! qemu-system-aarch64 -accel help 2>&1 | grep -q hvf; then
    die "HVF acceleration not available — this script requires Apple Silicon"
  fi

  OVMF=""
  for fw in \
    /opt/homebrew/share/qemu/edk2-aarch64-code.fd \
    /usr/local/share/qemu/edk2-aarch64-code.fd; do
    if [ -f "${fw}" ]; then
      OVMF="${fw}"
      break
    fi
  done
  [ -n "${OVMF}" ] || die "aarch64 UEFI firmware not found. Install: brew install qemu"

  if command -v rsync >/dev/null 2>&1; then
    SYNC_CMD="rsync"
  else
    SYNC_CMD="scp"
    warn "rsync not found, falling back to scp"
  fi

  info "HVF: yes | Firmware: ${OVMF}"
}

# ── Download and prepare image ───────────────────────────
prepare_image() {
  mkdir -p "${CACHE_DIR}"

  if [ ! -f "${CACHE_DIR}/${IMAGE_BASE}" ]; then
    if [ ! -f "${CACHE_DIR}/${IMAGE_BASE}.xz" ]; then
      info "Downloading FreeBSD 15.0 aarch64 cloud image..."
      curl -L --progress-bar -o "${CACHE_DIR}/${IMAGE_BASE}.xz" "${IMAGE_URL}" \
        || die "Failed to download image"
    fi
    info "Decompressing image..."
    xz -dk "${CACHE_DIR}/${IMAGE_BASE}.xz" \
      || die "Failed to decompress image"
  else
    info "Using cached base image"
  fi

  info "Creating test overlay (20G)..."
  rm -f "${CACHE_DIR}/test-overlay.qcow2"
  qemu-img create -f qcow2 -b "${CACHE_DIR}/${IMAGE_BASE}" \
    -F qcow2 "${CACHE_DIR}/test-overlay.qcow2" \
    || die "Failed to create overlay"
  qemu-img resize "${CACHE_DIR}/test-overlay.qcow2" 20G \
    || die "Failed to resize overlay"

  # Generate ephemeral SSH key for this test run
  info "Generating ephemeral SSH key..."
  rm -f "${SSH_KEY}" "${SSH_KEY}.pub"
  ssh-keygen -t ed25519 -f "${SSH_KEY}" -N "" -q \
    || die "Failed to generate SSH key"

  # Create cloud-init seed ISO to inject SSH key + enable root
  info "Creating cloud-init seed..."
  CIDIR="${CACHE_DIR}/cidata"
  mkdir -p "${CIDIR}"
  PUBKEY="$(cat "${SSH_KEY}.pub")"

  cat > "${CIDIR}/meta-data" <<EOF
instance-id: aurelia-bloom-test-$(date +%s)
local-hostname: freebsd
EOF

  cat > "${CIDIR}/user-data" <<EOF
#cloud-config
ssh_pwauth: true
disable_root: false
chpasswd:
  expire: false
  users:
    - name: root
      password: vagrant
      type: text
    - name: freebsd
      password: freebsd
      type: text
ssh_authorized_keys:
  - ${PUBKEY}
users:
  - default
  - name: root
    lock_passwd: false
    ssh_authorized_keys:
      - ${PUBKEY}
  - name: freebsd
    ssh_authorized_keys:
      - ${PUBKEY}
    sudo: ALL=(ALL) NOPASSWD:ALL
    groups: wheel
runcmd:
  - sed -i '' 's/^#*PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config
  - service sshd restart
EOF

  rm -f "${CACHE_DIR}/seed.iso"
  if command -v mkisofs >/dev/null 2>&1; then
    mkisofs -quiet -output "${CACHE_DIR}/seed.iso" -volid cidata -joliet -rock "${CIDIR}/"
  elif command -v hdiutil >/dev/null 2>&1; then
    hdiutil makehybrid -o "${CACHE_DIR}/seed.iso" "${CIDIR}/" -iso -joliet -default-volume-name cidata -quiet 2>/dev/null
  else
    die "Need mkisofs or hdiutil to create cloud-init seed ISO"
  fi
  # hdiutil appends .iso automatically sometimes
  if [ ! -f "${CACHE_DIR}/seed.iso" ] && [ -f "${CACHE_DIR}/seed.iso.iso" ]; then
    mv "${CACHE_DIR}/seed.iso.iso" "${CACHE_DIR}/seed.iso"
  fi
}

# ── Boot VM ──────────────────────────────────────────────
boot_vm() {
  info "Booting FreeBSD aarch64 VM (HVF, port ${SSH_PORT} -> 22)..."

  qemu-system-aarch64 \
    -accel hvf \
    -cpu host \
    -M virt,highmem=on \
    -m "${VM_RAM}" \
    -smp "${VM_CPUS}" \
    -drive if=pflash,format=raw,readonly=on,file="${OVMF}" \
    -drive file="${CACHE_DIR}/test-overlay.qcow2",if=virtio,format=qcow2 \
    -drive file="${CACHE_DIR}/seed.iso",if=virtio,media=cdrom \
    -netdev user,id=net0,hostfwd=tcp::${SSH_PORT}-:22 \
    -device virtio-net-pci,netdev=net0 \
    -nographic \
    -serial mon:stdio \
    </dev/null >"${CACHE_DIR}/qemu.log" 2>&1 &
  QEMU_PID=$!

  info "QEMU started (pid ${QEMU_PID}), waiting for SSH..."
  # cloud-init restarts sshd, so we wait for it to stabilize
  info "Waiting 90s for boot + cloud-init..."
  sleep 90
  elapsed=90
  while [ "${elapsed}" -lt "${TIMEOUT_BOOT}" ]; do
    # Try key-based root first, then freebsd with sudo
    if vm_ssh "echo ok" >/dev/null 2>&1; then
      info "SSH is up after ${elapsed}s (root)"
      return 0
    fi
    if ssh ${SSH_OPTS} -p "${SSH_PORT}" freebsd@127.0.0.1 "echo ok" >/dev/null 2>&1; then
      info "SSH is up after ${elapsed}s (freebsd user — promoting to root)"
      # Enable root SSH via sudo
      ssh ${SSH_OPTS} -p "${SSH_PORT}" freebsd@127.0.0.1 \
        "sudo sed -i '' 's/^#*PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config && sudo service sshd restart" 2>/dev/null
      sleep 3
      # Copy key to root
      ssh ${SSH_OPTS} -p "${SSH_PORT}" freebsd@127.0.0.1 \
        "sudo mkdir -p /root/.ssh && sudo cp /home/freebsd/.ssh/authorized_keys /root/.ssh/ && sudo chmod 700 /root/.ssh && sudo chmod 600 /root/.ssh/authorized_keys" 2>/dev/null
      if vm_ssh "echo ok" >/dev/null 2>&1; then
        info "Root SSH enabled"
        return 0
      fi
    fi
    sleep 5
    elapsed=$((elapsed + 5))
    printf "."
  done
  echo ""
  error "SSH did not come up within ${TIMEOUT_BOOT}s"
  error "Last 40 lines of qemu.log:"
  tail -40 "${CACHE_DIR}/qemu.log" 2>/dev/null
  die "Boot failed"
}

# ── Configure VM for provisioning ────────────────────────
configure_vm() {
  info "Configuring VM for provisioning..."

  # Recover GPT backup header after disk resize and grow the filesystem
  info "Growing filesystem to fill 20G disk..."
  vm_ssh "gpart recover vtbd0 2>/dev/null; gpart resize -i 3 vtbd0 2>/dev/null; growfs -y /dev/vtbd0p3 2>/dev/null || true"
  vm_ssh "df -h /"

  vm_ssh "env ASSUME_ALWAYS_YES=yes pkg bootstrap -f && pkg update -f" \
    || die "Failed to bootstrap pkg"

  info "VM configured"
}

# ── Sync repo into VM ───────────────────────────────────
sync_repo() {
  info "Syncing repository into VM..."
  # Install rsync on VM if not present, otherwise fall back to scp
  if [ "${SYNC_CMD}" = "rsync" ]; then
    vm_ssh "command -v rsync || env ASSUME_ALWAYS_YES=yes pkg install -y rsync" 2>/dev/null
    rsync -az --delete \
      -e "ssh ${SSH_OPTS} -p ${SSH_PORT}" \
      --exclude '.cache' --exclude '.vagrant' --exclude '.git' \
      --exclude 'packer/output-*' --exclude 'packer/iso' --exclude '*.box' \
      "${REPO}/" root@127.0.0.1:/root/aurelia-bloom/ \
    || {
      warn "rsync failed, falling back to scp"
      SYNC_CMD="scp"
    }
  fi
  if [ "${SYNC_CMD}" = "scp" ]; then
    vm_ssh "mkdir -p /root/aurelia-bloom"
    scp -i "${SSH_KEY}" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
      -o LogLevel=ERROR -P "${SSH_PORT}" -r \
      "${REPO}/provisioning" "${REPO}/lab" "${REPO}/tests" \
      root@127.0.0.1:/root/aurelia-bloom/
  fi
}

# ── Run provisioning ────────────────────────────────────
provision() {
  info "Running provisioning scripts..."
  RDIR="/root/aurelia-bloom"

  # Install base box packages that base.sh normally provides
  info "  Installing base box prerequisites (busybox, dropbear)..."
  vm_ssh "env ASSUME_ALWAYS_YES=yes pkg install -y busybox dropbear || env ASSUME_ALWAYS_YES=yes pkg install -y busybox-freebsd dropbear" \
    || die "Failed to install base prerequisites"

  for script in common.sh jails.sh jail-attacker.sh jail-targets.sh jail-services.sh; do
    info "  Running ${script}..."
    vm_ssh "cd ${RDIR} && ASSUME_ALWAYS_YES=yes /bin/sh provisioning/${script}" \
      || die "Provisioning failed at ${script}"
  done

  info "Provisioning complete"
}

# ── Run in-VM tests ──────────────────────────────────────
run_tests() {
  info "Running in-VM jail validation tests..."

  vm_ssh "/bin/sh /root/aurelia-bloom/tests/test_jails.sh"
  TEST_RC=$?

  if [ "${TEST_RC}" -eq 0 ]; then
    info "All in-VM tests passed!"
  else
    error "In-VM tests failed (${TEST_RC} failures)"
  fi
  return "${TEST_RC}"
}

# ── Main ─────────────────────────────────────────────────
main() {
  echo ""
  info "=============================================="
  info "  FreeBSD aarch64 Integration Test (HVF)"
  info "=============================================="
  echo ""

  check_deps
  prepare_image
  boot_vm
  configure_vm
  sync_repo
  provision
  run_tests
  RC=$?

  echo ""
  if [ "${RC}" -eq 0 ]; then
    info "=== ALL TESTS PASSED ==="
  else
    error "=== TESTS FAILED ==="
  fi
  exit "${RC}"
}

main "$@"
