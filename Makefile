# Compiler
CC = gcc

# Compiler flags
CFLAGS = -g -Wall -std=c99 -fsanitize=address -fsanitize=undefined

# Source files
SRCS = rpsd.c player.c

# Header files
HEADERS = player.h

# Object files
OBJS = rpsd.o player.o

# Executable name
TARGET = rpsd

# Default build
all: $(TARGET)

# Link object files into final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile each .c file into a .o file
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)