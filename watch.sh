#!/bin/bash

# watch for changes in the source files and run the init script
(find src tst -name "*.h" -o -name "*.cpp"; echo CMakeLists.txt) | entr -d ./init.sh
