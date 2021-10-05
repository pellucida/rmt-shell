
OBJ= access.o util.o cmd_extend.o parse_args.o rmt_mtio.o

RMT_OBJ=rmt.o
CFLAGS= -I. -c -g -DHAVE_MTIO_H
#CFLAGS= -I. -c -g 
LDFLAGS= -g

CC=cc
LD=$(CC)

rmt:	$(RMT_OBJ) $(OBJ) 
	$(LD) $(LDFLAGS) -o rmt $(RMT_OBJ) $(OBJ) 

clean:
	rm -rf $(OBJ) $(RMT_OBJ) 

distclean:	clean
	rm -f rmt

access.o: access.c util.h
cmd_extend.o: cmd_extend.c constant.h util.h rmt.h access.h cmd_extend.h
rmt.o: rmt.c constant.h util.h access.h rmt.h cmd_extend.h
util.o: util.c constant.h util.h
parse_args.o: parse_args.c util.h
rmt_mtio.o: rmt_mtio.c constant.h rmt.h
