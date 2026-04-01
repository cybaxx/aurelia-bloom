#!/bin/sh
# install-macos.sh — Install Tor chat on macOS
# Sets up Tor hidden service, builds chatd/chat/irc-gateway, creates LaunchAgents
#
# Usage: sh chat/scripts/install-macos.sh
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

[ "$(uname)" = "Darwin" ] || die "This script is for macOS only"

# ── Install dependencies ─────────────────────────────────
info "Checking dependencies..."
if ! command -v brew >/dev/null 2>&1; then
  die "Homebrew required. Install: https://brew.sh"
fi

for pkg in tor libsodium; do
  if ! brew list "$pkg" >/dev/null 2>&1; then
    info "Installing ${pkg}..."
    brew install "$pkg"
  else
    info "${pkg}: installed"
  fi
done

# ── Build chat binaries ──────────────────────────────────
info "Building chat..."
cd "${CHAT_DIR}"

if command -v pkg-config >/dev/null 2>&1; then
  CF="-Wall -Wextra -O2 $(pkg-config --cflags libsodium)"
  LF="$(pkg-config --libs libsodium)"
else
  CF="-Wall -Wextra -O2 -I/opt/homebrew/include"
  LF="-L/opt/homebrew/lib -lsodium"
fi

make clean
make CFLAGS="${CF}" LDFLAGS="${LF}"

INSTALL_DIR="${HOME}/.local/bin"
mkdir -p "${INSTALL_DIR}"
install -m 755 chatd "${INSTALL_DIR}/chatd"
install -m 755 chat "${INSTALL_DIR}/chat"
install -m 755 irc-gateway "${INSTALL_DIR}/irc-gateway"
info "Installed to ${INSTALL_DIR}/"

# Add to PATH if needed
case ":${PATH}:" in
  *":${INSTALL_DIR}:"*) ;;
  *)
    warn "${INSTALL_DIR} not in PATH"
    info "Add to your shell profile:"
    echo "  export PATH=\"${INSTALL_DIR}:\${PATH}\""
    ;;
esac

# ── Configure Tor hidden service ─────────────────────────
TORDIR="${HOME}/.aurelia-bloom/tor"
info "Configuring Tor hidden service..."
mkdir -p "${TORDIR}/chatd" "${TORDIR}/data"
chmod 700 "${TORDIR}/chatd"

TORRC="${TORDIR}/torrc"
cat > "${TORRC}" <<EOF
SocksPort 9050
DataDirectory ${TORDIR}/data
HiddenServiceDir ${TORDIR}/chatd
HiddenServicePort 9876 127.0.0.1:9876
Log notice file ${TORDIR}/tor.log
EOF
info "Tor config: ${TORRC}"

# Make .onion address available to chatd
mkdir -p "${HOME}/.tor/chatd"

# ── Create LaunchAgent for Tor ───────────────────────────
LAUNCH_DIR="${HOME}/Library/LaunchAgents"
mkdir -p "${LAUNCH_DIR}"

TOR_BIN="$(command -v tor)"

cat > "${LAUNCH_DIR}/com.aurelia-bloom.tor.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.aurelia-bloom.tor</string>
    <key>ProgramArguments</key>
    <array>
        <string>${TOR_BIN}</string>
        <string>-f</string>
        <string>${TORRC}</string>
    </array>
    <key>RunAtLoad</key>
    <false/>
    <key>KeepAlive</key>
    <false/>
    <key>StandardOutPath</key>
    <string>${TORDIR}/tor-stdout.log</string>
    <key>StandardErrorPath</key>
    <string>${TORDIR}/tor-stderr.log</string>
</dict>
</plist>
EOF
info "LaunchAgent: com.aurelia-bloom.tor"

# ── Create LaunchAgent for chatd ─────────────────────────
cat > "${LAUNCH_DIR}/com.aurelia-bloom.chatd.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.aurelia-bloom.chatd</string>
    <key>ProgramArguments</key>
    <array>
        <string>${INSTALL_DIR}/chatd</string>
    </array>
    <key>RunAtLoad</key>
    <false/>
    <key>KeepAlive</key>
    <false/>
    <key>EnvironmentVariables</key>
    <dict>
        <key>HOME</key>
        <string>${HOME}</string>
    </dict>
    <key>StandardOutPath</key>
    <string>${TORDIR}/chatd-stdout.log</string>
    <key>StandardErrorPath</key>
    <string>${TORDIR}/chatd-stderr.log</string>
</dict>
</plist>
EOF
info "LaunchAgent: com.aurelia-bloom.chatd"

# ── Create LaunchAgent for irc-gateway ───────────────────
cat > "${LAUNCH_DIR}/com.aurelia-bloom.irc-gateway.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.aurelia-bloom.irc-gateway</string>
    <key>ProgramArguments</key>
    <array>
        <string>${INSTALL_DIR}/irc-gateway</string>
    </array>
    <key>RunAtLoad</key>
    <false/>
    <key>KeepAlive</key>
    <false/>
    <key>StandardOutPath</key>
    <string>${TORDIR}/ircgw-stdout.log</string>
    <key>StandardErrorPath</key>
    <string>${TORDIR}/ircgw-stderr.log</string>
</dict>
</plist>
EOF
info "LaunchAgent: com.aurelia-bloom.irc-gateway"

# ── Initialize crypto keys ───────────────────────────────
info "Initializing crypto keys..."
"${INSTALL_DIR}/chat" init

# ── Done ─────────────────────────────────────────────────
info "Installation complete!"
echo ""
info "Start/stop services:"
echo "  sh chat/scripts/torchat.sh start      # start tor + chatd + irc-gateway"
echo "  sh chat/scripts/torchat.sh stop       # stop all"
echo "  sh chat/scripts/torchat.sh status     # show status"
echo ""
info "Use with irssi:"
echo "  irssi -c 127.0.0.1 -p 6667"
echo "  /join #aurelia"
echo ""
info "Or use the CLI:"
echo "  chat id                     # show your .onion address"
echo "  chat add-peer <addr.onion>  # connect to a peer"
echo "  chat                        # interactive mode"
