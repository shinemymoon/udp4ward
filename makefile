CC= gcc

LDFLAGS = -Wall
LDLIBS = 

OBJS = udp4ward.o
OBJS += error.o
OBJS += init.o

.PHONY: all
.PHONY: clean

all: ud

ud: $(OBJS)
	$(CC) $(LDFLAGS) $(LDLIBS) $(OBJS) -o $@ 

clean:
	rm -f *.o
	rm -f ud
