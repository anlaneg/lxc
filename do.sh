#! /bin/bash
./autogen.sh
CFLAGS="-g -O0" ./configure --disable-doc
CFLAGS="-g -O0" make

#create debian container
#lxc-create -n debian -t /usr/local/share/lxc/templates/lxc-download -- --dist debian --release stretch --arch amd64

