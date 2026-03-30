#!/bin/sh
set -e

echo "=== Target Jails Provisioning ==="

# ── Build lab binaries on host, install into jails ─────────

if [ -d /vagrant/lab ] && [ -f /vagrant/lab/Makefile ]; then
  cd /vagrant/lab

  # Basic target — protections disabled
  echo "--- Building basic lab binaries ---"
  gmake clean 2>/dev/null || true
  gmake basic DESTDIR=/jails/basic/opt/lab/bin

  # Hardened target — full protections
  echo "--- Building hardened lab binaries ---"
  gmake clean 2>/dev/null || true
  gmake hardened DESTDIR=/jails/hardened/opt/lab/bin

  # Service target gets hardened binaries too
  gmake clean 2>/dev/null || true
  gmake hardened DESTDIR=/jails/service/opt/lab/bin
fi

# ── Target-basic: disable protections ──────────────────────
echo "--- Configuring basic jail (protections disabled) ---"

BASIC="/jails/basic"

# ASLR disable — applied via jail sysctl at start
# We write a startup addition
cat > "${BASIC}/etc/rc" <<'RCEOF'
#!/bin/sh
sysctl hardening.pax.aslr.status=0 2>/dev/null || \
  sysctl kern.elf64.aslr.enable=0 2>/dev/null || true
/usr/local/sbin/dropbear -R -p 22 2>/dev/null &
echo "Basic target jail started: $(hostname)"
RCEOF
chmod +x "${BASIC}/etc/rc"

# sysctl.conf for persistence within jail
cat > "${BASIC}/etc/sysctl.conf" <<'SYSEOF'
hardening.pax.aslr.status=0
SYSEOF

# Convenience wrapper
mkdir -p "${BASIC}/usr/local/bin"
cat > "${BASIC}/usr/local/bin/gcc-noprotect" <<'EOF'
#!/bin/sh
exec cc -fno-stack-protector -no-pie "$@"
EOF
chmod +x "${BASIC}/usr/local/bin/gcc-noprotect"

chown -R 1001:1001 "${BASIC}/opt/lab"
chown -R 1001:1001 "${BASIC}/home/labuser"

# ── Target-hardened: full protections ──────────────────────
echo "--- Configuring hardened jail (full protections) ---"

HARDENED="/jails/hardened"

cat > "${HARDENED}/etc/rc" <<'RCEOF'
#!/bin/sh
sysctl hardening.pax.aslr.status=1 2>/dev/null || \
  sysctl kern.elf64.aslr.enable=1 2>/dev/null || true
/usr/local/sbin/dropbear -R -p 22 2>/dev/null &
echo "Hardened target jail started: $(hostname)"
RCEOF
chmod +x "${HARDENED}/etc/rc"

cat > "${HARDENED}/etc/sysctl.conf" <<'SYSEOF'
hardening.pax.aslr.status=1
kern.securelevel=1
SYSEOF

# pf rules — applied at host level for this jail's interface
cat > "${HARDENED}/etc/pf.conf" <<'EOFPF'
set block-policy drop

pass on lo0
pass in proto tcp from 192.168.56.10 to port 22
pass out keep state
block in log all
EOFPF

# Convenience wrapper
mkdir -p "${HARDENED}/usr/local/bin"
cat > "${HARDENED}/usr/local/bin/gcc-hardened" <<'EOF'
#!/bin/sh
exec cc -fstack-protector-all -fPIE -pie "$@"
EOF
chmod +x "${HARDENED}/usr/local/bin/gcc-hardened"

chown -R 1001:1001 "${HARDENED}/opt/lab"
chown -R 1001:1001 "${HARDENED}/home/labuser"

echo "=== Target jails provisioning complete ==="
echo "  basic    (192.168.56.20): ASLR disabled, no protections"
echo "  hardened (192.168.56.30): ASLR enabled, securelevel=1, pf"
