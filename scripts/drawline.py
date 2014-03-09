#!/usr/bin/python3

import argparse
from os import mkdir,path
from matplotlib import pyplot as plt
import re

def parse_one_line(line):
    ar=line.split('\t')
    isConstant=ar[0][:8]=='Constant'
    end=ar[0].find(')')
    count=int(ar[0][9:end])
    #remove last \n
    compact=tuple(ar[1].split(',')[:-1])
    content=[[],[]] #x and y
    x=0
    for item in compact:
        c = re.findall('(\d+)<repeat (\d+) times>',item)
        if c:
            content[0]+=[x]
            content[1]+=[int(c[0][0])]
            x+=int(c[0][1])
            content[0]+=[x-1]
            content[1]+=[int(c[0][0])]
        else:
            content[0]+=[x]
            content[1]+=[int(item)]
            x+=1
    return (isConstant,count,content)

def main():
    parser = argparse.ArgumentParser(description="draw lines for value profile processed by llvm-prof -value-content")
    parser.add_argument('filepath',type=str)
    parser.add_argument('outdir',type=str)
    args = parser.parse_args()
    print(args)
    outdir=args.outdir
    if not path.isdir(outdir):
        mkdir(outdir)

    f=open(args.filepath)
    idx=0
    for line in f:
        data = parse_one_line(line)
        print('#',idx)
        if len(data[2][0])==1:
            plt.plot(data[2][0],data[2][1],'.')
        else:
            plt.plot(data[2][0],data[2][1],'-')
        plt.savefig(path.join(outdir,str(idx)+".png"))
        plt.close()
        idx+=1

if __name__ == '__main__':
    main()
