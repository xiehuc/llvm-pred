#!/usr/bin/python3
# 通过Value-To-Edge 和 Edge 来比较差异. 使用算数平均. 

import argparse
import re

def perror(*args, **keys):
    from sys import stderr
    print(file=stderr, *args, **keys)
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
        diff = abs(predict - real)/min(real, predict)
        if diff > 2:
            print("predicted:%7d real:%7d rate:%.2f\t%s" % (predict, real, diff, k))
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
    anchor_ready = cmd_ready = False # found begin position in text
    data = {}
    pattern = re.compile(r'(\d+)/[0-9.e+]+\s+(.+)')
    is_v2e = True 
    for line in handle:
        if anchor_ready:
            m = pattern.search(line)
            if m == None:
                continue
            # freq == m.group(1)
            # name == m.group(2)
            data[m.group(2)] = m.group(1)
        if cmd_ready: 
            print(line)
            is_v2e = line.strip('\r\n') == ""
            cmd_ready = False
        if line.startswith('Sorted executed basic blocks'):
            anchor_ready = True
        if line.startswith('LLVM profiling output for execution'):
            cmd_ready = True
    handle.close()

    if not anchor_ready:
        return None
    return data, is_v2e

def main():
    parser = argparse.ArgumentParser(description = 'caculate average compare between value-to-edge and edge profiling')
    parser.add_argument('value_to_edge', type=str)
    parser.add_argument('edge', type=str)
    args = parser.parse_args()

    v2e_map, yes_v2e = read_profiling(args.value_to_edge)
    e_map, no_v2e = read_profiling(args.edge)
    print(yes_v2e, no_v2e)
    if e_map == None or v2e_map == None:
        perror('file format unmatch, please use ``llvm-prof -list-all`` to generate output')
    if yes_v2e == no_v2e:
        perror('arg order unmatch, first arg should be value_to_edge, second should be edge')
    if yes_v2e == False: # swap order
        v2e_map, e_map = e_map, v2e_map
    calc_average(v2e_map, e_map)

main()
