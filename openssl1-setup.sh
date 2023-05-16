#/bin/bash

ENV=$(PWD)

rm -rf openssl-build/ && \
wget https://www.openssl.org/source/openssl-1.1.1t.tar.gz && \
mkdir openssl-build && \
tar xvf openssl-1.1.1t.tar.gz -C openssl-build --strip-components 1 && \
cd openssl-build/ && \
mkdir ./build/ && \
./config --prefix=$ENV/openssl-build/build/ --openssldir=$ENV/openssl-build/build/ -Wl,-rpath,$ENV/openssl-build/build/ &&\
make && make install && \
cd .. && \
make -C cpp -f posix_openssl.mk
