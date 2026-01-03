#!/bin/sh
# aesdsocket start/stop/status script for assignment 5
# Init script for aesdsocket daemon

DAEMON=/usr/bin/aesdsocket
# DAEMON=$PWD/aesdsocket
NAME=aesdsocket
# PIDFILE=/var/run/aesdsocket.pid
PIDFILE=/var/tmp/aesdsocket.pid

start() {
    echo "Starting $NAME..."
    # start-stop-daemon -S -b -m -p "$PIDFILE" -x "$DAEMON" # the -b flag demonizes the program without parsing -d
    start-stop-daemon -S -n $NAME -x "$DAEMON" -- -d # status will not work cos there is no pidfile
    # start-stop-daemon -S -m -p "$PIDFILE" -x "$DAEMON" -- -d #uses the -d arg to demonize, status wont still work because the program demonizes late and start-stop-demon doest catch the childs pid
}

stop() {
    echo "Stopping $NAME..."
    start-stop-daemon -K -n $NAME
    # start-stop-daemon -K -p "$PIDFILE" --retry TERM/5
    # rm -f "$PIDFILE"
}

status() {
    if [ -f "$PIDFILE" ]; then
        pid=$(cat "$PIDFILE")
        if kill -0 "$pid" 2>/dev/null; then
            echo "$NAME is running (pid $pid)"
            return 0
        else
            echo "$NAME not running but PID file exists"
            return 1
        fi
    else
        echo "$NAME is not running"
        return 3
    fi
}

case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        start
        ;;
    status)
        status
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac

exit 0
