
OBJ= access.o util.o cmd_extend.o parse_args.o rmt_mtio.o

ACCESS_FILE	= /etc/opt/rmt/access
RMT_OBJ		= rmt.o
CFLAGS		= -I. -c -g -DHAVE_MTIO_H

# Use an access file
CFLAGS	+= -DUSE_ACCESS
CFLAGS	+= -DRESTRICT_ACCESS=\"$(ACCESS_FILE)\"

# Implement builtin commands - only uname will be generally useful
CFLAGS	+= -DRMT_MKDIR
CFLAGS	+= -DRMT_STAT
CFLAGS	+= -DRMT_UNAME

LDFLAGS= -g

CC=cc
LD=$(CC)

rmt:	$(RMT_OBJ) $(OBJ) 
	$(LD) $(LDFLAGS) -o rmt $(RMT_OBJ) $(OBJ) 

clean:
	rm -rf $(OBJ) $(RMT_OBJ) 

distclean:	clean
	rm -f rmt

$(OBJ): Makefile
access.o: access.c util.h
cmd_extend.o: cmd_extend.c util.h rmt.h access.h cmd_extend.h
rmt.o: rmt.c util.h access.h rmt.h cmd_extend.h
util.o: util.c util.h
parse_args.o: parse_args.c util.h
rmt_mtio.o: rmt_mtio.c util.h rmt.h
