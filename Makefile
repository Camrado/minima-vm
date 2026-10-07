CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
TARGET = vm
SRC = src/main.c src/cpu.c src/asm.c src/isa.c

all: $(TARGET)

$(TARGET): $(SRC) src/vm.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
