// Bluetooth serial listener for a Palm dialling this Mac.
//
// macOS's own Bluetooth-Incoming-Port accepts the Palm's connection but drops
// the data it sends ("rxBuf is null or invalid data length" in bluetoothd's
// log). This publishes its own RFCOMM service and copies the bytes between
// the Bluetooth channel and a pseudo-terminal, which pppd then runs on.
//
//   bt-serial-bridge <link-path> [lan|serial]
//
// <link-path> becomes a symlink to the pty (give it to mac-bt-gateway.sh).
// "lan" (default) publishes the LAN Access profile, which the Palm uses when
// its connection is set to "Local Network"; "serial" publishes Serial Port,
// for "PC". Runs as the user, not root: macOS asks once for Bluetooth access.
#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>
#include <util.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static void say(NSString *fmt, ...) NS_FORMAT_FUNCTION(1, 2);
static void say(NSString *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    NSString *s = [[NSString alloc] initWithFormat:fmt arguments:ap];
    va_end(ap);
    NSDateFormatter *f = [NSDateFormatter new];
    f.dateFormat = @"HH:mm:ss";
    printf("%s %s\n", [[f stringFromDate:[NSDate date]] UTF8String], [s UTF8String]);
    fflush(stdout);
}

static NSString *hex(const void *data, size_t len)
{
    NSMutableString *s = [NSMutableString string];
    const uint8_t *p = data;
    for (size_t i = 0; i < len && i < 24; i++) [s appendFormat:@"%02x", p[i]];
    if (len > 24) [s appendString:@"..."];
    return s;
}

@interface Bridge : NSObject <IOBluetoothRFCOMMChannelDelegate>
@property(nonatomic) int master;
@property(nonatomic) int slave;
@property(nonatomic, strong) IOBluetoothRFCOMMChannel *channel;
@property(nonatomic, strong) IOBluetoothSDPServiceRecord *record;
@property(nonatomic, strong) dispatch_source_t reader;
@property(nonatomic) unsigned long toPalm, fromPalm;
@end

@implementation Bridge

- (BOOL)openPty:(NSString *)linkPath
{
    char name[128];
    int m, s;
    if (openpty(&m, &s, name, NULL, NULL) != 0) {
        perror("openpty");
        return NO;
    }
    // Raw both ways; pppd sets its own modes when it opens the slave. The
    // slave stays open here too, so the master does not see a hangup each
    // time pppd closes it between calls.
    struct termios t;
    tcgetattr(s, &t);
    cfmakeraw(&t);
    tcsetattr(s, TCSANOW, &t);
    fcntl(m, F_SETFL, fcntl(m, F_GETFL) | O_NONBLOCK);
    self.master = m;
    self.slave = s;
    unlink(linkPath.fileSystemRepresentation);
    if (symlink(name, linkPath.fileSystemRepresentation) != 0) {
        perror("symlink");
        return NO;
    }
    say(@"pty %s, linked from %@", name, linkPath);

    self.reader = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)m, 0,
                                         dispatch_get_main_queue());
    __weak Bridge *weakSelf = self;
    dispatch_source_set_event_handler(self.reader, ^{ [weakSelf fromPty]; });
    dispatch_resume(self.reader);
    return YES;
}

- (BOOL)publish:(BOOL)lan
{
    uint16_t uuid = lan ? 0x1102 : 0x1101;
    uint8_t uuidBytes[2] = { (uint8_t)(uuid >> 8), (uint8_t)uuid };
    NSData *serviceClass = [NSData dataWithBytes:uuidBytes length:2];
    uint8_t l2cap[2] = { 0x01, 0x00 }, rfcomm[2] = { 0x00, 0x03 }, browse[2] = { 0x10, 0x02 };
    NSDictionary *channel = @{ @"DataElementType" : @1, @"DataElementSize" : @1,
                               @"DataElementValue" : @1 };
    NSDictionary *version = @{ @"DataElementType" : @1, @"DataElementSize" : @2,
                               @"DataElementValue" : @0x0100 };
    NSDictionary *dict = @{
        @"0001 - ServiceClassIDList" : @[ serviceClass ],
        @"0004 - ProtocolDescriptorList" : @[
            @[ [NSData dataWithBytes:l2cap length:2] ],
            @[ [NSData dataWithBytes:rfcomm length:2], channel ],
        ],
        @"0005 - BrowseGroupList*" : @[ [NSData dataWithBytes:browse length:2] ],
        @"0009 - BluetoothProfileDescriptorList" : @[ @[ serviceClass, version ] ],
        @"0100 - ServiceName*" : lan ? @"FujiNet LAN Access" : @"FujiNet Serial",
    };
    self.record = [IOBluetoothSDPServiceRecord publishedServiceRecordWithDictionary:dict];
    if (!self.record) {
        say(@"could not publish the service record (Bluetooth off, or no permission?)");
        return NO;
    }
    BluetoothRFCOMMChannelID ch = 0;
    if ([self.record getRFCOMMChannelID:&ch] != kIOReturnSuccess) {
        say(@"published record has no RFCOMM channel");
        return NO;
    }
    [IOBluetoothRFCOMMChannel registerForChannelOpenNotifications:self
                                                         selector:@selector(opened:channel:)
                                                    withChannelID:ch
                                                        direction:kIOBluetoothUserNotificationChannelDirectionIncoming];
    say(@"published %@ on RFCOMM channel %u; waiting for the Palm",
        lan ? @"LAN Access (Palm: Connect to Local Network)" : @"Serial Port (Palm: Connect to PC)", ch);
    return YES;
}

- (void)opened:(IOBluetoothUserNotification *)note channel:(IOBluetoothRFCOMMChannel *)channel
{
    if (self.channel) [self.channel closeChannel];
    self.channel = channel;
    self.toPalm = self.fromPalm = 0;
    [channel setDelegate:self];
    say(@"Palm connected: %@ (%@), MTU %u", channel.getDevice.name ?: @"?",
        channel.getDevice.addressString, channel.getMTU);
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)channel data:(void *)data length:(size_t)length
{
    if (self.fromPalm == 0) say(@"first bytes from Palm: %@", hex(data, length));
    self.fromPalm += length;
    const uint8_t *p = data;
    while (length > 0) {
        ssize_t n = write(self.master, p, length);
        if (n < 0) {
            if (errno == EAGAIN) { usleep(1000); continue; }
            perror("write pty");
            return;
        }
        p += n;
        length -= (size_t)n;
    }
}

- (void)fromPty
{
    uint8_t buf[1024];
    ssize_t n = read(self.master, buf, sizeof(buf));
    if (n <= 0) return;
    IOBluetoothRFCOMMChannel *ch = self.channel;
    if (!ch || !ch.isOpen) return; // nobody to send to; PPP will retry
    if (self.toPalm == 0) say(@"first bytes to Palm: %@", hex(buf, (size_t)n));
    self.toPalm += (unsigned long)n;
    uint16_t mtu = ch.getMTU ?: 127;
    for (ssize_t at = 0; at < n; at += mtu) {
        uint16_t len = (uint16_t)MIN((ssize_t)mtu, n - at);
        IOReturn r = [ch writeSync:buf + at length:len];
        if (r != kIOReturnSuccess) {
            say(@"write to Palm failed: 0x%x", r);
            return;
        }
    }
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)channel
{
    say(@"Palm disconnected (%lu bytes from it, %lu to it)", self.fromPalm, self.toPalm);
    if (channel == self.channel) self.channel = nil;
}

@end

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        if (argc < 2) {
            fprintf(stderr, "usage: %s <link-path> [lan|serial]\n", argv[0]);
            return 2;
        }
        BOOL lan = argc < 3 || strcmp(argv[2], "serial") != 0;
        Bridge *bridge = [Bridge new];
        if (![bridge openPty:[NSString stringWithUTF8String:argv[1]]]) return 1;
        if (![bridge publish:lan]) return 1;
        [[NSRunLoop mainRunLoop] run];
    }
    return 0;
}
