#!/bin/sh
while true
do
ps -ef | grep "xybombservice" | grep -v "grep"
if [ "$?" -eq 1 ]
then
date >> /usr/local/src/server-source/xyservice/log/restart.log
nohup '/usr/local/src/server-source/xyservice/xybombservice' &
fi
sleep 20
done
