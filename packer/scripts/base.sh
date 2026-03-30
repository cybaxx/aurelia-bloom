#!/bin/sh
set -e

echo "=== HardenedBSD Base Box Provisioning ==="

# Bootstrap pkg
env ASSUME_ALWAYS_YES=yes pkg bootstrap -f
pkg update -f

# Install essential packages
pkg install -y sudo curl busybox-freebsd dropbear

# VirtualBox guest additions only when running on VirtualBox
if pciconf -lv 2>/dev/null | grep -qi virtualbox; then
  pkg install -y virtualbox-ose-additions-nox11
  sysrc vboxguest_enable="YES"
  sysrc vboxservice_enable="YES"
fi

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

# --- Minimize image ---
rm -rf /usr/share/man /usr/share/info /usr/share/examples
rm -rf /usr/share/locale/[a-df-z]* /usr/share/locale/e[a-mo-z]*
rm -rf /usr/lib/*.a
pkg clean -ay
rm -rf /tmp/*
dd if=/dev/zero of=/EMPTY bs=1m || true
rm -f /EMPTY
sync

echo "=== Base provisioning complete ==="
