#!/usr/bin/env python3
import subprocess
import uuid
import os
import random
import sys
import argparse
import csv
import time
from tqdm import tqdm
import copy
import signal

#TODO:
#generate layouts multi-processed in a single script execution

# default config
config = {
    "number_of_executions": 1,
    "cpu": -1,
    "findingsFolder": ""
}

# 00 00 11 0e 00 00 
PREFIX_LEN = 10
OFFSET_SECRET = 8
OFFSET_GUESS = 0x7c

CONTROLLED_AREA_SIZE = 4096 - 256 - 0x7c

findings = []
timestamp = ""

def getTestfiles(path):
    testfiles = []
    for file in os.listdir(path):
        testfiles.append([path + os.sep + file, os.stat(path + os.sep + file).st_size])
    return testfiles
testfiles = None
# TODO: how & where to save testcases?
# TODO: how to analyze

class Testcase():
    "A testcase in the population"
    memFile: str
    offsetSecret: int
    offsetGuess: int
    guessLength: int
    compressionLevel: int
    numberOfTimesOfGuess: int
    random_modulus: int
    number_of_fill_pages: int
    score: int

    def __init__(self):
        global guess_len
        # Pick only 4K random file from testfiles (should only have one candidate)
        assert(len(testfiles) == 1)
        self.mem = random.choices(testfiles)
        self.memSize = CONTROLLED_AREA_SIZE#self.mem[0][1]
        self.memFile = self.mem[0][0]
        self.score = 0

        # length of guess
        self.lengthSecret = PREFIX_LEN + guess_len + 1

        #self.numberOfTimesOfGuess = random.randint(1, 20)
        self.random_modulus = random.randint(1,256)

        # placement of secret
        # placement of guess

        self.offsetSecret = OFFSET_SECRET#random.randint(0, self.memSize - self.lengthSecret) # TODO how random? or predfined steps?
        self.offsetGuess = random.randint(OFFSET_GUESS, self.memSize - self.lengthSecret)
        self.numberOfTimesOfGuess = random.randint(1,int((self.memSize-self.offsetGuess)/self.lengthSecret) + 1)
        
        self.guessLength = guess_len#random.randint(6, 6)
        self.compressionLevel = 8#random.randint(1, 9)
        self.number_of_fill_pages = 0#random.randint(0,10)

        # retry for valid args 
        while (abs(self.offsetSecret - self.offsetGuess) < self.lengthSecret or (self.offsetGuess + (self.lengthSecret * self.numberOfTimesOfGuess) > self.memSize)):
            self.offsetSecret = OFFSET_SECRET#random.randint(0, self.memSize - self.lengthSecret)
            self.offsetGuess = random.randint(OFFSET_GUESS, self.memSize - self.lengthSecret)
        
    def mutate(self):
        global guess_len        
        new_testcase = copy.deepcopy(self)
        # choose a random file from testfiles (content of random bytes in mem?)
        new_testcase.score = 0
        new_testcase.lengthSecret = PREFIX_LEN + guess_len + 1

        # bitmap to represent which field should be mutated
        field_to_mutate = random.randint(1, 0xffff)

        #Fixed Mem-Size and File.
        #Fixed memory4
        assert(len(testfiles) == 1)
        new_testcase.mem = random.choices(testfiles)
        new_testcase.memSize = CONTROLLED_AREA_SIZE#new_testcase.mem[0][1]
        new_testcase.memFile = new_testcase.mem[0][0]

        #Fixed guess_len
        new_testcase.guessLength = guess_len#random.randint(6, 6)

        #Fixed compression level
        new_testcase.compressionLevel = 8 #random.randint(1, 9)

        #Fixed offset secret
        new_testcase.offsetSecret = OFFSET_SECRET#random.randint(0, new_testcase.memSize - new_testcase.lengthSecret) # TODO how random? or predfined steps?

        #Mutate offset guess
        if (field_to_mutate >> 0) & 1 or new_testcase.offsetGuess > new_testcase.memSize - new_testcase.lengthSecret:
            new_testcase.offsetGuess = random.randint(OFFSET_GUESS, new_testcase.memSize - new_testcase.lengthSecret)
        #Mutate number of guesses amd random modulus
        if (field_to_mutate >> 1) & 1:
            #new_testcase.numberOfTimesOfGuess = random.randint(1, 20)
            #new_testcase.numberOfTimesOfGuess = random.randint(1,int(new_testcase.memSize/new_testcase.lengthSecret) - 1)
            self.numberOfTimesOfGuess = random.randint(1,int((self.memSize-self.offsetGuess)/self.lengthSecret) + 1)

        if (field_to_mutate >> 2) & 1:
            new_testcase.random_modulus = random.randint(1,256)        

        #Fixed num_fill_pages
        new_testcase.number_of_fill_pages = 0

        # always retry for valid args 
        while (abs(new_testcase.offsetSecret - new_testcase.offsetGuess) < new_testcase.lengthSecret or \
          (new_testcase.offsetGuess + (new_testcase.lengthSecret * new_testcase.numberOfTimesOfGuess) > new_testcase.memSize)):
            new_testcase.offsetSecret = OFFSET_SECRET #random.randint(0, new_testcase.memSize - new_testcase.lengthSecret)
            new_testcase.offsetGuess = random.randint(OFFSET_GUESS, new_testcase.memSize - new_testcase.lengthSecret)
        return new_testcase

    def to_args(self):
        if(config["library"] == "zram"):                        
            if config["cpu"] == -1:
                
                return ("sudo","cgexec","-g","memory:"+config["cgroup"], "./test_" + config["library"], self.memFile, str(self.offsetSecret), str(self.offsetGuess), str(self.guessLength), str(self.compressionLevel), str(self.numberOfTimesOfGuess),str(self.random_modulus),str(self.number_of_fill_pages))
            else:
                return ("taskset", "--cpu-list", str(config["cpu"]), "sudo","cgexec","-g","memory:"+config["cgroup"], "./test_" + config["library"], self.memFile, str(self.offsetSecret), str(self.offsetGuess), str(self.guessLength), str(self.compressionLevel), str(self.numberOfTimesOfGuess),str(self.random_modulus),str(self.number_of_fill_pages))
            
        if config["cpu"] == -1:
            # print("Not executed on isolated core...")
            return ("./test_" + config["library"], self.memFile, str(self.offsetSecret), str(self.offsetGuess), str(self.guessLength), str(self.compressionLevel), str(self.numberOfTimesOfGuess),str(self.random_modulus),str(self.number_of_fill_pages))
        else:
            return ("taskset", "--cpu-list", str(config["cpu"]), "./test_" + config["library"], self.memFile, str(self.offsetSecret), str(self.offsetGuess), str(self.guessLength), str(self.compressionLevel), str(self.numberOfTimesOfGuess),str(self.random_modulus),str(self.number_of_fill_pages))
   
def initConfig(args):
  if args.number:
    config['number_of_executions'] = int(args.number)
  if args.cpu:
    config['cpu'] = int(args.cpu)
  if args.findings_folder:
    config["findingsFolder"] = args.findings_folder
  if args.library:
    config["library"] = args.library
  if args.cgroup:
    config["cgroup"] = args.cgroup
    
def saveFindings(filename, args, output):
    if not os.path.exists(config["findingsFolder"]):
      os.makedirs(config["findingsFolder"])
    fileFindings = open(os.path.join(config["findingsFolder"], timestamp + "-output"), "ab") 
    fileFindings.write(bytes("###" + os.linesep + filename + os.linesep, "utf-8")) 
    fileFindings.write(bytes(";".join(args) + os.linesep, "utf-8")) 
    fileFindings.write(output) 
    fileFindings.close()

def parseFindings(filename, args, output,secret='S',reps=1,number_of_guesses=10):
    outputStr = output.decode("utf-8")
    lines = outputStr.split(os.linesep)
    # 4 lines are recovered,fastest,second,diff first to sec
    total_lines = number_of_guesses + 4
    
    if len(lines) < reps*total_lines or (':' not in lines[10]):
        print(args)
        print(outputStr)
    
    #print(lines)
    diff_avg = 0
    for run in range(0,reps):
        recovered = lines[run * total_lines + (total_lines-4)].split(":")[1]
        #print(lines[run * total_lines + (total_lines-4)])
        diff = int(lines[run * total_lines + (total_lines-1)].split(":")[1])
        #print(lines[run * total_lines + (total_lines-1)])
        argsLen = len(args)
        if recovered == secret:
            diff_avg = diff_avg + diff 
        else:
            #once wrong so nope
            return 0

    findings.append({
        "filename": filename, 
        "isolatedCPU": config["cpu"] != -1,
        "memFile": args[argsLen-8],
        "offsetSecret": args[argsLen-7],
        "offsetGuess": args[argsLen-6],
        "guessLength": args[argsLen-5],
        "compressionLevel": args[argsLen-4],
        "numberOfTimesOfGuess": args[argsLen-3],
        "random_modulus": args[argsLen-2],
        "number_of_fill_pages": args[argsLen-1],
        "recovered": recovered, 
        "diff": diff_avg / reps
        })
    # return score
    if recovered == secret:
        return diff_avg
    else:
        return 0

def signal_handler(sig, frame):
    visualize()

    sys.exit(0)

def visualize():
    sortedByDiff = sorted(findings, key=lambda x: x["diff"], reverse=True)

    # TODO:?
    # filter by file
    # filter by length of guess
    # filter by placement of secret
    # filter by placement of guess
    # draw some plots 

    csv_file = os.path.join(config["findingsFolder"], timestamp + "-summary.csv")
    csv_columns = ["filename","isolatedCPU", "memFile", "offsetSecret",
        "offsetGuess","guessLength","compressionLevel","numberOfTimesOfGuess","random_modulus","number_of_fill_pages","recovered","diff"]
    
    with open(csv_file, 'w') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=csv_columns, delimiter=";")
        writer.writeheader()
        for data in sortedByDiff:
            try:
                writer.writerow(data)
            except IOError:
                print("I/O error")

POPULATION_RETENTION = 0.05
POPULATION_CHILDS = 0.70
POPULATION_RANDOMIC = 0.25
# REMINDER: should sum to 1.0

def main():
    global timestamp
    global guess_len
    global testfiles
    signal.signal(signal.SIGINT, signal_handler)

    timestamp = time.strftime("%Y%m%d-%H%M%S")
    parser = argparse.ArgumentParser()
    parser.add_argument('-n', '--number', help = 'number of executions', required=False)
    parser.add_argument('-c', '--cpu', help = 'cpu for taskset', required=False)
    parser.add_argument('-f', '--findings_folder', help='findings folder', required=True)
    parser.add_argument('-l', '--library', help='library to fuzz', required=False, default='zstd')
    parser.add_argument('-e', '--epochs', help='number of epochs for the genetic algorithm', required=False, type=int, default=1)
    parser.add_argument('-s', '--seed', help='seed for reproducibility', required=False, type=int)
    parser.add_argument('-x', '--secret', help='secret', required=False, type=str,default="SECRET")
    parser.add_argument('-r', '--reps', help='reps', required=False, type=int,default=1)
    parser.add_argument('-g', '--cgroup', help='cgroup name', required=False,default='small_mem')
    parser.add_argument('-ng', '--number_of_guesses', help='number_of_guesses', required=False, type=int,default=10)
    
    cmdline = parser.parse_args()

    if cmdline.library == 'pglz':
        testfiles = getTestfiles("./testfiles_printable")
    else:
        testfiles = getTestfiles("./testfiles")
    assert testfiles is not None and len(testfiles) > 0

    if cmdline.seed is not None:
        random.seed(cmdline.seed)
    initConfig(cmdline)
    guess_len = int(len(cmdline.secret) / 2)

    population: list[Testcase]
    population = [Testcase() for _ in range(config["number_of_executions"])]
    population_size = len(population)

    # execute
    for epoch in range(cmdline.epochs):
        max_score = 0
        testcase: Testcase
        # run all the testcases
        for testcase in tqdm(population, desc="Epoch %d"%epoch):
            filename = str(uuid.uuid4())
            args = testcase.to_args()
            popen = subprocess.Popen(args, stdout=subprocess.PIPE)
            popen.wait()
            output = popen.stdout.read()
            # TODO check return code?
            # print(popen.returncode)
            # print(output)

            # write findings to file
            try:

                saveFindings(filename, args, output)


                # analyse findings
                testcase.score = parseFindings(filename, args, output,cmdline.secret,cmdline.reps,cmdline.number_of_guesses)
                max_score = max(max_score, testcase.score)
            except:
                pass
        
        # sort the population by score
        #print("sorting")
        population.sort(key=lambda t: t.score, reverse=True)
        new_population: list[Testcase]
        new_population = []
        
        # keep the best parents
        new_population += population[0: int(population_size*POPULATION_RETENTION)]
        #print(f"len new population: len(new_population))")
        #print("mutating")
        # mutate the best parents into new childs
        for i in range(int(population_size * POPULATION_CHILDS)):
            if int(population_size*POPULATION_RETENTION) != 0:
                parent_idx = i % int(population_size*POPULATION_RETENTION)
            else:
                parent_idx = i % population_size
            print(parent_idx)
            #print(population)
            new_population.append(population[parent_idx].mutate())
            #print("after mutate and append")
        
        #print("repopulation")
        # add some new fully randomic elements
        for i in range(int(population_size * POPULATION_RANDOMIC)):
            new_population.append(Testcase())

        #print(len(new_population))
        population = new_population
        print("Max score: " + str(max_score))

    # visualize
    visualize()

main()
