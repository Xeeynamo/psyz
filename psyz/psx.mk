CPPFLAGS += -I../../psyz/include  -I../../nugget/psyq/include
LDFLAGS += -L../../nugget/psyq/lib
LDFLAGS += -Wl,--start-group
LDFLAGS += -lapi -lc -lc2 -lcard -lcd -lcomb -letc -lgpu -lgs -lgte -lgun -lhmd
LDFLAGS += -lmath -lmcrd -lmcx -lpad -lpress -lsio -lsnd -lspu -ltap
LDFLAGS += -Wl,--end-group

include ../../nugget/common.mk
