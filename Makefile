MAKEFLAGS += -j4 -r
ARCH = -march=native
CC = clang
FLAGS = $(ARCH) -O3 -ffast-math -MMD -MP -Iinc/
FLAGS += -Rpass-missed=inline
CFLAGS = $(FLAGS) -std=c2y

LD = clang -fuse-ld=lld
LDFLAGS = $(ARCH) -flto
LDFLAGS += -Wl,-X,-s,-S,--as-needed,--gc-sections,--icf=all
LDFLAGS += -lSDL3 -lvulkan -lm

SRCS = $(wildcard src/*)
OBJS = $(SRCS:src/%=obj/%.o)
DEPS = $(wildcard obj/*.d)

TARGET = obj/main

all: $(TARGET)
	$(TARGET)

build: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

obj/%.c.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: obj/%.spv | obj
	llvm-objcopy -I binary -O elf64-x86-64 $< $@

obj/%.spv: src/% | obj
	glslc -O -Iinc/ -MD -std=460 -c $< -o $@

obj:
	mkdir obj

-include $(DEPS)

clean:
	rm -rf obj/*

.PHONY: all build clean

