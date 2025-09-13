# colors
RESET	:= \033[0m
RED		:= \033[31m
GREEN	:= \033[32m
BLUE	:= \033[34m
BOLD	:= \e[1m

# body
CC		= gcc
CFLAGS	= -Wall -Wextra -g -Iinclude
LDFLAGS	= -lz -lm # lz -> for zlib, lm -> for math

TARGET	= png

SRCDIR  = src
OBJDIR  = obj

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@printf "%b\n" "$(GREEN)===> Linking...$(RESET)"
	$(CC) $^ -o $@ $(LDFLAGS)
	@printf "%b\n" "$(GREEN)===> Build finished successfully: $(RESET)✅"
	@printf "%b\n" "$(BLUE)            vvvvvvvvv$(RESET)"
	@printf "%b\n" "$(BLUE)            ==>$(RED)$(BOLD)$(TARGET)$(RESET)$(BLUE)<==$(RESET)"
	@printf "%b\n" "$(BLUE)            ^^^^^^^^^$(RESET)"

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	@printf "%b\n" "$(BLUE)==> Compiling 🚀 $<...$(RESET)"
	$(CC) $(CFLAGS) -c $< -o $@
	@printf "%b\n" "$(GREEN)Done!$(RESET) ✅"

.PHONY: clean
clean:
	@printf "%b\n" "$(RED)==> Cleaning up build files...$(RESET)"
	rm -rf $(OBJDIR) $(TARGET)
	@printf "%b\n" "$(GREEN)==> Clean complete!$(RESET)✅"
