#!/bin/sh
set -e

echo "=== Chat System Setup ==="

# Install dependencies
pkg install -y tor libsodium 2>/dev/null || true

# Build chat binaries
cd /vagrant/chat
make clean
make
make install

# Configure Tor hidden service
if ! grep -q "HiddenServiceDir /var/db/tor/chatd" /usr/local/etc/tor/torrc 2>/dev/null; then
    mkdir -p /usr/local/etc/tor
    if [ ! -f /usr/local/etc/tor/torrc ]; then
        cp /usr/local/etc/tor/torrc.sample /usr/local/etc/tor/torrc 2>/dev/null || \
            touch /usr/local/etc/tor/torrc
    fi
    cat /vagrant/chat/torrc.template >> /usr/local/etc/tor/torrc
fi

# Ensure tor hidden service directory
mkdir -p /var/db/tor/chatd
chown -R _tor:_tor /var/db/tor/chatd
chmod 700 /var/db/tor/chatd

# Enable and start tor
sysrc tor_enable="YES" 2>/dev/null || true
service tor restart 2>/dev/null || tor &

# Wait for .onion address
echo "Waiting for Tor hidden service..."
for i in $(seq 1 30); do
    if [ -f /var/db/tor/chatd/hostname ]; then
        echo "Onion address: $(cat /var/db/tor/chatd/hostname)"
        break
    fi
    sleep 2
done

# Initialize chat keys for labuser
su -l labuser -c '/opt/lab/bin/chat init' 2>/dev/null || true

# Start chatd as labuser
su -l labuser -c 'daemon -p /var/run/chatd.pid /opt/lab/bin/chatd' 2>/dev/null || true

echo "=== Chat setup complete ==="
