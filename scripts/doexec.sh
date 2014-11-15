#!/bin/bash

i=0
while [ $i -lt 1000 ];
do
   ./test-tsc/test           >> allinone.data
   ./test-fadd/test          >> allinone.data
   ./test-load/test          >> allinone.data
   ./test-loadc/test         >> allinone.data
   ./test-loadse/test        >> allinone.data
   ./test-loadwithmix/test   >> allinone.data
   ./test-loadwithrand/test  >> allinone.data
   ./test-loadwithorder/test >> allinone.data
   ./test-loadcwithmix/test   >> allinone.data
   ./test-loadcwithrand/test  >> allinone.data
   ./test-loadcwithorder/test >> allinone.data

   let i++
done
