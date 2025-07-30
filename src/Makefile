CC = gcc
CFLAGS = -Wall -Wextra -g
SRCS = main.c png_io.c image.c utils.c 
HEADERS = png_io.h image.h utils.h include.h
TARGET = main 

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean: rm -f $(TARGET)
