#!/bin/bash

echo "1..2"

function ok()
{
    local message=$1 status=$?;
    if [ $status -eq 0 ]; then
        status="ok"
    else
        status="not ok"
    fi
    if [ ! -z "$message" ]; then
        echo "$status $message" 
    else
        echo "$status"
    fi
}

result=$(echo " " | t/bin/server)
ok "server exited normally"
[ $result == "RUNNING" ]; ok "server state clean"
