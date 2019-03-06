EXEC = fat
SRC = fat.c
OBJS := \
	fat.o \
	# intentionally left blank so last line can end in a \

CC	=	gcc
CFLAGS	=	-g -Og -Wall $(PKGFLAGS)

PKGFLAGS	=	`pkg-config fuse --cflags --libs`

# gcc -Wall -o fusexmp fusexmp.c `pkg-config fuse --cflags --libs`

all: $(OBJS)
	$(CC) -o $(EXEC) $(SRC) $(CFLAGS)

.PHONY: all clean

clean:
	rm $(EXEC) $(OBJS)
