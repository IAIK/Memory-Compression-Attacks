#http://billauer.co.il/blog/2016/05/linux-cgroups-swap-quartus/

name_grp=$1

# Delete
sudo cgdelete -g memory:$name_grp

# Create Cgroup:
sudo cgcreate -a $USER:$USER -t $USER:$USER -g:memory:$name_grp
# This creates the /sys/fs/cgroup/memory/small_mem/ subdirectory

echo "$((120*1024))" > /sys/fs/cgroup/memory/$name_grp/memory.limit_in_bytes

# Get details about processes in cgroup
echo -n "MemSize Limit for $name_grp: "
cat /sys/fs/cgroup/memory/$name_grp/memory.limit_in_bytes

# Start a program
echo "Run a program with cgroup 'cgexec -g memory:$name_grp ./test'"

# OR Move a existing program (needs sudo)
# cgclassify -g memory:small_mem 4550

# indeed, the process has joined the group (as non-root):
# cat /sys/fs/cgroup/memory/small_mem/cgroup.procs
# 4550

# Set the memory limit to  8MB
# cat /sys/fs/cgroup/memory/small_mem/memory.limit_in_bytes
# 18446744073709551615
