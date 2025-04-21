CC = clang
CFLAGS = -Wall -Wextra -O3

SRCS = tokenizer.c main.c
OBJS = $(SRCS:.c=.o)
TARGET = jsc

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tokenizer.o: tokenizer.c tokenizer.h
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c tokenizer.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

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

.PHONY: all clean debug sse avx avx512 test
