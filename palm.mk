# Run inside the palmdev image: builds the on-device NIO as a static library
# (fujinet-nio-lib's client calls + the NIO core + the NetLib socket layer),
# then FI apps from fujinet-palm linked against it instead of the serial lib.
#   /src      this repository        /nio-lib  fujinet-nio-lib
#   /apps     fujinet-palm (read only; apps are copied out before building)
CC      = m68k-palmos-gcc
AR      = m68k-palmos-ar
CFLAGS  = -Wall -O2 -palmos3.5 -DFN_MAX_PACKET_SIZE=512 -I/nio-lib/include \
          -I/nio-lib/include/palmos -I/src/core
OUT     = /src/build/palm
LIB     = $(OUT)/fujinet-nio-palmos-local.a
APPS   ?= FujiNetTest

LIB_SRCS = $(filter-out %/fn_transport_stream.c %/fn_session.c %/fn_slip.c, \
             $(wildcard /nio-lib/src/common/*.c))
SRCS     = $(LIB_SRCS) $(wildcard /src/core/*.c) /src/platform/palmos/nio_sock_palm.c \
           /src/transport/fn_transport_local.c
OBJS     = $(addprefix $(OUT)/obj/,$(notdir $(SRCS:.c=.o)))
VPATH    = /nio-lib/src/common /src/core /src/platform/palmos /src/transport

# SHARED=1: the apps link NioLib's client archive (fujinet-nio-lib's palmos-shared build) and use the installed
# NioLib, whose Network transport is this NIO; otherwise they carry the NIO themselves.
SHARED ?= 0
NIOLIB_BUILD ?= /nio-lib/build/palmos-shared-3.5
ifeq ($(SHARED),1)
APP_LIB = $(NIOLIB_BUILD)/libniolib-client.a
OUT := $(OUT)-shared
else
APP_LIB = $(LIB)
endif

all: $(APPS:%=$(OUT)/%.prc)

$(LIB): $(OBJS)
	rm -f $@
	@$(AR) rcs $@ $(OBJS)

$(OUT)/obj/%.o: %.c
	@mkdir -p $(OUT)/obj
	$(CC) $(CFLAGS) -c -o $@ $<

# Each app builds in a scratch copy, so fujinet-palm's own serial build is
# left alone.
$(OUT)/%.prc: $(if $(filter 1,$(SHARED)),,$(LIB)) FORCE
	@mkdir -p $(OUT)
	rm -rf /tmp/b && mkdir -p /tmp/b/apps
	cp -R /apps/common /tmp/b/common
	cp /apps/apps/app.mk /tmp/b/apps/
	cp -R /apps/apps/$* /tmp/b/apps/$*
	$(MAKE) -C /tmp/b/apps/$* clean
	$(MAKE) -C /tmp/b/apps/$* NIO_LIB=/nio-lib LIBS=$(APP_LIB)
	m68k-palmos-size /tmp/b/apps/$*/$*
	cp /tmp/b/apps/$*/$*.prc $@

FORCE:
.PHONY: all FORCE
