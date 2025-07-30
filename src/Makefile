CC = gcc
CFLAGS = -Wall -Wextra -g
SRCS = main.c png_io.c 
HEADERS = png_io.h 
TARGET = main 

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean: rm -f $(TARGET)
