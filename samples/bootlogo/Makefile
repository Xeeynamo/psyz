TARGET = bootlogo
TYPE = ps-exe
CFLAGS = -std=gnu11
CPPFLAGS += -DPSYZ_TITLE='"Sample: bootlogo testing ground"'

SRCS = $(TARGET).c \
../../psyz/src/psyz/bootlogo.c \
../../nugget/common/crt0/crt0.s \

include ../../psyz/psx.mk
