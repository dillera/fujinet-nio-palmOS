# fujinet-nio-palmOS: `make test` runs the core on this machine; `make palm`
# builds the Palm library and apps in the palmdev Docker image (built by
# `make image` in fujinet-palm).
NIO_LIB  ?= $(abspath ../nio/fujinet-nio-workspace/repos/fujinet-nio-lib)
PALM_APPS ?= $(abspath ../nio/fujinet-palm)
APPS     ?= FujiNetTest
DOCKER = docker run --rm --platform linux/amd64 -v $(CURDIR):/src -v $(NIO_LIB):/nio-lib:ro \
         -v $(PALM_APPS):/apps:ro -w /src palmdev

test:
	NIO_LIB=$(NIO_LIB) tests/run.sh

palm:
	$(DOCKER) make -f palm.mk APPS="$(APPS)"

# Bluetooth serial listener for the Mac gateway (docs/mac-bluetooth-gateway.md).
bt-bridge: build/bt-serial-bridge
build/bt-serial-bridge: tools/bt-serial-bridge.m
	mkdir -p build
	clang -fobjc-arc -Wall -O1 -o $@ $< -framework Foundation -framework IOBluetooth

clean:
	rm -rf build

.PHONY: test palm bt-bridge clean
