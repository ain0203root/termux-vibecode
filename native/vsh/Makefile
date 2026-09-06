CC ?= cc
CFLAGS ?= -O3 -pipe -std=c11 -Wall -Wextra -Wpedantic -DNDEBUG
LDFLAGS ?=
TARGET := vsh

.PHONY: all clean check
all: $(TARGET)

$(TARGET): vsh.c
	$(CC) $(CFLAGS) vsh.c -o $@ $(LDFLAGS)

check: $(TARGET)
	printf 'printf ok\\n' | ./$(TARGET)

clean:
	rm -f $(TARGET)
