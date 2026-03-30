#!/bin/sh
# Provisioning and jail configuration validation tests
# Run from repo root: sh tests/test_provisioning.sh

PASS=0
FAIL=0
TOTAL=0

pass() {
  PASS=$((PASS + 1))
  TOTAL=$((TOTAL + 1))
  printf "  PASS: %s\n" "$1"
}

fail() {
  FAIL=$((FAIL + 1))
  TOTAL=$((TOTAL + 1))
  printf "  FAIL: %s\n" "$1"
}

check() {
  TOTAL=$((TOTAL + 1))
  if eval "$2"; then
    PASS=$((PASS + 1))
    printf "  PASS: %s\n" "$1"
  else
    FAIL=$((FAIL + 1))
    printf "  FAIL: %s\n" "$1"
  fi
}

# Find repo root (allow running from any directory)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=== Provisioning Validation Tests ==="
echo "Repo: ${REPO}"
echo ""

# ── 1. Shell script syntax (sh -n) ────────────────────────
echo "--- Shell Script Syntax ---"

for script in \
  provisioning/common.sh \
  provisioning/jails.sh \
  provisioning/jail-attacker.sh \
  provisioning/jail-targets.sh \
  provisioning/jail-services.sh \
  packer/scripts/base.sh; do

  if [ -f "${REPO}/${script}" ]; then
    check "${script} syntax valid" "sh -n '${REPO}/${script}' 2>/dev/null"
  else
    fail "${script} not found"
  fi
done

# ── 2. Shebang lines ──────────────────────────────────────
echo ""
echo "--- Shebang Lines (must be /bin/sh) ---"

for script in provisioning/*.sh packer/scripts/*.sh; do
  if [ -f "${REPO}/${script}" ]; then
    check "${script} has /bin/sh shebang" \
      "head -1 '${REPO}/${script}' | grep -q '^#!/bin/sh'"
  fi
done

# ── 3. No bash dependency in provisioning ─────────────────
echo ""
echo "--- No Bash Dependencies ---"

for script in provisioning/*.sh; do
  if [ -f "${REPO}/${script}" ]; then
    check "${script} has no bash-isms (arrays)" \
      "! grep -qE '\\[\\[|\\$\\{[a-zA-Z]+\\[|declare |local -[aA]' '${REPO}/${script}'"
  fi
done

# ── 4. Required files exist ───────────────────────────────
echo ""
echo "--- Required Files ---"

for f in \
  Vagrantfile \
  Vagrantfile.multi-vm \
  CLAUDE.md \
  packer/hardenedbsd.pkr.hcl \
  packer/http/installerconfig \
  packer/scripts/base.sh \
  provisioning/common.sh \
  provisioning/jails.sh \
  provisioning/jail-attacker.sh \
  provisioning/jail-targets.sh \
  provisioning/jail-services.sh \
  lab/Makefile \
  chat/Makefile; do

  check "${f} exists" "[ -f '${REPO}/${f}' ]"
done

# ── 5. No redundant packages ─────────────────────────────
echo ""
echo "--- Package Redundancy Checks ---"

check "common.sh does not install lldb" \
  "! grep -q 'lldb' '${REPO}/provisioning/common.sh'"

check "common.sh does not install vim" \
  "! grep -q 'vim' '${REPO}/provisioning/common.sh'"

check "common.sh does not install bash" \
  "! grep -q 'bash' '${REPO}/provisioning/common.sh'"

check "common.sh does not install tmux" \
  "! grep -q 'tmux' '${REPO}/provisioning/common.sh'"

check "base.sh does not install wget" \
  "! grep -q 'wget' '${REPO}/packer/scripts/base.sh'"

check "attacker.sh has no redundant pip pwntools" \
  "! grep -q 'pip install.*pwntools' '${REPO}/provisioning/jail-attacker.sh'"

# ── 6. Packer config checks ──────────────────────────────
echo ""
echo "--- Packer Configuration ---"

check "disk_size <= 8192" \
  "grep -q 'default = 6144' '${REPO}/packer/hardenedbsd.pkr.hcl'"

check "base.sh installs busybox" \
  "grep -q 'busybox' '${REPO}/packer/scripts/base.sh'"

check "base.sh installs dropbear" \
  "grep -q 'dropbear' '${REPO}/packer/scripts/base.sh'"

check "base.sh cleans man pages" \
  "grep -q '/usr/share/man' '${REPO}/packer/scripts/base.sh'"

check "base.sh zeros free space" \
  "grep -q 'dd if=/dev/zero' '${REPO}/packer/scripts/base.sh'"

# ── 7. Vagrantfile checks ────────────────────────────────
echo ""
echo "--- Vagrantfile (Single VM) ---"

check "Vagrantfile references single VM (no config.vm.define)" \
  "! grep -q 'config.vm.define' '${REPO}/Vagrantfile'"

check "Vagrantfile provisions jails.sh" \
  "grep -q 'jails.sh' '${REPO}/Vagrantfile'"

check "Vagrantfile sets promiscuous mode" \
  "grep -q 'nicpromisc' '${REPO}/Vagrantfile'"

# ── 8. Jail config checks ────────────────────────────────
echo ""
echo "--- Jail Configuration ---"

check "jails.sh creates bridge0" \
  "grep -q 'bridge0' '${REPO}/provisioning/jails.sh'"

check "jails.sh defines all 4 jails" \
  "grep -c 'epair.*inet 192.168.56' '${REPO}/provisioning/jails.sh' | grep -q '[4-9]'"

check "jails.sh installs BusyBox into jails" \
  "grep -q 'busybox' '${REPO}/provisioning/jails.sh'"

check "jails.sh generates dropbear host keys" \
  "grep -q 'dropbearkey' '${REPO}/provisioning/jails.sh'"

check "jails.sh creates labuser in jails" \
  "grep -q 'labuser' '${REPO}/provisioning/jails.sh'"

# ── 9. Network address consistency ───────────────────────
echo ""
echo "--- Network Address Consistency ---"

check "attacker is .10" \
  "grep -q '192.168.56.10' '${REPO}/provisioning/jails.sh'"

check "basic is .20" \
  "grep -q '192.168.56.20' '${REPO}/provisioning/jails.sh'"

check "hardened is .30" \
  "grep -q '192.168.56.30' '${REPO}/provisioning/jails.sh'"

check "service is .40" \
  "grep -q '192.168.56.40' '${REPO}/provisioning/jails.sh'"

# ── 10. Lab Makefile checks ──────────────────────────────
echo ""
echo "--- Lab Makefile ---"

check "lab Makefile strips binaries" \
  "grep -q 'strip\|STRIP' '${REPO}/lab/Makefile'"

check "lab Makefile has no -g flag" \
  "! grep -qE 'CFLAGS.*-g ' '${REPO}/lab/Makefile'"

# ── 11. .gitignore checks ────────────────────────────────
echo ""
echo "--- Repository Hygiene ---"

check ".gitignore exists" \
  "[ -f '${REPO}/.gitignore' ]"

check ".gitignore excludes .vagrant/" \
  "grep -q '.vagrant' '${REPO}/.gitignore'"

check ".gitignore excludes chat obj files" \
  "grep -q 'chat/obj' '${REPO}/.gitignore'"

check "no .o files in chat/obj/" \
  "! ls '${REPO}/chat/obj/'*.o 2>/dev/null | grep -q '.'"

# ── Summary ──────────────────────────────────────────────
echo ""
echo "================================="
printf "Results: %d/%d passed" "${PASS}" "${TOTAL}"
if [ "${FAIL}" -gt 0 ]; then
  printf " (%d FAILED)" "${FAIL}"
fi
echo ""
echo "================================="

exit "${FAIL}"
