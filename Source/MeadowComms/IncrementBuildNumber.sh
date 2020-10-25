#!/bin/bash

declare next_version

increment_version ()
{
  declare -a part=( ${1//\./ } )
  declare -i carry=1

  for (( CNTR=${#part[@]}-1; CNTR>=0; CNTR-=1 )); do
    len=${#part[CNTR]}
    next_version=$((part[CNTR]+carry))
    [ ${#next_version} -gt $len ] && carry=1 || carry=0
    [ $CNTR -gt 0 ] && part[CNTR]=${next_version: -len} || part[CNTR]=${next_version}
  done
  next_version="${part[*]}"
} 

input="version.txt"
output="next_version.txt"

read -r version < "$input"

increment_version $version

echo -e "${next_version// /.}" > "$output"

rm "$input"
mv "$output" "$input"
