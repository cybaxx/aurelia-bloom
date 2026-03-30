#!/bin/sh
set -e

echo "=== Common Provisioning ==="

# Update package repo
env ASSUME_ALWAYS_YES=yes pkg update -f

# Install host build tools (minimal — heavy tools go in attacker jail)
pkg install -y gcc nasm gmake binutils

# Create lab user
if ! pw usershow labuser >/dev/null 2>&1; then
  pw useradd -n labuser -m -s /bin/sh -G wheel
  echo "labuser" | pw usermod labuser -h 0
fi

# Lab directories
mkdir -p /opt/lab/src /opt/lab/bin
chown -R labuser:labuser /opt/lab

# Copy lab materials from shared repo
if [ -d /vagrant/lab ]; then
  cp -R /vagrant/lab/* /opt/lab/src/ 2>/dev/null || true
  chown -R labuser:labuser /opt/lab/src
fi

# Hosts file for easy VM addressing
cat >> /etc/hosts <<'EOF'
192.168.56.10  attacker
192.168.56.20  target-basic
192.168.56.30  target-hardened
192.168.56.40  target-service
EOF

echo "=== Common provisioning complete ==="
