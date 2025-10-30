#!/bin/bash
set -e  # exit immediately if a command fails

# clean and rebuild
make clean
make

# run the program from bin
./bin/frost
