#!/bin/bash

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
