#!/bin/bash

set -e

#this clears anything out that we might have put in before
printf "\033c"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B build

printf "\033c"
pushd build
ninja

printf "\033c"
ctest

set +e
