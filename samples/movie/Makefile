TARGET = movie
TYPE = ps-exe
CFLAGS = -std=gnu11
PSYQ ?= ../../nugget/psyq
CPPFLAGS += -I../../psyz/include -I$(PSYQ)/include
LDFLAGS += -L$(PSYQ)/lib -Wl,--start-group
LDFLAGS += -lapi -lc -lc2 -lcard -lcd -lcomb -lds -letc -lgpu -lgs -lgte -lgun -lhmd
LDFLAGS += -lmath -lmcrd -lmcx -lpad -lpress -lsio -lsnd -lspu -ltap
LDFLAGS += -Wl,--end-group
SRCS = movie.c ../../nugget/common/crt0/crt0.s
include ../../nugget/common.mk
