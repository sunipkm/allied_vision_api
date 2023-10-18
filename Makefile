EDCFLAGS:= -O2 -std=gnu11 -I include/ -Wall $(CFLAGS)
EDLDFLAGS:= -L lib/ -lpthread -lm -lVmbC $(LDFLAGS)

CSRCS := $(wildcard src/*.c)
COBJS := $(patsubst %.c,%.o,$(CSRCS))
CDEPS := $(patsubst %.c,%.d,$(CSRCS))
LIBTARGET := liballiedcam.a

all: $(LIBTARGET) test

$(LIBTARGET): $(COBJS)
	ar -crs $(LIBTARGET) $(COBJS)

test: $(LIBTARGET)
	$(CC) $(EDCFLAGS) examples/main.c $(LIBTARGET) -o alliedcam.out $(EDLDFLAGS)
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(PWD)/lib ./alliedcam.out

-include $(CDEPS)

%.o: %.c Makefile
	$(CC) $(EDCFLAGS) -MMD -MP -o $@ -c $<

.PHONY: clean

clean:
	rm -vf $(COBJS)
	rm -vf *.out

spotless: clean
	rm -vf $(CDEPS)
	rm -vf $(LIBTARGET)
	rm -rf doc

doc:
	doxygen .doxyconfig