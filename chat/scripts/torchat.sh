#!/bin/sh
# torchat.sh — Start/stop/status for Tor chat services
# Works on both macOS and FreeBSD
#
# Usage:
#   sh chat/scripts/torchat.sh start   [--no-irc]
#   sh chat/scripts/torchat.sh stop
#   sh chat/scripts/torchat.sh status
#   sh chat/scripts/torchat.sh restart
set -e

# Colors
if [ -t 1 ]; then
  G='\033[0;32m'; R='\033[0;31m'; Y='\033[1;33m'; C='\033[0;36m'; N='\033[0m'
else
  G=''; R=''; Y=''; C=''; N=''
fi
info()  { printf "${G}[+]${N} %s\n" "$1"; }
warn()  { printf "${Y}[!]${N} %s\n" "$1"; }
error() { printf "${R}[-]${N} %s\n" "$1"; }
cyan()  { printf "${C}%s${N}" "$1"; }

OS="$(uname)"
CMD="${1:-status}"
NO_IRC=0
for arg in "$@"; do
  [ "$arg" = "--no-irc" ] && NO_IRC=1
done

# ── Paths ─────────────────────────────────────────────────

if [ "$OS" = "Darwin" ]; then
  TORDIR="${HOME}/.aurelia-bloom/tor"
  TORRC="${TORDIR}/torrc"
  TOR_BIN="$(command -v tor 2>/dev/null || echo /opt/homebrew/bin/tor)"
  ONION_FILE="${TORDIR}/chatd/hostname"
  CHATD_BIN="${HOME}/.local/bin/chatd"
  CHAT_BIN="${HOME}/.local/bin/chat"
  IRCGW_BIN="${HOME}/.local/bin/irc-gateway"
  CHATD_LOG="${TORDIR}/chatd.log"
  IRCGW_LOG="${TORDIR}/ircgw.log"
  TOR_LOG="${TORDIR}/tor.log"
  CHATD_PID="${TORDIR}/chatd.pid"
  IRCGW_PID="${TORDIR}/ircgw.pid"
  TOR_PID="${TORDIR}/tor.pid"

  # Fall back to local build if not installed
  SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
  CHAT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
  [ -x "$CHATD_BIN" ] || CHATD_BIN="${CHAT_DIR}/chatd"
  [ -x "$CHAT_BIN" ]  || CHAT_BIN="${CHAT_DIR}/chat"
  [ -x "$IRCGW_BIN" ] || IRCGW_BIN="${CHAT_DIR}/irc-gateway"

elif [ "$OS" = "FreeBSD" ]; then
  TORRC="/usr/local/etc/tor/torrc"
  TOR_BIN="/usr/local/bin/tor"
  ONION_FILE="/var/db/tor/chatd/hostname"
  CHATD_BIN="/usr/local/bin/chatd"
  CHAT_BIN="/usr/local/bin/chat"
  IRCGW_BIN="/usr/local/bin/irc-gateway"
  CHATD_LOG="/var/log/chatd.log"
  IRCGW_LOG="/var/log/ircgw.log"
  TOR_LOG="/var/log/tor.log"
  CHATD_PID="/var/run/chatd.pid"
  IRCGW_PID="/var/run/ircgw.pid"
  TOR_PID=""

  # Fall back to local build
  SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
  CHAT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
  [ -x "$CHATD_BIN" ] || CHATD_BIN="${CHAT_DIR}/chatd"
  [ -x "$CHAT_BIN" ]  || CHAT_BIN="${CHAT_DIR}/chat"
  [ -x "$IRCGW_BIN" ] || IRCGW_BIN="${CHAT_DIR}/irc-gateway"
else
  echo "Unsupported OS: $OS"
  exit 1
fi

# ── Helpers ───────────────────────────────────────────────

is_running() {
  pgrep -f "$1" >/dev/null 2>&1
}

pid_of() {
  pgrep -f "$1" 2>/dev/null | head -1
}

wait_for_file() {
  _file="$1"
  _timeout="${2:-120}"
  _elapsed=0
  while [ "$_elapsed" -lt "$_timeout" ]; do
    [ -f "$_file" ] && return 0
    sleep 2
    _elapsed=$((_elapsed + 2))
    printf "."
  done
  echo ""
  return 1
}

# ── Start ─────────────────────────────────────────────────

do_start() {
  info "Starting Tor chat services..."
  echo ""

  # Tor
  if is_running "tor.*aurelia-bloom\|tor.*chatd"; then
    info "Tor: already running (pid $(pid_of 'tor.*aurelia-bloom\|tor.*chatd'))"
  else
    if [ ! -f "$TORRC" ]; then
      error "Tor config not found at ${TORRC}"
      error "Run the install script first"
      exit 1
    fi

    if [ "$OS" = "FreeBSD" ]; then
      service tor start 2>/dev/null || "$TOR_BIN" -f "$TORRC" &
    else
      "$TOR_BIN" -f "$TORRC" &
      echo $! > "$TOR_PID"
    fi
    info "Tor: starting..."

    printf "  Waiting for hidden service"
    if wait_for_file "$ONION_FILE" 120; then
      echo ""
      info "Tor: ready"
    else
      error "Tor: hidden service did not start"
      exit 1
    fi
  fi

  ONION="$(cat "$ONION_FILE" 2>/dev/null)"
  info "Onion: ${ONION}"
  echo ""

  # Copy onion to $HOME path for macOS chatd
  if [ "$OS" = "Darwin" ]; then
    mkdir -p "${HOME}/.tor/chatd"
    cp "$ONION_FILE" "${HOME}/.tor/chatd/hostname"
  fi

  # chatd
  if is_running "$CHATD_BIN"; then
    info "chatd: already running (pid $(pid_of "$CHATD_BIN"))"
  else
    if [ ! -x "$CHATD_BIN" ]; then
      error "chatd not found at ${CHATD_BIN}"
      exit 1
    fi
    "$CHATD_BIN" > "$CHATD_LOG" 2>&1 &
    CPID=$!
    echo "$CPID" > "$CHATD_PID"
    sleep 2
    if kill -0 "$CPID" 2>/dev/null; then
      info "chatd: running (pid ${CPID})"
    else
      error "chatd: failed to start"
      cat "$CHATD_LOG" 2>/dev/null
      exit 1
    fi
  fi

  # irc-gateway
  if [ "$NO_IRC" -eq 1 ]; then
    info "irc-gateway: skipped (--no-irc)"
  elif is_running "$IRCGW_BIN"; then
    info "irc-gateway: already running (pid $(pid_of "$IRCGW_BIN"))"
  else
    if [ ! -x "$IRCGW_BIN" ]; then
      warn "irc-gateway not found at ${IRCGW_BIN} — skipping"
    else
      "$IRCGW_BIN" > "$IRCGW_LOG" 2>&1 &
      IPID=$!
      echo "$IPID" > "$IRCGW_PID"
      sleep 1
      if kill -0 "$IPID" 2>/dev/null; then
        info "irc-gateway: running (pid ${IPID}) on 127.0.0.1:6667"
      else
        error "irc-gateway: failed to start"
        cat "$IRCGW_LOG" 2>/dev/null
      fi
    fi
  fi

  echo ""
  info "=== Tor Chat Ready ==="
  echo ""
  echo "  Identity:  $(cyan "$ONION")"
  FP="$("$CHAT_BIN" id 2>/dev/null | grep key | awk '{print $2}' || echo '?')"
  echo "  Key:       $(cyan "$FP")"
  echo ""
  echo "  CLI:       $CHAT_BIN"
  echo "  irssi:     irssi -c 127.0.0.1 -p 6667"
  echo "  Logs:      $CHATD_LOG"
  echo ""
}

# ── Stop ──────────────────────────────────────────────────

do_stop() {
  info "Stopping Tor chat services..."

  # irc-gateway
  if is_running "$IRCGW_BIN\|irc-gateway"; then
    pkill -f "irc-gateway" 2>/dev/null || true
    info "irc-gateway: stopped"
  else
    info "irc-gateway: not running"
  fi

  # chatd
  if is_running "$CHATD_BIN\|chatd$"; then
    pkill -f "chatd" 2>/dev/null || true
    info "chatd: stopped"
  else
    info "chatd: not running"
  fi

  # Tor
  if [ "$OS" = "FreeBSD" ]; then
    service tor stop 2>/dev/null || pkill -f "tor" 2>/dev/null || true
  else
    pkill -f "tor.*aurelia-bloom" 2>/dev/null || true
  fi
  info "tor: stopped"

  # Clean pidfiles
  rm -f "$CHATD_PID" "$IRCGW_PID" 2>/dev/null
  [ -n "$TOR_PID" ] && rm -f "$TOR_PID" 2>/dev/null

  echo ""
  info "All services stopped"
}

# ── Status ────────────────────────────────────────────────

do_status() {
  echo ""
  printf "  %-16s " "tor:"
  if is_running "tor"; then
    printf "${G}running${N} (pid %s)\n" "$(pid_of 'tor')"
  else
    printf "${R}stopped${N}\n"
  fi

  printf "  %-16s " "chatd:"
  if is_running "chatd"; then
    printf "${G}running${N} (pid %s)\n" "$(pid_of 'chatd')"
  else
    printf "${R}stopped${N}\n"
  fi

  printf "  %-16s " "irc-gateway:"
  if is_running "irc-gateway"; then
    printf "${G}running${N} (pid %s)\n" "$(pid_of 'irc-gateway')"
  else
    printf "${R}stopped${N}\n"
  fi

  echo ""
  if [ -f "$ONION_FILE" ]; then
    printf "  onion:  %s\n" "$(cat "$ONION_FILE")"
  fi

  if is_running "chatd" && [ -x "$CHAT_BIN" ]; then
    FP="$("$CHAT_BIN" id 2>/dev/null | grep key | awk '{print $2}' || true)"
    [ -n "$FP" ] && printf "  key:    %s\n" "$FP"

    PEERS="$("$CHAT_BIN" peers 2>/dev/null || true)"
    if [ -n "$PEERS" ] && [ "$PEERS" != "No peers connected" ]; then
      echo ""
      info "Connected peers:"
      echo "$PEERS" | while IFS= read -r line; do
        echo "    $line"
      done
    else
      echo ""
      info "No peers connected"
    fi
  fi
  echo ""
}

# ── Main ──────────────────────────────────────────────────

case "$CMD" in
  start)   do_start ;;
  stop)    do_stop ;;
  restart) do_stop; sleep 2; do_start ;;
  status)  do_status ;;
  *)
    echo "Usage: $0 {start|stop|restart|status} [--no-irc]"
    exit 1
    ;;
esac
