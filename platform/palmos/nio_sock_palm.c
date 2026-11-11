/* nio_sock.h on Palm OS NetLib. NetLib brings up whatever service the
 * Network preferences panel names (PPP over Bluetooth, a modem, a cradle),
 * so the first nio_sock_start() may show the system's connection progress.
 * A 68k app sees network byte order as its own, on OS 5 (PACE) too. */
#include <PalmOS.h>
#include <NetMgr.h>
#include "nio_sock.h"

/* DNS and connect can cross a slow dial-up style link. */
#define CONNECT_SECONDS 20
#define SEND_SECONDS 10

static UInt16 gNet;
static UInt8 gOpen;

int nio_sock_start(void)
{
    UInt16 ifErrs = 0;
    Err err;

    if (gOpen) return 0;
    if (SysLibFind("Net.lib", &gNet) != errNone) return NIO_SOCK_ERROR;
    err = NetLibOpen(gNet, &ifErrs);
    if (err != errNone && err != netErrAlreadyOpen) return NIO_SOCK_ERROR;
    if (ifErrs) {
        NetLibClose(gNet, true);
        return NIO_SOCK_ERROR;
    }
    gOpen = 1;
    return 0;
}

void nio_sock_stop(void)
{
    /* Not immediate: NetLib keeps the link for its close-wait time, so the
     * next app does not have to dial again. */
    if (gOpen) NetLibClose(gNet, false);
    gOpen = 0;
}

static Boolean resolve(const char *host, NetIPAddr *addr)
{
    NetHostInfoBufPtr info;
    Err err;
    Boolean ok = false;

    *addr = NetLibAddrAToIN(gNet, (Char *)host);
    if (*addr != (NetIPAddr)-1) return true;
    info = (NetHostInfoBufPtr)MemPtrNew(sizeof(NetHostInfoBufType));
    if (!info) return false;
    if (NetLibGetHostByName(gNet, (Char *)host, info, CONNECT_SECONDS * SysTicksPerSecond(),
                            &err) && info->address[0]) {
        *addr = info->address[0];
        ok = true;
    }
    MemPtrFree(info);
    return ok;
}

int nio_sock_tcp_connect(const char *host, uint16_t port, long *sock)
{
    NetSocketAddrINType to;
    NetSocketRef s;
    Err err;
    Int32 wait = CONNECT_SECONDS * SysTicksPerSecond();

    if (!gOpen || !resolve(host, &to.addr)) return NIO_SOCK_ERROR;
    to.family = netSocketAddrINET;
    to.port = port;
    s = NetLibSocketOpen(gNet, netSocketAddrINET, netSocketTypeStream, 0, wait, &err);
    if (s < 0) return NIO_SOCK_ERROR;
    if (NetLibSocketConnect(gNet, s, (NetSocketAddrType *)&to, sizeof(to), wait, &err) < 0) {
        NetLibSocketClose(gNet, s, wait, &err);
        return NIO_SOCK_ERROR;
    }
    *sock = s;
    return 0;
}

int nio_sock_send(long sock, const uint8_t *data, uint16_t len)
{
    Err err;
    Int16 n = NetLibSend(gNet, (NetSocketRef)sock, (void *)data, len, 0, 0, 0,
                         SEND_SECONDS * SysTicksPerSecond(), &err);

    return n <= 0 ? NIO_SOCK_ERROR : n;
}

int nio_sock_recv(long sock, uint8_t *data, uint16_t max, uint16_t wait_ms)
{
    Err err;
    Int32 wait = ((Int32)wait_ms * SysTicksPerSecond() + 999) / 1000;
    Int16 n = NetLibReceive(gNet, (NetSocketRef)sock, data, max, 0, 0, 0, wait, &err);

    if (n >= 0) return n;
    if (err == netErrTimeout || err == netErrWouldBlock) return NIO_SOCK_WOULDBLOCK;
    /* The peer went away: the reader sees it as the end of the stream. */
    if (err == netErrSocketClosedByRemote) return 0;
    return NIO_SOCK_ERROR;
}

void nio_sock_close(long sock)
{
    Err err;

    NetLibSocketClose(gNet, (NetSocketRef)sock, SysTicksPerSecond(), &err);
}

void nio_sock_local_ip(char *out, uint16_t cap)
{
    UInt32 creator;
    UInt16 instance, index, len;
    NetIPAddr addr;
    Char text[20];

    out[0] = 0;
    if (!gOpen || cap < 16) return;
    /* The first interface that is not loopback and has an address. */
    for (index = 0; NetLibIFGet(gNet, index, &creator, &instance) == errNone; index++) {
        if (creator == netIFCreatorLoop) continue;
        addr = 0;
        len = sizeof(addr);
        if (NetLibIFSettingGet(gNet, creator, instance, netIFSettingActualIPAddr, &addr,
                               &len) == errNone && addr) {
            NetLibAddrINToA(gNet, addr, text);
            StrCopy(out, text);
            return;
        }
    }
}
