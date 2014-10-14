#!/usr/bin/python3
# 通过Value-To-Edge 和 Edge 来比较差异. 使用算数平均. 

import argparse
import re

def perror(*args, **keys):
    import sys.stderr
    print(args, keys, file=sys.stderr)
    exit(-1)

def calc_average(v2e_map, e_map):
    print("### following predication exceeds too much ###")
    part_n = 0
    total_n = 0
    total = 0.0
    part = 0.0
    for k in v2e_map.keys() & e_map.keys():
        predict = int(v2e_map[k])
        real = int(e_map[k])
        diff = abs(predict - real)/real
        if diff > 2:
            print("predicted:", predict, "\treal:", real, "\t rate:", diff, "\t", k)
        else:
            part += diff
            part_n += 1
        total += diff
        total_n += 1
    print()
    print("### general report ###")
    # 平均公式选用算数平均.
    print("total diverse rate:", round(total/total_n*100,2), "%")
    print("diverse rate without exceeded predication:", round(part/part_n*100, 2), "%") #去除差别过大的再求平均

def read_profiling(filename):
    handle = open(filename, 'r')
    anchor_ready = False # found begin position in text
    data = {}
    pattern = re.compile(r'(\d+)/[0-9.e+]+\s+(.+)')
    for line in handle:
        if line.startswith('Sorted executed basic blocks'):
            anchor_ready = True
        if anchor_ready:
            m = pattern.search(line)
            if m == None:
                continue
            # freq == m.group(1)
            # name == m.group(2)
            data[m.group(2)] = m.group(1)
    handle.close()

    if not anchor_ready:
        return None
    return data

def main():
    parser = argparse.ArgumentParser(description = 'caculate average compare between value-to-edge and edge profiling')
    parser.add_argument('value_to_edge', type=str)
    parser.add_argument('edge', type=str)
    args = parser.parse_args()

    v2e_map = read_profiling(args.value_to_edge)
    e_map = read_profiling(args.edge)
    if e_map == None or v2e_map == None:
        perror('file format unmatch, please use ``llvm-prof -list-all`` to generate output')
    calc_average(v2e_map, e_map)

main()
