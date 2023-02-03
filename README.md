﻿# xyGameService
linux c++ server

# 环境
```
redis-4.0.2
cpp_redis-master
curl-master
gcc-4.8.5.tar
jsoncpp.tar
libevent-2.1.8-stable.tar
```

```
https://github.com/curl/curl/releases
https://curl.se/docs/install.html
cd curl-7.17.1
./configure --without-openssl
make
make install
--------------------------------------
https://github.com/open-source-parsers/jsoncpp/wiki/Building
mkdir -p build/debug
cd build/debug
cmake -DCMAKE_BUILD_TYPE=debug -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF -DARCHIVE_INSTALL_DIR=. -G "Unix Makefiles" ../..
make
--------------------------------------
https://github.com/redis/hiredis
cd hiredis-1.1.0/
make
make install
--------------------------------------
libevent-2.1.8-stable.tar.gz
cd libevent-2.1.8-stable
./configure -prefix=/usr
sudo make && make install
--------------------------------------
boost：sudo apt-get install libboost-dev
--------------------------------------
ldconfig
```