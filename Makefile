CC = clang
CFLAGS = -Wall -Wextra -O3
LDFLAGS = -lm

ifeq ($(shell uname -s),Darwin)
	CFLAGS += -Wno-deprecated-declarations
endif

SRCS = tokenizer.c bytecode.c main.c
OBJS = $(SRCS:.c=.o)
TARGET = jsc

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tokenizer.o: tokenizer.c tokenizer.h
	$(CC) $(CFLAGS) -c $< -o $@

bytecode.o: bytecode.c bytecode.h
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c tokenizer.h bytecode.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) *.class

debug: CFLAGS += -g -DDEBUG
debug: clean all

sse: CFLAGS += -msse2 -msse3 -mssse3 -msse4.1 -msse4.2
sse: clean all

avx: CFLAGS += -mavx -mavx2
avx: clean all

avx512: CFLAGS += -mavx512f -mavx512vl -mavx512bw -mavx512dq
avx512: clean all

test: $(TARGET)
	./$(TARGET)

format:
	find . -type f \( -name "*.c" -o -name "*.h" \) -exec clang-format -i {} +

.PHONY: all clean debug sse avx avx512 test
