#! /bin/bash
./autogen.sh
CFLAGS="-g -O0" ./configure --disable-doc
CFLAGS="-g -O0" make

