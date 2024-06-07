#!/usr/bin/bash

pushd build;
make -f ../Makefile;
popd;
./build/cd small;
