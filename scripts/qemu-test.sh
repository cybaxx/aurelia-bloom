#!/bin/sh
# QEMU-based integration test for jail provisioning
# Downloads FreeBSD 15.0 cloud image, boots with QEMU, provisions jails, validates
# Usage: sh scripts/qemu-test.sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "${SCRIPT_DIR}/.." && pwd)"
CACHE_DIR="${REPO}/.cache/qemu-test"
IMAGE_BASE="FreeBSD-15.0-RELEASE-amd64-BASIC-CLOUDINIT.qcow2"
IMAGE_URL="https://download.freebsd.org/releases/VM-IMAGES/15.0-RELEASE/amd64/Latest/${IMAGE_BASE}.xz"
SSH_PORT=2222
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5 -o LogLevel=ERROR"
VM_RAM=2048
VM_CPUS=2
TIMEOUT_BOOT=180
TIMEOUT_PROVISION=600

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

# ── Detect platform and acceleration ─────────────────────
detect_accel() {
  case "$(uname -s)" in
    Darwin)
      ACCEL="hvf"
      # OVMF firmware locations on macOS
      for fw in \
        /opt/homebrew/share/qemu/edk2-x86_64-code.fd \
        /usr/local/share/qemu/edk2-x86_64-code.fd \
        /opt/homebrew/share/OVMF/OVMF_CODE.fd \
        /usr/local/share/OVMF/OVMF_CODE.fd; do
        if [ -f "${fw}" ]; then
          OVMF_CODE="${fw}"
          break
        fi
      done
      ;;
    Linux)
      if [ -e /dev/kvm ]; then
        ACCEL="kvm"
      else
        ACCEL="tcg"
        warn "KVM not available, falling back to TCG (slow)"
      fi
      for fw in \
        /usr/share/OVMF/OVMF_CODE.fd \
        /usr/share/edk2/ovmf/OVMF_CODE.fd \
        /usr/share/qemu/OVMF_CODE.fd; do
        if [ -f "${fw}" ]; then
          OVMF_CODE="${fw}"
          break
        fi
      done
      ;;
    *)
      ACCEL="tcg"
      ;;
  esac

  if [ -z "${OVMF_CODE}" ]; then
    die "OVMF/EDK2 firmware not found. Install: brew install qemu (macOS) or apt install ovmf (Linux)"
  fi
  info "Acceleration: ${ACCEL}, Firmware: ${OVMF_CODE}"
}

# ── Check dependencies ───────────────────────────────────
check_deps() {
  for cmd in qemu-system-x86_64 ssh scp xz; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
      die "Required command not found: ${cmd}"
    fi
  done
  # rsync is preferred but scp works as fallback
  if command -v rsync >/dev/null 2>&1; then
    SYNC_CMD="rsync"
  else
    SYNC_CMD="scp"
    warn "rsync not found, falling back to scp"
  fi
}

# ── Download and prepare image ───────────────────────────
prepare_image() {
  mkdir -p "${CACHE_DIR}"

  if [ ! -f "${CACHE_DIR}/${IMAGE_BASE}" ]; then
    if [ ! -f "${CACHE_DIR}/${IMAGE_BASE}.xz" ]; then
      info "Downloading FreeBSD 15.0 cloud image..."
      curl -L -o "${CACHE_DIR}/${IMAGE_BASE}.xz" "${IMAGE_URL}" \
        || die "Failed to download image"
    fi
    info "Decompressing image..."
    xz -dk "${CACHE_DIR}/${IMAGE_BASE}.xz" \
      || die "Failed to decompress image"
  else
    info "Using cached base image"
  fi

  # Create overlay so base image stays clean
  info "Creating test overlay..."
  rm -f "${CACHE_DIR}/test-overlay.qcow2"
  qemu-img create -f qcow2 -b "${CACHE_DIR}/${IMAGE_BASE}" \
    -F qcow2 "${CACHE_DIR}/test-overlay.qcow2" \
    || die "Failed to create overlay"
}

# ── Boot VM ──────────────────────────────────────────────
boot_vm() {
  info "Booting FreeBSD VM (port ${SSH_PORT} -> 22)..."

  qemu-system-x86_64 \
    -accel "${ACCEL}" \
    -m "${VM_RAM}" \
    -smp "${VM_CPUS}" \
    -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
    -drive file="${CACHE_DIR}/test-overlay.qcow2",if=virtio,format=qcow2 \
    -netdev user,id=net0,hostfwd=tcp::${SSH_PORT}-:22 \
    -device virtio-net-pci,netdev=net0 \
    -nographic \
    -serial mon:stdio \
    </dev/null >"${CACHE_DIR}/qemu.log" 2>&1 &
  QEMU_PID=$!

  info "QEMU started (pid ${QEMU_PID}), waiting for SSH..."
  elapsed=0
  while [ "${elapsed}" -lt "${TIMEOUT_BOOT}" ]; do
    if ssh ${SSH_OPTS} -p "${SSH_PORT}" root@127.0.0.1 "echo ok" >/dev/null 2>&1; then
      info "SSH is up after ${elapsed}s"
      return 0
    fi
    sleep 5
    elapsed=$((elapsed + 5))
    printf "."
  done
  echo ""
  die "SSH did not come up within ${TIMEOUT_BOOT}s (check ${CACHE_DIR}/qemu.log)"
}

# ── Sync repo into VM ───────────────────────────────────
sync_repo() {
  info "Syncing repository into VM..."
  if [ "${SYNC_CMD}" = "rsync" ]; then
    rsync -az --delete \
      -e "ssh ${SSH_OPTS} -p ${SSH_PORT}" \
      --exclude '.cache' --exclude '.vagrant' --exclude '.git' \
      --exclude 'packer/output-*' --exclude '*.box' \
      "${REPO}/" root@127.0.0.1:/root/aurelia-bloom/
  else
    # scp fallback — less efficient but works
    ssh ${SSH_OPTS} -p "${SSH_PORT}" root@127.0.0.1 "mkdir -p /root/aurelia-bloom"
    scp ${SSH_OPTS} -P "${SSH_PORT}" -r \
      "${REPO}/provisioning" "${REPO}/lab" "${REPO}/tests" \
      root@127.0.0.1:/root/aurelia-bloom/
  fi
}

# ── Run provisioning ────────────────────────────────────
provision() {
  info "Running provisioning scripts..."
  REMOTE="ssh ${SSH_OPTS} -p ${SSH_PORT} root@127.0.0.1"
  RDIR="/root/aurelia-bloom"

  for script in common.sh jails.sh jail-attacker.sh jail-targets.sh jail-services.sh; do
    info "  Running ${script}..."
    ${REMOTE} "cd ${RDIR} && ASSUME_ALWAYS_YES=yes /bin/sh provisioning/${script}" \
      || die "Provisioning failed at ${script}"
  done

  info "Provisioning complete"
}

# ── Run in-VM tests ──────────────────────────────────────
run_tests() {
  info "Running in-VM jail validation tests..."
  REMOTE="ssh ${SSH_OPTS} -p ${SSH_PORT} root@127.0.0.1"

  ${REMOTE} "/bin/sh /root/aurelia-bloom/tests/test_jails.sh"
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
  info "QEMU Integration Test for Aurelia-Bloom Jail Architecture"
  echo ""

  check_deps
  detect_accel
  prepare_image
  boot_vm
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
