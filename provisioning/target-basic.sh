#!/bin/sh
set -e

echo "=== Target Basic Setup (Protections Disabled) ==="

# Disable ASLR
sysctl hardening.pax.aslr.status=0 2>/dev/null || \
  sysctl kern.elf64.aslr.enable=0 2>/dev/null || true

# Persist across reboots
cat >> /etc/sysctl.conf <<'EOF'
hardening.pax.aslr.status=0
EOF

# Convenience wrapper: compile without protections
cat > /usr/local/bin/gcc-noprotect <<'EOF'
#!/bin/sh
exec cc -fno-stack-protector -no-pie -g "$@"
EOF
chmod +x /usr/local/bin/gcc-noprotect

echo "=== Target basic setup complete ==="
echo "  ASLR: disabled"
echo "  Use gcc-noprotect to compile without protections"
