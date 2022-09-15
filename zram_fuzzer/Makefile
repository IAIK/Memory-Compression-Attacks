CC ?= gcc
CFLAGS += -fno-omit-frame-pointer
DFLAGS += -gdwarf-3


all: zram

zram: test.c
	${CC} ${CFLAGS} -DZRAM test.c -o test_zram

random:
	mkdir -p testfiles
	dd if=/dev/urandom of=testfiles/random_memory${MEM_SIZE} bs=$(MEM_SIZE)K count=1

randomAll:
	mkdir -p testfiles
	dd if=/dev/urandom of=testfiles/random_memory1 bs=1K count=1
	dd if=/dev/urandom of=testfiles/random_memory2 bs=2K count=1
	dd if=/dev/urandom of=testfiles/random_memory4 bs=4K count=1
	dd if=/dev/urandom of=testfiles/random_memory8 bs=8K count=1
	dd if=/dev/urandom of=testfiles/random_memory16 bs=16K count=1
	dd if=/dev/urandom of=testfiles/random_memory32 bs=32K count=1
	dd if=/dev/urandom of=testfiles/random_memory48 bs=48K count=1
	dd if=/dev/urandom of=testfiles/random_memory64 bs=64K count=1

clean:
	rm -f test_*
