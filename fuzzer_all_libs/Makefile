ZLIB_SRC := ../libraries/zlib
ZLIB_NG_SRC := ../libraries/zlib-ng
ZSTD_SRC := ../libraries/zstd-facebook
FASTLZ_SRC := ../libraries/FastLZ
LZO_SRC := ../libraries/lzo
LZ4_SRC := ../libraries/lz4
PGLZ_SRC := ../pglz-compression/pglz

CC ?= gcc
CFLAGS += -fno-omit-frame-pointer
DFLAGS += -gdwarf-3


all: test.c
	mkdir -p findings
	${CC} ${CFLAGS} ${DFLAGS} test.c -o test -Iinclude -lz

zlib: ${ZLIB_SRC}/libz.so

${ZLIB_SRC}/libz.so: ${ZLIB_SRC}/Makefile ${ZLIB_SRC}/zutil.h ${ZLIB_SRC}/deflate.c
	cd ${ZLIB_SRC} && make



zlib-ng: ${ZLIB_NG_SRC}/libz.so
zstd_lib: ${ZSTD_SRC}/lib/libzstd.so
lzo_lib: ${LZO_SRC}/src/liblzo2.la

lz4_lib: ${LZ4_SRC}/lib/liblz4.so

${ZLIB_SRC}/Makefile:
	cd ${ZLIB_SRC} && ./configure

${ZLIB_NG_SRC}/Makefile:
	cd ${ZLIB_NG_SRC} && ./configure

${ZLIB_NG_SRC}/libz.so: ${ZLIB_NG_SRC}/Makefile ${ZLIB_NG_SRC}/zutil.h ${ZLIB_NG_SRC}/deflate.c
	cd ${ZLIB_NG_SRC} && make

${LZ4_SRC}/lib/liblz4.so: ${LZ4_SRC}/Makefile
	cd ${LZ4_SRC} && make

custom: zlib test.c
	${CC} ${CFLAGS} ${DFLAGS} -DZLIB -Wl,-rpath "${ZLIB_SRC}" test.c -o test_zlib -Iinclude -L"${ZLIB_SRC}" -lz

custom-ng: zlib-ng test.c
	${CC} ${CFLAGS} ${DFLAGS} -DZLIBNG -Wl,-rpath "${ZLIB_NG_SRC}" -I ${ZLIB_NG_SRC} test.c -o test_zlib_ng -DZLIB_NG=1 -L"${ZLIB_NG_SRC}" -lz-ng

zstd: zstd_lib test.c
	${CC} ${CFLAGS} -Wl,-rpath "${ZSTD_SRC}/lib" -DZSTD test.c -o test_zstd -Iinclude -L"${ZSTD_SRC}/lib" -lzstd

lzo: lzo_lib test.c
	${CC} ${CFLAGS} -Wl,-rpath "${LZO_SRC}/src" -DLZO test.c -o test_lzo -Iinclude -L"${LZO_SRC}/src" -llzo2

fastlz: test.c
	${CC} ${CFLAGS} -DFASTLZ test.c ${FASTLZ_SRC}/fastlz.c -o test_fastlz

lz4: lz4_lib test.c
	${CC} ${CFLAGS} -Wl,-rpath "${LZ4_SRC}/lib" -DLZ4 test.c -o test_lz4 -Iinclude -L"${LZ4_SRC}/lib" -llz4

pglz: ${PGLZ_SRC}/pg_lzcompress.c test.c
	${CC} ${CFLAGS} -DPGLZ test.c -I${PGLZ_SRC} ${PGLZ_SRC}/pg_lzcompress.c -o test_pglz


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
