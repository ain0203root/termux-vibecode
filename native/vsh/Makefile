CC ?= cc
CFLAGS ?= -O3 -pipe -std=c11 -Wall -Wextra -Wpedantic -DNDEBUG
LDFLAGS ?=
TARGET := vsh
BENCH := vsh-bench

.PHONY: all clean check bench
all: $(TARGET)

$(TARGET): vsh.c
	$(CC) $(CFLAGS) vsh.c -o $@ $(LDFLAGS)

$(BENCH): bench.c
	$(CC) $(CFLAGS) bench.c -o $@ $(LDFLAGS)

check: $(TARGET)
	printf 'printf ok\\n' | ./$(TARGET)

bench: $(BENCH)
	./$(BENCH) 10000000

clean:
	rm -f $(TARGET) $(BENCH)
