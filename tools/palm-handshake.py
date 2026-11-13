"""pppd connect script for a Palm dialling in over Bluetooth serial.

A Palm whose connection is set to "PC" may first send the Windows direct
cable handshake ("CLIENT") and wait for "CLIENTSERVER" before it starts PPP.
Others start PPP straight away. This waits for the first bytes from the
Palm, answers the handshake if it is there, and otherwise returns so pppd
can start; the Palm resends the PPP frame it lost.

stdin and stdout are the serial line; messages go to stderr (pppd's log).
"""
import os
import select
import sys
import time

seen = b""
while True:
    ready, _, _ = select.select([0], [], [], 1.0 if seen else 5.0)
    if not ready:
        if seen:
            break
        continue
    data = os.read(0, 256)
    if not data:
        time.sleep(0.2)
        continue
    seen += data
    if b"CLIENT" in seen:
        os.write(1, b"CLIENTSERVER")
        sys.stderr.write("palm-handshake: answered CLIENT\n")
        sys.exit(0)
    if b"\x7e" in seen:
        break
sys.stderr.write("palm-handshake: Palm sent %r, starting PPP\n" % seen[:32])
