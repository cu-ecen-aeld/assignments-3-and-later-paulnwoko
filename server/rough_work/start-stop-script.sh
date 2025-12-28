#!/bin/sh
### BEGIN INIT INFO
# Provides:          aesdsocket
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     90 2 3 4 5
# Default-Stop:      10 0 1 6
# Short-Description: Start/stop aesdsocket daemon
### END INIT INFO

DAEMON="/usr/bin/aesdsocket"
NAME="aesdsocket"
PIDFILE="/var/run/aesdsocket.pid"

start() {
    echo "Starting $NAME..."

    if [ -f "$PIDFILE" ]; then
        if kill -0 "$(cat $PIDFILE)" 2>/dev/null; then
            echo "$NAME already running"
            return 0
        else
            echo "Removing stale PID file"
            rm -f "$PIDFILE"
        fi
    fi

    # Start the program and let it daemonize (after binding)
    start-stop-daemon -S -m -p "$PIDFILE" -x "$DAEMON" -- -d

    sleep 1

    if [ -f "$PIDFILE" ]; then
        echo "$NAME started with PID $(cat $PIDFILE)"
    else
        echo "Failed to start $NAME"
        return 1
    fi
}

stop() {
    echo "Stopping $NAME..."

    if [ ! -f "$PIDFILE" ]; then
        echo "$NAME is not running"
        return 0
    fi

    start-stop-daemon -K -p "$PIDFILE" --retry TERM/5
    RET=$?

    rm -f "$PIDFILE"

    return $RET
}

status() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if kill -0 "$PID" 2>/dev/null; then
            echo "$NAME is running (pid $PID)"
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



#!/bin/sh
# Init script for aesdsocket daemon
# Installs to: /etc/init.d/S99aesdsocket

DAEMON=/usr/bin/aesdsocket
PIDFILE=/var/run/aesdsocket.pid
NAME=aesdsocket

start() {
    echo "Starting $NAME..."

    # If already running, do nothing
    if [ -f "$PIDFILE" ]; then
        pid=$(cat "$PIDFILE" 2>/dev/null)
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            echo "$NAME already running (pid $pid)"
            return 0
        fi
    fi

    # Use busybox start-stop-daemon
    start-stop-daemon -S -b -m \
        -p "$PIDFILE" \
        -x "$DAEMON" -- -d

    if [ $? -eq 0 ]; then
        echo "$NAME started"
    else
        echo "Failed to start $NAME"
        return 1
    fi
}

stop() {
    echo "Stopping $NAME..."

    if [ ! -f "$PIDFILE" ]; then
        echo "$NAME not running"
        return 0
    fi

    pid=$(cat "$PIDFILE" 2>/dev/null)
    if [ -z "$pid" ]; then
        rm -f "$PIDFILE"
        echo "No PID found, cleaned up"
        return 0
    fi

    # Send SIGTERM for graceful shutdown
    kill -TERM "$pid" 2>/dev/null

    # Wait for exit
    for i in 1 2 3 4 5; do
        if kill -0 "$pid" 2>/dev/null; then
            sleep 1
        else
            break
        fi
    done

    # If still alive, force kill
    if kill -0 "$pid" 2>/dev/null; then
        echo "Process still running, sending SIGKILL"
        kill -KILL "$pid" 2>/dev/null
    fi

    rm -f "$PIDFILE"
    echo "$NAME stopped"
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
        if [ -f "$PIDFILE" ]; then
            pid=$(cat "$PIDFILE" 2>/dev/null)
            if kill -0 "$pid" 2>/dev/null; then
                echo "$NAME running (pid $pid)"
                exit 0
            fi
        fi

        echo "$NAME not running"
        exit 1
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac
