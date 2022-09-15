#Ensure  arguments supplied.
if [ $# -eq 0 ]
  then
      echo "No arguments supplied"
      exit
fi
algo=$1

#Check Previous Algorithm:
echo "Before: "
zramctl | tail -n1 | awk '{print $2}'

## Change Algorithm:
sudo swapoff /dev/zram0
sudo zramctl -a $algo -s 536870912 /dev/zram0
sudo mkswap /dev/zram0
sudo swapon /dev/zram0

# Check if Algorithm Changed:
echo "After:"
cat /proc/swaps 
zramctl




