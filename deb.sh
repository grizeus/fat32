#!/usr/bin/bash

pushd build;
make -f ../Makefile;
popd;
./build/format small;
./build/mkdir small directory;
./build/dir small ls -l;
mdir -i small ::;
