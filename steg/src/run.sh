#!/usr/bin/env bash

set -xe

gcc -o main main.c png_io.c image.c utils.c
mv main ../
cd ..
./main out1.png out.png $1

rm main
