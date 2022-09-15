#!/usr/bin/python3

import sys
import os
import numpy 

if(len(sys.argv) != 3):
    print(f"Usage: python3 {sys.argv[0]} <fill_value>")
    os._exit(1)

with open(sys.argv[1],"rb") as f:
    content = f.read()

content = content.replace(sys.argv[2].encode()*0xdff,os.urandom(0xdff))
print(content)

with open(sys.argv[1]+"_rand","wb") as f:
    f.write(content)
