CC=gcc
CFLAGS= -Wall -Wextra -g -Iinclude
LDFLAGS= -lz -lm # lz -> for zlib, lm -> for math

TARGET=png

SRCDIR=src
OBJDIR=obj

SOURCES=$(wildcard $(SRCDIR)/*.c)
OBJECTS=$(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "==> Linking..."
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "==> Build finished successfully: $(TARGET)"

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	@echo "===> Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@echo "=> Cleaning up build files..."
	rm -rf $(OBJDIR) $(TARGET)
	@echo "=> Clean complete!"
