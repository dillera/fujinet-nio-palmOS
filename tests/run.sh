#!/bin/sh
# Builds the core with fujinet-nio-lib for this machine and runs host_test.
set -e
cd "$(dirname "$0")/.."
LIB=${NIO_LIB:-../nio/fujinet-nio-workspace/repos/fujinet-nio-lib}
mkdir -p build
SRCS=$(ls "$LIB"/src/common/*.c | grep -v -e fn_transport_stream.c -e fn_session.c -e fn_slip.c)
cc -std=c99 -Wall -O1 -g -fsanitize=address,undefined -DFN_MAX_PACKET_SIZE=512 \
    -I"$LIB/include" -Icore -o build/host_test \
    tests/host_test.c core/*.c platform/posix/nio_sock_posix.c transport/fn_transport_local.c $SRCS
rm -f build/ports
python3 tests/servers.py > build/ports &
SERVERS=$!
trap 'kill $SERVERS' EXIT
while [ ! -s build/ports ]; do sleep 0.1; done
build/host_test $(cat build/ports)
