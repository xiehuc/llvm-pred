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
    x=0
    for item in compact:
        c = re.findall('(\d+)<repeat (\d+) times>',item)
        if c:
            plt.plot([x,x+int(c[0][1])-1],[int(c[0][0]),int(c[0][0])],'-b')
            x+=int(c[0][1])
        else:
            plt.plot([x],[int(item)],'.b')
            x+=1
    return (isConstant,count,compact)

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
        print('#',idx)
        plt.hold(True)
        parse_one_line(line)
        plt.hold(False)
        plt.margins(0.05)
        plt.savefig(path.join(outdir,str(idx)+".png"))
        plt.close()
        idx+=1

if __name__ == '__main__':
    main()
