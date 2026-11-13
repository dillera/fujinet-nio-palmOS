# fujinet-nio-palmOS

FujiNet NIO's device side running on the Palm itself, so FujiNet apps need no
FujiNet hardware: the Palm's own network link does the work. The target
device is a Sony Clie PEG-TG50 (Palm OS 5, Bluetooth), reaching the network
over Bluetooth through a Mac ([docs/mac-bluetooth-gateway.md](docs/mac-bluetooth-gateway.md),
working) or a Linux machine ([docs/bluetooth-gateway.md](docs/bluetooth-gateway.md),
untested). Any Palm with a working NetLib
connection (OS 3.5 and later, 68k or OS 5) should do.

This is a reimplementation in C, not a build of fujinet-nio's C++20 source,
which the Palm toolchain cannot compile. Palm OS runs one app at a time, so
NIO is a library inside each app, not a background process.

## How it fits together

```
FI app -> fujinet-nio-lib calls (fn_open, fn_read, ...)
       -> transport/fn_transport_local.c   one FujiBus packet per call
       -> core/                            the devices: network, fuji, wifi
       -> platform/palmos/                 Palm OS NetLib sockets
```

The apps in fujinet-palm are built unchanged; only the library they link
differs. `platform/posix/` is the same socket layer on BSD sockets, for the
tests.

## What works

- NetworkDevice: `tcp://` streams; `http://` GET, HEAD, DELETE, POST and PUT,
  with the status, length and requested response headers from Info.
- FujiDevice GetInfo (reports `palmos-0.1`, profile `palmos-netlib`).
- Wi-Fi service status and config, reporting the NetLib link. The link itself
  is set up in the Palm's Network preferences.

## What does not, yet

- `https://` and TLS (answers Unsupported).
- HTTP redirects and chunked replies (requests are HTTP/1.0).
- FileDevice and TNFS, so FI Config cannot browse or install yet.
- UDP, the clock device, content translation (JSON selectors, images).
- Session idle timeouts and the Ready command.

## Build and test

```
make test    # core + fujinet-nio-lib on this machine, against local servers
make palm    # Palm library and FI Test, in the palmdev Docker image
make palm APPS="FujiNetTest BounceWorld"
```

Paths default to the `~/code/FujiPalm` layout (`NIO_LIB`, `PALM_APPS`
override them). Output is in `build/palm/`.

In the emulator, networking needs CloudpilotEmu's native app with
`--net-native`:

```
rig.sh start <run-dir> <68k image> none --net-native
rig.sh cmd <run-dir> install build/palm/FujiNetTest.prc
```

CloudpilotEmu's OS 5 emulation has no networking, and nothing emulates
Bluetooth, so the Bluetooth link can only be tried on the real device.

On the Clie, install over USB with palm-sync (`shared/palm-sync`, after
`npm ci && npx tsc`): run `node dist/bin/cli.js push --usb <file.prc>`, then
tap HotSync (Local, Cradle/Cable) on the Clie.
