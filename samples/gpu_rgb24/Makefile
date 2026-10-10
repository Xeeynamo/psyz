TARGET = rgb24
TYPE = ps-exe
CFLAGS = -std=gnu11
NUGGET ?= ../../nugget
PSYQ ?= $(NUGGET)/psyq
CPPFLAGS += -I../../psyz/include -I$(PSYQ)/include
LDFLAGS += -L$(PSYQ)/lib -Wl,--start-group -letc -lgpu -lapi -lc -lc2 -Wl,--end-group
SRCS = rgb24.c
OBJS = crt0.o
include $(NUGGET)/common.mk

crt0.o: $(NUGGET)/common/crt0/crt0.s
	$(CC) $(ARCHFLAGS) -I$(ROOTDIR) -g -c -o $@ $<
