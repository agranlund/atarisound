
OUTBIN  = $(OUTDIR)/midi.jam

ROOTDIR = ../../

SYMFLAGS = -Wl,--traditional-format

OBJS = 	$(JAM_OBJS) \
		$(LGCC_OBJ) \
		main.o \
		md_midi.o 

CPUFLAGS = -m68000

#OPTS = -DDEBUG
OPTS =

SFLAGS = 

CFLAGS = \
	$(LIBCMINI_INC) \
	$(LIBCMINI_CFL) \
	$(OPTS) \
	-I$(ROOTDIR)ext/jam \
	-std=c99 \
	-fomit-frame-pointer -fno-exceptions \
	-Wall -Wno-multichar -Wno-unused-but-set-variable -Wno-unused-variable

LFLAGS = \
	$(LIBCMINI_LIB) $(LIBCMINI_LFL) \
 	-Wl,--traditional-format \
	-Wl,-Map,mapfile


## Rules
.PHONY: clean all

all: $(OUTBIN)

$(OUTBIN): common Makefile $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LFLAGS) -o $(OUTBIN)
	$(STRIP) $(OUTBIN)

clean:
	rm -f *.o
	rm -f mapfile
#	rm -f $(OUTBIN)


include ../Makefile.common


