#!/usr/bin/env bash

set -xe

CC=gcc
FLAGS="-lz -lm -Wall -Wextra"
TARGET=png
FILES=src/*
$CC -o $TARGET $FILES $FLAGS

# if [[ $# -eq 1 ]]; then
#     ./png $1
# else
#      ./png
# fi
