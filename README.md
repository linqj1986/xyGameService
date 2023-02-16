# xyGameService
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
# 安装
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
https://github.com/Cylix/cpp_redis/wiki/Mac-&-Linux-Install
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
make install
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
# gdb调试定位Segmentation fault

gdb xybombservice

(gdb)run
```
Thread 5 "xybombservice" received signal SIGSEGV, Segmentation fault.
[Switching to Thread 0x7ffff61ad700 (LWP 238048)]
0x00005555555da436 in CCustomerService::CheckAlive (this=0x555555687440 <g_oDataService>) at CustomerService.cpp:756
756		int x = *error;
(gdb) where
#0  0x00005555555da436 in CCustomerService::CheckAlive (this=0x555555687440 <g_oDataService>) at CustomerService.cpp:756
#1  0x00005555555d9780 in CCustomerService::CheckAliveThread (param=0x555555687440 <g_oDataService>) at CustomerService.cpp:610
#2  0x00007ffff7f91609 in start_thread (arg=<optimized out>) at pthread_create.c:477
#3  0x00007ffff7af0133 in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95
(gdb) break 756
Breakpoint 1 at 0x5555555da42f: file CustomerService.cpp, line 756.
(gdb) r
The program being debugged has been started already.
Start it from the beginning? (y or n) y
Starting program: /home/admin/xygame/xyservice/xybombservice 
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
[New Thread 0x7ffff79b0700 (LWP 238057)]
[New Thread 0x7ffff71af700 (LWP 238058)]
[New Thread 0x7ffff69ae700 (LWP 238059)]
[New Thread 0x7ffff61ad700 (LWP 238060)]
[New Thread 0x7ffff59ac700 (LWP 238061)]
[New Thread 0x7ffff51ab700 (LWP 238062)]
[New Thread 0x7ffff49aa700 (LWP 238063)]
[New Thread 0x7fffeffff700 (LWP 238064)]
[New Thread 0x7fffef7fe700 (LWP 238065)]
[New Thread 0x7fffeeffd700 (LWP 238066)]
[New Thread 0x7fffee7fc700 (LWP 238067)]
[Thread 0x7fffee7fc700 (LWP 238067) exited]
[Switching to Thread 0x7ffff61ad700 (LWP 238060)]

Thread 5 "xybombservice" hit Breakpoint 1, CCustomerService::CheckAlive (this=0x555555687440 <g_oDataService>) at CustomerService.cpp:756
756		int x = *error;
(gdb) print error
$1 = (int *) 0x0
(gdb) print x
$2 = 1610060800
(gdb) print *error
Cannot access memory at address 0x0

```


