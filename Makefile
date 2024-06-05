# Define the compiler
CC = gcc

# Define compiler flags
CFLAGS = -Wall -Wextra -pedantic -std=c11

# Define the target executable
TARGET = shell

# Define the source files
SRCS = shell.c
HEADERS = shell.h

# Define the object files
OBJS = $(SRCS:.c=.o)

# Rule to build the target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Rule to build object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to clean the build
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)

# Rule to run the shell
.PHONY: run
run: $(TARGET)
	./$(TARGET)
