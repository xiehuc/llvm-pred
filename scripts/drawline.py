#!/usr/bin/python3

import argparse
from os import mkdir,path
from matplotlib import pyplot as plt

def parse_one_line(line):
    ar=line.split('\t')
    isConstant=ar[0][:8]=='Constant'
    end=ar[0].find(')')
    count=int(ar[0][9:end])
    #remove last \n
    content=tuple(map(int,ar[1].split(',')[:-1]))
    if not isConstant:
        assert(len(content)==count)
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
        plt.plot(data[2],'.')
        plt.savefig(path.join(outdir,str(idx)+".png"))
        plt.close()
        idx+=1

if __name__ == '__main__':
    main()
