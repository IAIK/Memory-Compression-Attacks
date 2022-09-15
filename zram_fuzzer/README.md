# ZRAM

How to enable zRAM
The zRAM module is controlled by systemd, so there's no need for an fstab entry. And since everything is already installed out of the box, we only need to create a few files and modify one. 

Open a terminal window and create a new file with the command:

`sudo nano /etc/modules-load.d/zram.conf`
In that file, add the word:

`zram`

Save and close the file. 

Next, create a second new file with the command:

`sudo nano /etc/modprobe.d/zram.conf`
In that file, paste the line:

`options zram num_devices=1`

Save and close the file.

Next, we need to configure the size of the zRAM partition. Create a new file with the command:

`sudo nano /etc/udev/rules.d/99-zram.rules`

In that file, paste the following (modifying the disksize attribute to fit your needs):

`KERNEL=="zram0", ATTR{disksize}="512M",TAG+="systemd"`

Save and close the file.

How to disable traditional swap
In order for zRAM to function, you're going to need to disable the traditional swap. This is handled within the fstab file. Open that file with the command:

`sudo nano /etc/fstab`

In that file, comment out (add a leading # character) the line starting with /swap.img.

Save and close the file.

How to create a systemd unit file
In order for zRAM to run, we need to create a systemd unit file. Create this file with the command:


`sudo nano /etc/systemd/system/zram.service`

In that file, paste the following contents:

```
[Unit]
Description=Swap with zram
After=multi-user.target

[Service]
Type=oneshot 
RemainAfterExit=true
ExecStartPre=/sbin/mkswap /dev/zram0
ExecStart=/sbin/swapon /dev/zram0
ExecStop=/sbin/swapoff /dev/zram0

[Install]
WantedBy=multi-user.target
Save and close the file. 
```
Enable the new unit with the command:

`sudo systemctl enable zram`

Reboot the machine.

How to find out if zRAM is working
Once the system reboots, log back in. From a terminal window, issue the command:

`cat /proc/swaps`

You should now see that /dev/zram0 is handling your swap

# Cgroups

## Install Cgroup tools

`apt-get install cgroup-tools`

## Create group and add user

sudo addgroup $USER
sudo adduser $USER $USER

## Create Cgroup:

`sudo cgcreate -a $USER:$USER  -t $USER:$USER -g:memory:small_mem`

```
## This creates the /sys/fs/cgroup/memory/small_mem/ subdirectory

# indeed, the process has joined the group (as non-root):
# cat /sys/fs/cgroup/memory/small_mem/cgroup.procs
# 4550

# Set the memory limit to  8MB
# cat /sys/fs/cgroup/memory/small_mem/memory.limit_in_bytes
# 18446744073709551615
```

`echo "$((8*1024*1024))" > /sys/fs/cgroup/memory/small_mem/memory.limit_in_bytes`


## Start a program

 `cgexec -g memory:small_mem ./test`

OR Move a existing program (needs sudo)

`cgclassify -g memory:small_mem 4550`

Get details about processes in cgroup:

`watch cat /sys/fs/cgroup/memory/small_mem/cgroup.procs`


### Change ZRAM algorithm

``` 
Check Algorithm:
zramctl | tail -n1 | awk '{print $2}'

Change Algorithm:
sudo swapoff /dev/zram0
sudo zramctl -a <ALGO> -s 536870912 /dev/zram0
sudo mkswap /dev/zram0
sudo swapon /dev/zram0

Check if Algorithm Changed:
cat /proc/swaps 
zramctl
```




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
usage: fuzzer.py [-h] [-n NUMBER] [-c CPU] -f FINDINGS_FOLDER [-l LIBRARY] [-e EPOCHS] [-s SEED] [-x SECRET] [-r REPS] [-g CGROUP] [-ng NUMBER_OF_GUESSES]

optional arguments:
  -h, --help            show this help message and exit
  -n NUMBER, --number NUMBER
                        number of executions
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
  -g CGROUP, --cgroup CGROUP
                        cgroup name
  -ng NUMBER_OF_GUESSES, --number_of_guesses NUMBER_OF_GUESSES
                        number_of_guesses
```

For example `python3 ./fuzzer.py -c 3 -e 100 -n 1000 -f findings -l pglz -g 26 -s 0 ` runs the fuzzer on core 3, for 100 epochs, each epoch of 1000 samples. It saves the results and summary of the results in the `findings` folder and fuzzes the `PGLZ` decompression algorithm, with 26 guesses per test. This using a random seed of 0 for reproducibility.
Notice that the `-g` argument has to match the value of `NUMBER_OF_GUESSES` in `test.c`.


### ZRAM-Fuzzing
After setting up ZRAM properly run run `make zram` to generate the test target `test_zram`.

In the ZRAM case you can use the provided `run_fuzzer.sh` script to starting fuzzing (Uses 8 concurrent instances of fuzzing).
