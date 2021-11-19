#!/bin/bash
# A simple test to regex match and diff all the files found.
for filename in $1; do
  echo $filename
  ./ft_nm $2 $filename > diff1.txt 2> /dev/null
  nm $2 $filename > diff2.txt 2> /dev/null
  diff diff1.txt diff2.txt
done