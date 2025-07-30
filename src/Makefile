CC = gcc
CFLAGS = -Wall -Wextra -g -I./include
LDFLAGS = -lz -lm

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)

png_processor: $(OBJS)
	$(CC) $(OBJS) -o $@ $(CFLAGS) 

clean: 
	rm -f src/*.o png_processor
