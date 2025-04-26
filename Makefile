CC = clang
CFLAGS = -Wall -Wextra -O3 # curr: ~85kB strip --strip-all: ~76kB
# CFLAGS = -Os -ffunction-sections -fdata-sections -Wl,--gc-sections -s -flto -fuse-ld=lld # curr: ~29.1kB strip --strip-all: no change
# CFLAGS = -Oz -flto -fuse-ld=lld -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden -fomit-frame-pointer -fno-stack-protector -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro,-z,now # curr: ~25.1kB strip --strip-all: no change
LDFLAGS = -lm

JNI_INCLUDE = -I/usr/lib/jvm/java-17-openjdk-amd64/include -I/usr/lib/jvm/java-17-openjdk-amd64/include/linux
CFLAGS += $(JNI_INCLUDE)
LDFLAGS += -L/usr/lib/jvm/java-17-openjdk-amd64/lib/server -ljvm

ifeq ($(shell uname -s),Darwin)
	CFLAGS += -Wno-deprecated-declarations
endif

SRCS = jsc_tokenizer.c jsc_bytecode.c jsc_engine.c main.c
OBJS = $(SRCS:.c=.o)
TARGET = jsc

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tokenizer.o: jsc_tokenizer.c jsc_tokenizer.h
	$(CC) $(CFLAGS) -c $< -o $@

bytecode.o: jsc_bytecode.c jsc_bytecode.h
	$(CC) $(CFLAGS) -c $< -o $@

engine.o: jsc_engine.c jsc_engine.h
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c jsc_tokenizer.h jsc_bytecode.h jsc_engine.h
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
	@export LD_LIBRARY_PATH=/usr/lib/jvm/java-17-openjdk-amd64/lib/server:$LD_LIBRARY_PATH && ./$(TARGET)

format:
	find . -type f \( -name "*.c" -o -name "*.h" \) -exec clang-format -i {} +

.PHONY: all clean debug sse avx avx512 test
