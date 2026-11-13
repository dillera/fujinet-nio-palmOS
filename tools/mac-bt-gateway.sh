#!/bin/sh
# Make this Mac the network gateway for a Palm on Bluetooth.
# The Palm connects to the Mac's Bluetooth serial port and runs PPP; the
# Mac gives it 192.168.77.2 and forwards its traffic with NAT.
#
#   sudo tools/mac-bt-gateway.sh [serial-device]
#
# The device defaults to build/bt-pty, the pty that tools/bt-serial-bridge
# makes (macOS's own /dev/cu.Bluetooth-Incoming-Port drops the Palm's data).
#
# Ctrl-C stops PPP and removes the NAT rule. Log: build/bt-gateway.log.
set -eu

if [ "$(id -u)" != 0 ]; then
    echo "Run with sudo: sudo $0" >&2
    exit 1
fi

HERE=$(cd "$(dirname "$0")" && pwd)
LOG="$HERE/../build/bt-gateway.log"
PORT=${1:-$HERE/../build/bt-pty}
if [ ! -e "$PORT" ]; then
    echo "$PORT does not exist: start build/bt-serial-bridge first" >&2
    exit 1
fi
# pppd only takes a real device path, not a symlink to one.
[ -L "$PORT" ] && PORT=$(readlink "$PORT")
UPLINK=$(route -n get default | awk '/interface:/ { print $2 }')
PY=/usr/bin/python3
OLD_FWD=$(sysctl -n net.inet.ip.forwarding)

mkdir -p "$(dirname "$LOG")"
: > "$LOG"
chmod 644 "$LOG"

cleanup() {
    pfctl -a com.apple/fujinet-palm -F all 2>/dev/null || true
    [ -n "${PF_TOKEN:-}" ] && pfctl -X "$PF_TOKEN" 2>/dev/null || true
    sysctl -w net.inet.ip.forwarding="$OLD_FWD" >/dev/null
    echo "Gateway stopped."
}
trap cleanup EXIT
trap 'exit 130' INT TERM

sysctl -w net.inet.ip.forwarding=1 >/dev/null
# The system ruleset holds the com.apple anchors the NAT rule goes into.
pfctl -q -f /etc/pf.conf
echo "nat on $UPLINK from 192.168.77.0/24 to any -> ($UPLINK)" |
    pfctl -q -a com.apple/fujinet-palm -f -
PF_TOKEN=$(pfctl -E 2>&1 | awk '/Token/ { print $3 }')

echo "Forwarding 192.168.77.0/24 out of $UPLINK."
echo "Waiting for the Palm on $PORT. Start the connection on the Palm now."

/usr/sbin/pppd "$PORT" 115200 \
    192.168.77.1:192.168.77.2 ms-dns 1.1.1.1 \
    noauth local nocrtscts passive persist maxfail 0 \
    asyncmap 0 noccp novj \
    connect "$PY -I $HERE/palm-handshake.py" \
    nodetach debug logfile "$LOG"
