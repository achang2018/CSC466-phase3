#!/bin/bash

_dir="results"
make clean
make
cd tests
rm -r ${_dir}
_tfiles="$(ls -I '*.*')"
mkdir ${_dir}
cd ..

for f in $_tfiles
do
        echo ">> $f"
        timeout 15 ./tests/${f} | tee tests/${_dir}/${f}.txt
done