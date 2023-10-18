CC=gcc

EDCFLAGS=-Wall -O2 -I include/ $(CFLAGS)
EDLDFLAGS= -Llib/ -lm -lpthread -lVmbC $(LDFLAGS)

SRC = examples/main.c src/alliedcam.c

all: run alliedcam

run: alliedcam
	LD_LIBRARY_PATH=lib/:$(LD_LIBRARY_PATH) ./alliedcam.out

alliedcam: $(SRC)
	$(CC) $(EDCFLAGS) $(SRC) -o alliedcam.out $(EDLDFLAGS)

clean:
	rm -f alliedcam.out