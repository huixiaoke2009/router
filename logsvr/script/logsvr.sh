start()
{
    /data/knight/logsvr/bin/logsvr /data/knight/logsvr/conf/logsvr.ini
}

stop()
{
    killall logsvr
}

usage()
{
    echo "$0 <start|stop|restart>"
}

if [ "$1" = "start" ];then
    start

elif [ "$1" = "stop" ];then
    stop

elif [ "$1" = "restart" ];then
    stop
    start
else
    usage
fi

