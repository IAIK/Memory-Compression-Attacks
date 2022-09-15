#!/usr/bin/env python3

from pathlib import Path
import random

TESTFILES_DIR = "./testfiles"
Path(TESTFILES_DIR).mkdir(parents=True, exist_ok=True)

alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&\\'()*+,-./:;<=>?@[\\]^_`{|}~ "

def generatePrintableTestfile(n):
    testfile = ""
    for _ in range(n * 1024):
        testfile += ''.join(random.sample(alphabet, 1))
    with open("{}/printable_memory{}".format(TESTFILES_DIR, n), "w") as f:
        f.write(testfile)

for i in [1, 2, 4, 8, 16, 32, 48, 64]:
    generatePrintableTestfile(i)