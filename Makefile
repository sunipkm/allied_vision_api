EDCFLAGS:= -O2 -std=gnu11 -I include/ -Wall $(CFLAGS)
EDLDFLAGS:= -L lib/ -lpthread -lm -lVmbC $(LDFLAGS) -Wl,-rpath lib/

CSRCS := $(wildcard src/*.c)
COBJS := $(patsubst %.c,%.o,$(CSRCS))
CDEPS := $(patsubst %.c,%.d,$(CSRCS))
LIBTARGET := liballiedcam.a

all: $(LIBTARGET) test

$(LIBTARGET): $(COBJS)
	ar -crs $(LIBTARGET) $(COBJS)

test: $(LIBTARGET)
	$(CC) $(EDCFLAGS) -Wno-unused-result examples/main.c $(LIBTARGET) -o alliedcam.out $(EDLDFLAGS)

-include $(CDEPS)

%.o: %.c Makefile
	$(CC) $(EDCFLAGS) -MMD -MP -o $@ -c $<

install:
	cti/VimbaUSBTL_Install.sh
	cti/VimbaGigETL_Install.sh

uninstall:
	cti/VimbaUSBTL_Uninstall.sh
	cti/VimbaGigETL_Uninstall.sh

.PHONY: clean

clean:
	rm -vf $(COBJS)
	rm -vf $(CDEPS)
	rm -vf $(LIBTARGET)
	rm -vf *.out

spotless: clean
	rm -rf doc

doc:
	doxygen .doxyconfig
