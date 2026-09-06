CC ?= cc
CFLAGS ?= -O3 -pipe -std=c11 -Wall -Wextra -Wpedantic -Werror -DNDEBUG
LDFLAGS ?=
TARGET := vsh
BENCH := vsh-bench

.PHONY: all clean check bench
all: $(TARGET) $(BENCH)

$(TARGET): vsh.c
	$(CC) $(CFLAGS) vsh.c -o $@ $(LDFLAGS)

$(BENCH): bench.c
	$(CC) $(CFLAGS) bench.c -o $@ $(LDFLAGS)

check: all
	printf 'printf ok\\n' | ./$(TARGET)
	./$(BENCH) 100000

bench: $(BENCH)
	./$(BENCH) 10000000

clean:
	rm -f $(TARGET) $(BENCH)
