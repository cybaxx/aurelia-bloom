#!/bin/sh
set -e

echo "=== Attacker Machine Setup ==="

pkg install -y radare2 py311-pwntools binutils nasm python3 \
  py311-capstone 2>/dev/null || true

# Python exploitation helpers
python3 -m pip install --quiet pwntools 2>/dev/null || true

# Attacker workspace
mkdir -p /opt/exploits/tools /opt/exploits/payloads /opt/exploits/rop

# Copy exploit tools from repo
if [ -d /vagrant/lab/tools ]; then
  cp /vagrant/lab/tools/* /opt/exploits/tools/ 2>/dev/null || true
fi

chown -R labuser:labuser /opt/exploits

# Encrypted P2P chat system
/bin/sh /vagrant/provisioning/chat.sh

echo "=== Attacker setup complete ==="
