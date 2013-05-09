CROSS = mipsel-linux-
CC  = $(CROSS)gcc
CPP = $(CROSS)g++
LD  = $(CROSS)gcc

SRCS := main

TARGET=$(basename $(SRCS))
OBJS=$(addsuffix .o, $(basename $(SRCS)))
DEPS=$(addsuffix .d, $(basename $(SRCS)))

CFLAGS  += -Wall -Werror
CFLAGS  += -g
CFLAGS  += -I$(PWD)/../include

$(warning ${CFLAGS})

.PHONY: all
all: fshmem

fshmem: main.o
	$(QUIET)echo "  [LINK]    $@"
	$(LD) -o $@ $(CFLAGS) $^ $(LDFLAGS)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean::
	$(Q)$(foreach target,$(TARGET),rm -f *.o;)

