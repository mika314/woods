#!/bin/bash
set -x
for bin in server client; do
    cd $bin
    echo Entering directory \`$bin\'
    coddle release || exit 1
    echo Leaving directory \`$bin\'
    cd ..
done
