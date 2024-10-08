#!/bin/bash

# This script is used to format the code in the repository.
find src/ -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i
find tst/ -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i
# find src/ -iname '*.h' -o -iname '*.cpp' | xargs clang-tidy -checks='-*,modernize-concat-nested-namespaces' -fix
