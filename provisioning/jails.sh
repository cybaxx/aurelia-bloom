#!/bin/sh
set -e

echo "=== Jail Infrastructure Setup ==="

JAIL_BASE="/jails"
JAILS="attacker basic hardened service"
BUSYBOX="/usr/local/bin/busybox"

# ── Networking: bridge + epair ─────────────────────────────
echo "--- Configuring bridge0 network ---"

# Enable vnet and bridge in rc.conf
sysrc cloned_interfaces="bridge0"
sysrc ifconfig_bridge0="inet 192.168.56.1/24 up"
sysrc gateway_enable="YES"

# Create bridge now
ifconfig bridge0 create 2>/dev/null || true
ifconfig bridge0 inet 192.168.56.1/24 up 2>/dev/null || true

# Enable jail subsystem
sysrc jail_enable="YES"
sysrc jail_list="attacker basic hardened service"

# ── Jail filesystem creation ───────────────────────────────

# BusyBox applet list for symlinks
BUSYBOX_CMDS="sh ls cat cp mv rm mkdir rmdir chmod chown
  ln echo printf test expr sleep date hostname id whoami
  env head tail wc sort uniq grep sed awk cut tr tee
  kill ps mount umount df du free top ping ping6
  vi less more passwd su login getty sysctl ifconfig
  route nc telnet wget tar gzip gunzip"

create_busybox_jail() {
  _name="$1"
  _root="${JAIL_BASE}/${_name}"

  echo "--- Creating BusyBox jail: ${_name} ---"

  # Directory skeleton
  mkdir -p "${_root}/bin" "${_root}/sbin" "${_root}/lib" "${_root}/libexec"
  mkdir -p "${_root}/etc" "${_root}/tmp" "${_root}/var/run" "${_root}/var/log"
  mkdir -p "${_root}/dev" "${_root}/home/labuser" "${_root}/opt/lab/bin"
  mkdir -p "${_root}/usr/local/bin" "${_root}/usr/local/sbin"
  mkdir -p "${_root}/usr/local/etc/dropbear"

  # Install BusyBox
  cp "${BUSYBOX}" "${_root}/bin/busybox"
  chmod 755 "${_root}/bin/busybox"

  # Create symlinks for all applets
  for cmd in ${BUSYBOX_CMDS}; do
    ln -sf busybox "${_root}/bin/${cmd}" 2>/dev/null || true
  done

  # Minimal shared libraries (libc, rtld, libgcc, libthr)
  for lib in /lib/libc.so.7 /lib/libthr.so.3 /lib/libm.so.5 \
             /lib/libutil.so.9 /lib/libcrypt.so.5 /libexec/ld-elf.so.1; do
    if [ -f "${lib}" ]; then
      _dest="${_root}$(dirname "${lib}")"
      mkdir -p "${_dest}"
      cp "${lib}" "${_root}${lib}"
    fi
  done

  # Also copy libgcc_s if present
  for lib in /lib/libgcc_s.so.1 /usr/lib/libgcc_s.so.1; do
    if [ -f "${lib}" ]; then
      cp "${lib}" "${_root}/lib/libgcc_s.so.1"
      break
    fi
  done

  # Minimal /etc
  cat > "${_root}/etc/passwd" <<'ETCEOF'
root:*:0:0:Charlie &:/root:/bin/sh
labuser:*:1001:1001:Lab User:/home/labuser:/bin/sh
nobody:*:65534:65534:Unprivileged user:/nonexistent:/bin/sh
sshd:*:22:22:Secure Shell Daemon:/var/empty:/bin/sh
ETCEOF

  cat > "${_root}/etc/group" <<'ETCEOF'
wheel:*:0:root,labuser
labuser:*:1001:
nobody:*:65534:
sshd:*:22:
ETCEOF

  cat > "${_root}/etc/hosts" <<'ETCEOF'
127.0.0.1  localhost
192.168.56.10  attacker
192.168.56.20  basic target-basic
192.168.56.30  hardened target-hardened
192.168.56.40  service target-service
ETCEOF

  # Set labuser password (password: labuser)
  # Use host pw to set password in jail passwd
  echo "labuser" | pw -V "${_root}/etc" usermod labuser -h 0 2>/dev/null || true

  cat > "${_root}/etc/resolv.conf" <<'ETCEOF'
nameserver 192.168.56.1
ETCEOF

  # Dropbear SSH (lightweight ~110KB sshd)
  cp /usr/local/sbin/dropbear "${_root}/usr/local/sbin/dropbear" 2>/dev/null || true
  cp /usr/local/bin/dropbearkey "${_root}/usr/local/bin/dropbearkey" 2>/dev/null || true
  cp /usr/local/bin/dbclient "${_root}/usr/local/bin/dbclient" 2>/dev/null || true

  # Copy dropbear's required libs
  for lib in /usr/local/lib/libz.so* /usr/lib/libz.so*; do
    if [ -f "${lib}" ]; then
      mkdir -p "${_root}/usr/local/lib"
      cp "${lib}" "${_root}/usr/local/lib/" 2>/dev/null || true
    fi
  done

  # Generate dropbear host keys
  mkdir -p "${_root}/usr/local/etc/dropbear"
  if [ -x /usr/local/bin/dropbearkey ]; then
    dropbearkey -t rsa -f "${_root}/usr/local/etc/dropbear/dropbear_rsa_host_key" 2>/dev/null || true
    dropbearkey -t ecdsa -f "${_root}/usr/local/etc/dropbear/dropbear_ecdsa_host_key" 2>/dev/null || true
    dropbearkey -t ed25519 -f "${_root}/usr/local/etc/dropbear/dropbear_ed25519_host_key" 2>/dev/null || true
  fi

  # Jail startup script — starts dropbear
  cat > "${_root}/etc/rc" <<'RCEOF'
#!/bin/sh
# Minimal jail rc — start dropbear SSH
/usr/local/sbin/dropbear -R -p 22 2>/dev/null &
echo "Jail started: $(hostname)"
RCEOF
  chmod +x "${_root}/etc/rc"

  cat > "${_root}/etc/rc.shutdown" <<'RCEOF'
#!/bin/sh
kill $(cat /var/run/dropbear.pid 2>/dev/null) 2>/dev/null || true
RCEOF
  chmod +x "${_root}/etc/rc.shutdown"

  # Permissions
  chown -R 1001:1001 "${_root}/home/labuser"
  chown -R 1001:1001 "${_root}/opt/lab"
  chmod 1777 "${_root}/tmp"
}

create_attacker_jail() {
  _root="${JAIL_BASE}/attacker"

  echo "--- Creating attacker jail (full userland) ---"

  # Attacker gets a full FreeBSD base extracted from host
  mkdir -p "${_root}"

  # Use host's base as a template — copy essential directories
  for d in bin sbin lib libexec usr etc var tmp; do
    if [ ! -d "${_root}/${d}" ]; then
      cp -R "/${d}" "${_root}/${d}" 2>/dev/null || true
    fi
  done

  mkdir -p "${_root}/dev" "${_root}/proc"
  mkdir -p "${_root}/home/labuser" "${_root}/opt/lab/bin"
  mkdir -p "${_root}/opt/exploits/tools" "${_root}/opt/exploits/payloads"
  mkdir -p "${_root}/opt/exploits/rop"

  # Hosts file
  cat > "${_root}/etc/hosts" <<'ETCEOF'
127.0.0.1  localhost
192.168.56.10  attacker
192.168.56.20  basic target-basic
192.168.56.30  hardened target-hardened
192.168.56.40  service target-service
ETCEOF

  cat > "${_root}/etc/resolv.conf" <<'ETCEOF'
nameserver 192.168.56.1
ETCEOF

  # labuser in attacker jail
  echo "labuser" | pw -V "${_root}/etc" usermod labuser -h 0 2>/dev/null || \
    echo "labuser" | pw -V "${_root}/etc" useradd labuser -m -s /bin/sh -G wheel -h 0 2>/dev/null || true

  # Attacker jail rc — starts sshd
  cat > "${_root}/etc/rc" <<'RCEOF'
#!/bin/sh
/usr/sbin/sshd
echo "Attacker jail started: $(hostname)"
RCEOF
  chmod +x "${_root}/etc/rc"

  cat > "${_root}/etc/rc.shutdown" <<'RCEOF'
#!/bin/sh
kill $(cat /var/run/sshd.pid 2>/dev/null) 2>/dev/null || true
RCEOF
  chmod +x "${_root}/etc/rc.shutdown"

  chown -R 1001:1001 "${_root}/home/labuser"
  chown -R 1001:1001 "${_root}/opt/lab"
  chown -R 1001:1001 "${_root}/opt/exploits"
}

# ── Create jails ───────────────────────────────────────────

# Attacker gets full userland
create_attacker_jail

# Targets get BusyBox minimal
for jail in basic hardened service; do
  create_busybox_jail "${jail}"
done

# ── jail.conf ──────────────────────────────────────────────
echo "--- Writing /etc/jail.conf ---"

cat > /etc/jail.conf <<'JAILCONF'
# HardenedBSD Cyber Range — Jail Configuration
# vnet jails with bridge0 + epair networking

exec.clean;
mount.devfs;
allow.raw_sockets;
path = "/jails/$name";
host.hostname = "$name";

exec.prestart  = "ifconfig epair${id} create 2>/dev/null || true";
exec.prestart += "ifconfig bridge0 addm epair${id}a 2>/dev/null || true";
exec.prestart += "ifconfig epair${id}a up";

exec.start     = "/bin/sh /etc/rc";
exec.stop      = "/bin/sh /etc/rc.shutdown";

exec.poststop  = "ifconfig bridge0 deletem epair${id}a 2>/dev/null || true";
exec.poststop += "ifconfig epair${id}a destroy 2>/dev/null || true";

attacker {
    $id = 0;
    vnet;
    vnet.interface = "epair0b";
    exec.start    += "ifconfig epair0b inet 192.168.56.10/24 up";
    exec.start    += "route add default 192.168.56.1 2>/dev/null || true";
    allow.sysvipc;
}

basic {
    $id = 1;
    vnet;
    vnet.interface = "epair1b";
    exec.start    += "ifconfig epair1b inet 192.168.56.20/24 up";
    exec.start    += "route add default 192.168.56.1 2>/dev/null || true";
}

hardened {
    $id = 2;
    vnet;
    vnet.interface = "epair2b";
    exec.start    += "ifconfig epair2b inet 192.168.56.30/24 up";
    exec.start    += "route add default 192.168.56.1 2>/dev/null || true";
    securelevel = 1;
}

service {
    $id = 3;
    vnet;
    vnet.interface = "epair3b";
    exec.start    += "ifconfig epair3b inet 192.168.56.40/24 up";
    exec.start    += "route add default 192.168.56.1 2>/dev/null || true";
}
JAILCONF

echo "=== Jail infrastructure setup complete ==="
echo "  Jails: ${JAILS}"
echo "  Bridge: bridge0 (192.168.56.1/24)"
echo "  Start jails: service jail restart"
