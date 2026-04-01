#!/bin/sh
# install-freebsd.sh — Install Tor chat on FreeBSD
# Sets up Tor hidden service, builds chatd/chat/irc-gateway, creates rc.d services
#
# Usage: sh chat/scripts/install-freebsd.sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CHAT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Colors
if [ -t 1 ]; then
  G='\033[0;32m'; R='\033[0;31m'; Y='\033[1;33m'; N='\033[0m'
else
  G=''; R=''; Y=''; N=''
fi
info()  { printf "${G}[+]${N} %s\n" "$1"; }
warn()  { printf "${Y}[!]${N} %s\n" "$1"; }
die()   { printf "${R}[-]${N} %s\n" "$1"; exit 1; }

[ "$(uname)" = "FreeBSD" ] || die "This script is for FreeBSD only"
[ "$(id -u)" = "0" ] || die "Run as root"

# ── Install dependencies ─────────────────────────────────
info "Installing packages..."
env ASSUME_ALWAYS_YES=yes pkg install -y tor libsodium irssi

# ── Build chat binaries ──────────────────────────────────
info "Building chat..."
cd "${CHAT_DIR}"
rm -rf obj chatd chat irc-gateway
mkdir -p obj

CF="-Wall -Wextra -O2 -I/usr/local/include"
LF="-L/usr/local/lib -lsodium"

cc $CF -Isrc -c -o obj/crypto.o src/crypto.c
cc $CF -Isrc -c -o obj/peer.o src/peer.c
cc $CF -Isrc -c -o obj/chatd.o src/chatd.c
cc $CF -Isrc -c -o obj/chat.o src/chat.c
cc -o chatd obj/chatd.o obj/crypto.o obj/peer.o $LF
cc -o chat obj/chat.o obj/crypto.o obj/peer.o $LF
cc $CF -Isrc -o irc-gateway src/irc-gateway.c $LF

info "Installing to /usr/local/bin/..."
install -m 755 chatd /usr/local/bin/chatd
install -m 755 chat /usr/local/bin/chat
install -m 755 irc-gateway /usr/local/bin/irc-gateway

# Install torchat control script
install -m 755 scripts/torchat.sh /usr/local/bin/torchat

# ── Configure Tor hidden service ─────────────────────────
info "Configuring Tor hidden service..."
mkdir -p /var/db/tor/chatd
chown -R _tor:_tor /var/db/tor
chmod 700 /var/db/tor /var/db/tor/chatd

TORRC="/usr/local/etc/tor/torrc"
if [ ! -f "${TORRC}" ] || ! grep -q "HiddenServiceDir /var/db/tor/chatd" "${TORRC}" 2>/dev/null; then
  cat >> "${TORRC}" <<EOF

# --- aurelia-bloom chat hidden service ---
SocksPort 9050
HiddenServiceDir /var/db/tor/chatd
HiddenServicePort 9876 127.0.0.1:9876
EOF
  info "Added hidden service to ${TORRC}"
else
  info "Hidden service already configured in ${TORRC}"
fi

# ── Create rc.d service for chatd ────────────────────────
info "Creating rc.d services..."
cat > /usr/local/etc/rc.d/chatd <<'RCEOF'
#!/bin/sh

# PROVIDE: chatd
# REQUIRE: DAEMON tor
# KEYWORD: shutdown

. /etc/rc.subr

name="chatd"
rcvar="chatd_enable"
pidfile="/var/run/${name}.pid"

start_cmd="${name}_start"
stop_cmd="${name}_stop"
status_cmd="${name}_status"

chatd_start()
{
    echo "Starting ${name}."
    HOME="${chatd_home:-/root}" /usr/local/bin/chatd > /var/log/chatd.log 2>&1 &
    echo $! > "${pidfile}"
}

chatd_stop()
{
    if [ -f "${pidfile}" ]; then
        echo "Stopping ${name}."
        kill $(cat "${pidfile}") 2>/dev/null
        rm -f "${pidfile}"
    fi
}

chatd_status()
{
    if [ -f "${pidfile}" ] && kill -0 $(cat "${pidfile}") 2>/dev/null; then
        echo "${name} is running as pid $(cat "${pidfile}")."
    else
        echo "${name} is not running."
        return 1
    fi
}

load_rc_config $name
run_rc_command "$1"
RCEOF
chmod 755 /usr/local/etc/rc.d/chatd

# ── Create rc.d service for irc-gateway ──────────────────
cat > /usr/local/etc/rc.d/ircgateway <<'RCEOF'
#!/bin/sh

# PROVIDE: ircgateway
# REQUIRE: chatd
# KEYWORD: shutdown

. /etc/rc.subr

name="ircgateway"
rcvar="ircgateway_enable"
pidfile="/var/run/${name}.pid"

start_cmd="${name}_start"
stop_cmd="${name}_stop"
status_cmd="${name}_status"

ircgateway_start()
{
    echo "Starting ${name}."
    /usr/local/bin/irc-gateway > /var/log/ircgw.log 2>&1 &
    echo $! > "${pidfile}"
}

ircgateway_stop()
{
    if [ -f "${pidfile}" ]; then
        echo "Stopping ${name}."
        kill $(cat "${pidfile}") 2>/dev/null
        rm -f "${pidfile}"
    fi
}

ircgateway_status()
{
    if [ -f "${pidfile}" ] && kill -0 $(cat "${pidfile}") 2>/dev/null; then
        echo "${name} is running as pid $(cat "${pidfile}")."
    else
        echo "${name} is not running."
        return 1
    fi
}

load_rc_config $name
run_rc_command "$1"
RCEOF
chmod 755 /usr/local/etc/rc.d/ircgateway

# ── Enable services ──────────────────────────────────────
info "Enabling services..."
sysrc tor_enable="YES"
sysrc chatd_enable="YES"
sysrc ircgateway_enable="YES"

# ── Initialize crypto keys ───────────────────────────────
info "Initializing crypto keys..."
chat init

# ── Done ─────────────────────────────────────────────────
info "Installation complete!"
echo ""
info "Start all services:"
echo "  torchat start        # start tor + chatd + irc-gateway"
echo "  torchat stop         # stop all"
echo "  torchat status       # show status"
echo "  torchat restart      # restart all"
echo ""
info "Or use rc.d individually:"
echo "  service tor start"
echo "  service chatd start"
echo "  service ircgateway start"
echo ""
info "Use with irssi:"
echo "  irssi -c 127.0.0.1 -p 6667"
echo "  /join #aurelia"
echo ""
info "CLI:"
echo "  chat id                     # show your .onion address"
echo "  chat add-peer <addr.onion>  # connect to a peer"
echo "  chat                        # interactive mode"
echo ""
info "Logs: /var/log/chatd.log, /var/log/ircgw.log"
info "Services enabled at boot: tor, chatd, ircgateway"
