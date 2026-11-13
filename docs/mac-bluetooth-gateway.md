# Mac as the Bluetooth gateway

Tested on 2026-11-12: a Sony Clie PEG-TG50 (Palm OS 5.0) running FI Test
fetched `http://example.com/` through a Mac on macOS 27.0.1.

    Clie (FI app + NIO + NetLib) --PPP over Bluetooth--> Mac --NAT--> Wi-Fi

macOS's own `/dev/cu.Bluetooth-Incoming-Port` accepts the Clie's Bluetooth
serial connection but drops everything the Clie sends (bluetoothd logs
`rxBuf is null or invalid data length`), so PPP times out. `bt-serial-bridge`
replaces it: it publishes a LAN Access service of its own and copies bytes
between the Bluetooth channel and a pseudo-terminal that pppd runs on.

## Once

1. Build the bridge: `make bt-bridge` (needs Xcode or the Command Line Tools).
2. Pair: open System Settings → Bluetooth on the Mac; on the Clie, Prefs →
   Bluetooth → on, Trusted Devices → Add Device, pick the Mac, enter a PIN
   (for example `1234`) on both.
3. Clie, Prefs → Connection → New: name `Mac BT`, Connect to **Local
   Network**, Via **Bluetooth**, Device the Mac. ("PC" uses the Serial Port
   service, which the bridge only offers when started with `serial`.)
4. Clie, Prefs → Network → Service → New: name `Mac BT`, user name and
   password blank, Connection `Mac BT`; Details: PPP, Query DNS on, IP
   Address Automatic, Script just End. Leave this service selected: NIO
   uses whichever service the Network panel shows.

## Each time

In one Terminal, as yourself (the first run asks for Bluetooth access):

    build/bt-serial-bridge "$PWD/build/bt-pty"

In a second Terminal:

    sudo tools/mac-bt-gateway.sh

Then use any FI app on the Clie; the first network call dials. The Clie gets
`192.168.77.2`, DNS `1.1.1.1`, and its traffic leaves through the Mac's
default-route interface with NAT. `ping 192.168.77.2` from the Mac checks
the link.

Ctrl-C in the gateway Terminal ends PPP, removes the NAT rule and puts IP
forwarding back as it was. Run only one gateway at a time: stopping a second
one removes the NAT rule the other relies on.

## Reading the logs

- The bridge's output (it prints to its
  Terminal): `Palm connected`, then the first bytes each way. Bytes from the
  Palm starting `7eff7d23c021` are PPP; if nothing arrives, the Clie
  connected to a different service.
- `build/bt-gateway.log`: pppd's negotiation. A good session ends with
  `remote IP address 192.168.77.2`.
