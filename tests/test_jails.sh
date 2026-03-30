#!/bin/sh
# In-VM jail validation tests
# Run inside a provisioned FreeBSD/HardenedBSD VM: sh tests/test_jails.sh
set -u

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
  if eval "$2" >/dev/null 2>&1; then
    PASS=$((PASS + 1))
    printf "  PASS: %s\n" "$1"
  else
    FAIL=$((FAIL + 1))
    printf "  FAIL: %s\n" "$1"
  fi
}

echo "=== In-VM Jail Validation Tests ==="
echo ""

# ── 1. Jails are running ─────────────────────────────────
echo "--- Jail Status ---"

for jail in attacker basic hardened service; do
  check "${jail} jail is running" "jls -j ${jail} >/dev/null 2>&1"
done

check "exactly 4 jails running" "[ \$(jls | grep -c '^[[:space:]]*[0-9]') -eq 4 ]"

# ── 2. Jail IP addresses ─────────────────────────────────
echo ""
echo "--- Jail IP Addresses ---"

check "attacker has 192.168.56.10" \
  "jexec attacker ifconfig | grep -q '192.168.56.10'"

check "basic has 192.168.56.20" \
  "jexec basic ifconfig | grep -q '192.168.56.20'"

check "hardened has 192.168.56.30" \
  "jexec hardened ifconfig | grep -q '192.168.56.30'"

check "service has 192.168.56.40" \
  "jexec service ifconfig | grep -q '192.168.56.40'"

# ── 3. Inter-jail connectivity ───────────────────────────
echo ""
echo "--- Inter-Jail Connectivity ---"

for ip in 192.168.56.10 192.168.56.20 192.168.56.30 192.168.56.40; do
  check "host can ping ${ip}" "ping -c 1 -W 2 ${ip}"
done

check "attacker can ping basic" \
  "jexec attacker ping -c 1 -W 2 192.168.56.20"

check "attacker can ping hardened" \
  "jexec attacker ping -c 1 -W 2 192.168.56.30"

check "attacker can ping service" \
  "jexec attacker ping -c 1 -W 2 192.168.56.40"

# ── 4. SSH services ──────────────────────────────────────
echo ""
echo "--- SSH Services ---"

# Attacker jail runs OpenSSH
check "attacker SSH responds (port 22)" \
  "jexec attacker /bin/sh -c 'echo | nc -w 2 127.0.0.1 22' | grep -qi ssh"

# Target jails run Dropbear
for jail in basic hardened service; do
  IP=$(jexec ${jail} ifconfig 2>/dev/null | grep 'inet 192.168.56' | awk '{print $2}')
  if [ -n "${IP}" ]; then
    check "${jail} Dropbear SSH responds" \
      "echo | nc -w 2 ${IP} 22 | grep -qi ssh"
  else
    fail "${jail} Dropbear SSH responds (no IP found)"
  fi
done

# ── 5. Lab binaries ──────────────────────────────────────
echo ""
echo "--- Lab Binaries ---"

for jail in basic hardened; do
  check "${jail} has /opt/lab/bin/" \
    "jexec ${jail} ls /opt/lab/bin/"

  # Check at least one binary exists
  check "${jail} has lab binaries" \
    "jexec ${jail} ls /opt/lab/bin/ | grep -q '.'"
done

# ── 6. Service jail (nginx) ─────────────────────────────
echo ""
echo "--- Service Jail ---"

check "nginx is running in service jail" \
  "jexec service ps aux | grep -q '[n]ginx'"

check "nginx responds on port 80" \
  "echo 'GET / HTTP/1.0\r\n\r\n' | jexec service nc -w 2 127.0.0.1 80 | grep -qi http"

# ── 7. BusyBox in target jails ───────────────────────────
echo ""
echo "--- BusyBox ---"

for jail in basic hardened service; do
  check "${jail} has busybox" \
    "jexec ${jail} busybox --help"

  check "${jail} busybox ls symlink works" \
    "jexec ${jail} /bin/ls /"
done

# ── 8. labuser account ──────────────────────────────────
echo ""
echo "--- Lab User ---"

for jail in attacker basic hardened service; do
  check "${jail} has labuser" \
    "jexec ${jail} id labuser"
done

# ── 9. Bridge networking ────────────────────────────────
echo ""
echo "--- Host Networking ---"

check "bridge0 exists" "ifconfig bridge0"

check "bridge0 has 192.168.56.1" \
  "ifconfig bridge0 | grep -q '192.168.56.1'"

# ── 10. HardenedBSD-specific (skip gracefully on FreeBSD) ─
echo ""
echo "--- Security Features (optional) ---"

if sysctl hardening.pax.aslr.status >/dev/null 2>&1; then
  check "ASLR enabled on host" \
    "[ \$(sysctl -n hardening.pax.aslr.status) -ge 1 ]"

  check "basic jail has ASLR disabled" \
    "jexec basic sysctl -n hardening.pax.aslr.status 2>/dev/null | grep -q '0'"

  check "hardened jail has ASLR enabled" \
    "jexec hardened sysctl -n hardening.pax.aslr.status 2>/dev/null | grep -q '[1-2]'"
else
  printf "  SKIP: HardenedBSD PAX/ASLR sysctls not available (FreeBSD host)\n"
fi

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
