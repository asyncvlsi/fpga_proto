#!/bin/bash

if [ "$#" -lt 3 ]; then
  echo "Usage: ./act2fpga act_file \"process_name<>\" \"output_path\""
  exit 1
fi

act_file=$1
process_name=$2
output_path=$3

chp2fpga -p "$2" -o "$3" $1

while read -r line
do
  file_name=$(echo "$line" | cut -d "<" -f 1)
  prs2fpga -p "$line" $1 -o "$3/$file_name.v" > /dev/null
done < "prs_list.txt"

rm prs_list.txt
