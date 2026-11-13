# Bluetooth gateway on Linux for the Clie PEG-TG50

**Status: untested.** These steps are written from the BlueZ and pppd
documentation and from how Palm OS 5 Bluetooth networking works; nothing here
has been run against a real TG50 yet. Expect to adjust it.

The Clie has no Wi-Fi. It reaches the network by running PPP over a Bluetooth
serial channel (RFCOMM) to a Linux machine, which forwards the traffic out of
its own Wi-Fi or Ethernet:

    Clie (FI app + NIO + NetLib) --PPP over Bluetooth--> Linux --NAT--> Wi-Fi

The addresses used below are `192.168.77.1` (Linux end) and `192.168.77.2`
(Clie end). Replace `wlan0` with the Linux machine's uplink interface
(`ip route show default` names it).

## 1. Packages

Debian, Ubuntu, Raspberry Pi OS:

    sudo apt install bluez ppp iptables

`sdptool` and `rfcomm` must be present (`which sdptool rfcomm`). Debian-family
`bluez` ships them. On Arch they are in `bluez-deprecated-tools`; on Fedora in
`bluez-deprecated`.

## 2. Run bluetoothd in compatibility mode

`sdptool` only works when `bluetoothd` runs with `--compat`.

    systemctl cat bluetooth | grep ExecStart     # note the bluetoothd path
    sudo systemctl edit bluetooth

In the editor, add (use the path printed above):

    [Service]
    ExecStart=
    ExecStart=/usr/libexec/bluetooth/bluetoothd --compat

Then:

    sudo systemctl daemon-reload
    sudo systemctl restart bluetooth

## 3. Make the adapter visible and pair

    bluetoothctl
    [bluetooth]# power on
    [bluetooth]# agent KeyboardDisplay
    [bluetooth]# default-agent
    [bluetooth]# pairable on
    [bluetooth]# discoverable on

Leave `bluetoothctl` open. On the Clie: Prefs → Bluetooth → on, then add the
Linux machine as a trusted device. Enter a PIN on the Clie (for example
`1234`); `bluetoothctl` asks for the same PIN. Then, still in `bluetoothctl`:

    [bluetooth]# devices                      # shows the Clie's address
    [bluetooth]# trust XX:XX:XX:XX:XX:XX

If pairing fails straight away, the Clie's Bluetooth 1.1 PIN pairing may be
refused by Secure Simple Pairing. Turn that off and pair again:

    sudo btmgmt ssp off

## 4. Advertise the service

    sudo sdptool add --channel=1 LAN
    sudo sdptool add --channel=1 SP
    sudo sdptool browse local | grep -A4 "Service Name"

`LAN` is the LAN Access profile the Clie looks for when its connection is set
to "Local Network"; `SP` (Serial Port) covers the "PC" setting. Both point at
RFCOMM channel 1. These records are lost when `bluetoothd` restarts.

## 5. Forwarding and NAT

    sudo sysctl -w net.ipv4.ip_forward=1
    sudo iptables -t nat -A POSTROUTING -s 192.168.77.0/24 -o wlan0 -j MASQUERADE
    sudo iptables -A FORWARD -s 192.168.77.0/24 -j ACCEPT
    sudo iptables -A FORWARD -d 192.168.77.0/24 -m state --state ESTABLISHED,RELATED -j ACCEPT

## 6. Wait for the Clie and start PPP

Leave this running in a terminal. It starts `pppd` each time the Clie
connects; `{}` is replaced with the RFCOMM device.

    sudo rfcomm watch hci0 1 pppd {} 115200 \
        192.168.77.1:192.168.77.2 ms-dns 1.1.1.1 \
        noauth local nocrtscts noccp nodefaultroute nodetach debug

`debug` prints the PPP negotiation, which is the first thing to read when the
Clie fails to connect. A good session ends with lines like
`local IP address 192.168.77.1` and `remote IP address 192.168.77.2`.

## 7. Clie settings

Menu names are from Palm OS 5 generally and may differ slightly on the Clie.

1. **Prefs → Connection → New**
   - Name: `Linux BT`
   - Connect to: `Local Network`
   - Via: `Bluetooth`
   - Device: tap to search, pick the Linux machine
2. **Prefs → Network**
   - Service: new, `Linux BT`
   - User name: anything (for example `palm`); password: leave unassigned
   - Connection: `Linux BT`
   - Details: Connection type `PPP`, Query DNS on, IP Address `Automatic`
   - Details → Script: only `End`
3. Tap **Connect**. The `rfcomm watch` terminal should show the connection and
   the PPP negotiation.

Then open FI Test: Ping should show an IP of `192.168.77.2`, and Fetch should
return `HTTP 200`.

## Troubleshooting

- **Clie's device search does not list the Linux machine.** Check
  `discoverable on`. The Clie may only list devices whose class says "network
  access point"; set that and search again:

      sudo hciconfig hci0 class 0x020300

- **Clie finds the machine but says the service is not available.** Re-run
  step 4 and check `sdptool browse local`. Try Connect to: `PC` instead of
  `Local Network`.
- **`rfcomm watch` shows a connection but PPP never comes up.** Read the
  `debug` output. If the Clie sends text such as `CLIENT` before PPP starts,
  the connection is in "PC" mode and expects a Windows-style handshake; switch
  to `Local Network`, or add `connect 'chat -v CLIENT CLIENTSERVER'` to the
  pppd options.
- **PPP is up but Fetch fails.** From Linux, `ping 192.168.77.2`. If that
  works, check step 5 (`sudo iptables -t nat -L -v`, `sysctl net.ipv4.ip_forward`)
  and that `wlan0` is the real uplink.
- **Nothing reaches `rfcomm watch` at all.** Confirm the channel:
  `sdptool browse local` must show channel 1 for the LAN/Serial records, and
  nothing else may already hold RFCOMM channel 1.

## Undo

    sudo iptables -t nat -D POSTROUTING -s 192.168.77.0/24 -o wlan0 -j MASQUERADE
    sudo iptables -D FORWARD -s 192.168.77.0/24 -j ACCEPT
    sudo iptables -D FORWARD -d 192.168.77.0/24 -m state --state ESTABLISHED,RELATED -j ACCEPT
    sudo sysctl -w net.ipv4.ip_forward=0
    sudo systemctl revert bluetooth && sudo systemctl restart bluetooth
