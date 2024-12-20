#!/bin/bash
nsamples=1000
sleeptime=0.0001

for x in $(seq 1 $nsamples)
  do
    echo "Sample $x"
    gdb-multiarch -nx -ex "target extended-remote :1234" -ex "file Kernel" -ex "set pagination 0" -ex "thread apply all bt" -batch
    sleep $sleeptime
  done > results.txt 
  
  
"\
awk '
  BEGIN { s = ""; } 
  /^Thread/ { print s; s = ""; } 
  /^\#/ { if (s != "" ) { s = s "," $4} else { s = $4 } } 
  END { print s }' | \
sort | uniq -c | sort -r -n -k 1,1
"
