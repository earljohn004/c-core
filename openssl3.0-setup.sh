#/bin/bash

ENV=$(PWD)

rm -rf openssl-build/ && \
wget https://www.openssl.org/source/openssl-3.0.8.tar.gz && \
mkdir openssl-build && \
tar xvf openssl-3.0.8.tar.gz -C openssl-build --strip-components 1 && \
cd openssl-build/ && \
mkdir ./build/ && \
./config --prefix=$ENV/openssl-build/build/ --openssldir=$ENV/openssl-build/build/ -Wl,-rpath,$ENV/openssl-build/build/ shared no-idea no-mdc2  &&\
make && make install && \
cd .. && \
USE_OPENSSL=1 PUBNUB_CRYPTO_API=1  USE_GZIP_COMPRESSION=0 RECEIVE_GZIP_RESPONSE=0 USE_SUBSCRIBE_V2=1 USE_ADVANCED_HISTORY=1 USE_OBJECTS_API=0 USE_ACTIONS_API=0 USE_AUTO_HEARTBEAT=0 USE_GRANT_TOKEN=1 USE_REVOKE_TOKEN=0 USE_DNS_SERVERS=0 USE_PROXY=0 ONLY_PUBSUB_API=0 PUBNUB_RAND_INIT_VECTOR=1 make -C cpp -f posix_openssl.mk

