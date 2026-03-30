#!/bin/sh
set -e

echo "=== Attacker Jail Provisioning ==="

JAIL_ROOT="/jails/attacker"

# Install packages into attacker jail via host pkg
# (jail shares host's pkg repos)
pkg -c "${JAIL_ROOT}" install -y radare2 py311-pwntools python3 \
  py311-capstone gdb git vim tmux tor libsodium 2>/dev/null || true

# Exploit workspace (already created by jails.sh)
mkdir -p "${JAIL_ROOT}/opt/exploits/tools"
mkdir -p "${JAIL_ROOT}/opt/exploits/payloads"
mkdir -p "${JAIL_ROOT}/opt/exploits/rop"

# Copy exploit tools from repo
if [ -d /vagrant/lab/tools ]; then
  cp /vagrant/lab/tools/* "${JAIL_ROOT}/opt/exploits/tools/" 2>/dev/null || true
fi

# Build and install chat system into attacker jail
if [ -d /vagrant/chat ] && [ -f /vagrant/chat/Makefile ]; then
  cd /vagrant/chat
  make clean 2>/dev/null || true
  make
  install -d "${JAIL_ROOT}/opt/lab/bin"
  install -m 755 chatd "${JAIL_ROOT}/opt/lab/bin/chatd"
  install -m 755 chat "${JAIL_ROOT}/opt/lab/bin/chat"
fi

# Configure Tor hidden service inside jail
TORRC="${JAIL_ROOT}/usr/local/etc/tor/torrc"
mkdir -p "$(dirname "${TORRC}")"
if [ ! -f "${TORRC}" ]; then
  cp "${JAIL_ROOT}/usr/local/etc/tor/torrc.sample" "${TORRC}" 2>/dev/null || \
    touch "${TORRC}"
fi
if ! grep -q "HiddenServiceDir" "${TORRC}" 2>/dev/null; then
  if [ -f /vagrant/chat/torrc.template ]; then
    cat /vagrant/chat/torrc.template >> "${TORRC}"
  fi
fi

mkdir -p "${JAIL_ROOT}/var/db/tor/chatd"
chmod 700 "${JAIL_ROOT}/var/db/tor/chatd"

# Update attacker jail rc to also start tor and chatd
cat > "${JAIL_ROOT}/etc/rc" <<'RCEOF'
#!/bin/sh
/usr/sbin/sshd
/usr/local/bin/tor 2>/dev/null &
sleep 2
su -l labuser -c 'daemon -p /var/run/chatd.pid /opt/lab/bin/chatd' 2>/dev/null || true
echo "Attacker jail started: $(hostname)"
RCEOF
chmod +x "${JAIL_ROOT}/etc/rc"

# Copy lab sources
if [ -d /vagrant/lab ]; then
  mkdir -p "${JAIL_ROOT}/opt/lab/src"
  cp -R /vagrant/lab/* "${JAIL_ROOT}/opt/lab/src/" 2>/dev/null || true
fi

chown -R 1001:1001 "${JAIL_ROOT}/opt/exploits"
chown -R 1001:1001 "${JAIL_ROOT}/opt/lab"
chown -R 1001:1001 "${JAIL_ROOT}/home/labuser"

echo "=== Attacker jail provisioning complete ==="
