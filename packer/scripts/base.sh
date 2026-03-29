#!/bin/sh
set -e

echo "=== HardenedBSD Base Box Provisioning ==="

# Bootstrap pkg
env ASSUME_ALWAYS_YES=yes pkg bootstrap -f
pkg update -f

# Install essential packages
pkg install -y sudo bash curl wget virtualbox-ose-additions-nox11

# --- Vagrant user ---
pw useradd -n vagrant -m -s /bin/sh -G wheel
echo "vagrant" | pw usermod vagrant -h 0

# Vagrant insecure public key
mkdir -p /home/vagrant/.ssh
chmod 700 /home/vagrant/.ssh
fetch -o /home/vagrant/.ssh/authorized_keys \
  https://raw.githubusercontent.com/hashicorp/vagrant/main/keys/vagrant.pub
chmod 600 /home/vagrant/.ssh/authorized_keys
chown -R vagrant:vagrant /home/vagrant/.ssh

# Passwordless sudo for vagrant
echo "vagrant ALL=(ALL) NOPASSWD: ALL" > /usr/local/etc/sudoers.d/vagrant
chmod 440 /usr/local/etc/sudoers.d/vagrant

# --- SSH hardening (allow key and password for Vagrant) ---
cat >> /etc/ssh/sshd_config <<'EOF'
UseDNS no
EOF

# --- VirtualBox Guest Additions ---
sysrc vboxguest_enable="YES"
sysrc vboxservice_enable="YES"

# --- Minimize image ---
pkg clean -ay
rm -rf /tmp/*
dd if=/dev/zero of=/EMPTY bs=1m || true
rm -f /EMPTY
sync

echo "=== Base provisioning complete ==="
