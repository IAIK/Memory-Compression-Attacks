## Fuzzing for edge cases in memory-compression algorithms

The fuzzer uses an evolutionary algorithm to find and amplify inputs to compression algorithms that trigger edge cases when the attacker input matches a given secret in the payload.
It generates memory layouts with different entropy, input size, and alignment of the guesses for the secret leading to measurable differences in decompression times. In each iteration, samples with the highest latency difference are used as input to generate newer layouts.

### Setup
The fuzzer is implemented as a python script with a few dependencies.
Install them with:

`$ python3 -m pip install tqdm`

The fuzzer uses a test program to evaluate different compression algorithms (zlib, zstd, lzo, fastlz, lz4, pglz).
Run `make <algorithm>` to compile test program for the `algorithm` needed. E.g., `make pglz`. Set the right `<ALGORITHM>_SRC` in the makefile to point to the compression library folder.

### Running 
The fuzzer takes different arguments:
```
$ python3 fuzzer.py --help                                                                                                                                                                                
usage: fuzzer.py [-h] [-n NUMBER] [-c CPU] -f FINDINGS_FOLDER [-l LIBRARY]
                 [-e EPOCHS] [-s SEED] [-x SECRET] [-r REPS]
                 [-g NUMBER_OF_GUESSES]

optional arguments:
  -h, --help            show this help message and exit
  -n NUMBER, --number NUMBER
                        number of executions per epoch
  -c CPU, --cpu CPU     cpu for taskset
  -f FINDINGS_FOLDER, --findings_folder FINDINGS_FOLDER
                        findings folder
  -l LIBRARY, --library LIBRARY
                        library to fuzz
  -e EPOCHS, --epochs EPOCHS
                        number of epochs for the genetic algorithm
  -s SEED, --seed SEED  seed for reproducibility
  -x SECRET, --secret SECRET
                        secret
  -r REPS, --reps REPS  reps
  -g NUMBER_OF_GUESSES, --number_of_guesses NUMBER_OF_GUESSES
                        number_of_guesses
```

For example `python3 ./fuzzer.py -c 3 -e 100 -n 1000 -f findings -l pglz -g 26 -s 0 ` runs the fuzzer on core 3, for 100 epochs, each epoch of 1000 samples. It saves the results and summary of the results in the `findings` folder and fuzzes the `PGLZ` decompression algorithm, with 26 guesses per test. This using a random seed of 0 for reproducibility.
Notice that the `-g` argument has to match the value of `NUMBER_OF_GUESSES` in `test.c`.

### Details

The input evolution uses templates of random memory included in the folders `./testfiles` and `./testfiles_printable` to start from high entropy memory of different sizes.
It then varies the entropy of the input to increase its compressibility factor while moving around the guesses of the attacker to trigger edge cases.

For each epoch and for each sample in the population, the fuzzer forwards the layout to be tested to the test program.
The test program takes as input the parameters to generate the memory layout selected by the fuzzer, inserts the secret and the guesses, compresses it and measures the decompression time.
The average decompression time with the right and wrong guesses is reported.

The score to each sample of the population is given by the difference in decompression time for the testcase with the right guess vs the wrong one.
At the end of each epoch, only the best 5% of the population is maintained and randomly mutated to generate 70% of the new population. 25% of the new population is generated completely randomly to combat local minima.

The `[...]-summary.csv` file in the findings folder contains the best layout configuration for each epoch.

### Adding a new libary

To add a new library simply add a function in the `test.c` file in the form of:

```c
#ifdef LIBRARY
size_t testLibrary(uint8_t* in, long unsigned int comprLen, long unsigned int uncomprLen,int level) {
    - compress `in`
    - return the time taken to decompress `in`
}
#endif
```

And add a rule in the `Makefile` to generate the program `test_library`.


Sample call:
```
make custom
taskset -c 3 ./test_zlib ./testfiles/random_memory64 0 41077 3 6 6 10 2

SED;293430
SEE;290793
SEF;303059
SEG;299743
SEH;305037
SEI;309249
SEJ;309971
SEK;319303
SEL;321730
SEM;320311
SEN;321264
SEO;320337
SEP;332226
SEC;208620
SER;334137
SES;333523
SET;332414
SEU;333280
SEV;332437
SEW;332560
SEX;332692
SEY;333589
SEZ;345739
SE[;346587
SE\;354671
SE];346532
Recovered secret:SEC
Fastest:208620
Second fastest:290793
diff fastest to second fastest:82173
...

```
