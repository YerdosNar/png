#!/usr/bin/env bash

set -xe

CC=gcc
FLAGS="-lz -lm -Wall -Wextra"
$CC png.c -o png $FLAGS

if [[ $# -eq 1 ]]; then
    ./png $1
else
     ./png
fi
