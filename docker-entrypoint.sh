#!/bin/sh
# Fix /data permissions when mounted from host (runs as root)
if [ -d /data ]; then
    HOST_UID=$(stat -c %u /data 2>/dev/null || echo "0")
    HOST_GID=$(stat -c %g /data 2>/dev/null || echo "0")
    RATS_UID=$(id -u rats 2>/dev/null || echo "0")

    if [ "$HOST_UID" = "0" ]; then
        # /data owned by root (fresh volume) — chown to rats
        chown rats:rats /data 2>/dev/null || true
    elif [ "$HOST_UID" != "$RATS_UID" ]; then
        # /data owned by host user — sync rats uid/gid to match host
        usermod -u "$HOST_UID" rats 2>/dev/null
        groupmod -g "$HOST_GID" rats 2>/dev/null
    fi
fi

# Switch to rats user and run CMD
exec runuser -u rats -- "$@"
