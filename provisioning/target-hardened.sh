#!/bin/sh
set -e

echo "=== Target Hardened Setup (Full Protections) ==="

# Ensure ASLR is on
sysctl hardening.pax.aslr.status=1 2>/dev/null || \
  sysctl kern.elf64.aslr.enable=1 2>/dev/null || true

# Harden sysctl
cat >> /etc/sysctl.conf <<'EOF'
hardening.pax.aslr.status=1
kern.securelevel=1
net.inet.ip.forwarding=0
net.inet.icmp.rediraccept=0
net.inet.tcp.blackhole=2
net.inet.udp.blackhole=1
EOF

# Convenience wrapper: compile with full protections
cat > /usr/local/bin/gcc-hardened <<'EOF'
#!/bin/sh
exec cc -fstack-protector-all -fPIE -pie -g "$@"
EOF
chmod +x /usr/local/bin/gcc-hardened

# pf firewall
cat > /etc/pf.conf <<'EOFPF'
set block-policy drop
set loginterface egress

# Loopback
pass on lo0

# Allow SSH from attacker
pass in proto tcp from 192.168.56.10 to port 22

# Allow established connections out
pass out keep state

# Block everything else inbound
block in log on egress
EOFPF

pfctl -f /etc/pf.conf 2>/dev/null || true
sysrc pf_enable="YES" 2>/dev/null || true

echo "=== Target hardened setup complete ==="
echo "  ASLR: enabled"
echo "  Securelevel: 1 (after reboot)"
echo "  pf firewall: active"
