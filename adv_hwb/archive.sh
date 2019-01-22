#!/bin/bash

#clean hwb code and remove files
sed -i '/adv_hwb/d' $@
rm -rf adv_hwb

#create archive files
name=$(basename "$(pwd)")

files=""
for i in *; do
    files=$files"$name/$i "
done

cd ..
tar zcvf "${name}_src.tgz" $files > /dev/null 2>&1
